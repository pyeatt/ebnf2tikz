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

#ifndef AST_HH
#define AST_HH

#include <map>
#include <string>
#include <vector>

using namespace std;

typedef map<string,string> annotmap;
typedef pair<string,string> annot_t;

namespace ast {

enum class ASTKind {
  Terminal,
  Nonterminal,
  Epsilon,
  Sequence,
  Choice,
  Optional,
  Loop,
  Newline
};

// Base class for all AST nodes
class ASTNode {
public:
  ASTKind kind;
  ASTNode(ASTKind k) : kind(k) {}
  virtual ~ASTNode() {}
  virtual ASTNode* clone() const = 0;
};

// Terminal symbol (stores text including quotes)
class TerminalNode : public ASTNode {
public:
  string text;
  TerminalNode(const string &s) : ASTNode(ASTKind::Terminal), text(s) {}
  ASTNode* clone() const override;
};

// Nonterminal symbol
class NonterminalNode : public ASTNode {
public:
  string name;
  NonterminalNode(const string &s) : ASTNode(ASTKind::Nonterminal), name(s) {}
  ASTNode* clone() const override;
};

// Epsilon (empty/null)
class EpsilonNode : public ASTNode {
public:
  EpsilonNode() : ASTNode(ASTKind::Epsilon) {}
  ASTNode* clone() const override;
};

// Newline marker (multi-row separator)
class NewlineNode : public ASTNode {
public:
  NewlineNode() : ASTNode(ASTKind::Newline) {}
  ASTNode* clone() const override;
};

// Sequence of children (comma-separated concatenation)
class SequenceNode : public ASTNode {
public:
  vector<ASTNode*> children;
  SequenceNode() : ASTNode(ASTKind::Sequence) {}
  ~SequenceNode() override;
  void append(ASTNode *child);
  ASTNode* clone() const override;
};

// Choice among alternatives (pipe-separated)
class ChoiceNode : public ASTNode {
public:
  vector<ASTNode*> alternatives;
  ChoiceNode() : ASTNode(ASTKind::Choice) {}
  ~ChoiceNode() override;
  void addAlternative(ASTNode *alt);
  ASTNode* clone() const override;
};

// Optional expression (bracket-wrapped)
class OptionalNode : public ASTNode {
public:
  ASTNode *child;
  OptionalNode(ASTNode *c) : ASTNode(ASTKind::Optional), child(c) {}
  ~OptionalNode() override;
  ASTNode* clone() const override;
};

// Loop/repetition (brace-wrapped), stores body and repeat alternatives
class LoopNode : public ASTNode {
public:
  ASTNode *body;  // may be nullptr (zero-or-more with no extracted body)
  vector<ASTNode*> repeats;
  LoopNode() : ASTNode(ASTKind::Loop), body(NULL) {}
  ~LoopNode() override;
  void addRepeat(ASTNode *r);
  ASTNode* clone() const override;
};

} // namespace ast

// Production: name + annotations + body expression
class ASTProduction {
public:
  string name;
  annotmap *annotations;
  ast::ASTNode *body;
  int needsWrap;    /* set by astAutoWrapGrammar when body exceeds row width */
  ASTProduction(annotmap *a, const string &n, ast::ASTNode *b);
  ~ASTProduction();
};

// Grammar: collection of productions
class ASTGrammar {
public:
  vector<ASTProduction*> productions;
  void insert(ASTProduction *p);
  ~ASTGrammar();
};

#endif
