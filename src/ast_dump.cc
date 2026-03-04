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
 * @file ast_dump.cc
 * @brief Implementation of the AST tree-format dumper.
 *
 * Recursively walks the AST and prints each node type with indentation
 * proportional to tree depth.  The output format matches the expected
 * files in @c unit_tests/expected/ for regression testing.
 *
 * @see ast_dump.hh
 */

#include "ast_dump.hh"
#include "ast_visitor.hh"
#include <iostream>

using namespace std;
using namespace ast;

/**
 * @brief Print indentation spaces for the given tree depth.
 * @param depth Number of indentation levels (2 spaces each).
 */
static void indent(int depth)
{
  int i;
  for(i = 0; i < depth; i++)
    cout << "  ";
}

/**
 * @brief Visitor that dumps AST nodes in indented tree format.
 */
class ASTDumper : public ASTVisitor {
public:
  int depth;

  ASTDumper(int d) : depth(d) {}

  void visitTerminal(TerminalNode *n) override
  {
    indent(depth);
    cout << "terminal " << n->text << endl;
  }

  void visitNonterminal(NonterminalNode *n) override
  {
    indent(depth);
    cout << "nonterminal " << n->name << endl;
  }

  void visitEpsilon(EpsilonNode *n) override
  {
    indent(depth);
    cout << "epsilon" << endl;
  }

  void visitNewline(NewlineNode *n) override
  {
    indent(depth);
    cout << "newline" << endl;
  }

  void visitSequence(SequenceNode *n) override
  {
    size_t i;

    indent(depth);
    cout << "sequence" << endl;
    depth++;
    for(i = 0; i < n->children.size(); i++)
      n->children[i]->accept(*this);
    depth--;
  }

  void visitChoice(ChoiceNode *n) override
  {
    size_t i;

    indent(depth);
    cout << "choice" << endl;
    depth++;
    for(i = 0; i < n->alternatives.size(); i++)
      n->alternatives[i]->accept(*this);
    depth--;
  }

  void visitOptional(OptionalNode *n) override
  {
    indent(depth);
    cout << "optional" << endl;
    depth++;
    n->child->accept(*this);
    depth--;
  }

  void visitLoop(LoopNode *n) override
  {
    size_t i;

    indent(depth);
    cout << "loop" << endl;
    if(n->body != nullptr)
      {
        indent(depth + 1);
        cout << "body:" << endl;
        depth += 2;
        n->body->accept(*this);
        depth -= 2;
      }
    for(i = 0; i < n->repeats.size(); i++)
      {
        indent(depth + 1);
        cout << "repeat:" << endl;
        depth += 2;
        n->repeats[i]->accept(*this);
        depth -= 2;
      }
  }
};

void astDumpGrammar(ASTGrammar *grammar)
{
  size_t i;
  ASTProduction *prod;
  ASTDumper dumper(1);

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];
      cout << "production: " << prod->name << endl;
      dumper.depth = 1;
      prod->body->accept(dumper);
    }
}
