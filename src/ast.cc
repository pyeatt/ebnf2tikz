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
 * @file ast.cc
 * @brief Implementation of AST node clone(), destructor, and append methods.
 *
 * All AST nodes support deep cloning (used during subsumption) and
 * proper ownership-based destruction of child subtrees.
 *
 * @see ast.hh for the node class declarations
 */

#include "ast.hh"
#include "ast_visitor.hh"

using namespace std;

namespace ast {

// --- clone implementations ---

ASTNode* TerminalNode::clone() const
{
  return new TerminalNode(text);
}

ASTNode* NonterminalNode::clone() const
{
  return new NonterminalNode(name);
}

ASTNode* EpsilonNode::clone() const
{
  return new EpsilonNode();
}

ASTNode* NewlineNode::clone() const
{
  return new NewlineNode();
}

ASTNode* SequenceNode::clone() const
{
  SequenceNode *s = new SequenceNode();
  size_t i;
  for(i = 0; i < children.size(); i++)
    s->append(children[i]->clone());
  return s;
}

ASTNode* ChoiceNode::clone() const
{
  ChoiceNode *c = new ChoiceNode();
  size_t i;
  for(i = 0; i < alternatives.size(); i++)
    c->addAlternative(alternatives[i]->clone());
  return c;
}

ASTNode* OptionalNode::clone() const
{
  return new OptionalNode(child->clone());
}

ASTNode* LoopNode::clone() const
{
  LoopNode *n = new LoopNode();
  size_t i;
  if(body != nullptr)
    n->body = body->clone();
  for(i = 0; i < repeats.size(); i++)
    n->addRepeat(repeats[i]->clone());
  return n;
}

// --- destructors ---

SequenceNode::~SequenceNode()
{
  size_t i;
  for(i = 0; i < children.size(); i++)
    delete children[i];
}

ChoiceNode::~ChoiceNode()
{
  size_t i;
  for(i = 0; i < alternatives.size(); i++)
    delete alternatives[i];
}

OptionalNode::~OptionalNode()
{
  delete child;
}

LoopNode::~LoopNode()
{
  size_t i;
  if(body != nullptr)
    delete body;
  for(i = 0; i < repeats.size(); i++)
    delete repeats[i];
}

// --- append/add methods ---

void SequenceNode::append(ASTNode *child)
{
  children.push_back(child);
}

void ChoiceNode::addAlternative(ASTNode *alt)
{
  alternatives.push_back(alt);
}

void LoopNode::addRepeat(ASTNode *r)
{
  repeats.push_back(r);
}

// --- accept implementations ---

void TerminalNode::accept(ASTVisitor &v) { v.visitTerminal(this); }
void NonterminalNode::accept(ASTVisitor &v) { v.visitNonterminal(this); }
void EpsilonNode::accept(ASTVisitor &v) { v.visitEpsilon(this); }
void NewlineNode::accept(ASTVisitor &v) { v.visitNewline(this); }
void SequenceNode::accept(ASTVisitor &v) { v.visitSequence(this); }
void ChoiceNode::accept(ASTVisitor &v) { v.visitChoice(this); }
void OptionalNode::accept(ASTVisitor &v) { v.visitOptional(this); }
void LoopNode::accept(ASTVisitor &v) { v.visitLoop(this); }

// --- DefaultASTVisitor implementations ---

void DefaultASTVisitor::visitTerminal(TerminalNode *) {}
void DefaultASTVisitor::visitNonterminal(NonterminalNode *) {}
void DefaultASTVisitor::visitEpsilon(EpsilonNode *) {}
void DefaultASTVisitor::visitNewline(NewlineNode *) {}

void DefaultASTVisitor::visitSequence(SequenceNode *n)
{
  size_t i;
  for(i = 0; i < n->children.size(); i++)
    n->children[i]->accept(*this);
}

void DefaultASTVisitor::visitChoice(ChoiceNode *n)
{
  size_t i;
  for(i = 0; i < n->alternatives.size(); i++)
    n->alternatives[i]->accept(*this);
}

void DefaultASTVisitor::visitOptional(OptionalNode *n)
{
  n->child->accept(*this);
}

void DefaultASTVisitor::visitLoop(LoopNode *n)
{
  size_t i;
  if(n->body != nullptr)
    n->body->accept(*this);
  for(i = 0; i < n->repeats.size(); i++)
    n->repeats[i]->accept(*this);
}

} // namespace ast


// --- ASTProduction ---

ASTProduction::ASTProduction(annotmap *a, const string &n, ast::ASTNode *b)
  : name(n), annotations(a), body(b), needsWrap(0)
{
}

ASTProduction::~ASTProduction()
{
  if(annotations != nullptr)
    delete annotations;
  if(body != nullptr)
    delete body;
}

// --- ASTGrammar ---

void ASTGrammar::insert(ASTProduction *p)
{
  productions.push_back(p);
}

ASTGrammar::~ASTGrammar()
{
  size_t i;
  for(i = 0; i < productions.size(); i++)
    delete productions[i];
}
