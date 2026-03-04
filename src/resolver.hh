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
 * @file resolver.hh
 * @brief Reference checking and subsumption declarations.
 *
 * Provides semantic analysis passes that run after parsing:
 * - Reference checking: warns about nonterminals used but never defined.
 * - Subsumption: inlines production bodies into other productions based
 *   on @c \\<\\<-- subsume --\\>\\> annotations with POSIX regex matching.
 *
 * @see resolver.cc for the implementations
 */

#ifndef RESOLVER_HH
#define RESOLVER_HH

#include "ast.hh"
#include "diagnostics.hh"

/** @brief Namespace for semantic analysis passes. */
namespace resolver {

/**
 * @brief Check that all nonterminal references have definitions.
 *
 * Collects all defined production names and all referenced nonterminals,
 * then warns about any references that have no corresponding definition.
 * Undefined references still render correctly but may indicate typos.
 *
 * @param grammar The grammar to check.
 */
void checkReferences(ASTGrammar *grammar);

/**
 * @brief Inline productions annotated with @c subsume into other productions.
 *
 * For each production with a @c subsume annotation, compiles the annotation
 * value as a POSIX extended regex and replaces matching NonterminalNode
 * references in all other productions with deep clones of the subsumed
 * production's body.  If the annotation value is the sentinel
 * @c "emusbussubsume", the regex defaults to an exact match on the
 * production's own name.
 *
 * @param grammar The grammar to process (modified in place).
 */
void subsume(ASTGrammar *grammar);

} // namespace resolver

#endif
