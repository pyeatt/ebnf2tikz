/*
ebnf2tikz
    An optimizing compiler to convert (possibly annotated) Extended
    Backus-Naur Form (EBNF) to railroad diagrams expressed as LaTeX
    TikZ commands.

    Copyright (C) 2021  Larry D. Pyeatt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file ast.hh
 * @brief Abstract Syntax Tree node hierarchy for EBNF grammars.
 *
 * Defines the AST node types used to represent parsed EBNF grammar
 * productions. The tree is purely structural with no layout or
 * coordinate information; layout is handled separately by ast_layout.
 *
 * @see ast.cc for clone() and destructor implementations
 * @see ast_optimizer.hh for optimization passes over this AST
 */

#ifndef AST_HH
#define AST_HH

#include <map>
#include <string>
#include <vector>

using namespace std;

/** @brief Map of annotation key-value pairs (e.g., subsume, caption). */
typedef map<string,string> annotmap;

/** @brief A single annotation key-value pair. */
typedef pair<string,string> annot_t;

/** @brief Namespace for all AST node types. */
namespace ast {

/**
 * @brief Discriminator for AST node types.
 *
 * Used for runtime type identification without RTTI.
 */
enum class ASTKind {
  Terminal,     /**< Terminal symbol (literal string). */
  Nonterminal,  /**< Nonterminal reference (production name). */
  Epsilon,      /**< Empty/null production. */
  Sequence,     /**< Concatenation of children (comma-separated). */
  Choice,       /**< Alternation among children (pipe-separated). */
  Optional,     /**< Optional expression (bracket-wrapped, `[X]`). */
  Loop,         /**< Repetition/loop (brace-wrapped, `{X}`). */
  Newline       /**< Multi-row line break marker. */
};

/**
 * @brief Abstract base class for all AST nodes.
 *
 * Every node stores its ASTKind and supports deep cloning.
 * Nodes own their children and delete them in destructors.
 */
class ASTNode {
public:
  ASTKind kind;  /**< Runtime type tag for this node. */

  /**
   * @brief Construct a node with the given kind.
   * @param k The node's type discriminator.
   */
  ASTNode(ASTKind k) : kind(k) {}

  /** @brief Virtual destructor for proper polymorphic cleanup. */
  virtual ~ASTNode() {}

  /**
   * @brief Create a deep copy of this node and all descendants.
   * @return A newly allocated clone of the entire subtree.
   */
  virtual ASTNode* clone() const = 0;
};

/**
 * @brief A terminal (literal) symbol in the grammar.
 *
 * Stores the terminal text including its surrounding quotes
 * (e.g., `"if"` or `'+'`).
 */
class TerminalNode : public ASTNode {
public:
  string text;  /**< Terminal text including surrounding quotes. */

  /**
   * @brief Construct a terminal node.
   * @param s The terminal text (with quotes).
   */
  TerminalNode(const string &s) : ASTNode(ASTKind::Terminal), text(s) {}

  /** @brief Deep copy this terminal node. */
  ASTNode* clone() const override;
};

/**
 * @brief A nonterminal reference in the grammar.
 *
 * Refers to another production by name.
 */
class NonterminalNode : public ASTNode {
public:
  string name;  /**< The referenced production name. */

  /**
   * @brief Construct a nonterminal node.
   * @param s The production name being referenced.
   */
  NonterminalNode(const string &s) : ASTNode(ASTKind::Nonterminal), name(s) {}

  /** @brief Deep copy this nonterminal node. */
  ASTNode* clone() const override;
};

/**
 * @brief An epsilon (empty) node representing the empty string.
 *
 * Used in optional constructs and as an explicit empty alternative.
 */
class EpsilonNode : public ASTNode {
public:
  EpsilonNode() : ASTNode(ASTKind::Epsilon) {}

  /** @brief Deep copy this epsilon node. */
  ASTNode* clone() const override;
};

/**
 * @brief A newline marker for multi-row production layout.
 *
 * Inserted by the parser when the input contains an explicit line
 * break directive, or by the auto-wrap pass for overly wide productions.
 */
class NewlineNode : public ASTNode {
public:
  NewlineNode() : ASTNode(ASTKind::Newline) {}

  /** @brief Deep copy this newline node. */
  ASTNode* clone() const override;
};

/**
 * @brief An ordered sequence (concatenation) of child nodes.
 *
 * Represents comma-separated items in EBNF (e.g., `A , B , C`).
 * Owns all children and deletes them on destruction.
 */
class SequenceNode : public ASTNode {
public:
  vector<ASTNode*> children;  /**< Ordered list of child nodes. */

  SequenceNode() : ASTNode(ASTKind::Sequence) {}

  /** @brief Destroy this node and all children. */
  ~SequenceNode() override;

  /**
   * @brief Append a child to the end of the sequence.
   * @param child The node to append (ownership transferred).
   */
  void append(ASTNode *child);

  /** @brief Deep copy this sequence and all children. */
  ASTNode* clone() const override;
};

/**
 * @brief A choice among alternative expressions.
 *
 * Represents pipe-separated alternatives in EBNF (e.g., `A | B | C`).
 * Owns all alternatives and deletes them on destruction.
 */
class ChoiceNode : public ASTNode {
public:
  vector<ASTNode*> alternatives;  /**< List of alternative subtrees. */

  ChoiceNode() : ASTNode(ASTKind::Choice) {}

  /** @brief Destroy this node and all alternatives. */
  ~ChoiceNode() override;

  /**
   * @brief Add an alternative to this choice.
   * @param alt The alternative node to add (ownership transferred).
   */
  void addAlternative(ASTNode *alt);

  /** @brief Deep copy this choice and all alternatives. */
  ASTNode* clone() const override;
};

/**
 * @brief An optional expression (`[X]` in EBNF).
 *
 * Represents a construct that may appear zero or one times.
 * This is a first-class AST node rather than being desugared
 * into a choice with epsilon.
 */
class OptionalNode : public ASTNode {
public:
  ASTNode *child;  /**< The optional sub-expression (owned). */

  /**
   * @brief Construct an optional node.
   * @param c The child expression (ownership transferred).
   */
  OptionalNode(ASTNode *c) : ASTNode(ASTKind::Optional), child(c) {}

  /** @brief Destroy this node and the child. */
  ~OptionalNode() override;

  /** @brief Deep copy this optional and its child. */
  ASTNode* clone() const override;
};

/**
 * @brief A loop/repetition node (`{X}` in EBNF).
 *
 * Models repetition with an optional forward body and one or more
 * repeat (backward/separator) alternatives. The body may be nullptr
 * for zero-or-more loops where no forward element was extracted.
 *
 * In railroad diagram terms, the body goes along the forward path
 * and the repeats go along the backward (loopback) path.
 */
class LoopNode : public ASTNode {
public:
  ASTNode *body;              /**< Forward-path body, may be nullptr. */
  vector<ASTNode*> repeats;   /**< Backward-path repeat alternatives (owned). */

  LoopNode() : ASTNode(ASTKind::Loop), body(NULL) {}

  /** @brief Destroy this node, the body, and all repeats. */
  ~LoopNode() override;

  /**
   * @brief Add a repeat alternative to this loop.
   * @param r The repeat node to add (ownership transferred).
   */
  void addRepeat(ASTNode *r);

  /** @brief Deep copy this loop, its body, and all repeats. */
  ASTNode* clone() const override;
};

} // namespace ast

/**
 * @brief A single grammar production (rule).
 *
 * Binds a production name to its body expression and optional
 * annotations (subsume, caption, sideways, etc.).
 */
class ASTProduction {
public:
  string name;                /**< Production name (left-hand side). */
  annotmap *annotations;      /**< Annotation key-value map, may be nullptr. */
  ast::ASTNode *body;         /**< Body expression tree (owned). */
  int needsWrap;              /**< Set by astAutoWrapGrammar when body exceeds row width. */

  /**
   * @brief Construct a production.
   * @param a  Annotation map (ownership transferred, may be nullptr).
   * @param n  Production name.
   * @param b  Body expression (ownership transferred).
   */
  ASTProduction(annotmap *a, const string &n, ast::ASTNode *b);

  /** @brief Destroy the production, its annotations, and body. */
  ~ASTProduction();
};

/**
 * @brief A complete grammar: an ordered collection of productions.
 *
 * Productions are stored in the order they appear in the input file.
 * Owns all productions and deletes them on destruction.
 */
class ASTGrammar {
public:
  vector<ASTProduction*> productions;  /**< Ordered list of productions (owned). */

  /**
   * @brief Add a production to the grammar.
   * @param p The production to insert (ownership transferred).
   */
  void insert(ASTProduction *p);

  /** @brief Destroy the grammar and all productions. */
  ~ASTGrammar();
};

#endif
