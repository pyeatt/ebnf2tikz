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
 * @file ast_visitor.hh
 * @brief Visitor pattern interface and default implementation for the AST.
 *
 * Provides ASTVisitor (abstract interface) and DefaultASTVisitor (base
 * class with default child iteration).  Subclass DefaultASTVisitor and
 * override only the visit methods you need; the defaults recurse into
 * children automatically.
 *
 * @see ast.hh for the AST node hierarchy
 */

#ifndef AST_VISITOR_HH
#define AST_VISITOR_HH

#include "ast.hh"

namespace ast {

/**
 * @brief Abstract visitor interface for AST nodes.
 *
 * Provides a pure virtual visit method for each concrete AST node type.
 * Implement all methods to create a concrete visitor.  For a base with
 * default child iteration, subclass DefaultASTVisitor instead.
 */
class ASTVisitor {
public:
  virtual ~ASTVisitor() {}
  virtual void visitTerminal(TerminalNode *n) = 0;
  virtual void visitNonterminal(NonterminalNode *n) = 0;
  virtual void visitEpsilon(EpsilonNode *n) = 0;
  virtual void visitNewline(NewlineNode *n) = 0;
  virtual void visitSequence(SequenceNode *n) = 0;
  virtual void visitChoice(ChoiceNode *n) = 0;
  virtual void visitOptional(OptionalNode *n) = 0;
  virtual void visitLoop(LoopNode *n) = 0;
};

/**
 * @brief Default visitor base with automatic child iteration.
 *
 * All leaf visit methods are no-ops.  Container visit methods recurse
 * into children via accept().  Override individual methods to add
 * node-specific behavior; call the base method (or not) to control
 * whether children are visited.
 */
class DefaultASTVisitor : public ASTVisitor {
public:
  void visitTerminal(TerminalNode *n) override;
  void visitNonterminal(NonterminalNode *n) override;
  void visitEpsilon(EpsilonNode *n) override;
  void visitNewline(NewlineNode *n) override;
  void visitSequence(SequenceNode *n) override;
  void visitChoice(ChoiceNode *n) override;
  void visitOptional(OptionalNode *n) override;
  void visitLoop(LoopNode *n) override;
};

} // namespace ast

#endif
