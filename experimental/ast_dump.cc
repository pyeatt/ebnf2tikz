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

#include "ast_dump.hh"
#include <iostream>

using namespace std;
using namespace ast;

static void indent(int depth)
{
  int i;
  for(i = 0; i < depth; i++)
    cout << "  ";
}

static void dumpNode(ASTNode *n, int depth)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  size_t i;

  switch(n->kind)
    {
    case ASTKind::Terminal:
      indent(depth);
      cout << "terminal " << static_cast<TerminalNode*>(n)->text << endl;
      return;

    case ASTKind::Nonterminal:
      indent(depth);
      cout << "nonterminal " << static_cast<NonterminalNode*>(n)->name << endl;
      return;

    case ASTKind::Epsilon:
      indent(depth);
      cout << "epsilon" << endl;
      return;

    case ASTKind::Newline:
      indent(depth);
      cout << "newline" << endl;
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      indent(depth);
      cout << "sequence" << endl;
      for(i = 0; i < seq->children.size(); i++)
        dumpNode(seq->children[i], depth + 1);
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      indent(depth);
      cout << "choice" << endl;
      for(i = 0; i < ch->alternatives.size(); i++)
        dumpNode(ch->alternatives[i], depth + 1);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      indent(depth);
      cout << "optional" << endl;
      dumpNode(opt->child, depth + 1);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      indent(depth);
      cout << "loop" << endl;
      if(loop->body != NULL)
        {
          indent(depth + 1);
          cout << "body:" << endl;
          dumpNode(loop->body, depth + 2);
        }
      for(i = 0; i < loop->repeats.size(); i++)
        {
          indent(depth + 1);
          cout << "repeat:" << endl;
          dumpNode(loop->repeats[i], depth + 2);
        }
      return;
    }
}

void astDumpGrammar(ASTGrammar *grammar)
{
  size_t i;
  ASTProduction *prod;

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];
      cout << "production: " << prod->name << endl;
      dumpNode(prod->body, 1);
    }
}
