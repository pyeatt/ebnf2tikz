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

#ifndef TIKZWRITER_HH
#define TIKZWRITER_HH

#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <layout.hh>

using namespace std;

class node;
class productionnode;
class nodesizes;

class TikzWriter {
private:
  ofstream &outs;
  nodesizes *sizes;

  string nameFor(node *n);
  void emitNode(node *n, const map<node*, NodeGeom> &geom);
  void emitPolyline(const Polyline &pl);
  void emitNodes(node *n, const map<node*, NodeGeom> &geom);

public:
  TikzWriter(ofstream &out, nodesizes *sz);
  void writeProduction(productionnode *prod, const ProductionLayout &layout);
};

#endif
