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

#include <graph.hh>
#include <layout.hh>
#include <tikzwriter.hh>
#include <iostream>

void grammar::place(ofstream &outs)
{
  nodesizes *sizes;
  TikzWriter writer(outs, node::getSizes());
  ProductionLayout layout;
  int i, n;
  node *body;
  float bodyWidth;

  sizes = node::getSizes();
  n = productions.size();

  for(i = 0; i < n; i++)
    {
      if(productions[i]->getSubsume() == NULL)
	{
	  layout = layoutProduction(productions[i], sizes);

	  /* Check if production exceeds \textwidth */
	  if(sizes->textwidth > 0)
	    {
	      body = productions[i]->getChild(0);
	      bodyWidth = 0;
	      if(body != NULL &&
		 layout.geom.find(body) != layout.geom.end())
		bodyWidth = layout.geom[body].width;
	      if(bodyWidth > sizes->textwidth)
		cerr << "Warning: production '"
		     << productions[i]->getName()
		     << "' width (" << bodyWidth
		     << "pt) exceeds \\textwidth ("
		     << sizes->textwidth << "pt) by "
		     << bodyWidth - sizes->textwidth << "pt"
		     << endl;
	    }

	  writer.writeProduction(productions[i], layout);
	}
    }
}
