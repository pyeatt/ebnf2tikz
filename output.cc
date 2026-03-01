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
#include <algorithm>
#include <vector>
#include <string>

/* ----------------------------------------------------------------
   hasWrappableSpaces - check if a nonterminal's display name has
   spaces (i.e., its original name had underscores).  Parses the
   texName() output which has the form \railname{some text\strut}.
   ---------------------------------------------------------------- */

static int hasWrappableSpaces(nontermnode *n)
{
  string name;
  string::size_type pos;

  name = n->texName();
  pos = name.find('{');
  if(pos != string::npos)
    {
      name = name.substr(pos + 1);
      pos = name.find("\\strut}");
      if(pos != string::npos)
	name = name.substr(0, pos);
      if(name.find(' ') != string::npos)
	return 1;
    }
  return 0;
}

/* ----------------------------------------------------------------
   collectWrappable - walk a node tree and collect nonterminal nodes
   whose display names contain spaces (can be split into multiple
   lines).  Each entry pairs the node with its current width.
   ---------------------------------------------------------------- */

static void collectWrappable(node *n, map<node*, NodeGeom> &geom,
			     vector<pair<nontermnode*, float>> &result)
{
  int i, nc;

  if(n == NULL)
    return;

  if(n->is_nonterm())
    {
      if(geom.find(n) != geom.end() &&
	 !((nontermnode*)n)->getWrapped() &&
	 hasWrappableSpaces((nontermnode*)n))
	{
	  result.push_back(make_pair((nontermnode*)n, geom[n].width));
	}
      return;
    }

  nc = n->numChildren();
  for(i = 0; i < nc; i++)
    {
      if(n->getChild(i) != NULL)
	collectWrappable(n->getChild(i), geom, result);
    }
}

/* Compare function for sorting by width descending */
static bool widthDescending(const pair<nontermnode*, float> &a,
			    const pair<nontermnode*, float> &b)
{
  return a.second > b.second;
}

void grammar::place(ofstream &outs)
{
  nodesizes *sizes;
  TikzWriter writer(outs, node::getSizes());
  ProductionLayout layout;
  int i, n, j;
  node *body;
  float bodyWidth;
  vector<pair<nontermnode*, float>> wrappable;
  int changed;

  sizes = node::getSizes();
  n = productions.size();

  for(i = 0; i < n; i++)
    {
      if(productions[i]->getSubsume() == NULL)
	{
	  layout = layoutProduction(productions[i], sizes);

	  /* Check if production exceeds \textwidth and try to narrow */
	  if(sizes->textwidth > 0)
	    {
	      body = productions[i]->getChild(0);
	      bodyWidth = 0;
	      if(body != NULL &&
		 layout.geom.find(body) != layout.geom.end())
		bodyWidth = layout.geom[body].width;

	      if(bodyWidth > sizes->textwidth)
		{
		  cerr << "Warning: production '"
		       << productions[i]->getName()
		       << "' width (" << bodyWidth
		       << "pt) exceeds \\textwidth ("
		       << sizes->textwidth << "pt) by "
		       << bodyWidth - sizes->textwidth << "pt"
		       << ", wrapping long node names"
		       << endl;

		  /* Collect wrappable nonterminals sorted widest first */
		  wrappable.clear();
		  collectWrappable(body, layout.geom, wrappable);
		  sort(wrappable.begin(), wrappable.end(),
		       widthDescending);

		  /* Flag all wrappable nodes and re-layout */
		  changed = 0;
		  for(j = 0; j < (int)wrappable.size(); j++)
		    {
		      wrappable[j].first->setWrapped(1);
		      changed = 1;
		    }

		  if(changed)
		    layout = layoutProduction(productions[i], sizes);
		}
	    }

	  writer.writeProduction(productions[i], layout);
	}
    }
}
