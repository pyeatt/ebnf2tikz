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
   collectWrappableNodes - walk a node tree and collect nonterminal
   nodes whose display names contain spaces (can be split into
   multiple lines).
   ---------------------------------------------------------------- */

static void collectWrappableNodes(node *n,
				  vector<nontermnode*> &result)
{
  int i, nc;

  if(n == NULL)
    return;

  if(n->is_nonterm())
    {
      if(!((nontermnode*)n)->getWrapped() &&
	 hasWrappableSpaces((nontermnode*)n))
	result.push_back((nontermnode*)n);
      return;
    }

  nc = n->numChildren();
  for(i = 0; i < nc; i++)
    {
      if(n->getChild(i) != NULL)
	collectWrappableNodes(n->getChild(i), result);
    }
}

/* ----------------------------------------------------------------
   estimateUnwrappedExtra - estimate how much wider the production
   would be if previously-wrapped nodes were displayed as single
   lines.  A wrapped node with N text lines has bnfnodes.dat width
   W (the widest line).  Unwrapped, the width would be roughly
   N*W, so the extra is (N-1)*W.  We estimate N from the height:
   N = round(height / minsize).

   This lets us detect whether the production would still be
   overwide without wrapping, so we can stop wrapping when the
   user increases \textwidth enough to fit.
   ---------------------------------------------------------------- */

static float estimateUnwrappedExtra(node *n, nodesizes *sizes)
{
  int i, nc, numLines;
  float extra;

  if(n == NULL)
    return 0;

  if(n->is_nonterm())
    {
      /* Single-line nodes are ~minsize+2 tall (16pt for minsize=14).
	 Wrapped two-line nodes are ~2*minsize+3 tall (31pt).
	 Use 1.5*minsize as the threshold between them. */
      if(n->height() > 1.5f * sizes->minsize &&
	 hasWrappableSpaces((nontermnode*)n))
	{
	  numLines = (int)(n->height() / sizes->minsize + 0.5f);
	  if(numLines < 2)
	    numLines = 2;
	  return (float)(numLines - 1) * n->width();
	}
      return 0;
    }

  extra = 0;
  nc = n->numChildren();
  for(i = 0; i < nc; i++)
    {
      if(n->getChild(i) != NULL)
	extra += estimateUnwrappedExtra(n->getChild(i), sizes);
    }
  return extra;
}

void grammar::place(ofstream &outs)
{
  nodesizes *sizes;
  TikzWriter writer(outs, node::getSizes());
  ProductionLayout layout;
  int i, n, j;
  node *body;
  float bodyWidth;
  vector<nontermnode*> wrappable;
  int needsWrap;
  pair<float,float> bodySize;
  float extraWidth;

  sizes = node::getSizes();
  n = productions.size();

  for(i = 0; i < n; i++)
    {
      if(productions[i]->getSubsume() == NULL)
	{
	  body = productions[i]->getChild(0);

	  /* Pre-layout: determine if wrapping is needed */
	  needsWrap = 0;
	  if(sizes->textwidth > 0 && body != NULL)
	    {
	      bodySize = computeSize(body, sizes);

	      /* Check if production exceeds \textwidth */
	      if(bodySize.first > sizes->textwidth)
		needsWrap = 1;

	      /* If nodes were wrapped on a previous pass, the
		 bnfnodes.dat dimensions reflect wrapped (narrow)
		 sizes.  Estimate the unwrapped width to decide
		 whether wrapping is still needed.  This allows
		 wrapping to turn off if \textwidth increases. */
	      if(!needsWrap)
		{
		  extraWidth = estimateUnwrappedExtra(body, sizes);
		  if(extraWidth > 0 &&
		     bodySize.first + extraWidth > sizes->textwidth)
		    needsWrap = 1;
		}

	      if(needsWrap)
		{
		  wrappable.clear();
		  collectWrappableNodes(body, wrappable);
		  for(j = 0; j < (int)wrappable.size(); j++)
		    wrappable[j]->setWrapped(1);
		}
	    }

	  /* Layout with wrapping already applied */
	  layout = layoutProduction(productions[i], sizes);

	  /* Report if still overwide after wrapping */
	  if(sizes->textwidth > 0 && body != NULL)
	    {
	      bodyWidth = 0;
	      if(layout.geom.find(body) != layout.geom.end())
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
