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

#include <layout.hh>
#include <graph.hh>
#include <nodesize.hh>

using namespace std;

/* Forward declarations of layout helpers */
static NodeGeom layoutLeaf(node *n, coordinate origin,
			   map<node*, NodeGeom> &geom, nodesizes *sizes);
static NodeGeom layoutNull(node *n, coordinate origin,
			   map<node*, NodeGeom> &geom, nodesizes *sizes);
static NodeGeom layoutRail(node *n, coordinate origin,
			   map<node*, NodeGeom> &geom, nodesizes *sizes);
static NodeGeom layoutConcat(node *n, coordinate origin,
			     map<node*, NodeGeom> &geom, nodesizes *sizes);
static NodeGeom layoutChoice(node *n, coordinate origin,
			     map<node*, NodeGeom> &geom, nodesizes *sizes);
static NodeGeom layoutRow(node *n, coordinate origin,
			  map<node*, NodeGeom> &geom, nodesizes *sizes);
static NodeGeom layoutNewline(node *n, coordinate origin,
			      map<node*, NodeGeom> &geom, nodesizes *sizes);

static pair<float,float> computeSizeConcat(node *n, nodesizes *sizes);
static pair<float,float> computeSizeChoice(node *n, nodesizes *sizes);
static pair<float,float> computeSizeRow(node *n, nodesizes *sizes);

/* Connection computation helpers */
static void connectConcat(node *n, map<node*, NodeGeom> &geom,
			  vector<Polyline> &lines, nodesizes *sizes);
static void connectChoice(node *n, map<node*, NodeGeom> &geom,
			  vector<Polyline> &lines, nodesizes *sizes);
static void connectLoop(node *n, map<node*, NodeGeom> &geom,
			vector<Polyline> &lines, nodesizes *sizes);

/* ----------------------------------------------------------------
   addPolyline - add a polyline to the list, skipping zero-length lines
   ---------------------------------------------------------------- */

static void addPolyline(vector<Polyline> &lines, Polyline &pl)
{
  unsigned int i;
  int allSame;

  if(pl.points.size() < 2)
    return;

  /* check if all points are the same (zero-length line) */
  allSame = 1;
  for(i = 1; i < pl.points.size(); i++)
    {
      if(pl.points[i].x != pl.points[0].x ||
	 pl.points[i].y != pl.points[0].y)
	{
	  allSame = 0;
	  i = pl.points.size(); /* exit loop */
	}
    }

  if(!allSame)
    lines.push_back(pl);
}

/* ----------------------------------------------------------------
   computeSize - measure a node subtree without placing it
   ---------------------------------------------------------------- */

pair<float,float> computeSize(node *n, nodesizes *sizes)
{
  float w, h;

  if(n->is_terminal() || n->is_nonterm())
    {
      w = n->width();
      h = n->height();
      if(w < sizes->colsep)
	w = sizes->colsep;
      if(h < sizes->rowsep)
	h = sizes->rowsep;
      return make_pair(w, h);
    }

  if(n->is_null())
    return make_pair(0.0f, 0.0f);

  if(n->is_rail())
    return make_pair(0.0f, 0.0f);

  if(n->is_newline())
    return make_pair(0.0f, 0.0f);

  if(n->is_concat())
    return computeSizeConcat(n, sizes);

  if(n->is_choice() || n->is_loop())
    return computeSizeChoice(n, sizes);

  if(n->is_row())
    return computeSizeRow(n, sizes);

  /* singlenode or production: measure the body */
  if(n->numChildren() == 1 && n->getChild(0) != NULL)
    return computeSize(n->getChild(0), sizes);

  return make_pair(0.0f, 0.0f);
}

static pair<float,float> computeSizeConcat(node *n, nodesizes *sizes)
{
  float totalWidth, maxHeight, rowWidth, rowHeight;
  int i, nc;
  int firstContent, contentCount, soloChoiceLoop;
  node *child;
  pair<float,float> csz;

  nc = n->numChildren();

  /* Count content children (not null, rail, or newline).  When
     the concat is a single-child wrapper around a loop/choice,
     skip rail padding — the child already has its own railPad
     and the parent will add another. */
  contentCount = 0;
  soloChoiceLoop = 0;
  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      if(!child->is_null() && !child->is_rail() && !child->is_newline())
	contentCount++;
    }
  if(contentCount == 1)
    {
      for(i = 0; i < nc; i++)
	{
	  child = n->getChild(i);
	  if(!child->is_null() && !child->is_rail() &&
	     !child->is_newline())
	    {
	      if(child->is_choice() || child->is_loop())
		soloChoiceLoop = 1;
	      i = nc; /* done */
	    }
	}
    }

  totalWidth = 0;
  maxHeight = 0;
  rowWidth = 0;
  rowHeight = 0;
  firstContent = 1;

  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      if(child->is_newline())
	{
	  if(rowWidth > totalWidth)
	    totalWidth = rowWidth;
	  maxHeight += rowHeight + 3 * sizes->rowsep;
	  rowWidth = 0;
	  rowHeight = 0;
	  firstContent = 1;
	}
      else
	{
	  csz = computeSize(child, sizes);
	  if(child->is_rail())
	    rowWidth += 0.5f * sizes->colsep;
	  else if(child->is_null())
	    {
	      if(child->getBeforeSkip() > 0)
		rowWidth += child->getBeforeSkip() / 2.0f;
	      if(firstContent)
		firstContent = 0;
	    }
	  else
	    {
	      if(!firstContent)
		rowWidth += sizes->colsep;
	      else
		firstContent = 0;
	      rowWidth += csz.first;
	      /* A choice or loop child normally has rail siblings that
		 add 0.5*colsep each side.  When a rail is absent on
		 one or both sides (e.g. at the edge of a concat after
		 subsume merges rails away), add 0.5*colsep per missing
		 side to compensate.  Skip this when the concat is a
		 single-child wrapper: the child already has railPad. */
	      if(!soloChoiceLoop &&
		 (child->is_choice() || child->is_loop()))
		{
		  if(i == 0 || !n->getChild(i-1)->is_rail())
		    rowWidth += 0.5f * sizes->colsep;
		  if(i+1 >= nc || !n->getChild(i+1)->is_rail())
		    rowWidth += 0.5f * sizes->colsep;
		}
	    }
	  if(csz.second > rowHeight)
	    rowHeight = csz.second;
	}
    }

  /* account for last row */
  if(rowWidth > totalWidth)
    totalWidth = rowWidth;
  maxHeight += rowHeight;

  return make_pair(totalWidth, maxHeight);
}

static pair<float,float> computeSizeChoice(node *n, nodesizes *sizes)
{
  float maxWidth, totalHeight;
  int i, nc;
  int widestIsRailed;
  node *child;
  pair<float,float> csz;

  maxWidth = 0;
  totalHeight = 0;
  widestIsRailed = 0;
  nc = n->numChildren();

  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      csz = computeSize(child, sizes);
      if(csz.first > maxWidth)
	{
	  maxWidth = csz.first;
	  widestIsRailed = (child->is_choice() || child->is_loop());
	}
      if(i > 0)
	totalHeight += sizes->rowsep;
      totalHeight += csz.second;
      /* ensure minimum height per alternative */
      if(csz.second < sizes->rowsep)
	totalHeight += sizes->rowsep - csz.second;
    }

  /* add left and right padding for the rail columns.
     Loops always own their own rails.  For standalone choices,
     skip railPad when the widest child already includes rails. */
  if(n->is_loop() || !widestIsRailed)
    maxWidth += 2.0f * sizes->colsep;

  return make_pair(maxWidth, totalHeight);
}

static pair<float,float> computeSizeRow(node *n, nodesizes *sizes)
{
  /* row contains a single child (usually a concat) */
  if(n->numChildren() == 1 && n->getChild(0) != NULL)
    return computeSize(n->getChild(0), sizes);
  return make_pair(0.0f, 0.0f);
}

/* ----------------------------------------------------------------
   layoutNode - dispatch to type-specific layout
   ---------------------------------------------------------------- */

NodeGeom layoutNode(node *n, coordinate origin,
		    map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  if(n->is_terminal() || n->is_nonterm())
    return layoutLeaf(n, origin, geom, sizes);

  if(n->is_null())
    return layoutNull(n, origin, geom, sizes);

  if(n->is_rail())
    return layoutRail(n, origin, geom, sizes);

  if(n->is_newline())
    return layoutNewline(n, origin, geom, sizes);

  if(n->is_concat())
    return layoutConcat(n, origin, geom, sizes);

  if(n->is_choice() || n->is_loop())
    return layoutChoice(n, origin, geom, sizes);

  if(n->is_row())
    return layoutRow(n, origin, geom, sizes);

  /* singlenode / productionnode: just layout the body */
  if(n->numChildren() == 1 && n->getChild(0) != NULL)
    {
      NodeGeom g;
      g = layoutNode(n->getChild(0), origin, geom, sizes);
      geom[n] = g;
      return g;
    }

  /* fallback: zero-size at origin */
  {
    NodeGeom g;
    g.origin = origin;
    g.width = 0;
    g.height = 0;
    g.entry = origin;
    g.exit = origin;
    geom[n] = g;
    return g;
  }
}

/* ----------------------------------------------------------------
   Leaf node layout (terminal / nonterminal)
   ---------------------------------------------------------------- */

static NodeGeom layoutLeaf(node *n, coordinate origin,
			   map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g;
  float w, h, singleH, yShift;

  w = n->width();
  h = n->height();
  if(w < sizes->colsep)
    w = sizes->colsep;
  if(h < sizes->rowsep)
    h = sizes->rowsep;

  /* For multi-line wrapped nodes, TikZ anchor=west places the
     geometric center at origin.y.  Shift the node down so that
     the center of the top line aligns with origin.y (the rail).
     Single-line height is minsize + 2 (inner sep top+bottom). */
  yShift = 0;
  singleH = sizes->minsize + 2.0f;
  if(h > 1.5f * singleH)
    yShift = -(h - singleH) / 2.0f;

  g.origin = coordinate(origin.x, origin.y + yShift);
  g.width = w;
  g.height = h;
  g.entry = origin;
  g.exit = coordinate(origin.x + w, origin.y);
  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Null node layout
   ---------------------------------------------------------------- */

static NodeGeom layoutNull(node *n, coordinate origin,
			   map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g;

  g.origin = origin;
  g.width = 0;
  g.height = 0;
  g.entry = origin;
  g.exit = origin;
  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Rail node layout (zero-width marker)
   ---------------------------------------------------------------- */

static NodeGeom layoutRail(node *n, coordinate origin,
			   map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g;

  g.origin = origin;
  g.width = 0;
  g.height = 0;
  g.entry = origin;
  g.exit = origin;
  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Newline node layout (zero-width, marks row break)
   ---------------------------------------------------------------- */

static NodeGeom layoutNewline(node *n, coordinate origin,
			      map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g;

  g.origin = origin;
  g.width = 0;
  g.height = 3.0f * sizes->rowsep;
  g.entry = origin;
  g.exit = origin;
  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Row node layout
   ---------------------------------------------------------------- */

static NodeGeom layoutRow(node *n, coordinate origin,
			  map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g, childg;

  childg = layoutNode(n->getChild(0), origin, geom, sizes);
  g.origin = origin;
  g.width = childg.width;
  g.height = childg.height;
  g.entry = childg.entry;
  g.exit = childg.exit;
  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Concat node layout
   ---------------------------------------------------------------- */

static NodeGeom layoutConcat(node *n, coordinate origin,
			     map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g, childg;
  float cursorX, cursorY, rowStartX;
  float lineWidth, maxWidth, rowHeight, totalHeight;
  int i, nc;
  node *child;
  coordinate childOrigin;
  int firstContent, contentCount, soloChoiceLoop;

  nc = n->numChildren();

  /* Detect single-child wrapper around a loop/choice (same logic
     as computeSizeConcat) to skip redundant rail padding. */
  contentCount = 0;
  soloChoiceLoop = 0;
  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      if(!child->is_null() && !child->is_rail() && !child->is_newline())
	contentCount++;
    }
  if(contentCount == 1)
    {
      for(i = 0; i < nc; i++)
	{
	  child = n->getChild(i);
	  if(!child->is_null() && !child->is_rail() &&
	     !child->is_newline())
	    {
	      if(child->is_choice() || child->is_loop())
		soloChoiceLoop = 1;
	      i = nc; /* done */
	    }
	}
    }

  cursorX = origin.x;
  cursorY = origin.y;
  rowStartX = origin.x;
  lineWidth = 0;
  maxWidth = 0;
  rowHeight = 0;
  totalHeight = 0;
  firstContent = 1;

  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);

      if(child->is_newline())
	{
	  /* finish current row */
	  if(lineWidth > maxWidth)
	    maxWidth = lineWidth;
	  totalHeight += rowHeight + 3.0f * sizes->rowsep;

	  /* reset for next row */
	  cursorX = rowStartX;
	  cursorY = cursorY - rowHeight - 3.0f * sizes->rowsep;
	  lineWidth = 0;
	  rowHeight = 0;
	  firstContent = 1;

	  /* place the newline marker */
	  childOrigin = coordinate(cursorX, cursorY);
	  layoutNewline(child, childOrigin, geom, sizes);
	}
      else if(child->is_rail())
	{
	  /* rail: half-colsep spacing */
	  cursorX += 0.5f * sizes->colsep;
	  lineWidth += 0.5f * sizes->colsep;
	  childOrigin = coordinate(cursorX, cursorY);
	  layoutRail(child, childOrigin, geom, sizes);
	}
      else if(child->is_null())
	{
	  /* null nodes are zero-width markers; advance by half their
	     beforeskip to create short leading/trailing lines */
	  if(child->getBeforeSkip() > 0)
	    {
	      cursorX += child->getBeforeSkip() / 2.0f;
	      lineWidth += child->getBeforeSkip() / 2.0f;
	    }
	  childOrigin = coordinate(cursorX, cursorY);
	  layoutNull(child, childOrigin, geom, sizes);
	  if(firstContent)
	    firstContent = 0;
	}
      else
	{
	  /* content node: add colsep before it */
	  if(!firstContent)
	    {
	      cursorX += sizes->colsep;
	      lineWidth += sizes->colsep;
	    }
	  else
	    {
	      firstContent = 0;
	    }

	  /* A choice or loop without an adjacent rail on a given side
	     needs 0.5*colsep of spacing on that side to compensate
	     for the missing rail.  Skip when this is a single-child
	     wrapper: the child already has railPad. */
	  if(!soloChoiceLoop &&
	     (child->is_choice() || child->is_loop()) &&
	     (i == 0 || !n->getChild(i-1)->is_rail()))
	    {
	      cursorX += 0.5f * sizes->colsep;
	      lineWidth += 0.5f * sizes->colsep;
	    }

	  childOrigin = coordinate(cursorX, cursorY);
	  childg = layoutNode(child, childOrigin, geom, sizes);
	  cursorX += childg.width;
	  lineWidth += childg.width;
	  if(childg.height > rowHeight)
	    rowHeight = childg.height;

	  if(!soloChoiceLoop &&
	     (child->is_choice() || child->is_loop()) &&
	     (i+1 >= nc || !n->getChild(i+1)->is_rail()))
	    {
	      cursorX += 0.5f * sizes->colsep;
	      lineWidth += 0.5f * sizes->colsep;
	    }
	}
    }

  /* finish final row */
  if(lineWidth > maxWidth)
    maxWidth = lineWidth;
  totalHeight += rowHeight;

  /* build geometry for this concat */
  g.origin = origin;
  g.width = maxWidth;
  g.height = totalHeight;

  /* entry = first content child's entry; exit = last content child's exit */
  g.entry = origin;
  g.exit = coordinate(origin.x + maxWidth, origin.y);

  /* find actual entry from first non-rail child */
  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      if(!child->is_rail())
	{
	  if(geom.find(child) != geom.end())
	    g.entry = geom[child].entry;
	  i = nc; /* exit loop without break */
	}
    }

  /* find actual exit from last non-rail, non-newline child */
  for(i = nc - 1; i >= 0; i--)
    {
      child = n->getChild(i);
      if(!child->is_rail() && !child->is_newline())
	{
	  if(geom.find(child) != geom.end())
	    g.exit = geom[child].exit;
	  i = -1; /* exit loop without break */
	}
    }

  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Choice / Loop node layout
   Stacks children vertically, centers horizontally.
   First child is the "main" path; additional children are alternatives.
   For loopnode: child 0 = body, child 1 = repeat path.
   ---------------------------------------------------------------- */

static NodeGeom layoutChoice(node *n, coordinate origin,
			     map<node*, NodeGeom> &geom, nodesizes *sizes)
{
  NodeGeom g, childg;
  float maxWidth, totalHeight, cursorY, childWidth, childHeight;
  float railPad;
  int i, nc;
  int widestIsRailed;
  node *child;
  pair<float,float> csz;
  coordinate childOrigin;

  nc = n->numChildren();

  /* first pass: measure all children to find max width */
  maxWidth = 0;
  widestIsRailed = 0;
  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      csz = computeSize(child, sizes);
      if(csz.first > maxWidth)
	{
	  maxWidth = csz.first;
	  widestIsRailed = (child->is_choice() || child->is_loop());
	  if(!widestIsRailed && child->is_concat() &&
	     child->numChildren() == 1 &&
	     (child->getChild(0)->is_choice() ||
	      child->getChild(0)->is_loop()))
	    widestIsRailed = 1;
	}
    }

  /* Loops always own their own rails.  For standalone choices,
     skip railPad when the widest child already includes rails. */
  if(n->is_loop() || !widestIsRailed)
    railPad = sizes->colsep;
  else
    railPad = 0;

  /* second pass: place children vertically, centered within maxWidth */
  cursorY = origin.y;
  totalHeight = 0;

  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);
      csz = computeSize(child, sizes);
      childWidth = csz.first;
      childHeight = csz.second;
      if(childHeight < sizes->rowsep)
	childHeight = sizes->rowsep;

      if(i > 0)
	{
	  cursorY -= sizes->rowsep;
	  totalHeight += sizes->rowsep;
	}

      /* center horizontally within maxWidth, offset by left railPad */
      childOrigin = coordinate(origin.x + railPad +
			       (maxWidth - childWidth) / 2.0f,
			       cursorY);
      childg = layoutNode(child, childOrigin, geom, sizes);

      cursorY -= childHeight;
      totalHeight += childHeight;
    }

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  /* entry and exit are at the left and right edges of the choice,
     at the first child's Y level */
  if(nc > 0 && geom.find(n->getChild(0)) != geom.end())
    {
      g.entry = coordinate(origin.x, geom[n->getChild(0)].entry.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
			  geom[n->getChild(0)].exit.y);
    }
  else
    {
      g.entry = origin;
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
			  origin.y);
    }

  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Connection computation
   ---------------------------------------------------------------- */

void computeConnections(node *n, map<node*, NodeGeom> &geom,
			vector<Polyline> &lines, nodesizes *sizes)
{
  int i, nc;

  if(n->is_concat())
    {
      connectConcat(n, geom, lines, sizes);
      return;
    }

  if(n->is_choice())
    {
      connectChoice(n, geom, lines, sizes);
      return;
    }

  if(n->is_loop())
    {
      connectLoop(n, geom, lines, sizes);
      return;
    }

  if(n->is_row())
    {
      if(n->numChildren() > 0 && n->getChild(0) != NULL)
	computeConnections(n->getChild(0), geom, lines, sizes);
      return;
    }

  /* singlenode/productionnode: recurse into body */
  if(n->numChildren() == 1 && n->getChild(0) != NULL)
    {
      computeConnections(n->getChild(0), geom, lines, sizes);
      return;
    }

  /* leaf nodes: no connections to compute */
  nc = n->numChildren();
  for(i = 0; i < nc; i++)
    {
      if(n->getChild(i) != NULL)
	computeConnections(n->getChild(i), geom, lines, sizes);
    }
}

/* ----------------------------------------------------------------
   Concat connections: draw horizontal lines between consecutive
   content children within each row, plus newline routing.
   ---------------------------------------------------------------- */

static void connectConcat(node *n, map<node*, NodeGeom> &geom,
			  vector<Polyline> &lines, nodesizes *sizes)
{
  int i, j, nc;
  node *child, *prevContent, *nextContent, *nextChild;
  NodeGeom prevGeom, curGeom, nextGeom, concatGeom, childGeom;
  Polyline pl;
  float rightEdge, leftEdge, midY, rowMaxBelow, belowExtent;
  float rowBottom, nextTop;

  nc = n->numChildren();
  prevContent = NULL;
  rowMaxBelow = 0;

  for(i = 0; i < nc; i++)
    {
      child = n->getChild(i);

      /* recurse into children first */
      computeConnections(child, geom, lines, sizes);

      /* track the maximum below-rail extent in the current row.
	 Choice/loop nodes extend mostly below the rail (full
	 height minus half a node box at the bottom).  Concats
	 containing a choice/loop behave similarly.  Centered
	 nodes (terminals, nonterminals) extend half above and
	 half below. */
      if(!child->is_newline() && !child->is_rail() &&
	 geom.find(child) != geom.end())
	{
	  int k, hasRailed;
	  childGeom = geom[child];
	  if(child->is_choice() || child->is_loop())
	    belowExtent = childGeom.height - sizes->minsize / 2.0f;
	  else
	    {
	      belowExtent = childGeom.height / 2.0f;
	      if(child->is_concat())
		{
		  hasRailed = 0;
		  for(k = 0; k < child->numChildren(); k++)
		    if(child->getChild(k)->is_choice() ||
		       child->getChild(k)->is_loop())
		      {
			hasRailed = 1;
			k = child->numChildren(); /* exit loop */
		      }
		  if(hasRailed)
		    belowExtent = childGeom.height -
		      sizes->minsize / 2.0f;
		}
	    }
	  if(belowExtent > rowMaxBelow)
	    rowMaxBelow = belowExtent;
	}

      if(child->is_newline())
	{
	  /* newline: route wrap-around from previous row to next row */
	  if(prevContent != NULL &&
	     geom.find(prevContent) != geom.end() &&
	     geom.find(n) != geom.end())
	    {
	      /* find next content node after the newline */
	      nextContent = NULL;
	      for(j = i + 1; j < nc; j++)
		{
		  nextChild = n->getChild(j);
		  if(!nextChild->is_rail() && !nextChild->is_newline())
		    {
		      if(geom.find(nextChild) != geom.end())
			nextContent = nextChild;
		      j = nc; /* exit loop */
		    }
		}

	      if(nextContent != NULL)
		{
		  concatGeom = geom[n];
		  prevGeom = geom[prevContent];
		  nextGeom = geom[nextContent];
		  rightEdge = concatGeom.origin.x + concatGeom.width;
		  leftEdge = concatGeom.origin.x;

		  /* 6-point wrap-around: right, down, left, down, right.
		     Extend past content edges by colsep so the path
		     routes outside the content area.  Place the
		     horizontal crossover midway between the visual
		     bottom of the departing row and the visual top
		     of the arriving row.  For terminals/nonterminals,
		     content extends minsize/2 above the rail line.
		     For choice/loop nodes (and concats containing
		     them), the entry is at the top rail and nothing
		     extends above, so nextTop = entry.y. */
		  rightEdge += sizes->colsep;
		  leftEdge -= sizes->colsep;
		  rowBottom = prevGeom.exit.y - rowMaxBelow;
		  if(nextContent->is_choice() || nextContent->is_loop())
		    nextTop = nextGeom.entry.y;
		  else if(nextContent->is_concat())
		    {
		      int k2, nextHasRailed;
		      nextHasRailed = 0;
		      for(k2 = 0; k2 < nextContent->numChildren(); k2++)
			if(nextContent->getChild(k2)->is_choice() ||
			   nextContent->getChild(k2)->is_loop())
			  {
			    nextHasRailed = 1;
			    k2 = nextContent->numChildren();
			  }
		      if(nextHasRailed)
			nextTop = nextGeom.entry.y;
		      else
			nextTop = nextGeom.entry.y +
			  sizes->minsize / 2.0f;
		    }
		  else
		    nextTop = nextGeom.entry.y + sizes->minsize / 2.0f;
		  /* Bias the crossover slightly above center (55%)
		     because visual weight is concentrated in the
		     upper portion of each row (node boxes) while
		     the lower portion is sparser (rail arcs). */
		  midY = nextTop + 0.55f * (rowBottom - nextTop);
		  pl.points.clear();
		  pl.points.push_back(prevGeom.exit);
		  pl.points.push_back(coordinate(rightEdge, prevGeom.exit.y));
		  pl.points.push_back(coordinate(rightEdge, midY));
		  pl.points.push_back(coordinate(leftEdge, midY));
		  pl.points.push_back(coordinate(leftEdge, nextGeom.entry.y));
		  pl.points.push_back(nextGeom.entry);
		  addPolyline(lines, pl);
		}
	    }
	  prevContent = NULL;
	  rowMaxBelow = 0;
	}
      else if(child->is_rail())
	{
	  /* rails: draw a short line from/to adjacent content */
	  if(prevContent != NULL &&
	     geom.find(prevContent) != geom.end() &&
	     geom.find(child) != geom.end())
	    {
	      prevGeom = geom[prevContent];
	      curGeom = geom[child];
	      pl.points.clear();
	      pl.points.push_back(prevGeom.exit);
	      pl.points.push_back(curGeom.entry);
	      addPolyline(lines, pl);
	    }
	  prevContent = child;
	}
      else
	{
	  /* content node (including null nodes): connect to previous.
	     Skip the entry stub between start1 and start2 (the first
	     two null nodes) — it is redundant with the line from
	     start2 to the first real content node. */
	  if(prevContent != NULL &&
	     !(prevContent->is_null() && child->is_null() &&
	       prevContent->getBeforeSkip() == 0) &&
	     geom.find(prevContent) != geom.end() &&
	     geom.find(child) != geom.end())
	    {
	      prevGeom = geom[prevContent];
	      curGeom = geom[child];
	      pl.points.clear();
	      pl.points.push_back(prevGeom.exit);
	      pl.points.push_back(curGeom.entry);
	      addPolyline(lines, pl);
	    }
	  prevContent = child;
	}
    }
}

/* ----------------------------------------------------------------
   Choice connections: left and right rails
   ---------------------------------------------------------------- */

static void connectChoice(node *n, map<node*, NodeGeom> &geom,
			  vector<Polyline> &lines, nodesizes *sizes)
{
  int i, nc;
  int parentIsRailed;
  node *child;
  NodeGeom choiceGeom, firstGeom, childGeom;
  Polyline pl;
  float railX, exitRailX;

  nc = n->numChildren();
  if(nc < 1)
    return;
  if(geom.find(n) == geom.end())
    return;

  choiceGeom = geom[n];

  /* When this choice shares rail columns with a parent choice,
     the extensions beyond the rail are handled by the parent.
     A parent loop always owns its own wider rails, so the choice
     should extend outward to reach the loop's rail positions. */
  parentIsRailed = (n->getParent() != NULL &&
		    n->getParent()->is_choice());

  /* recurse into all children first */
  for(i = 0; i < nc; i++)
    computeConnections(n->getChild(i), geom, lines, sizes);

  if(geom.find(n->getChild(0)) == geom.end())
    return;
  firstGeom = geom[n->getChild(0)];

  /* rail X positions: left rail at origin, right rail at right edge */
  railX = choiceGeom.origin.x;
  exitRailX = choiceGeom.origin.x + choiceGeom.width;

  /* first child: straight horizontal entry and exit lines */
  pl.points.clear();
  pl.points.push_back(coordinate(railX, firstGeom.entry.y));
  pl.points.push_back(firstGeom.entry);
  addPolyline(lines, pl);

  pl.points.clear();
  pl.points.push_back(firstGeom.exit);
  pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
  addPolyline(lines, pl);

  /* remaining children: polylines through the rails to each
     alternative.  When the parent is a loop or choice with shared
     rail columns, draw 3-point lines (no extension past the rail).
     Otherwise draw 4-point lines that extend past the rail so the
     approach is RIGHT->DOWN (clockwise = inward curve at top). */
  for(i = 1; i < nc; i++)
    {
      child = n->getChild(i);
      if(geom.find(child) == geom.end())
	return;
      childGeom = geom[child];

      /* Use 3-point lines when the parent already provides rails.
	 Use 4-point lines with a half-colsep inward offset when
	 the child's entry aligns with the rail (e.g. a loop
	 sharing the choice's rail columns).  Otherwise use
	 4-point lines that extend past the rail. */
      if(childGeom.entry.x == railX)
	{
	  /* 4-point: down then inward with half-colsep offset */
	  float halfCol = 0.5f * sizes->colsep;

	  pl.points.clear();
	  pl.points.push_back(coordinate(railX - halfCol,
					 firstGeom.entry.y));
	  pl.points.push_back(coordinate(railX, firstGeom.entry.y));
	  pl.points.push_back(coordinate(railX, childGeom.entry.y));
	  pl.points.push_back(coordinate(railX + halfCol,
					 childGeom.entry.y));
	  addPolyline(lines, pl);

	  pl.points.clear();
	  pl.points.push_back(coordinate(exitRailX - halfCol,
					 childGeom.exit.y));
	  pl.points.push_back(coordinate(exitRailX, childGeom.exit.y));
	  pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
	  pl.points.push_back(coordinate(exitRailX + halfCol,
					 firstGeom.exit.y));
	  addPolyline(lines, pl);
	}
      else if(parentIsRailed)
	{
	  /* 3-point: rail down to entry, no extension beyond rail */
	  pl.points.clear();
	  pl.points.push_back(coordinate(railX, firstGeom.entry.y));
	  pl.points.push_back(coordinate(railX, childGeom.entry.y));
	  pl.points.push_back(childGeom.entry);
	  addPolyline(lines, pl);

	  pl.points.clear();
	  pl.points.push_back(childGeom.exit);
	  pl.points.push_back(coordinate(exitRailX, childGeom.exit.y));
	  pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
	  addPolyline(lines, pl);
	}
      else
	{
	  /* 4-point: extend past rail for inward curve */
	  pl.points.clear();
	  pl.points.push_back(coordinate(railX - sizes->colsep,
					 firstGeom.entry.y));
	  pl.points.push_back(coordinate(railX, firstGeom.entry.y));
	  pl.points.push_back(coordinate(railX, childGeom.entry.y));
	  pl.points.push_back(childGeom.entry);
	  addPolyline(lines, pl);

	  pl.points.clear();
	  pl.points.push_back(childGeom.exit);
	  pl.points.push_back(coordinate(exitRailX, childGeom.exit.y));
	  pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
	  pl.points.push_back(coordinate(exitRailX + sizes->colsep,
					 firstGeom.exit.y));
	  addPolyline(lines, pl);
	}
    }
}

/* ----------------------------------------------------------------
   Loop connections: body path forward, repeat path backward
   Child 0 = body (forward), Children 1..N-1 = repeat (backward/feedback)
   ---------------------------------------------------------------- */

static void connectLoop(node *n, map<node*, NodeGeom> &geom,
			vector<Polyline> &lines, nodesizes *sizes)
{
  int i, nc;
  NodeGeom loopGeom, bodyGeom, repeatGeom;
  Polyline pl;
  float railX, exitRailX;

  nc = n->numChildren();
  if(nc < 2)
    return;
  if(geom.find(n) == geom.end())
    return;

  /* recurse into children */
  for(i = 0; i < nc; i++)
    computeConnections(n->getChild(i), geom, lines, sizes);

  loopGeom = geom[n];

  if(geom.find(n->getChild(0)) == geom.end())
    return;

  bodyGeom = geom[n->getChild(0)];

  railX = loopGeom.origin.x;
  exitRailX = loopGeom.origin.x + loopGeom.width;

  /* Straight horizontal lines for body entry and exit, connecting
     the rail positions to the body endpoints.  Only needed when the
     loop has its own rails.  When the loop shares its rails with
     an ancestor choice (after mergeRails), the choice handles the
     connections and stubs are redundant. */
  {
    int needStubs;
    node *p;
    needStubs = 1;
    p = n->getParent();
    while(p != NULL)
      {
	if(p->is_choice() &&
	   p->getLeftRail() == n->getLeftRail() &&
	   p->getRightRail() == n->getRightRail())
	  { needStubs = 0; p = NULL; }
	else if(p->is_production())
	  { p = NULL; }
	else
	  { p = p->getParent(); }
      }
    if(needStubs)
    {
      pl.points.clear();
      pl.points.push_back(coordinate(railX, bodyGeom.entry.y));
      pl.points.push_back(bodyGeom.entry);
      addPolyline(lines, pl);

      pl.points.clear();
      pl.points.push_back(bodyGeom.exit);
      pl.points.push_back(coordinate(exitRailX, bodyGeom.exit.y));
      addPolyline(lines, pl);
    }
  }

  /* The repeat (feedback) path flows right-to-left, but layoutConcat
     always sets entry=left, exit=right.  So for the feedback connections
     we swap: use repeatGeom.exit (right side) for the right rail, and
     repeatGeom.entry (left side) for the left rail.
     Loop over all repeat children (1..nc-1) to handle loops with
     multiple repeat alternatives (e.g. { A | B | C }). */
  for(i = 1; i < nc; i++)
  {
    if(geom.find(n->getChild(i)) == geom.end())
      return;
    repeatGeom = geom[n->getChild(i)];

    /* Right feedback: body exit → right rail → down → repeat right side.
       The corner at (exitRailX, bodyY) curves inward. */
    pl.points.clear();
    pl.points.push_back(bodyGeom.exit);
    pl.points.push_back(coordinate(exitRailX, bodyGeom.exit.y));
    pl.points.push_back(coordinate(exitRailX, repeatGeom.exit.y));
    pl.points.push_back(repeatGeom.exit);
    addPolyline(lines, pl);

    /* Left feedback: repeat left side → left rail → up → body entry.
       The corner at (railX, bodyY) curves inward. */
    pl.points.clear();
    pl.points.push_back(repeatGeom.entry);
    pl.points.push_back(coordinate(railX, repeatGeom.entry.y));
    pl.points.push_back(coordinate(railX, bodyGeom.entry.y));
    pl.points.push_back(bodyGeom.entry);
    addPolyline(lines, pl);
  }
}

/* ----------------------------------------------------------------
   Newline connections: route from end of row to start of next row.
   The newline node itself is placed at the start of the next row.
   We need to find the previous row's last content exit and the
   next row's first content entry.
   ---------------------------------------------------------------- */


/* ----------------------------------------------------------------
   layoutProduction - lay out an entire production
   ---------------------------------------------------------------- */

ProductionLayout layoutProduction(productionnode *prod, nodesizes *sizes)
{
  ProductionLayout layout;
  coordinate origin;
  node *body;

  /* production body starts below the production name */
  origin = coordinate(0.0f, -1.5f * sizes->minsize);

  body = prod->getChild(0);
  if(body != NULL)
    layoutNode(body, origin, layout.geom, sizes);

  /* compute all connection polylines */
  if(body != NULL)
    computeConnections(body, layout.geom, layout.connections, sizes);

  return layout;
}
