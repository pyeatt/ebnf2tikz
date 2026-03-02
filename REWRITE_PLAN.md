# ebnf2tikz Complete Rewrite Plan

## 1. Current Codebase Diagnosis

### Source File Summary (5,841 lines total)

| File | Lines | Role |
|------|-------|------|
| `graph.hh` | 520 | AST node class hierarchy |
| `graph.cc` | 912 | AST node implementations, clone, dump, constructors |
| `optimize.cc` | 1128 | Fixed-point optimization passes |
| `layout.cc` | 1204 | Geometry computation, coordinate placement |
| `parser.yy` | 494 | Bison grammar, AST construction |
| `subsume.cc` | 243 | Regex-based production inlining |
| `output.cc` | 209 | Overwide detection, wrapping, orchestration |
| `annot_parser.yy` | 175 | Annotation block parsing |
| `tikzwriter.cc` | 169 | TikZ command emission |
| `main.cc` | 137 | CLI argument parsing, program lifecycle |
| `lexer.ll` | 132 | Flex tokenizer for EBNF |
| `nodesize.hh` | 125 | Node dimension cache (header-only) |
| `annot_lexer.ll` | 94 | Flex tokenizer for annotations |
| `driver.hh` | 75 | Parser driver class declaration |
| `layout.hh` | 71 | Layout data structures |
| `tikzwriter.hh` | 53 | TikzWriter class declaration |
| `driver.cc` | 49 | Parser driver implementation |
| `util.cc` | 42 | `camelcase()` utility |
| `util.hh` | 9 | Utility header |

### What Works Well

- The **two-parser architecture** (main EBNF + annotation) is sound and keeps
  concerns separate.
- The **composite node pattern** is a reasonable AST representation with clean
  separation of single-child and multi-child nodes.
- The **layout/tikzwriter separation** (done in the recent backend rewrite) is
  clean: layout computes geometry, tikzwriter emits TikZ, output.cc orchestrates.
- The **test infrastructure** (167 regression tests with AST dumps plus a
  PDF-generation target) provides a solid safety net for any rewrite.
- **Detailed location tracking** in the lexer enables precise error messages.
- **Regex-based subsumption** allows flexible production matching.

### Systemic Problems

#### P1: Manual Memory Management Everywhere

All nodes are `new`'d and `delete`'d by hand. The `forgetChild()` method orphans
a node without deleting it, relying on the caller to `delete` later. The optimizer
has ~30 mutation sites that each manually maintain pointer invariants. A single
missed `delete` leaks; a single wrong `delete` crashes.

Specific issues:
- `multinode` destructor loops through `nodes` vector deleting children
- Clone-on-insert pattern in subsumption creates temporary copies
- No RAII or smart pointers anywhere
- Stack-based destruction can miss orphaned grandchildren in error paths

#### P2: Rail Nodes Are Structural, Not Semantic

Rails (`railnode`) are inserted into the AST by the parser to represent visual
connectors, but they carry no semantic meaning. They contaminate the AST with
layout concerns, forcing every optimization pass to understand and preserve rail
invariants.

The parser has a known bug (`setLeftRail` instead of `setRightRail` at line 445
of `parser.yy`) that isn't fixed at the source but instead worked around in the
optimizer via `clearRailsRecursive()`. This function uses heuristics to
cross-check both `leftrail` and `rightrail` against both parameters.

Rail-related complexity is scattered across:
- `parser.yy`: Rail insertion at 4+ locations (lines 272-273, 375-378, 415-419, 434-437)
- `graph.cc`: `multinode` copy constructor rewires rails during cloning
- `optimize.cc`: `clearRailsRecursive()`, `replaceRailRecursive()`, `mergeRails()`
- `layout.cc`: Rail padding decisions in `computeSizeChoice`, `layoutChoice`, `connectChoice`

#### P3: Global Mutable Static State

Node name counters (`nextNode()`, `nextCoord()`) and the `nodesizes` instance
are process-wide statics. This means:
- Separate ebnf2tikz invocations produce colliding names, requiring the
  concatenation hack in `build_tikz_tests.sh`
- Code is untestable in isolation
- Impossible to parallelize
- `node::sizes` is a global singleton with no locking

#### P4: Optimization Passes Deeply Entangled with Tree Structure

`analyzeOptLoops` and `analyzeNonOptLoops` are 300+ lines each with cyclomatic
complexity ~40. They manually extract children, clone subtrees, rewire rail
pointers, reparent nodes, and delete wrappers — all in one monolithic function.

The three-tier fixed-point loop ordering is undocumented and fragile:
- Tier 1: `mergeChoices` alone (must flatten before loop analysis)
- Tier 2: `mergeConcats` + `liftConcats` (clean up after mergeChoices)
- Tier 3: `analyzeNonOptLoops` + `mergeConcats` + `liftConcats` + `analyzeOptLoops` + `flattenLoopChoices`

Changing the order breaks semantics with no documentation explaining why.

#### P5: No Const Correctness, No Type Safety

- `dynamic_cast` results aren't null-checked in many places
- Rail pointers are raw `node*` that should be `railnode*`
- `operator==` has inconsistent semantics across node types:
  - `nontermnode::operator==` compares style, format, and str
  - `railnode::operator==` only compares direction, ignoring position
- `void*` is used to pass location objects between parsers
- Methods that only read (e.g., `dump()`) don't take `const` arguments

#### P6: No Error Recovery or Semantic Validation

- The parser dies on the first syntax error (no `error` production rules)
- No verification that all referenced nonterminals have definitions
- No cycle detection in subsume patterns
- Annotation keys not validated (typo `"sideays"` silently ignored)
- File I/O failures call `exit()` directly with no recovery
- `regcomp()` failure exits with `exit(2)` — no logging, no recovery
- Silent failures in annotation parsing (error printed but execution continues)

#### P7: Header-Only nodesize.hh with C-Style Parsing

The `bnfnodes.dat` reader uses character-by-character comma skipping, has no
input validation, and accepts negative dimensions silently. Being header-only
adds to build time since it's included in every `.cc` file.

---

## 2. Detailed Current Code Analysis

### 2.1 Lexer and Parser (`lexer.ll`, `parser.yy`)

**Architecture**: Clean two-parser design with proper AST generation and location
tracking via `YY_USER_ACTION` macro that manually updates line/column for each
token.

**Issues**:
- **Rail insertion scattered**: `LPAREN`/`RPAREN`, `LBRACK`/`RBRACK`,
  `LBRACE`/`RBRACE` all insert rails with nearly identical code
- **Row handling is convoluted**: Complex nested conditionals in lines 218-294
  checking `is_concat()`, managing `beforeSkip`/`drawToPrev`
- **Manual child migration**: Code manually moves children from parsed `rows`
  concat to production concat (duplicated in two places, labeled "FIRST PLACE"
  and "SECOND PLACE")
- **Tab handling hardcoded**: Assumes 8-space tabs (line 78), not configurable
- **Precedence undocumented**: Five levels of operator precedence with no comments
  explaining the ordering

### 2.2 Annotation Parser (`annot_lexer.ll`, `annot_parser.yy`)

**Architecture**: Separate Flex/Bison pair with `-P annot` prefix for symbol
collision avoidance. Main lexer captures `<<-- ... -->>` text in state `<A>`,
then passes it to `scanAnnot()` for parsing.

**Issues**:
- `\".*\"` regex is greedy and doesn't handle escaped quotes
- `<A>"--"[^>]` and `<A>"-->"[^>]` patterns are fragile edge cases
- Location initialization/addition logic is unclear (lines 155-157)
- `annotmap` is `map<string,string>` with no key validation
- `"emusbussubsume"` magic sentinel string for subsume marker

### 2.3 Graph/AST Node Hierarchy (`graph.hh`, `graph.cc`)

**Class hierarchy**:
```
node (abstract base)
├── singlenode (single child wrapper)
│   ├── rownode (expression row)
│   └── productionnode (production definition)
├── multinode (multiple children)
│   ├── concatnode (sequential composition)
│   ├── choicenode (alternatives)
│   └── loopnode (repetition)
├── railnode (vertical connectors)
│   └── newlinenode (row separation)
├── nontermnode (leaf - nonterminal reference)
│   └── termnode (leaf - terminal string)
└── nullnode (leaf - zero-width placeholder)

grammar (container, NOT a node subclass)
```

**Issues**:
- Mutual pointer dependencies: nodes hold parent/sibling pointers AND parent
  holds children. During optimization, invalidating one breaks traversal.
- `concatnode` constructor auto-inserts rails (implicit behavior)
- `multinode` copy constructor rewires rails during cloning (only for adjacent
  rail→choice/loop pairs; other references remain stale)
- No separation between tree structure and layout — rails are part of the AST
- No invariant checking after optimization
- `operator==` has identity semantics in some subclasses, structural semantics
  in others

### 2.4 Optimizer (`optimize.cc`)

**Passes** (in execution order within the fixed-point loops):

| Pass | Input Pattern | Output Pattern | Complexity |
|------|---------------|----------------|------------|
| `mergeChoices` | `choice(choice(...))` or `concat(rail,choice(...),rail)` | Flattened choice | ~20 |
| `mergeConcats` | `concat(concat(...), ...)` | Flattened concat | ~15 |
| `liftConcats` | `(any, concat(single_child))` | Unwrapped child | ~10 |
| `analyzeNonOptLoops` | `rail_UP, loop(null, repeats), rail_UP` | `loop(body, simplified_repeats)` | ~45 |
| `analyzeOptLoops` | `rail_UP, choice(null, loop(null, ...)), rail_UP` | `loop(body, repeats)` | ~40 |
| `flattenLoopChoices` | `loop(body, choice(...))` | `loop(body, flattened_alts)` | ~10 |
| `mergeRails` | `concat(rail, choice/loop, ...)` | Unwrapped internal rail | ~10 |

**Critical complexity**: `analyzeNonOptLoops` and `analyzeOptLoops` each contain:
- Match counting that walks backward through parent concat elements
- Body extraction via deep clone with rail pointer clearing
- Repeat alternative stripping with multiple structural cases
- Null repeat reordering via `stable_partition`
- Choice-to-choiceloop flattening
- Body-choice-to-repeat conversion for optional contexts
- Rail rewiring throughout

### 2.5 Subsumption (`subsume.cc`)

**Algorithm**: For each production marked `subsume`, iterate all other
productions and replace matching nonterminal references with cloned production
bodies.

**Issues**:
- Bookend nullnode stripping assumes specific production structure
- Rail stripping after substitution mirrors optimizer logic (duplication)
- `fixSkips()` post-processing has complex parent/grandparent structure checks
- Each regex match creates a deep clone (memory overhead)

### 2.6 Layout (`layout.cc`, `layout.hh`)

**Architecture**: External tree walker with three phases:
1. `computeSize()` — recursive measurement without placement
2. `layoutNode()` — coordinate assignment
3. `computeConnections()` — polyline routing

**Issues**:
- Rail padding decisions replicated across `computeSize`, `layoutNode`, and
  `connectChoice` — must stay synchronized
- Complex conditional paths in `layoutConcat` (~200 lines of mixed concerns)
- Hard-coded constants: `3.0f * rowsep`, `0.55f` bias, `1.5f * singleH`
- Map lookup performance (`geom.find()` called repeatedly in loops)
- NULL parent pointer walk in `connectLoop` (line 1117) without guard
- `hasWrappableSpaces()` parses `texName()` output by string manipulation

### 2.7 TikZ Writer (`tikzwriter.cc`, `tikzwriter.hh`)

Clean module with minimal issues. Uses `rawName()` for node names, delegates
formatting to `operator<<` on `coordinate`.

### 2.8 Infrastructure (`main.cc`, `util.cc`, `nodesize.hh`, `driver.hh/cc`)

- `main.cc`: Standard `getopt_long` with clean option handling
- `util.cc`: `camelcase()` function converts identifiers to valid LaTeX command
  names (digits spelled out as words)
- `nodesize.hh`: Header-only with C-style file parsing
- `driver.hh`: Contains dead `nonterminals`/`terminals` set members

---

## 3. Proposed Architecture

### Design Principles

1. **Clean AST with no layout concerns.** Rails are layout artifacts, not grammar
   concepts.
2. **Owning smart pointers.** `std::unique_ptr<Node>` for tree ownership; raw
   pointers only for non-owning references (parent, siblings).
3. **Immutable AST after construction.** Optimization produces a *new* tree, not
   a mutation of the old one.
4. **Scoped naming.** Node IDs are production-scoped, not global counters.
5. **Visitor pattern for passes.** Tree-walking logic is external to the node
   classes.
6. **Strong typing.** Separate types for grammar entities vs. layout artifacts.

### Module Diagram

```
┌─────────────────────────────────────────────┐
│  main.cc  (CLI, orchestration)              │
├─────────────────────────────────────────────┤
│  Parsing Layer                               │
│  ┌─────────┐  ┌──────────┐  ┌───────────┐  │
│  │ lexer.ll│  │parser.yy │  │annot_*.ll/│  │
│  │         │  │          │  │annot_*.yy │  │
│  └────┬────┘  └────┬─────┘  └─────┬─────┘  │
│       └──────┬─────┘              │         │
│              ▼                    ▼         │
│         ┌─────────┐     ┌────────────┐      │
│         │  AST    │◄────│Annotations │      │
│         │(ast.hh) │     │(annot.hh)  │      │
│         └────┬────┘     └────────────┘      │
├──────────────┼──────────────────────────────┤
│  Semantic Layer                              │
│         ┌────┴────┐                          │
│         │Resolver │ (undefined-ref check,    │
│         │         │  subsumption, cycle       │
│         │         │  detection)               │
│         └────┬────┘                          │
│              ▼                               │
│         ┌─────────┐                          │
│         │Optimizer│ (visitor-based passes)    │
│         │         │                          │
│         └────┬────┘                          │
├──────────────┼──────────────────────────────┤
│  Layout Layer                                │
│         ┌────┴────┐                          │
│         │ Layout  │ (size, place, connect)    │
│         │         │ (rails created HERE)      │
│         └────┬────┘                          │
│              ▼                               │
│         ┌─────────┐                          │
│         │TikzEmit │ (pure TikZ output)       │
│         └─────────┘                          │
├─────────────────────────────────────────────┤
│  Infrastructure                              │
│  ┌─────────┐  ┌──────────┐  ┌───────────┐  │
│  │NodeSizes│  │Diagnostics│  │  Util     │  │
│  └─────────┘  └──────────┘  └───────────┘  │
└─────────────────────────────────────────────┘
```

---

## 4. Detailed Module Specifications

### 4.1 AST (`ast.hh` / `ast.cc`)

Replace the current `graph.hh/cc` (1432 lines) with a clean AST that has **no
rails, no coordinates, no layout state**.

```cpp
// Node types — semantic EBNF concepts only
enum class NodeKind {
    Terminal,       // quoted literal
    Nonterminal,    // identifier reference
    Epsilon,        // empty alternative (replaces nullnode)
    Sequence,       // A B C  (replaces concatnode)
    Choice,         // A | B | C  (replaces choicenode)
    Loop,           // {body, repeat?} (replaces loopnode)
    Optional,       // [A] — first-class, not a choice(epsilon,A) hack
    Newline,        // row-break marker
    Production,     // name -> body, with annotations
};

class Node {
    NodeKind kind;
    NodeId id;          // production-scoped, deterministic
    Node* parent;       // non-owning back-pointer
    // NO: leftrail, rightrail, previous, next, beforeskip, drawtoprev
    // NO: x, y, width, height, coordinates
};

class LeafNode : public Node {
    std::string text;   // terminal string or nonterminal name
    std::string style;  // nonterminal style (if any)
};

class UnaryNode : public Node {
    std::unique_ptr<Node> child;
};

class NaryNode : public Node {
    std::vector<std::unique_ptr<Node>> children;
};

class LoopNode : public Node {
    std::unique_ptr<Node> body;     // may be nullptr
    std::vector<std::unique_ptr<Node>> repeats;
};

class Production {
    std::string name;
    std::unique_ptr<Node> body;
    Annotations annot;  // subsume regex, caption, sideways
};

class Grammar {
    std::vector<std::unique_ptr<Production>> productions;
    // name -> Production* lookup index (non-owning)
    std::unordered_map<std::string, Production*> index;
};
```

**Key changes from current code:**
- `Optional` is a first-class node, not synthesized as `choice(epsilon, X)` with
  rail wrappers. The parser currently wraps `[X]` as
  `concat(rail, choice(null, X), rail)` — 4 nodes for 1 concept.
- `Epsilon` replaces `nullnode` with a clearer name.
- No rail pointers on any node. Rails are purely a layout concept.
- `unique_ptr` ownership eliminates manual `delete` and `forgetChild` patterns.
- `NodeId` is production-scoped (e.g., `"prod3_node7"`), not a global counter.
- `LoopNode` has an explicit `repeats` vector instead of overloading
  `multinode::children[0]` as body and `children[1..N]` as repeats.

### 4.2 Annotations (`annot.hh`)

```cpp
struct Annotations {
    std::optional<std::string> subsume_regex; // regex pattern, or nullopt
    std::optional<std::string> caption;
    bool sideways = false;
};
```

Replace the current `map<string,string>` with a typed struct. The
`"emusbussubsume"` magic sentinel string goes away.

### 4.3 Parser Layer (`lexer.ll`, `parser.yy`, `annot_lexer.ll`, `annot_parser.yy`)

**Keep Flex/Bison** — they work fine and the grammar is simple enough. But fix
these issues:

- **Fix the rail bug at source.** Don't insert rails in the parser at all. The
  parser should produce pure semantic trees: `[X]` becomes `Optional(X)`,
  `{X}` becomes `Loop(body=null, repeats=[X])`, `X | Y` becomes `Choice(X, Y)`.
- **Add `error` recovery rules.** At minimum, recover at production boundaries
  so multiple errors can be reported in a single run.
- **Validate annotation keys.** Reject unknown keys like `"sideays"` at parse
  time.
- **Remove the annotation lexer state machine from the main lexer.** Instead,
  capture `<<-- ... -->>` as a raw string token and parse it in a separate pass
  (as currently done), but with cleaner state management. The current `<A>` state
  with `"--"[^>]` and `"-->"[^>]` patterns is fragile.
- **Build a symbol table during parsing.** Track which nonterminals are defined
  vs. referenced. After parsing, report undefined references as warnings.

### 4.4 Resolver (`resolver.hh` / `resolver.cc`)

New module that runs between parsing and optimization:

1. **Reference resolution**: Verify every `Nonterminal` reference has a
   corresponding `Production` definition. Warn on undefined references (they'll
   render as nonterminal boxes, which is valid, but the user should know).
2. **Subsumption**: Inline productions marked with `subsume`, using the existing
   regex-matching approach but operating on the clean AST. Since `unique_ptr`
   owns nodes, subsumption clones replacement subtrees and splices them in.
3. **Cycle detection**: Check for circular subsumption (A subsumes B which
   references A). Currently undetected; would cause infinite expansion.
4. **Deduplication**: Identify duplicate production bodies (useful for large
   grammars).

### 4.5 Optimizer (`optimizer.hh` / `optimizer.cc`)

Rewrite the 1128-line `optimize.cc` using the **visitor pattern** with **tree
rewriting** that produces new nodes rather than mutating in place.

```cpp
class OptPass {
public:
    virtual ~OptPass() = default;
    // Returns new node if transformed, nullptr if unchanged
    virtual std::unique_ptr<Node> visit(Node& node) = 0;
};

class MergeSequences : public OptPass { ... };
class MergeChoices   : public OptPass { ... };
class LiftWrappers   : public OptPass { ... };
class AnalyzeLoops   : public OptPass { ... };
class FlattenLoopChoices : public OptPass { ... };

// Fixed-point runner
std::unique_ptr<Node> optimize(std::unique_ptr<Node> tree,
                                const std::vector<OptPass*>& passes);
```

**Key improvements:**
- Each pass is a self-contained class, testable in isolation.
- No rail pointer management — rails don't exist in the AST.
- `AnalyzeLoops` replaces both `analyzeOptLoops` and `analyzeNonOptLoops`. The
  `Optional` node type makes the distinction explicit: if the pattern is inside
  an `Optional`, it's an optional loop; otherwise it's a mandatory loop. No need
  for two nearly-identical 300-line functions.
- The fixed-point runner is explicit about pass ordering with a documented
  rationale.
- The `visit()` return convention (nullptr = unchanged) makes it easy to track
  whether the fixed point has been reached.
- No `dynamic_cast` needed — switch on `NodeKind` enum.

### 4.6 Layout (`layout.hh` / `layout.cc`)

Keep the current external-walker architecture (which was already rewritten
recently) but adapt it:

- **Rails are created here**, not in the parser. When laying out a `Choice` or
  `Loop` node, the layout engine creates visual rail connections as `Polyline`
  segments. The AST stays clean.
- **`NodeGeom`** struct stays mostly the same.
- **`ProductionLayout`** gains a `NodeId -> NodeGeom` map instead of
  `node* -> NodeGeom`, since node IDs are now stable identifiers.
- **Scoped naming**: Node IDs like `"myProd_node7"` serve as both layout keys
  and bnfnodes.dat keys, eliminating the global counter collision problem.
- **Row wrapping**: With `Optional` and `Loop` as first-class node types, the
  layout engine doesn't need to detect `concat(rail, choice(null, ...), rail)`
  patterns — it just checks `node.kind == NodeKind::Optional`.

### 4.7 TikZ Emitter (`tikzemitter.hh` / `tikzemitter.cc`)

Rename from `tikzwriter` for clarity. Mostly the same, but:

- Receives `ProductionLayout` and walks the AST, emitting `\node` for leaves
  and `\draw` for polylines.
- Uses `NodeId` directly for `\writenodesize` names (no more `rawName()`
  indirection).
- `camelcase()` still needed for LaTeX savebox command names.

### 4.8 Node Sizes (`nodesizes.hh` / `nodesizes.cc`)

Split into `.hh` and `.cc` (currently header-only). Improvements:

- **Proper file parsing** with `std::istringstream`, not character-by-character
  comma skipping.
- **Validation**: Reject negative widths/heights, warn on suspiciously large
  values.
- **Scoped access**: Pass `NodeSizes` by reference instead of global static
  pointer. Every function that needs sizes receives it as a parameter.

### 4.9 Diagnostics (`diagnostics.hh` / `diagnostics.cc`)

New module replacing the scattered `cerr` and `exit()` calls:

```cpp
enum class Severity { Note, Warning, Error };

struct Diagnostic {
    Severity severity;
    Location loc;      // file, line, column
    std::string message;
};

class DiagnosticEngine {
    std::vector<Diagnostic> diags;
    int errorCount = 0;
public:
    void report(Severity, Location, std::string msg);
    bool hasErrors() const;
    void emit(std::ostream&) const;
};
```

Replaces: direct `exit()` on file open failure, silently ignored annotation
typos, undetected undefined nonterminals, and the inconsistent error reporting
between the two parsers.

### 4.10 Main / CLI (`main.cc`)

Minor modernization:

- Replace `getopt_long` with a small argument parser (or keep `getopt_long` —
  it's fine).
- Pass `NodeSizes` by reference rather than loading into a global static.
- Report diagnostics at the end, not inline with `cerr`.
- Return meaningful exit codes (0 = success, 1 = warnings, 2 = errors).

---

## 5. File-by-File Rewrite Map

| Current File(s) | Lines | Rewritten As | Est. Lines | Key Changes |
|---|---|---|---|---|
| `graph.hh` + `graph.cc` | 1432 | `ast.hh` + `ast.cc` | ~500 | No rails, no coords, unique_ptr, Optional node |
| `parser.yy` | 494 | `parser.yy` | ~350 | No rail insertion, error recovery, symbol table |
| `lexer.ll` | 132 | `lexer.ll` | ~110 | Cleaner annotation capture |
| `annot_parser.yy` + `annot_lexer.ll` | 269 | Same names | ~200 | Typed Annotations struct, key validation |
| `optimize.cc` | 1128 | `optimizer.cc` | ~600 | Visitor passes, no rail mgmt, Optional node |
| `subsume.cc` | 243 | `resolver.cc` | ~200 | Combined with ref checking, cycle detection |
| `layout.cc` + `layout.hh` | 1275 | Same names | ~1100 | Rails created here, scoped naming |
| `tikzwriter.cc` + `tikzwriter.hh` | 222 | `tikzemitter.cc/hh` | ~200 | Minor API changes |
| `output.cc` | 209 | Merged into `layout.cc` | — | Overwide logic moves to layout |
| `nodesize.hh` | 125 | `nodesizes.hh/cc` | ~120 | Split to .cc, proper parsing, validation |
| `main.cc` | 137 | `main.cc` | ~120 | Scoped state, diagnostic reporting |
| `driver.hh/cc` | 124 | `driver.hh/cc` | ~100 | Remove dead nonterminals/terminals sets |
| `util.hh/cc` | 51 | `util.hh/cc` | ~50 | Minimal changes |
| — | — | `diagnostics.hh/cc` (NEW) | ~80 | Structured error reporting |
| — | — | `resolver.hh` (NEW) | ~30 | Header for resolver |
| — | — | `optimizer.hh` (NEW) | ~40 | Visitor pass interface |
| **Total** | **5841** | | **~3800** | ~35% reduction |

---

## 6. Implementation Order

The rewrite should be done bottom-up so that each layer can be tested
independently against the existing 167 regression tests.

### Phase 1: Foundation (no behavior change)

1. **`diagnostics.hh/cc`** — Write the diagnostic engine. Wire it into the
   existing code as a drop-in for `cerr`/`exit()`.
2. **`nodesizes.hh/cc`** — Split to header+implementation, add validation. Wire
   into existing `node::loadData()`.
3. **`util.cc`** — Minor cleanup (remove unnecessary casts).
4. **Run all 167 tests** — must still pass.

### Phase 2: AST Layer

5. **`ast.hh/cc`** — Define the new node hierarchy alongside the old `graph.hh`.
   Both exist temporarily.
6. **`parser.yy` + `lexer.ll`** — Modify to produce new AST nodes. Critical: no
   rails, `Optional` is first-class.
7. **Add a `dump()` visitor** for the new AST that produces identical output to
   the old `node::dump()`.
8. **Run all 167 dump-only tests** — validate that the new AST produces the same
   dumps (adjusting expected output for the `Optional` node type if needed; may
   need a migration script for `.expected` files).

### Phase 3: Semantic Layer

9. **`resolver.cc`** — Port subsumption to the new AST. Add reference checking
   and cycle detection.
10. **`optimizer.cc`** — Port optimization passes as visitor classes. Since there
    are no rails in the AST, `analyzeOptLoops`/`analyzeNonOptLoops` simplify
    dramatically — the `Optional` wrapper is explicit, no need to pattern-match
    `concat(rail, choice(null, ...), rail)`.
11. **Run all 167 tests including `-O -d` optimization tests** — validate
    optimizer produces equivalent ASTs.

### Phase 4: Layout Layer

12. **Adapt `layout.cc`** to walk the new AST. Rails are generated as `Polyline`
    connections during `connectChoice`/`connectLoop`, not read from the AST.
13. **Adapt `tikzemitter.cc`** to use `NodeId` strings instead of `rawName()`.
14. **`make check-tikz`** — visual regression with the PDF. This is the critical
    acceptance test.

### Phase 5: Cleanup

15. **Delete `graph.hh/cc`** — all code now uses `ast.hh`.
16. **Delete `output.cc`** — overwide logic integrated into layout.
17. **Update Makefile** with new source files.
18. **Update test expected files** if AST dump format changed.
19. **Update CLAUDE.md** with new architecture documentation.

---

## 7. Risk Assessment

| Risk | Severity | Mitigation |
|---|---|---|
| Optimizer parity | High | The 167 regression tests with `-O -d` flags verify AST equivalence. Run after every change. |
| Layout visual regression | High | `make check-tikz` produces `testdriver.pdf`. Compare page-by-page against current PDF. |
| Bison/Flex compatibility | Low | Keep the same Bison 3.7+ / Flex versions. Grammar rules change, not tool versions. |
| Test expected files need updating | Medium | The `Optional` node changes dump format. Write a script to mechanically update `.expected` files. |
| Performance regression | Low | The codebase processes small grammars. Even a 2x slowdown would be imperceptible. |
| Subsumption clone semantics | Medium | `unique_ptr` deep-clone is more explicit than current manual clone. Write targeted tests for subsume edge cases. |

---

## 8. Key Design Rationale

### Why remove rails from the AST?

Rails are the single biggest source of bugs and complexity. The parser bug (line
445), `clearRailsRecursive()`, `mergeRails()`, rail rewiring in every optimizer
pass, the `multinode` copy constructor's rail rewiring — all of this goes away
when rails are a layout-only concept. The optimizer becomes dramatically simpler
because it only needs to reason about grammar structure, not visual connectors.

### Why `Optional` as a first-class node?

Currently `[X]` produces `concat(rail_UP, choice(null, X), rail_UP)` — 5 nodes.
The optimizer must then pattern-match this specific structure to recognize
optionality. With a first-class `Optional`, the parser produces 1 node, the
optimizer checks `node.kind == Optional`, and the layout engine knows to draw the
bypass rail.

### Why visitor-based optimization?

The current approach embeds optimization logic in virtual methods on every node
class (`mergeConcats()`, `mergeChoices()`, etc.). This means adding a new pass
requires modifying `node`, `singlenode`, `multinode`, `concatnode`, `choicenode`,
and `loopnode`. A visitor pattern puts each pass in its own file, with a single
dispatch on `NodeKind`.

### Why immutable-ish tree rewriting?

The current optimizer mutates the tree in place, which requires maintaining
parent, sibling, and rail pointer invariants at every mutation point. If a visit
produces a new subtree, the parent swap is localized to one site (the visitor
runner), not 30 scattered mutation sites.

### Why scoped node IDs?

Global counters require concatenating all input into a single invocation. Scoped
IDs (e.g., `"myProd_node7"`) allow processing files independently while still
producing unique bnfnodes.dat keys. This also opens the door to incremental
recompilation (only re-layout productions whose sizes changed).

---

## 9. Appendix: Current Code Coupling Map

```
parser.yy ──creates──> graph.hh nodes (with rails)
    │                       │
    ├──calls──> optimize.cc ──mutates──> graph.hh nodes
    │               │
    │               ├──uses──> clearRailsRecursive (workaround for parser bug)
    │               ├──uses──> replaceRailRecursive
    │               └──uses──> mergeRails
    │
    ├──calls──> subsume.cc ──clones/replaces──> graph.hh nodes
    │               │
    │               └──uses──> liftConcats, fixSkips
    │
    └──calls──> output.cc ──orchestrates──> layout.cc + tikzwriter.cc
                    │                           │
                    ├──reads──> nodesize.hh      ├──reads──> graph.hh (node types, widths)
                    └──modifies──> nontermnode   └──reads──> nodesize.hh
                        (wrapped flag)

Proposed dependency flow (acyclic):
    parser.yy ──creates──> ast.hh nodes (no rails)
        │
        ▼
    resolver.cc ──transforms──> ast.hh nodes
        │
        ▼
    optimizer.cc ──rewrites──> ast.hh nodes
        │
        ▼
    layout.cc ──reads──> ast.hh (creates rails as Polylines)
        │
        ▼
    tikzemitter.cc ──reads──> layout results, ast.hh
```
