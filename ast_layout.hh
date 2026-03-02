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

#ifndef AST_LAYOUT_HH
#define AST_LAYOUT_HH

#include "ast.hh"
#include <nodesizes.hh>
#include <map>
#include <vector>
#include <string>

using namespace std;

/* Information about a leaf node assigned during the naming pass */
struct ASTLeafInfo {
  string tikzName;     /* "node1" or "coord1" */
  float width, height; /* from bnfnodes.dat */
  string style;        /* "terminal", "nonterminal" */
  string format;       /* "railtermname", "railname" */
  string rawText;      /* display text (quotes stripped for terminals) */
  int isTerminal;      /* 1 if terminal, 0 if nonterminal */
  int wrapped;         /* shortstack wrapping for wide names */
};

/* Geometry for an AST subtree after layout */
struct ASTNodeGeom {
  coordinate origin;
  float width;
  float height;
  coordinate entry;    /* left connection point */
  coordinate exit;     /* right connection point */
};

/* A polyline is a sequence of points to connect with rounded corners */
struct ASTPolyline {
  vector<coordinate> points;
};

/* A named coordinate emitted as \coordinate in TikZ */
struct ASTNamedCoord {
  string name;
  coordinate pos;
};

/* Complete layout result for one production */
struct ASTProductionLayout {
  map<ast::ASTNode*, ASTNodeGeom> geom;
  map<ast::ASTNode*, ASTLeafInfo> leafInfo;
  vector<ASTPolyline> connections;
  vector<ASTNamedCoord> stubs;
};

/* Context for naming and size lookup during layout */
struct ASTLayoutContext {
  int nodeCounter;
  int coordCounter;
  nodesizes *sizes;

  ASTLayoutContext(nodesizes *s);
  string nextNode();
  string nextCoord();
};

/* Assign TikZ names and look up sizes for all leaf nodes */
void astAssignNames(ast::ASTNode *n, ASTLayoutContext &ctx,
                    map<ast::ASTNode*, ASTLeafInfo> &info);

/* Compute the width and height of an AST subtree */
pair<float,float> astComputeSize(ast::ASTNode *n,
                                 map<ast::ASTNode*, ASTLeafInfo> &info,
                                 nodesizes *sizes);

/* Place an AST node at the given origin */
ASTNodeGeom astLayoutNode(ast::ASTNode *n, coordinate origin,
                          map<ast::ASTNode*, ASTNodeGeom> &geom,
                          map<ast::ASTNode*, ASTLeafInfo> &info,
                          nodesizes *sizes);

/* Compute all connection polylines for a subtree */
void astComputeConnections(ast::ASTNode *n,
                           map<ast::ASTNode*, ASTNodeGeom> &geom,
                           map<ast::ASTNode*, ASTLeafInfo> &info,
                           vector<ASTPolyline> &lines,
                           nodesizes *sizes);

/* Layout an entire production (stubs + body + connections) */
ASTProductionLayout astLayoutProduction(ASTProduction *prod,
                                        ASTLayoutContext &ctx);

#endif
