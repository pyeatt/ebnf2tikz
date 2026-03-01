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

#ifndef LAYOUT_HH
#define LAYOUT_HH

#include <map>
#include <vector>
#include <nodesize.hh>

using namespace std;

class node;
class productionnode;
class nodesizes;

/* Geometry for a single node after layout */
struct NodeGeom {
  coordinate origin;  /* placement position (west/left anchor) */
  float width;
  float height;
  coordinate entry;   /* line entry point (left side) */
  coordinate exit;    /* line exit point (right side) */
};

/* A polyline is a sequence of points to connect with rounded-corner lines */
struct Polyline {
  vector<coordinate> points;
};

/* Complete layout result for one production */
struct ProductionLayout {
  map<node*, NodeGeom> geom;
  vector<Polyline> connections;
};

/* Compute the width and height of a node subtree without placing it */
pair<float,float> computeSize(node *n, nodesizes *sizes);

/* Place a node at the given origin, storing geometry in geom map.
   Returns the NodeGeom for this node. */
NodeGeom layoutNode(node *n, coordinate origin,
		    map<node*, NodeGeom> &geom, nodesizes *sizes);

/* After layout, compute all connection polylines for a node subtree */
void computeConnections(node *n, map<node*, NodeGeom> &geom,
			vector<Polyline> &lines, nodesizes *sizes);

/* Layout an entire production: places all nodes, computes connections */
ProductionLayout layoutProduction(productionnode *prod, nodesizes *sizes);

#endif
