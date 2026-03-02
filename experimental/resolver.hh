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

#ifndef RESOLVER_HH
#define RESOLVER_HH

#include "ast.hh"
#include "diagnostics.hh"

namespace resolver {

// Check that all nonterminal references have definitions.
// Warns on undefined references (they render fine but may be typos).
void checkReferences(ASTGrammar *grammar);

// Perform subsumption: inline productions annotated with <<-- subsume -->>.
// For each marked production, replace matching nonterminal references in
// other productions with clones of the marked production's body.
void subsume(ASTGrammar *grammar);

} // namespace resolver

#endif
