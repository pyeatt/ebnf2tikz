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

#include <tikzwriter.hh>
#include <graph.hh>
#include <nodesize.hh>
#include <util.hh>
#include <sstream>
#include <algorithm>

using namespace std;

/* ---------------------------------------------------------------- */

TikzWriter::TikzWriter(ofstream &out, nodesizes *sz)
  : outs(out), sizes(sz)
{
}

/* Use the node's rawName() so that TikZ names match the names used
   to look up sizes in bnfnodes.dat.  The parser assigns names like
   "node1", "node2" (from nextNode()) and "coord1", "coord2" (from
   nextCoord()) during construction, and those same names are written
   by \writenodesize{} so LaTeX can measure them.  On subsequent runs,
   nodesizes::getSize() looks them up by the same name. */
string TikzWriter::nameFor(node *n)
{
  return n->rawName();
}

/* ----------------------------------------------------------------
   Emit a single TikZ node for a terminal or nonterminal
   ---------------------------------------------------------------- */

void TikzWriter::emitNode(node *n, const map<node*, NodeGeom> &geom)
{
  string name;
  string style;
  auto it = geom.find(n);

  if(it == geom.end())
    return;

  name = nameFor(n);

  if(n->is_terminal())
    {
      style = ((nontermnode*)n)->getStyle();
      outs << "\\node (" << name << ") at "
	   << it->second.origin << "[anchor=west,"
	   << style << "] {" << n->texName() << "};\n";
      outs << "\\writenodesize{" << name << "}\n";
    }
  else if(n->is_nonterm())
    {
      style = ((nontermnode*)n)->getStyle();
      outs << "\\node (" << name << ") at "
	   << it->second.origin << "[anchor=west,"
	   << style << "] {" << n->texName()
	   << "\\hspace*{1pt}};\n";
      outs << "\\writenodesize{" << name << "}\n";
    }
  else if(n->is_null())
    {
      outs << "\\coordinate (" << name << ") at "
	   << it->second.origin << ";\n";
    }
}

/* ----------------------------------------------------------------
   Recursively emit all TikZ nodes for a subtree
   ---------------------------------------------------------------- */

void TikzWriter::emitNodes(node *n, const map<node*, NodeGeom> &geom)
{
  int i, nc;

  if(n->is_terminal() || n->is_nonterm() || n->is_null())
    {
      emitNode(n, geom);
      return;
    }

  /* recurse into children */
  nc = n->numChildren();
  for(i = 0; i < nc; i++)
    {
      if(n->getChild(i) != NULL)
	emitNodes(n->getChild(i), geom);
    }
}

/* ----------------------------------------------------------------
   Emit a polyline as a TikZ \draw command
   ---------------------------------------------------------------- */

void TikzWriter::emitPolyline(const Polyline &pl)
{
  unsigned int i;

  if(pl.points.size() < 2)
    return;

  outs << "\\draw [rounded corners=\\railcorners] ";
  for(i = 0; i < pl.points.size(); i++)
    {
      outs << pl.points[i];
      if(i < pl.points.size() - 1)
	outs << " -- ";
    }
  outs << ";\n";
}

/* ----------------------------------------------------------------
   Write a complete production
   ---------------------------------------------------------------- */

void TikzWriter::writeProduction(productionnode *prod,
				 const ProductionLayout &layout)
{
  string boxname;
  unsigned int i;
  node *body;

  boxname = camelcase(prod->getName());

  outs << "\n\\newsavebox{\\" << boxname << "Box}\n\n";
  outs << "\\savebox{\\" << boxname << "Box}{";
  outs << "\\begin{tikzpicture}\n";

  /* production name label — replace underscores with spaces */
  {
    string prodDisplayName = prod->getName();
    replace(prodDisplayName.begin(), prodDisplayName.end(), '_', ' ');
    outs << "\\node at (0pt,0pt)[anchor=west](name){"
	 << latexwrite("railname", prodDisplayName) << "};\n";
  }

  /* emit all TikZ nodes */
  body = prod->getChild(0);
  if(body != NULL)
    emitNodes(body, layout.geom);

  /* emit all connection lines */
  for(i = 0; i < layout.connections.size(); i++)
    emitPolyline(layout.connections[i]);

  outs << "\\end{tikzpicture}\n";
  outs << "}\n\n";
}
