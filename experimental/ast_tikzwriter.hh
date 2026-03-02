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

#ifndef AST_TIKZWRITER_HH
#define AST_TIKZWRITER_HH

#include "ast.hh"
#include "ast_layout.hh"
#include <fstream>

using namespace std;

class ASTTikzWriter {
private:
  ofstream &outs;
  nodesizes *sizes;

  string texName(ASTLeafInfo &li);
  void emitLeafNode(ast::ASTNode *n, ASTLeafInfo &li,
                    ASTNodeGeom &geom);
  void emitPolyline(const ASTPolyline &pl);
  void emitAllNodes(ast::ASTNode *n, ASTProductionLayout &layout);

public:
  ASTTikzWriter(ofstream &out, nodesizes *sz);
  void writeProduction(ASTProduction *prod,
                       ASTProductionLayout &layout);
};

/* Top-level: layout and write all productions */
void astPlaceGrammar(ASTGrammar *grammar, ofstream &outs,
                     nodesizes *sizes);

#endif
