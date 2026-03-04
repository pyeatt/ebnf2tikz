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
 * @file ast_optimizer.hh
 * @brief AST optimization pass declarations.
 *
 * Provides the public interface for running iterative fixed-point
 * optimization passes over the AST.  Passes include merging nested
 * sequences/choices, lifting single-child wrappers, analyzing loop
 * patterns, and converting loop body structures.
 *
 * @see ast_optimizer.cc for the implementation of all passes
 */

#ifndef AST_OPTIMIZER_HH
#define AST_OPTIMIZER_HH

#include "ast.hh"

/** @brief Namespace for AST optimization passes. */
namespace ast_optimizer {

/**
 * @brief Optimize all productions in a grammar to fixed point.
 *
 * Iterates merge, lift, and loop-analysis passes over each production
 * body until no more transformations apply.
 *
 * @param grammar The grammar whose productions will be optimized in place.
 */
void optimize(ASTGrammar *grammar);

/**
 * @brief Optimize a single production body to fixed point.
 *
 * Runs two phases:
 * -# Merge/flatten: mergeChoices, mergeSequences, liftWrappers,
 *    expandOptionalSequence
 * -# Loop analysis: analyzeLoops, flattenLoopChoices,
 *    bodyChoiceToChoiceloop
 *
 * @param body The body AST node (may be replaced).
 * @return The optimized body node (caller must update its pointer).
 */
ast::ASTNode* optimizeBody(ast::ASTNode *body);

} // namespace ast_optimizer

#endif
