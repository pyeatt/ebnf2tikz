# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ebnf2tikz is an optimizing compiler that converts annotated Extended Backus-Naur Form (EBNF) grammar specifications into railroad (syntax) diagrams as LaTeX TikZ commands. It requires a multi-pass feedback loop with LaTeX: run ebnf2tikz, then pdflatex, then ebnf2tikz again, then pdflatex again — because LaTeX measures node sizes (written to `bnfnodes.dat` via `\writenodesize`) which ebnf2tikz reads on subsequent runs for accurate layout.

## Build Commands

```bash
make                # Build the ebnf2tikz binary (default "Debug" target)
make test           # Alias for make check
make check          # Run all 167 AST-dump unit tests
make check-tikz     # Build PDF with all railroad diagrams (requires pdflatex)
make clean          # Remove generated .cc/.hh (from flex/bison), .o files
make realclean      # clean + remove .depend
make depend         # Regenerate .depend dependency file
```

**Prerequisites**: g++, bison (>=3.7), flex, pdflatex (for testing)

**Usage**: `./ebnf2tikz [options] input_file output_file`
- `-n` / `--nooptimize`: Skip graph transformations
- `-f` / `--makefigures`: Wrap tikzpictures in LaTeX figures

## Architecture

Classic compiler pipeline: Lex → Parse → Optimize → Subsume → Layout/Output.

### Lexing & Parsing (Flex/Bison, two separate parser pairs)

- **Main parser**: `lexer.ll` + `parser.yy` — tokenizes and parses EBNF input, builds the AST (node graph). The `driver` class (`driver.hh`/`.cc`) coordinates scanning and parsing.
- **Annotation parser**: `annot_lexer.ll` + `annot_parser.yy` — parses `<<-- ... -->>` annotation blocks (subsume, caption, sideways). Uses `-P annot` prefix to avoid symbol collisions.

### Node Hierarchy (`graph.hh` / `graph.cc`)

Composite tree pattern:
- **`node`** — base class with coordinates, width/height, parent/sibling pointers, rail pointers, virtual methods
- **`singlenode`** — one child body (`productionnode`, `rownode`)
- **`multinode`** — `vector<node*>` children (`concatnode`, `choicenode`, `loopnode`)
- **Leaf nodes**: `termnode` (terminal), `nontermnode` (nonterminal), `nullnode` (zero-width placeholder), `railnode` (vertical rail connector), `newlinenode` (multi-line marker)
- **`grammar`** — collection of `productionnode*`, not a `node` subclass

Every node implements `clone()` for deep copying (used in subsumption).

### Optimization (`optimize.cc`)

Iterative fixed-point passes called by `productionnode::optimize()`:
- `mergeConcats()` — combine adjacent concat children
- `liftConcats()` — lift single-child concats
- `mergeChoices()` — flatten nested choice nodes
- `analyzeOptLoops()` / `analyzeNonOptLoops()` — transform optional/loop patterns into proper loop diagrams
- `mergeRails()` — merge compatible adjacent rail nodes

### Subsumption (`subsume.cc`)

Inline expansion: productions annotated with `subsume` have their references in other productions replaced with the production body, matched via POSIX regex.

### Layout & Output (`output.cc`)

`place()` methods on each node type compute coordinates and emit TikZ commands. `drawToLeftRail()`/`drawToRightRail()` draw vertical rail connections between alternatives. `grammar::place()` wraps each production in a LaTeX `\newsavebox`.

### Node Sizing (`nodesize.hh`)

`nodesizes` reads `bnfnodes.dat` (produced by LaTeX) to get actual rendered widths/heights. Defaults: `rowsep=6`, `colsep=8`, `minsize=14`.

### Entry Point (`main.cc`)

Parses command-line options with `getopt_long`, loads node sizes, calls `driver::parse()`, writes TikZ output.

## Test Infrastructure

All tests live in `unit_tests/`. Each test is a single `.ebnf` file with a matching `expected/<name>.expected` file containing the expected AST dump output. Tests requiring specific flags (e.g., `-O -d` for optimization) have a `flags/<name>.flags` file; otherwise the default flag is `-d` (dump AST).

- `unit_tests/*.ebnf` — 167 individual test grammars (01-167)
- `unit_tests/expected/*.expected` — expected AST dump output for each test
- `unit_tests/flags/*.flags` — per-test command-line flags (when not just `-d`)
- `unit_tests/run_tests.sh` — AST-dump regression runner (used by `make check`)
- `unit_tests/build_tikz_tests.sh` — concatenates all .ebnf files into one input, runs ebnf2tikz + pdflatex in a multi-pass loop to produce `testdriver.pdf` (used by `make check-tikz`)
- `unit_tests/testdriver.tex` — LaTeX document defining TikZ styles for PDF generation

To add a new test, create `unit_tests/<N>_<name>.ebnf`, optionally `unit_tests/flags/<N>_<name>.flags`, then run `./ebnf2tikz [flags] <file>.ebnf /dev/null` and save stdout to `unit_tests/expected/<N>_<name>.expected`. Production names must be globally unique across all .ebnf files (required for `build_tikz_tests.sh` which concatenates them all).

## Known Issues

- The `ebnf2tikz.sty` LaTeX style file has not been written yet; use `testdriver.tex` as reference for required TikZ setup
- Auto-wrap (automatically breaking long productions across multiple lines) is not yet implemented
