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

#include "ast_layout.hh"
#include <algorithm>

using namespace std;
using namespace ast;

/* Forward declarations */
static pair<float,float> computeSizeSequence(SequenceNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);
static pair<float,float> computeSizeChoice(ChoiceNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);
static pair<float,float> computeSizeOptional(OptionalNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);
static pair<float,float> computeSizeLoop(LoopNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);

static ASTNodeGeom layoutLeaf(ASTNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);
static ASTNodeGeom layoutEpsilon(ASTNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info);
static ASTNodeGeom layoutSequence(SequenceNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes, int reversed);
static ASTNodeGeom layoutChoicelike(ASTNode *n,
    vector<ASTNode*> &alternatives, int isLoop,
    coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);
static ASTNodeGeom layoutOptional(OptionalNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);
static ASTNodeGeom layoutLoop(LoopNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes);

static void connectSequence(SequenceNode *n,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes);
static void connectChoicelike(ASTNode *self,
    vector<ASTNode*> &alternatives,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes,
    int parentIsChoice);
static void connectOptional(OptionalNode *n,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes,
    int parentIsChoice);
static void connectLoop(LoopNode *n,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes);

/* ----------------------------------------------------------------
   Utility: add a polyline, skipping zero-length lines
   ---------------------------------------------------------------- */

static void addPolyline(vector<ASTPolyline> &lines, ASTPolyline &pl)
{
  unsigned int i;
  int allSame;

  if(pl.points.size() < 2)
    return;

  allSame = 1;
  for(i = 1; i < pl.points.size(); i++)
    {
      if(pl.points[i].x != pl.points[0].x ||
         pl.points[i].y != pl.points[0].y)
        {
          allSame = 0;
          i = pl.points.size();
        }
    }

  if(!allSame)
    lines.push_back(pl);
}

/* ----------------------------------------------------------------
   Utility: check if an AST node is a choice-like type (has rails)
   ---------------------------------------------------------------- */

static int isRailed(ASTNode *n)
{
  return (n->kind == ASTKind::Choice ||
          n->kind == ASTKind::Optional ||
          n->kind == ASTKind::Loop);
}

/* ----------------------------------------------------------------
   ASTLayoutContext
   ---------------------------------------------------------------- */

ASTLayoutContext::ASTLayoutContext(nodesizes *s)
  : nodeCounter(0), coordCounter(0), sizes(s)
{
}

string ASTLayoutContext::nextNode()
{
  return "node" + to_string(++nodeCounter);
}

string ASTLayoutContext::nextCoord()
{
  return "coord" + to_string(++coordCounter);
}

/* ----------------------------------------------------------------
   astAssignNames - walk AST assigning TikZ names to leaf nodes
   ---------------------------------------------------------------- */

void astAssignNames(ASTNode *n, ASTLayoutContext &ctx,
                    map<ASTNode*, ASTLeafInfo> &info)
{
  ASTLeafInfo li;
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  size_t i;

  switch(n->kind)
    {
    case ASTKind::Terminal:
      li.tikzName = ctx.nextNode();
      li.rawText = static_cast<TerminalNode*>(n)->text;
      /* strip surrounding quotes */
      if(li.rawText.size() >= 2)
        li.rawText = li.rawText.substr(1, li.rawText.size() - 2);
      li.style = "terminal";
      li.format = "railtermname";
      li.isTerminal = 1;
      li.wrapped = 0;
      ctx.sizes->getSize(li.tikzName, li.width, li.height);
      info[n] = li;
      return;

    case ASTKind::Nonterminal:
      li.tikzName = ctx.nextNode();
      li.rawText = static_cast<NonterminalNode*>(n)->name;
      li.style = "nonterminal";
      li.format = "railname";
      li.isTerminal = 0;
      li.wrapped = 0;
      ctx.sizes->getSize(li.tikzName, li.width, li.height);
      info[n] = li;
      return;

    case ASTKind::Epsilon:
      li.tikzName = ctx.nextCoord();
      li.rawText = "";
      li.style = "null";
      li.format = "";
      li.isTerminal = 0;
      li.wrapped = 0;
      li.width = 0;
      li.height = 0;
      info[n] = li;
      return;

    case ASTKind::Newline:
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        astAssignNames(seq->children[i], ctx, info);
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        astAssignNames(ch->alternatives[i], ctx, info);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      astAssignNames(opt->child, ctx, info);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        astAssignNames(loop->body, ctx, info);
      for(i = 0; i < loop->repeats.size(); i++)
        astAssignNames(loop->repeats[i], ctx, info);
      return;
    }
}

/* ----------------------------------------------------------------
   astComputeSize - measure an AST subtree
   ---------------------------------------------------------------- */

pair<float,float> astComputeSize(ASTNode *n,
                                 map<ASTNode*, ASTLeafInfo> &info,
                                 nodesizes *sizes)
{
  float w, h;
  ASTLeafInfo *li;
  auto it = info.find(n);

  switch(n->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
      if(it != info.end())
        {
          li = &it->second;
          w = li->width;
          h = li->height;
          if(w < sizes->colsep)
            w = sizes->colsep;
          if(h < sizes->rowsep)
            h = sizes->rowsep;
          return make_pair(w, h);
        }
      return make_pair(sizes->colsep, sizes->rowsep);

    case ASTKind::Epsilon:
      return make_pair(0.0f, 0.0f);

    case ASTKind::Newline:
      return make_pair(0.0f, 0.0f);

    case ASTKind::Sequence:
      return computeSizeSequence(static_cast<SequenceNode*>(n),
                                 info, sizes);

    case ASTKind::Choice:
      return computeSizeChoice(static_cast<ChoiceNode*>(n),
                               info, sizes);

    case ASTKind::Optional:
      return computeSizeOptional(static_cast<OptionalNode*>(n),
                                 info, sizes);

    case ASTKind::Loop:
      return computeSizeLoop(static_cast<LoopNode*>(n), info, sizes);
    }

  return make_pair(0.0f, 0.0f);
}

/* ----------------------------------------------------------------
   computeSizeSequence
   ---------------------------------------------------------------- */

static pair<float,float> computeSizeSequence(SequenceNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  float totalWidth, maxHeight, rowWidth, rowHeight;
  size_t i;
  ASTNode *child;
  pair<float,float> csz;
  int firstContent;

  totalWidth = 0;
  maxHeight = 0;
  rowWidth = 0;
  rowHeight = 0;
  firstContent = 1;

  for(i = 0; i < n->children.size(); i++)
    {
      child = n->children[i];

      if(child->kind == ASTKind::Newline)
        {
          if(rowWidth > totalWidth)
            totalWidth = rowWidth;
          maxHeight += rowHeight + 3.0f * sizes->rowsep;
          rowWidth = 0;
          rowHeight = 0;
          firstContent = 1;
        }
      else if(child->kind == ASTKind::Epsilon)
        {
          /* epsilon children in a sequence are zero-width */
          if(firstContent)
            firstContent = 0;
        }
      else
        {
          csz = astComputeSize(child, info, sizes);
          if(!firstContent)
            rowWidth += sizes->colsep;
          else
            firstContent = 0;
          rowWidth += csz.first;
          if(csz.second > rowHeight)
            rowHeight = csz.second;
        }
    }

  if(rowWidth > totalWidth)
    totalWidth = rowWidth;
  maxHeight += rowHeight;

  return make_pair(totalWidth, maxHeight);
}

/* ----------------------------------------------------------------
   computeSizeChoicelike - shared sizing for choice and optional
   Computes size for a list of alternatives with optional railPad.
   ---------------------------------------------------------------- */

static pair<float,float> computeSizeChoicelike(
    vector<ASTNode*> &alternatives, int alwaysRailPad,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  float maxWidth, totalHeight;
  size_t i;
  int widestIsRailed;
  ASTNode *child;
  pair<float,float> csz;

  maxWidth = 0;
  totalHeight = 0;
  widestIsRailed = 0;

  for(i = 0; i < alternatives.size(); i++)
    {
      child = alternatives[i];
      csz = astComputeSize(child, info, sizes);
      if(csz.first > maxWidth)
        {
          maxWidth = csz.first;
          widestIsRailed = isRailed(child);
        }
      if(i > 0)
        totalHeight += sizes->rowsep;
      totalHeight += csz.second;
      if(csz.second < sizes->rowsep)
        totalHeight += sizes->rowsep - csz.second;
    }

  /* Add rail padding.  Loops always add their own.  Choices skip
     when the widest child already includes rails. */
  if(alwaysRailPad || !widestIsRailed)
    maxWidth += 2.0f * sizes->colsep;

  return make_pair(maxWidth, totalHeight);
}

static pair<float,float> computeSizeChoice(ChoiceNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  return computeSizeChoicelike(n->alternatives, 0, info, sizes);
}

/* ----------------------------------------------------------------
   computeSizeOptional - like choice(epsilon, child)
   ---------------------------------------------------------------- */

static pair<float,float> computeSizeOptional(OptionalNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  vector<ASTNode*> alts;
  /* We don't have a real Epsilon node to put here; we'll use NULL
     as a sentinel and handle it specially. */

  /* Use the child's size plus a null first alternative */
  float maxWidth, totalHeight, childW, childH;
  pair<float,float> csz;
  int widestIsRailed;

  csz = astComputeSize(n->child, info, sizes);
  childW = csz.first;
  childH = csz.second;

  maxWidth = childW;
  widestIsRailed = isRailed(n->child);

  /* First alternative: epsilon (0 width, 0 height) */
  totalHeight = sizes->rowsep;  /* minimum height for epsilon */
  totalHeight += sizes->rowsep; /* separator */
  totalHeight += childH;
  if(childH < sizes->rowsep)
    totalHeight += sizes->rowsep - childH;

  if(!widestIsRailed)
    maxWidth += 2.0f * sizes->colsep;

  return make_pair(maxWidth, totalHeight);
}

/* ----------------------------------------------------------------
   computeSizeLoop - body on top, repeats below
   ---------------------------------------------------------------- */

static pair<float,float> computeSizeLoop(LoopNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  float maxWidth, totalHeight;
  size_t i;
  pair<float,float> csz;

  maxWidth = 0;
  totalHeight = 0;

  /* Body (child 0, may be null/epsilon) */
  if(n->body != NULL)
    {
      csz = astComputeSize(n->body, info, sizes);
      if(csz.first > maxWidth)
          maxWidth = csz.first;
      totalHeight += csz.second;
      if(csz.second < sizes->rowsep)
        totalHeight += sizes->rowsep - csz.second;
    }
  else
    {
      /* null body = epsilon */
      totalHeight += sizes->rowsep;
    }

  /* Repeats (children 1..N) */
  for(i = 0; i < n->repeats.size(); i++)
    {
      csz = astComputeSize(n->repeats[i], info, sizes);
      totalHeight += sizes->rowsep;
      totalHeight += csz.second;
      if(csz.second < sizes->rowsep)
        totalHeight += sizes->rowsep - csz.second;
      if(csz.first > maxWidth)
          maxWidth = csz.first;
    }

  /* Loops always add their own rail padding */
  maxWidth += 2.0f * sizes->colsep;

  return make_pair(maxWidth, totalHeight);
}

/* ----------------------------------------------------------------
   astLayoutNode - dispatch to type-specific layout
   ---------------------------------------------------------------- */

ASTNodeGeom astLayoutNode(ASTNode *n, coordinate origin,
                          map<ASTNode*, ASTNodeGeom> &geom,
                          map<ASTNode*, ASTLeafInfo> &info,
                          nodesizes *sizes)
{
  switch(n->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
      return layoutLeaf(n, origin, geom, info, sizes);

    case ASTKind::Epsilon:
      return layoutEpsilon(n, origin, geom, info);

    case ASTKind::Newline:
      {
        ASTNodeGeom g;
        g.origin = origin;
        g.width = 0;
        g.height = 3.0f * sizes->rowsep;
        g.entry = origin;
        g.exit = origin;
        geom[n] = g;
        return g;
      }

    case ASTKind::Sequence:
      return layoutSequence(static_cast<SequenceNode*>(n), origin,
                            geom, info, sizes, 0);

    case ASTKind::Choice:
      {
        ChoiceNode *ch = static_cast<ChoiceNode*>(n);
        return layoutChoicelike(n, ch->alternatives, 0,
                                origin, geom, info, sizes);
      }

    case ASTKind::Optional:
      return layoutOptional(static_cast<OptionalNode*>(n), origin,
                            geom, info, sizes);

    case ASTKind::Loop:
      return layoutLoop(static_cast<LoopNode*>(n), origin,
                        geom, info, sizes);
    }

  /* fallback */
  {
    ASTNodeGeom g;
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
   Leaf layout (terminal / nonterminal)
   ---------------------------------------------------------------- */

static ASTNodeGeom layoutLeaf(ASTNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  ASTNodeGeom g;
  ASTLeafInfo *li;
  float w, h, singleH, yShift;
  auto it = info.find(n);

  if(it == info.end())
    {
      w = sizes->colsep;
      h = sizes->rowsep;
    }
  else
    {
      li = &it->second;
      w = li->width;
      h = li->height;
      if(w < sizes->colsep)
        w = sizes->colsep;
      if(h < sizes->rowsep)
        h = sizes->rowsep;
    }

  /* For multi-line wrapped nodes, shift down so the top line center
     aligns with the rail */
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
   Epsilon layout (zero-width coordinate)
   ---------------------------------------------------------------- */

static ASTNodeGeom layoutEpsilon(ASTNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info)
{
  ASTNodeGeom g;

  g.origin = origin;
  g.width = 0;
  g.height = 0;
  g.entry = origin;
  g.exit = origin;
  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Sequence layout - place children left-to-right (or reversed)
   ---------------------------------------------------------------- */

static ASTNodeGeom layoutSequence(SequenceNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes,
    int reversed)
{
  ASTNodeGeom g, childg;
  float cursorX, cursorY, rowStartX;
  float lineWidth, maxWidth, rowHeight, totalHeight;
  size_t i, idx;
  ASTNode *child;
  coordinate childOrigin;
  int firstContent;
  pair<float,float> csz;
  size_t nc;

  nc = n->children.size();

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
      idx = reversed ? (nc - 1 - i) : i;
      child = n->children[idx];

      if(child->kind == ASTKind::Newline)
        {
          if(lineWidth > maxWidth)
            maxWidth = lineWidth;
          totalHeight += rowHeight + 3.0f * sizes->rowsep;

          cursorX = rowStartX;
          cursorY = cursorY - rowHeight - 3.0f * sizes->rowsep;
          lineWidth = 0;
          rowHeight = 0;
          firstContent = 1;

          childOrigin = coordinate(cursorX, cursorY);
          ASTNodeGeom nlg;
          nlg.origin = childOrigin;
          nlg.width = 0;
          nlg.height = 3.0f * sizes->rowsep;
          nlg.entry = childOrigin;
          nlg.exit = childOrigin;
          geom[child] = nlg;
        }
      else if(child->kind == ASTKind::Epsilon)
        {
          childOrigin = coordinate(cursorX, cursorY);
          layoutEpsilon(child, childOrigin, geom, info);
          if(firstContent)
            firstContent = 0;
        }
      else
        {
          if(!firstContent)
            {
              cursorX += sizes->colsep;
              lineWidth += sizes->colsep;
            }
          else
            {
              firstContent = 0;
            }

          childOrigin = coordinate(cursorX, cursorY);
          childg = astLayoutNode(child, childOrigin, geom, info, sizes);
          cursorX += childg.width;
          lineWidth += childg.width;
          if(childg.height > rowHeight)
            rowHeight = childg.height;
        }
    }

  if(lineWidth > maxWidth)
    maxWidth = lineWidth;
  totalHeight += rowHeight;

  g.origin = origin;
  g.width = maxWidth;
  g.height = totalHeight;

  /* entry = first placed child's entry */
  /* exit = last placed child's exit */
  g.entry = origin;
  g.exit = coordinate(origin.x + maxWidth, origin.y);

  /* find actual entry: first non-epsilon, non-newline child in
     placement order */
  for(i = 0; i < nc; i++)
    {
      idx = reversed ? (nc - 1 - i) : i;
      child = n->children[idx];
      if(child->kind != ASTKind::Newline)
        {
          if(geom.find(child) != geom.end())
            g.entry = geom[child].entry;
          i = nc;
        }
    }

  /* find actual exit: last non-newline child in placement order */
  for(i = nc; i > 0; )
    {
      i--;
      idx = reversed ? (nc - 1 - i) : i;
      child = n->children[idx];
      if(child->kind != ASTKind::Newline)
        {
          if(geom.find(child) != geom.end())
            g.exit = geom[child].exit;
          i = 0;
        }
    }

  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Choice-like layout - shared between Choice, Optional parts
   Stacks alternatives vertically, centers horizontally.
   ---------------------------------------------------------------- */

static ASTNodeGeom layoutChoicelike(ASTNode *n,
    vector<ASTNode*> &alternatives, int isLoop,
    coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  ASTNodeGeom g, childg;
  float maxWidth, totalHeight, cursorY, childWidth, childHeight;
  float railPad;
  size_t i;
  int widestIsRailed;
  ASTNode *child;
  pair<float,float> csz;
  coordinate childOrigin;

  /* first pass: find max width */
  maxWidth = 0;
  widestIsRailed = 0;
  for(i = 0; i < alternatives.size(); i++)
    {
      child = alternatives[i];
      csz = astComputeSize(child, info, sizes);
      if(csz.first > maxWidth)
        {
          maxWidth = csz.first;
          widestIsRailed = isRailed(child);
        }
    }

  if(isLoop || !widestIsRailed)
    railPad = sizes->colsep;
  else
    railPad = 0;

  /* second pass: place children vertically */
  cursorY = origin.y;
  totalHeight = 0;

  for(i = 0; i < alternatives.size(); i++)
    {
      child = alternatives[i];
      csz = astComputeSize(child, info, sizes);
      childWidth = csz.first;
      childHeight = csz.second;
      if(childHeight < sizes->rowsep)
        childHeight = sizes->rowsep;

      if(i > 0)
        {
          cursorY -= sizes->rowsep;
          totalHeight += sizes->rowsep;
        }

      childOrigin = coordinate(origin.x + railPad +
                               (maxWidth - childWidth) / 2.0f,
                               cursorY);
      childg = astLayoutNode(child, childOrigin, geom, info, sizes);

      cursorY -= childHeight;
      totalHeight += childHeight;
    }

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  if(alternatives.size() > 0 &&
     geom.find(alternatives[0]) != geom.end())
    {
      g.entry = coordinate(origin.x,
                            geom[alternatives[0]].entry.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           geom[alternatives[0]].exit.y);
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
   Optional layout - choice(epsilon, child) with rails
   ---------------------------------------------------------------- */

static ASTNodeGeom layoutOptional(OptionalNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  ASTNodeGeom g, childg;
  float maxWidth, totalHeight, cursorY, childWidth, childHeight;
  float railPad;
  int widestIsRailed;
  pair<float,float> csz;
  coordinate childOrigin;

  csz = astComputeSize(n->child, info, sizes);
  childWidth = csz.first;
  childHeight = csz.second;
  if(childHeight < sizes->rowsep)
    childHeight = sizes->rowsep;

  maxWidth = childWidth;
  widestIsRailed = isRailed(n->child);

  if(!widestIsRailed)
    railPad = sizes->colsep;
  else
    railPad = 0;

  /* Place epsilon (first alternative) at the top */
  cursorY = origin.y;
  totalHeight = sizes->rowsep; /* minimum height for epsilon path */

  /* Place child (second alternative) below */
  cursorY -= sizes->rowsep;
  totalHeight += sizes->rowsep; /* separator */

  childOrigin = coordinate(origin.x + railPad +
                           (maxWidth - childWidth) / 2.0f,
                           cursorY);
  childg = astLayoutNode(n->child, childOrigin, geom, info, sizes);

  totalHeight += childHeight;

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  /* entry and exit at the epsilon (top) level */
  g.entry = coordinate(origin.x, origin.y);
  g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                       origin.y);

  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   Loop layout - body on top, reversed repeats below
   ---------------------------------------------------------------- */

static ASTNodeGeom layoutLoop(LoopNode *n, coordinate origin,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  ASTNodeGeom g, childg;
  float maxWidth, totalHeight, cursorY;
  float childWidth, childHeight, railPad;
  size_t i;
  pair<float,float> csz;
  coordinate childOrigin;

  /* Compute max width across body and all repeats */
  maxWidth = 0;
  if(n->body != NULL)
    {
      csz = astComputeSize(n->body, info, sizes);
      if(csz.first > maxWidth)
        maxWidth = csz.first;
    }
  for(i = 0; i < n->repeats.size(); i++)
    {
      csz = astComputeSize(n->repeats[i], info, sizes);
      if(csz.first > maxWidth)
        maxWidth = csz.first;
    }

  /* Loops always add their own rail padding */
  railPad = sizes->colsep;

  /* Place body at top */
  cursorY = origin.y;
  totalHeight = 0;

  if(n->body != NULL)
    {
      csz = astComputeSize(n->body, info, sizes);
      childWidth = csz.first;
      childHeight = csz.second;
      if(childHeight < sizes->rowsep)
        childHeight = sizes->rowsep;

      childOrigin = coordinate(origin.x + railPad +
                               (maxWidth - childWidth) / 2.0f,
                               cursorY);
      childg = astLayoutNode(n->body, childOrigin, geom, info, sizes);
      totalHeight += childHeight;
    }
  else
    {
      /* null body = epsilon path */
      totalHeight += sizes->rowsep;
    }

  /* Place repeats below, reversed */
  for(i = 0; i < n->repeats.size(); i++)
    {
      csz = astComputeSize(n->repeats[i], info, sizes);
      childWidth = csz.first;
      childHeight = csz.second;
      if(childHeight < sizes->rowsep)
        childHeight = sizes->rowsep;

      cursorY -= sizes->rowsep;
      totalHeight += sizes->rowsep;

      childOrigin = coordinate(origin.x + railPad +
                               (maxWidth - childWidth) / 2.0f,
                               cursorY);

      /* If repeat is a Sequence, lay it out reversed */
      if(n->repeats[i]->kind == ASTKind::Sequence)
        childg = layoutSequence(
            static_cast<SequenceNode*>(n->repeats[i]),
            childOrigin, geom, info, sizes, 1);
      else
        childg = astLayoutNode(n->repeats[i], childOrigin,
                               geom, info, sizes);

      cursorY -= childHeight;
      totalHeight += childHeight;
    }

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  /* entry and exit at the body (top) level */
  if(n->body != NULL && geom.find(n->body) != geom.end())
    {
      g.entry = coordinate(origin.x, geom[n->body].entry.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           geom[n->body].exit.y);
    }
  else
    {
      g.entry = coordinate(origin.x, origin.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           origin.y);
    }

  geom[n] = g;
  return g;
}

/* ----------------------------------------------------------------
   astComputeConnections - dispatch
   ---------------------------------------------------------------- */

void astComputeConnections(ASTNode *n,
                           map<ASTNode*, ASTNodeGeom> &geom,
                           map<ASTNode*, ASTLeafInfo> &info,
                           vector<ASTPolyline> &lines,
                           nodesizes *sizes)
{
  switch(n->kind)
    {
    case ASTKind::Sequence:
      connectSequence(static_cast<SequenceNode*>(n),
                      geom, info, lines, sizes);
      return;

    case ASTKind::Choice:
      {
        ChoiceNode *ch = static_cast<ChoiceNode*>(n);
        connectChoicelike(n, ch->alternatives, geom, info,
                          lines, sizes, 0);
        return;
      }

    case ASTKind::Optional:
      connectOptional(static_cast<OptionalNode*>(n),
                      geom, info, lines, sizes, 0);
      return;

    case ASTKind::Loop:
      connectLoop(static_cast<LoopNode*>(n), geom, info,
                  lines, sizes);
      return;

    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return;
    }
}

/* ----------------------------------------------------------------
   connectSequence - horizontal connections between children,
   including newline wrap-around routing.
   ---------------------------------------------------------------- */

static void connectSequence(SequenceNode *n,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes)
{
  size_t i, j, nc;
  ASTNode *child, *prevContent, *nextContent;
  ASTNodeGeom prevGeom, curGeom, nextGeom, seqGeom, childGeom;
  ASTPolyline pl;
  float rightEdge, leftEdge, midY, rowMaxBelow, belowExtent;
  float rowBottom, nextTop;

  nc = n->children.size();
  prevContent = NULL;
  rowMaxBelow = 0;

  for(i = 0; i < nc; i++)
    {
      child = n->children[i];

      /* recurse into children first */
      astComputeConnections(child, geom, info, lines, sizes);

      /* track below-rail extent for newline routing */
      if(child->kind != ASTKind::Newline &&
         geom.find(child) != geom.end())
        {
          childGeom = geom[child];
          if(isRailed(child))
            belowExtent = childGeom.height - sizes->minsize / 2.0f;
          else
            belowExtent = childGeom.height / 2.0f;
          if(belowExtent > rowMaxBelow)
            rowMaxBelow = belowExtent;
        }

      if(child->kind == ASTKind::Newline)
        {
          /* Route wrap-around */
          if(prevContent != NULL &&
             geom.find(prevContent) != geom.end() &&
             geom.find((ASTNode*)n) != geom.end())
            {
              nextContent = NULL;
              for(j = i + 1; j < nc; j++)
                {
                  if(n->children[j]->kind != ASTKind::Newline)
                    {
                      if(geom.find(n->children[j]) != geom.end())
                        nextContent = n->children[j];
                      j = nc;
                    }
                }

              if(nextContent != NULL)
                {
                  seqGeom = geom[(ASTNode*)n];
                  prevGeom = geom[prevContent];
                  nextGeom = geom[nextContent];
                  rightEdge = seqGeom.origin.x + seqGeom.width +
                    sizes->colsep;
                  leftEdge = seqGeom.origin.x - sizes->colsep;

                  rowBottom = prevGeom.exit.y - rowMaxBelow;
                  if(isRailed(nextContent))
                    nextTop = nextGeom.entry.y;
                  else
                    nextTop = nextGeom.entry.y +
                      sizes->minsize / 2.0f;
                  midY = nextTop + 0.55f * (rowBottom - nextTop);

                  pl.points.clear();
                  pl.points.push_back(prevGeom.exit);
                  pl.points.push_back(
                      coordinate(rightEdge, prevGeom.exit.y));
                  pl.points.push_back(coordinate(rightEdge, midY));
                  pl.points.push_back(coordinate(leftEdge, midY));
                  pl.points.push_back(
                      coordinate(leftEdge, nextGeom.entry.y));
                  pl.points.push_back(nextGeom.entry);
                  addPolyline(lines, pl);
                }
            }
          prevContent = NULL;
          rowMaxBelow = 0;
        }
      else
        {
          /* content or epsilon node: connect to previous */
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
    }
}

/* ----------------------------------------------------------------
   connectChoicelike - rail connections for choices
   ---------------------------------------------------------------- */

static void connectChoicelike(ASTNode *self,
    vector<ASTNode*> &alternatives,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes,
    int parentIsChoice)
{
  size_t i;
  ASTNode *child;
  ASTNodeGeom choiceGeom, firstGeom, childGeom;
  ASTPolyline pl;
  float railX, exitRailX;

  if(alternatives.size() < 1)
    return;
  if(geom.find(self) == geom.end())
    return;

  choiceGeom = geom[self];

  /* recurse into children */
  for(i = 0; i < alternatives.size(); i++)
    astComputeConnections(alternatives[i], geom, info,
                          lines, sizes);

  if(geom.find(alternatives[0]) == geom.end())
    return;
  firstGeom = geom[alternatives[0]];

  railX = choiceGeom.origin.x;
  exitRailX = choiceGeom.origin.x + choiceGeom.width;

  /* first child: straight horizontal entry/exit */
  pl.points.clear();
  pl.points.push_back(coordinate(railX, firstGeom.entry.y));
  pl.points.push_back(firstGeom.entry);
  addPolyline(lines, pl);

  pl.points.clear();
  pl.points.push_back(firstGeom.exit);
  pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
  addPolyline(lines, pl);

  /* remaining children: rail connections */
  for(i = 1; i < alternatives.size(); i++)
    {
      child = alternatives[i];
      if(geom.find(child) == geom.end())
        return;
      childGeom = geom[child];

      if(childGeom.entry.x == railX)
        {
          /* child's entry aligns with rail (nested railed node) */
          float halfCol = 0.5f * sizes->colsep;

          pl.points.clear();
          pl.points.push_back(
              coordinate(railX - halfCol, firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, childGeom.entry.y));
          pl.points.push_back(
              coordinate(railX + halfCol, childGeom.entry.y));
          addPolyline(lines, pl);

          pl.points.clear();
          pl.points.push_back(
              coordinate(exitRailX - halfCol, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, firstGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX + halfCol, firstGeom.exit.y));
          addPolyline(lines, pl);
        }
      else if(parentIsChoice)
        {
          /* 3-point: parent handles outer extension */
          pl.points.clear();
          pl.points.push_back(
              coordinate(railX, firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, childGeom.entry.y));
          pl.points.push_back(childGeom.entry);
          addPolyline(lines, pl);

          pl.points.clear();
          pl.points.push_back(childGeom.exit);
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, firstGeom.exit.y));
          addPolyline(lines, pl);
        }
      else
        {
          /* 4-point: extend past rail for inward curve */
          pl.points.clear();
          pl.points.push_back(
              coordinate(railX - sizes->colsep,
                         firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, childGeom.entry.y));
          pl.points.push_back(childGeom.entry);
          addPolyline(lines, pl);

          pl.points.clear();
          pl.points.push_back(childGeom.exit);
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, firstGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX + sizes->colsep,
                         firstGeom.exit.y));
          addPolyline(lines, pl);
        }
    }
}

/* ----------------------------------------------------------------
   connectOptional - like choice(epsilon, child) connections
   ---------------------------------------------------------------- */

static void connectOptional(OptionalNode *n,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes,
    int parentIsChoice)
{
  ASTNodeGeom optGeom, childGeom;
  ASTPolyline pl;
  float railX, exitRailX;

  if(geom.find((ASTNode*)n) == geom.end())
    return;

  optGeom = geom[(ASTNode*)n];

  /* recurse into child */
  astComputeConnections(n->child, geom, info, lines, sizes);

  if(geom.find(n->child) == geom.end())
    return;
  childGeom = geom[n->child];

  railX = optGeom.origin.x;
  exitRailX = optGeom.origin.x + optGeom.width;

  /* epsilon path (first alternative): straight through at top */
  pl.points.clear();
  pl.points.push_back(coordinate(railX, optGeom.entry.y));
  pl.points.push_back(coordinate(exitRailX, optGeom.exit.y));
  addPolyline(lines, pl);

  /* child path (second alternative): through the rails */
  if(childGeom.entry.x == railX)
    {
      float halfCol = 0.5f * sizes->colsep;

      pl.points.clear();
      pl.points.push_back(
          coordinate(railX - halfCol, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, childGeom.entry.y));
      pl.points.push_back(
          coordinate(railX + halfCol, childGeom.entry.y));
      addPolyline(lines, pl);

      pl.points.clear();
      pl.points.push_back(
          coordinate(exitRailX - halfCol, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, optGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX + halfCol, optGeom.exit.y));
      addPolyline(lines, pl);
    }
  else if(parentIsChoice)
    {
      pl.points.clear();
      pl.points.push_back(
          coordinate(railX, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, childGeom.entry.y));
      pl.points.push_back(childGeom.entry);
      addPolyline(lines, pl);

      pl.points.clear();
      pl.points.push_back(childGeom.exit);
      pl.points.push_back(
          coordinate(exitRailX, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, optGeom.exit.y));
      addPolyline(lines, pl);
    }
  else
    {
      pl.points.clear();
      pl.points.push_back(
          coordinate(railX - sizes->colsep, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, childGeom.entry.y));
      pl.points.push_back(childGeom.entry);
      addPolyline(lines, pl);

      pl.points.clear();
      pl.points.push_back(childGeom.exit);
      pl.points.push_back(
          coordinate(exitRailX, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, optGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX + sizes->colsep, optGeom.exit.y));
      addPolyline(lines, pl);
    }
}

/* ----------------------------------------------------------------
   connectLoop - body stubs + repeat feedback paths
   ---------------------------------------------------------------- */

static void connectLoop(LoopNode *n,
    map<ASTNode*, ASTNodeGeom> &geom,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTPolyline> &lines, nodesizes *sizes)
{
  size_t i;
  ASTNodeGeom loopGeom, bodyGeom, repeatGeom;
  ASTPolyline pl;
  float railX, exitRailX, bodyEntryY, bodyExitY;

  if(geom.find((ASTNode*)n) == geom.end())
    return;
  loopGeom = geom[(ASTNode*)n];

  /* recurse into body */
  if(n->body != NULL)
    astComputeConnections(n->body, geom, info, lines, sizes);
  for(i = 0; i < n->repeats.size(); i++)
    astComputeConnections(n->repeats[i], geom, info, lines, sizes);

  railX = loopGeom.origin.x;
  exitRailX = loopGeom.origin.x + loopGeom.width;

  /* Body entry/exit Y */
  if(n->body != NULL && geom.find(n->body) != geom.end())
    {
      bodyGeom = geom[n->body];
      bodyEntryY = bodyGeom.entry.y;
      bodyExitY = bodyGeom.exit.y;
    }
  else
    {
      bodyEntryY = loopGeom.origin.y;
      bodyExitY = loopGeom.origin.y;
    }

  /* Body stubs: horizontal lines from rails to body */
  if(n->body != NULL && geom.find(n->body) != geom.end())
    {
      bodyGeom = geom[n->body];
      pl.points.clear();
      pl.points.push_back(coordinate(railX, bodyEntryY));
      pl.points.push_back(bodyGeom.entry);
      addPolyline(lines, pl);

      pl.points.clear();
      pl.points.push_back(bodyGeom.exit);
      pl.points.push_back(coordinate(exitRailX, bodyExitY));
      addPolyline(lines, pl);
    }
  else
    {
      /* null body: straight through */
      pl.points.clear();
      pl.points.push_back(coordinate(railX, bodyEntryY));
      pl.points.push_back(coordinate(exitRailX, bodyExitY));
      addPolyline(lines, pl);
    }

  /* Repeat feedback paths */
  for(i = 0; i < n->repeats.size(); i++)
    {
      if(geom.find(n->repeats[i]) == geom.end())
        return;
      repeatGeom = geom[n->repeats[i]];

      /* Right feedback: body exit → right rail → down → repeat
         right side (which is the exit for reversed layout) */
      pl.points.clear();
      pl.points.push_back(
          coordinate(exitRailX, bodyExitY));
      pl.points.push_back(
          coordinate(exitRailX, repeatGeom.exit.y));
      pl.points.push_back(repeatGeom.exit);
      addPolyline(lines, pl);

      /* Left feedback: repeat left side → left rail → up → body */
      pl.points.clear();
      pl.points.push_back(repeatGeom.entry);
      pl.points.push_back(
          coordinate(railX, repeatGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, bodyEntryY));
      addPolyline(lines, pl);
    }
}

/* ----------------------------------------------------------------
   astLayoutProduction - layout a complete production with stubs
   ---------------------------------------------------------------- */

ASTProductionLayout astLayoutProduction(ASTProduction *prod,
                                        ASTLayoutContext &ctx)
{
  ASTProductionLayout layout;
  coordinate bodyOrigin;
  ASTNodeGeom bodyGeom;
  float Y, cursorX;
  ASTNamedCoord stub;
  ASTPolyline pl;
  pair<float,float> bodySize;
  nodesizes *sizes;

  sizes = ctx.sizes;

  /* Assign names to all leaf nodes */
  astAssignNames(prod->body, ctx, layout.leafInfo);

  /* Production body starts below the name label */
  Y = -1.5f * sizes->minsize;

  /* Start stubs */
  cursorX = 0;

  stub.name = ctx.nextCoord();
  stub.pos = coordinate(cursorX, Y);
  layout.stubs.push_back(stub);  /* start1 */

  cursorX += sizes->colsep / 2.0f;

  stub.name = ctx.nextCoord();
  stub.pos = coordinate(cursorX, Y);
  layout.stubs.push_back(stub);  /* start2 */

  cursorX += sizes->colsep;

  /* Layout body */
  bodyOrigin = coordinate(cursorX, Y);
  bodyGeom = astLayoutNode(prod->body, bodyOrigin,
                           layout.geom, layout.leafInfo, sizes);

  cursorX = bodyOrigin.x + bodyGeom.width;

  /* End stubs */
  cursorX += sizes->colsep / 2.0f;

  stub.name = ctx.nextCoord();
  stub.pos = coordinate(cursorX, Y);
  layout.stubs.push_back(stub);  /* end1 */

  cursorX += sizes->colsep / 2.0f;

  stub.name = ctx.nextCoord();
  stub.pos = coordinate(cursorX, Y);
  layout.stubs.push_back(stub);  /* end2 */

  /* Connect stubs to body */
  /* start2 → body entry */
  pl.points.clear();
  pl.points.push_back(layout.stubs[1].pos);
  pl.points.push_back(bodyGeom.entry);
  addPolyline(layout.connections, pl);

  /* body exit → end1 */
  pl.points.clear();
  pl.points.push_back(bodyGeom.exit);
  pl.points.push_back(layout.stubs[2].pos);
  addPolyline(layout.connections, pl);

  /* end1 → end2 */
  pl.points.clear();
  pl.points.push_back(layout.stubs[2].pos);
  pl.points.push_back(layout.stubs[3].pos);
  addPolyline(layout.connections, pl);

  /* Compute all connections within the body */
  astComputeConnections(prod->body, layout.geom, layout.leafInfo,
                        layout.connections, sizes);

  return layout;
}
