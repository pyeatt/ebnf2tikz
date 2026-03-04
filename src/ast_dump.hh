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
 * @file ast_dump.hh
 * @brief AST dump interface for regression testing.
 *
 * Provides a function to print the AST in a human-readable indented
 * tree format to stdout.  Used with the @c -d command-line flag to
 * produce output that is compared against expected files in the
 * unit test suite.
 *
 * @see ast_dump.cc for the implementation
 */

#ifndef AST_DUMP_HH
#define AST_DUMP_HH

#include "ast.hh"

/**
 * @brief Dump an entire grammar's AST to stdout in indented tree format.
 *
 * Each production is printed as "production: <name>" followed by its
 * body tree indented with two spaces per level.  This output format
 * is the canonical representation used by the regression test suite.
 *
 * @param grammar The grammar to dump.
 */
void astDumpGrammar(ASTGrammar *grammar);

#endif
