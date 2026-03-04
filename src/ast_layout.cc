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

/**
 * @file ast_layout.cc
 * @brief Implementation of the AST-based layout engine.
 *
 * This file contains the complete layout pipeline:
 * - **Name assignment**: Unique TikZ names for leaf nodes.
 * - **Size computation**: Bottom-up measurement of subtree bounding boxes,
 *   accounting for column/row separation, rail padding, and multi-row wrapping.
 * - **Coordinate placement**: Top-down assignment of (x, y) positions for
 *   every node, computing entry and exit connection points.
 * - **Connection routing**: Generation of polyline paths for horizontal rails,
 *   vertical drops, loop-back arcs, and row wrap-around connections.
 * - **Auto-wrap**: Insertion of NewlineNode markers into wide sequences.
 *
 * The layout engine does not create rail nodes in the AST; instead, rails
 * are represented as polylines computed from the placed geometry.
 *
 * @see ast_layout.hh for data structures and public interface
 * @see ast_tikzwriter.hh for TikZ emission from layout results
 */

#include "ast_layout.hh"
#include <algorithm>

using namespace std;
using namespace ast;

/* Forward declarations */
static pair<float,float> computeSizeSequence(SequenceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);
static pair<float,float> computeSizeChoice(ChoiceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);
static pair<float,float> computeSizeOptional(OptionalNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);
static pair<float,float> computeSizeLoop(LoopNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);

static ASTNodeGeom layoutLeaf(ASTNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);
static ASTNodeGeom layoutEpsilon(ASTNode *n, coordinate origin,
    ASTProductionLayout &layout);
static ASTNodeGeom layoutSequence(SequenceNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes, int reversed);
static ASTNodeGeom layoutChoicelike(ASTNode *n,
    vector<ASTNode*> &alternatives, int isLoop,
    coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);
static ASTNodeGeom layoutOptional(OptionalNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);
static ASTNodeGeom layoutLoop(LoopNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);

static void connectSequence(SequenceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);
static void connectChoicelike(ASTNode *self,
    vector<ASTNode*> &alternatives,
    ASTProductionLayout &layout, nodesizes *sizes,
    int parentIsChoice);
static void connectOptional(OptionalNode *n,
    ASTProductionLayout &layout, nodesizes *sizes,
    int parentIsChoice);
static void connectLoop(LoopNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);

/**
 * @brief Add a polyline to the connection list, skipping degenerate lines.
 *
 * Filters out polylines with fewer than 2 points or where all points
 * are identical (zero-length lines).
 *
 * @param lines The connection list to append to.
 * @param pl    The polyline to add.
 */
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

/**
 * @brief Check if an AST node is a choice-like type that has vertical rails.
 * @param n The node to check.
 * @return 1 if Choice, Optional, or Loop; 0 otherwise.
 */
static int isRailed(ASTNode *n)
{
  return (n->kind == ASTKind::Choice ||
          n->kind == ASTKind::Optional ||
          n->kind == ASTKind::Loop);
}

/**
 * @brief Check whether a node is a sequence containing a NewlineNode.
 *
 * Used by choicelike/optional layout to add extra rail padding so
 * the choice branching rails don't overlap the row wrap-around.
 *
 * @param n The node to check.
 * @return 1 if n is a Sequence containing at least one NewlineNode.
 */
static int containsNewline(ASTNode *n)
{
  SequenceNode *seq;
  size_t i;

  if(n->kind != ASTKind::Sequence)
    return 0;
  seq = static_cast<SequenceNode*>(n);
  for(i = 0; i < seq->children.size(); i++)
    if(seq->children[i]->kind == ASTKind::Newline)
      return 1;
  return 0;
}

/** @name ASTLayoutContext implementation */
/** @{ */

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

/** @} */

/** @brief Walk AST assigning TikZ names to leaf nodes. @see astAssignNames() in ast_layout.hh */

void astAssignNames(ASTNode *n, ASTLayoutContext &ctx,
                    ASTProductionLayout &layout)
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
      layout.leafInfo[n] = li;
      return;

    case ASTKind::Nonterminal:
      li.tikzName = ctx.nextNode();
      li.rawText = static_cast<NonterminalNode*>(n)->name;
      li.style = "nonterminal";
      li.format = "railname";
      li.isTerminal = 0;
      li.wrapped = 0;
      ctx.sizes->getSize(li.tikzName, li.width, li.height);
      layout.leafInfo[n] = li;
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
      layout.leafInfo[n] = li;
      return;

    case ASTKind::Newline:
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        astAssignNames(seq->children[i], ctx, layout);
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        astAssignNames(ch->alternatives[i], ctx, layout);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      astAssignNames(opt->child, ctx, layout);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        astAssignNames(loop->body, ctx, layout);
      for(i = 0; i < loop->repeats.size(); i++)
        astAssignNames(loop->repeats[i], ctx, layout);
      return;
    }
}

/** @brief Measure an AST subtree. @see astComputeSize() in ast_layout.hh */

pair<float,float> astComputeSize(ASTNode *n,
                                 ASTProductionLayout &layout,
                                 nodesizes *sizes)
{
  float w, h;
  ASTLeafInfo *li;
  auto it = layout.leafInfo.find(n);

  switch(n->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
      if(it != layout.leafInfo.end())
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
                                 layout, sizes);

    case ASTKind::Choice:
      return computeSizeChoice(static_cast<ChoiceNode*>(n),
                               layout, sizes);

    case ASTKind::Optional:
      return computeSizeOptional(static_cast<OptionalNode*>(n),
                                 layout, sizes);

    case ASTKind::Loop:
      return computeSizeLoop(static_cast<LoopNode*>(n), layout, sizes);
    }

  return make_pair(0.0f, 0.0f);
}

/**
 * @brief Compute width and height for a SequenceNode.
 *
 * Children are laid out left-to-right with colsep between them.
 * NewlineNodes cause row breaks; the width is the max row width
 * and height accumulates across rows with rowsep.
 */

static pair<float,float> computeSizeSequence(SequenceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
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
          csz = astComputeSize(child, layout, sizes);
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

/**
 * @brief Shared sizing for choice-like constructs (Choice, Optional internals).
 *
 * Computes size for a list of alternatives stacked vertically, with
 * optional rail padding on each side for branching rails.
 */

static pair<float,float> computeSizeChoicelike(
    vector<ASTNode*> &alternatives, int alwaysRailPad,
    ASTProductionLayout &layout, nodesizes *sizes)
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
      csz = astComputeSize(child, layout, sizes);
      if(csz.first > maxWidth)
        {
          maxWidth = csz.first;
          widestIsRailed = (child->kind != ASTKind::Loop && isRailed(child));
        }
      if(i > 0)
        totalHeight += sizes->rowsep;
      totalHeight += csz.second;
      if(csz.second < sizes->rowsep)
        totalHeight += sizes->rowsep - csz.second;
    }

  /* Add rail padding.  Loops always add their own.  Choices skip
     when the widest child already includes rails (but not Loops,
     whose feedback rails serve a different purpose). */
  if(alwaysRailPad || !widestIsRailed)
    maxWidth += 2.0f * sizes->colsep;

  /* When any alternative has been auto-wrapped (contains newlines),
     add extra rail padding so the choice branching rails don't
     overlap the row wrap-around polylines. */
  for(i = 0; i < alternatives.size(); i++)
    if(containsNewline(alternatives[i]))
      {
        maxWidth += 2.0f * sizes->colsep;
        i = alternatives.size() - 1;
      }

  return make_pair(maxWidth, totalHeight);
}

static pair<float,float> computeSizeChoice(ChoiceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  return computeSizeChoicelike(n->alternatives, 0, layout, sizes);
}

/**
 * @brief Compute size for an OptionalNode (treated like Choice(epsilon, child)).
 */

static pair<float,float> computeSizeOptional(OptionalNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  vector<ASTNode*> alts;
  /* We don't have a real Epsilon node to put here; we'll use NULL
     as a sentinel and handle it specially. */

  /* Use the child's size plus a null first alternative.
     Matches computeSizeChoicelike for choice(epsilon, child). */
  float maxWidth, totalHeight, childW, childH;
  pair<float,float> csz;
  int widestIsRailed;

  csz = astComputeSize(n->child, layout, sizes);
  childW = csz.first;
  childH = csz.second;

  maxWidth = childW;
  widestIsRailed = (n->child->kind != ASTKind::Loop && isRailed(n->child));

  /* epsilon row + separator + child row (same as choicelike) */
  totalHeight = sizes->rowsep;  /* minimum height for epsilon */
  totalHeight += sizes->rowsep; /* separator */
  totalHeight += childH;
  if(childH < sizes->rowsep)
    totalHeight += sizes->rowsep - childH;

  if(!widestIsRailed)
    maxWidth += 2.0f * sizes->colsep;

  /* Extra padding when child is auto-wrapped, so Optional rails
     don't overlap the row wrap-around polylines. */
  if(containsNewline(n->child))
    maxWidth += 2.0f * sizes->colsep;

  return make_pair(maxWidth, totalHeight);
}

/**
 * @brief Compute size for a LoopNode (body on top, repeats below).
 */

static pair<float,float> computeSizeLoop(LoopNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  float maxWidth, totalHeight;
  size_t i;
  pair<float,float> csz;

  maxWidth = 0;
  totalHeight = 0;

  /* Body (child 0, may be null/epsilon) */
  if(n->body != NULL)
    {
      csz = astComputeSize(n->body, layout, sizes);
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
      csz = astComputeSize(n->repeats[i], layout, sizes);
      totalHeight += sizes->rowsep;
      totalHeight += csz.second;
      if(csz.second < sizes->rowsep)
        totalHeight += sizes->rowsep - csz.second;
      if(csz.first > maxWidth)
          maxWidth = csz.first;
    }

  /* Loops always add their own rail padding */
  maxWidth += 2.0f * sizes->colsep;

  /* Extra padding when body or any repeat is auto-wrapped, so the
     Loop's feedback rails don't overlap wrap-around polylines. */
  if(n->body != NULL && containsNewline(n->body))
    maxWidth += 2.0f * sizes->colsep;
  else
    {
      for(i = 0; i < n->repeats.size(); i++)
        if(containsNewline(n->repeats[i]))
          {
            maxWidth += 2.0f * sizes->colsep;
            i = n->repeats.size() - 1;
          }
    }

  return make_pair(maxWidth, totalHeight);
}

/** @brief Dispatch to type-specific layout. @see astLayoutNode() in ast_layout.hh */

ASTNodeGeom astLayoutNode(ASTNode *n, coordinate origin,
                          ASTProductionLayout &layout,
                          nodesizes *sizes)
{
  switch(n->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
      return layoutLeaf(n, origin, layout, sizes);

    case ASTKind::Epsilon:
      return layoutEpsilon(n, origin, layout);

    case ASTKind::Newline:
      {
        ASTNodeGeom g;
        g.origin = origin;
        g.width = 0;
        g.height = 3.0f * sizes->rowsep;
        g.entry = origin;
        g.exit = origin;
        layout.geom[n] = g;
        return g;
      }

    case ASTKind::Sequence:
      return layoutSequence(static_cast<SequenceNode*>(n), origin,
                            layout, sizes, 0);

    case ASTKind::Choice:
      {
        ChoiceNode *ch = static_cast<ChoiceNode*>(n);
        return layoutChoicelike(n, ch->alternatives, 0,
                                origin, layout, sizes);
      }

    case ASTKind::Optional:
      return layoutOptional(static_cast<OptionalNode*>(n), origin,
                            layout, sizes);

    case ASTKind::Loop:
      return layoutLoop(static_cast<LoopNode*>(n), origin,
                        layout, sizes);
    }

  /* fallback */
  {
    ASTNodeGeom g;
    g.origin = origin;
    g.width = 0;
    g.height = 0;
    g.entry = origin;
    g.exit = origin;
    layout.geom[n] = g;
    return g;
  }
}

/**
 * @brief Layout a leaf node (Terminal or Nonterminal).
 *
 * Places the node at the given origin and sets entry/exit points at
 * the left and right edges of the node box.
 */

static ASTNodeGeom layoutLeaf(ASTNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  ASTNodeGeom g;
  ASTLeafInfo *li;
  float w, h, singleH, yShift;
  auto it = layout.leafInfo.find(n);

  if(it == layout.leafInfo.end())
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
  layout.geom[n] = g;
  return g;
}

/**
 * @brief Layout an Epsilon node (zero-width coordinate).
 */

static ASTNodeGeom layoutEpsilon(ASTNode *n, coordinate origin,
    ASTProductionLayout &layout)
{
  ASTNodeGeom g;

  g.origin = origin;
  g.width = 0;
  g.height = 0;
  g.entry = origin;
  g.exit = origin;
  layout.geom[n] = g;
  return g;
}

/**
 * @brief Place sequence children left-to-right (or reversed for loop repeats).
 *
 * Handles multi-row layout when NewlineNodes are present.
 */

static ASTNodeGeom layoutSequence(SequenceNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes,
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
          layout.geom[child] = nlg;
        }
      else if(child->kind == ASTKind::Epsilon)
        {
          childOrigin = coordinate(cursorX, cursorY);
          layoutEpsilon(child, childOrigin, layout);
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
          childg = astLayoutNode(child, childOrigin, layout, sizes);
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
          if(layout.geom.find(child) != layout.geom.end())
            g.entry = layout.geom[child].entry;
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
          if(layout.geom.find(child) != layout.geom.end())
            g.exit = layout.geom[child].exit;
          i = 0;
        }
    }

  layout.geom[n] = g;
  return g;
}

/**
 * @brief Shared layout for choice-like constructs.
 *
 * Stacks alternatives vertically and centers them horizontally
 * within the widest alternative's bounding box.
 */

static ASTNodeGeom layoutChoicelike(ASTNode *n,
    vector<ASTNode*> &alternatives, int isLoop,
    coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes)
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
      csz = astComputeSize(child, layout, sizes);
      if(csz.first > maxWidth)
        {
          maxWidth = csz.first;
          widestIsRailed = (child->kind != ASTKind::Loop && isRailed(child));
        }
    }

  if(isLoop || !widestIsRailed)
    railPad = sizes->colsep;
  else
    railPad = 0;

  /* Extra padding when any alternative is auto-wrapped */
  for(i = 0; i < alternatives.size(); i++)
    if(containsNewline(alternatives[i]))
      {
        railPad += sizes->colsep;
        i = alternatives.size() - 1;
      }

  /* second pass: place children vertically */
  cursorY = origin.y;
  totalHeight = 0;

  for(i = 0; i < alternatives.size(); i++)
    {
      child = alternatives[i];
      csz = astComputeSize(child, layout, sizes);
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
      childg = astLayoutNode(child, childOrigin, layout, sizes);

      cursorY -= childHeight;
      totalHeight += childHeight;
    }

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  if(alternatives.size() > 0 &&
     layout.geom.find(alternatives[0]) != layout.geom.end())
    {
      g.entry = coordinate(origin.x,
                            layout.geom[alternatives[0]].entry.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           layout.geom[alternatives[0]].exit.y);
    }
  else
    {
      g.entry = origin;
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           origin.y);
    }

  layout.geom[n] = g;
  return g;
}

/**
 * @brief Layout an OptionalNode (treated as choice(epsilon, child) with rails).
 */

static ASTNodeGeom layoutOptional(OptionalNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  ASTNodeGeom g, childg;
  float maxWidth, totalHeight, cursorY, childWidth, childHeight;
  float railPad;
  int widestIsRailed;
  pair<float,float> csz;
  coordinate childOrigin;

  csz = astComputeSize(n->child, layout, sizes);
  childWidth = csz.first;
  childHeight = csz.second;
  if(childHeight < sizes->rowsep)
    childHeight = sizes->rowsep;

  maxWidth = childWidth;
  widestIsRailed = (n->child->kind != ASTKind::Loop && isRailed(n->child));

  if(!widestIsRailed)
    railPad = sizes->colsep;
  else
    railPad = 0;

  /* Extra padding when child is auto-wrapped */
  if(containsNewline(n->child))
    railPad += sizes->colsep;

  /* Place epsilon (first alternative) at the top.
     Matches layoutChoicelike for choice(epsilon, child). */
  cursorY = origin.y;
  totalHeight = sizes->rowsep; /* minimum height for epsilon path */

  /* Past epsilon row, then separator gap */
  cursorY -= sizes->rowsep;
  cursorY -= sizes->rowsep;
  totalHeight += sizes->rowsep; /* separator */

  childOrigin = coordinate(origin.x + railPad +
                           (maxWidth - childWidth) / 2.0f,
                           cursorY);
  childg = astLayoutNode(n->child, childOrigin, layout, sizes);

  totalHeight += childHeight;

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  /* entry and exit at the epsilon (top) level */
  g.entry = coordinate(origin.x, origin.y);
  g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                       origin.y);

  layout.geom[n] = g;
  return g;
}

/**
 * @brief Layout a LoopNode (body on top, reversed repeats below).
 */

static ASTNodeGeom layoutLoop(LoopNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes)
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
      csz = astComputeSize(n->body, layout, sizes);
      if(csz.first > maxWidth)
        maxWidth = csz.first;
    }
  for(i = 0; i < n->repeats.size(); i++)
    {
      csz = astComputeSize(n->repeats[i], layout, sizes);
      if(csz.first > maxWidth)
        maxWidth = csz.first;
    }

  /* Loops always add their own rail padding */
  railPad = sizes->colsep;

  /* Extra padding when body or any repeat is auto-wrapped */
  if(n->body != NULL && containsNewline(n->body))
    railPad += sizes->colsep;
  else
    {
      for(i = 0; i < n->repeats.size(); i++)
        if(containsNewline(n->repeats[i]))
          {
            railPad += sizes->colsep;
            i = n->repeats.size() - 1;
          }
    }

  /* Place body at top */
  cursorY = origin.y;
  totalHeight = 0;

  if(n->body != NULL)
    {
      csz = astComputeSize(n->body, layout, sizes);
      childWidth = csz.first;
      childHeight = csz.second;
      if(childHeight < sizes->rowsep)
        childHeight = sizes->rowsep;

      childOrigin = coordinate(origin.x + railPad +
                               (maxWidth - childWidth) / 2.0f,
                               cursorY);
      childg = astLayoutNode(n->body, childOrigin, layout, sizes);
      cursorY -= childHeight;
      totalHeight += childHeight;
    }
  else
    {
      /* null body = epsilon path; treat as a child with height rowsep
         so the separator before the first repeat gives a full gap */
      cursorY -= sizes->rowsep;
      totalHeight += sizes->rowsep;
    }

  /* Place repeats below, reversed */
  for(i = 0; i < n->repeats.size(); i++)
    {
      csz = astComputeSize(n->repeats[i], layout, sizes);
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
            childOrigin, layout, sizes, 1);
      else
        childg = astLayoutNode(n->repeats[i], childOrigin,
                               layout, sizes);

      cursorY -= childHeight;
      totalHeight += childHeight;
    }

  g.origin = origin;
  g.width = maxWidth + 2.0f * railPad;
  g.height = totalHeight;

  /* entry and exit at the body position, not the rail.
     The feedback paths already draw body-to-rail segments with
     rounded corners; setting entry/exit at the rail would create
     gaps where the rounded corner bypasses the exact rail point. */
  if(n->body != NULL && layout.geom.find(n->body) != layout.geom.end())
    {
      g.entry = layout.geom[n->body].entry;
      g.exit = layout.geom[n->body].exit;
    }
  else
    {
      /* null body: entry/exit at midpoint, matching the bodyEntry/
         bodyExit used by connectLoop's feedback paths */
      float midX = origin.x + (maxWidth + 2.0f * railPad) / 2.0f;
      g.entry = coordinate(midX, origin.y);
      g.exit = coordinate(midX, origin.y);
    }

  layout.geom[n] = g;
  return g;
}

/** @brief Compute connection polylines for a subtree. @see astComputeConnections() in ast_layout.hh */

void astComputeConnections(ASTNode *n,
                           ASTProductionLayout &layout,
                           nodesizes *sizes)
{
  switch(n->kind)
    {
    case ASTKind::Sequence:
      connectSequence(static_cast<SequenceNode*>(n),
                      layout, sizes);
      return;

    case ASTKind::Choice:
      {
        ChoiceNode *ch = static_cast<ChoiceNode*>(n);
        connectChoicelike(n, ch->alternatives, layout, sizes, 0);
        return;
      }

    case ASTKind::Optional:
      connectOptional(static_cast<OptionalNode*>(n),
                      layout, sizes, 0);
      return;

    case ASTKind::Loop:
      connectLoop(static_cast<LoopNode*>(n), layout, sizes);
      return;

    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return;
    }
}

/**
 * @brief Generate horizontal connections between sequence children.
 *
 * Handles straight horizontal runs between adjacent children and
 * newline wrap-around routing for multi-row sequences.
 */

static void connectSequence(SequenceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  size_t i, j, nc, idx;
  ASTNode *child, *prevContent, *nextContent;
  ASTNodeGeom prevGeom, curGeom, nextGeom, seqGeom, childGeom;
  ASTPolyline pl;
  float rightEdge, leftEdge, midY, rowMaxBelow, belowExtent;
  float rowBottom, nextTop, wrapRight, wrapLeft;
  int reversed;

  nc = n->children.size();
  prevContent = NULL;
  rowMaxBelow = 0;

  /* Detect if the sequence was laid out reversed (e.g. loop repeat)
     by checking if the first content child is to the right of the second */
  reversed = 0;
  {
    ASTNode *first, *second;
    first = NULL;
    second = NULL;
    for(i = 0; i < nc; i++)
      {
        if(n->children[i]->kind != ASTKind::Newline &&
           n->children[i]->kind != ASTKind::Epsilon)
          {
            if(first == NULL)
              first = n->children[i];
            else if(second == NULL)
              {
                second = n->children[i];
                i = nc - 1;
              }
          }
      }
    if(first != NULL && second != NULL &&
       layout.geom.find(first) != layout.geom.end() &&
       layout.geom.find(second) != layout.geom.end())
      {
        if(layout.geom[first].entry.x > layout.geom[second].entry.x)
          reversed = 1;
      }
  }

  for(i = 0; i < nc; i++)
    {
      idx = reversed ? (nc - 1 - i) : i;
      child = n->children[idx];

      /* recurse into children first */
      astComputeConnections(child, layout, sizes);

      /* track below-rail extent for newline routing */
      if(child->kind != ASTKind::Newline &&
         layout.geom.find(child) != layout.geom.end())
        {
          childGeom = layout.geom[child];
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
             layout.geom.find(prevContent) != layout.geom.end() &&
             layout.geom.find((ASTNode*)n) != layout.geom.end())
            {
              nextContent = NULL;
              for(j = i + 1; j < nc; j++)
                {
                  size_t jdx;
                  jdx = reversed ? (nc - 1 - j) : j;
                  if(n->children[jdx]->kind != ASTKind::Newline)
                    {
                      if(layout.geom.find(n->children[jdx]) != layout.geom.end())
                        nextContent = n->children[jdx];
                      j = nc - 1;
                    }
                }

              if(nextContent != NULL)
                {
                  seqGeom = layout.geom[(ASTNode*)n];
                  prevGeom = layout.geom[prevContent];
                  nextGeom = layout.geom[nextContent];
                  rightEdge = seqGeom.origin.x + seqGeom.width;
                  leftEdge = seqGeom.origin.x;

                  /* Add colsep clearance so rounded corners have
                     room to curve at wrap-around boundaries. */
                  wrapRight = rightEdge + sizes->colsep;
                  wrapLeft = leftEdge - sizes->colsep;

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
                      coordinate(wrapRight, prevGeom.exit.y));
                  pl.points.push_back(coordinate(wrapRight, midY));
                  pl.points.push_back(coordinate(wrapLeft, midY));
                  pl.points.push_back(
                      coordinate(wrapLeft, nextGeom.entry.y));
                  pl.points.push_back(nextGeom.entry);
                  addPolyline(layout.connections, pl);
                }
            }
          prevContent = NULL;
          rowMaxBelow = 0;
        }
      else
        {
          /* content or epsilon node: connect to previous */
          if(prevContent != NULL &&
             layout.geom.find(prevContent) != layout.geom.end() &&
             layout.geom.find(child) != layout.geom.end())
            {
              prevGeom = layout.geom[prevContent];
              curGeom = layout.geom[child];
              pl.points.clear();
              pl.points.push_back(prevGeom.exit);
              pl.points.push_back(curGeom.entry);
              addPolyline(layout.connections, pl);
            }
          prevContent = child;
        }
    }
}

/**
 * @brief Generate vertical rail connections for choice-like constructs.
 *
 * Creates the branching and merging rails that connect alternatives
 * in Choice, Optional, and Loop nodes.
 */

static void connectChoicelike(ASTNode *self,
    vector<ASTNode*> &alternatives,
    ASTProductionLayout &layout, nodesizes *sizes,
    int parentIsChoice)
{
  size_t i;
  ASTNode *child;
  ASTNodeGeom choiceGeom, firstGeom, childGeom;
  ASTPolyline pl;
  float railX, exitRailX;

  if(alternatives.size() < 1)
    return;
  if(layout.geom.find(self) == layout.geom.end())
    return;

  choiceGeom = layout.geom[self];

  /* recurse into children */
  for(i = 0; i < alternatives.size(); i++)
    astComputeConnections(alternatives[i], layout, sizes);

  if(layout.geom.find(alternatives[0]) == layout.geom.end())
    return;
  firstGeom = layout.geom[alternatives[0]];

  railX = choiceGeom.origin.x;
  exitRailX = choiceGeom.origin.x + choiceGeom.width;

  /* first child: straight horizontal entry/exit */
  pl.points.clear();
  pl.points.push_back(coordinate(railX, firstGeom.entry.y));
  pl.points.push_back(firstGeom.entry);
  addPolyline(layout.connections, pl);

  pl.points.clear();
  pl.points.push_back(firstGeom.exit);
  pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
  addPolyline(layout.connections, pl);

  /* remaining children: rail connections */
  for(i = 1; i < alternatives.size(); i++)
    {
      child = alternatives[i];
      if(layout.geom.find(child) == layout.geom.end())
        return;
      childGeom = layout.geom[child];

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
          addPolyline(layout.connections, pl);

          pl.points.clear();
          pl.points.push_back(
              coordinate(exitRailX - halfCol, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, firstGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX + halfCol, firstGeom.exit.y));
          addPolyline(layout.connections, pl);
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
          addPolyline(layout.connections, pl);

          pl.points.clear();
          pl.points.push_back(childGeom.exit);
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, firstGeom.exit.y));
          addPolyline(layout.connections, pl);
        }
      else
        {
          /* 4-point: extend past rail for inward curve */
          float halfCol = 0.5f * sizes->colsep;

          pl.points.clear();
          pl.points.push_back(
              coordinate(railX - halfCol,
                         firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, firstGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, childGeom.entry.y));
          pl.points.push_back(childGeom.entry);
          addPolyline(layout.connections, pl);

          pl.points.clear();
          pl.points.push_back(childGeom.exit);
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, firstGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX + halfCol,
                         firstGeom.exit.y));
          addPolyline(layout.connections, pl);
        }
    }
}

/**
 * @brief Generate connections for an OptionalNode.
 *
 * Routes the bypass (epsilon) path and the child path with
 * appropriate rail connections.
 */

static void connectOptional(OptionalNode *n,
    ASTProductionLayout &layout, nodesizes *sizes,
    int parentIsChoice)
{
  ASTNodeGeom optGeom, childGeom;
  ASTPolyline pl;
  float railX, exitRailX;

  if(layout.geom.find((ASTNode*)n) == layout.geom.end())
    return;

  optGeom = layout.geom[(ASTNode*)n];

  /* recurse into child */
  astComputeConnections(n->child, layout, sizes);

  if(layout.geom.find(n->child) == layout.geom.end())
    return;
  childGeom = layout.geom[n->child];

  railX = optGeom.origin.x;
  exitRailX = optGeom.origin.x + optGeom.width;

  /* epsilon path (first alternative): straight through at top */
  pl.points.clear();
  pl.points.push_back(coordinate(railX, optGeom.entry.y));
  pl.points.push_back(coordinate(exitRailX, optGeom.exit.y));
  addPolyline(layout.connections, pl);

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
      addPolyline(layout.connections, pl);

      pl.points.clear();
      pl.points.push_back(
          coordinate(exitRailX - halfCol, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, optGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX + halfCol, optGeom.exit.y));
      addPolyline(layout.connections, pl);
    }
  else if(parentIsChoice)
    {
      pl.points.clear();
      pl.points.push_back(
          coordinate(railX, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, childGeom.entry.y));
      pl.points.push_back(childGeom.entry);
      addPolyline(layout.connections, pl);

      pl.points.clear();
      pl.points.push_back(childGeom.exit);
      pl.points.push_back(
          coordinate(exitRailX, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, optGeom.exit.y));
      addPolyline(layout.connections, pl);
    }
  else
    {
      /* 4-point: extend past rail for inward curve */
      float halfCol = 0.5f * sizes->colsep;

      pl.points.clear();
      pl.points.push_back(
          coordinate(railX - halfCol, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, optGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, childGeom.entry.y));
      pl.points.push_back(childGeom.entry);
      addPolyline(layout.connections, pl);

      pl.points.clear();
      pl.points.push_back(childGeom.exit);
      pl.points.push_back(
          coordinate(exitRailX, childGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX, optGeom.exit.y));
      pl.points.push_back(
          coordinate(exitRailX + halfCol, optGeom.exit.y));
      addPolyline(layout.connections, pl);
    }
}

/**
 * @brief Generate connections for a LoopNode.
 *
 * Routes the forward body path and the backward repeat/feedback paths
 * with appropriate rail connections.
 */

static void connectLoop(LoopNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  size_t i;
  ASTNodeGeom loopGeom, bodyGeom, repeatGeom;
  ASTPolyline pl;
  float railX, exitRailX;
  coordinate bodyEntry, bodyExit;
  float midX;

  if(layout.geom.find((ASTNode*)n) == layout.geom.end())
    return;
  loopGeom = layout.geom[(ASTNode*)n];

  /* recurse into body */
  if(n->body != NULL)
    astComputeConnections(n->body, layout, sizes);
  for(i = 0; i < n->repeats.size(); i++)
    astComputeConnections(n->repeats[i], layout, sizes);

  railX = loopGeom.origin.x;
  exitRailX = loopGeom.origin.x + loopGeom.width;

  /* Body entry/exit coordinates */
  if(n->body != NULL && layout.geom.find(n->body) != layout.geom.end())
    {
      bodyGeom = layout.geom[n->body];
      bodyEntry = bodyGeom.entry;
      bodyExit = bodyGeom.exit;
    }
  else
    {
      /* null body: use midpoint of loop as the body position */
      midX = (railX + exitRailX) / 2.0f;
      bodyEntry = coordinate(midX, loopGeom.origin.y);
      bodyExit = coordinate(midX, loopGeom.origin.y);
    }

  /* Body stubs: horizontal lines from rails to body.
     Only needed when there are no repeats, because the feedback
     paths already include the body-to-rail segments. Drawing
     both causes "ears" where the straight stub extends past the
     rounded corner of the feedback path. */
  if(n->repeats.size() == 0)
    {
      pl.points.clear();
      pl.points.push_back(coordinate(railX, bodyEntry.y));
      pl.points.push_back(bodyEntry);
      addPolyline(layout.connections, pl);

      pl.points.clear();
      pl.points.push_back(bodyExit);
      pl.points.push_back(coordinate(exitRailX, bodyExit.y));
      addPolyline(layout.connections, pl);
    }

  /* Repeat feedback paths */
  for(i = 0; i < n->repeats.size(); i++)
    {
      if(layout.geom.find(n->repeats[i]) == layout.geom.end())
        return;
      repeatGeom = layout.geom[n->repeats[i]];

      /* Right feedback: body exit → right rail → down → repeat
         right side (which is the exit for reversed layout) */
      pl.points.clear();
      pl.points.push_back(bodyExit);
      pl.points.push_back(
          coordinate(exitRailX, bodyExit.y));
      pl.points.push_back(
          coordinate(exitRailX, repeatGeom.exit.y));
      pl.points.push_back(repeatGeom.exit);
      addPolyline(layout.connections, pl);

      /* Left feedback: repeat left side → left rail → up → body */
      pl.points.clear();
      pl.points.push_back(repeatGeom.entry);
      pl.points.push_back(
          coordinate(railX, repeatGeom.entry.y));
      pl.points.push_back(
          coordinate(railX, bodyEntry.y));
      pl.points.push_back(bodyEntry);
      addPolyline(layout.connections, pl);
    }
}

/** @brief Layout a complete production with stubs. @see astLayoutProduction() in ast_layout.hh */

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
  astAssignNames(prod->body, ctx, layout);

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
  bodyGeom = astLayoutNode(prod->body, bodyOrigin, layout, sizes);

  cursorX = bodyOrigin.x + bodyGeom.width;

  /* End stubs — use the body exit Y so multi-line productions
     terminate at the last row, not the first */
  cursorX += sizes->colsep / 2.0f;

  stub.name = ctx.nextCoord();
  stub.pos = coordinate(cursorX, bodyGeom.exit.y);
  layout.stubs.push_back(stub);  /* end1 */

  cursorX += sizes->colsep / 2.0f;

  stub.name = ctx.nextCoord();
  stub.pos = coordinate(cursorX, bodyGeom.exit.y);
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
  astComputeConnections(prod->body, layout, sizes);

  return layout;
}

/**
 * @brief Estimate narrower widths for nonterminals that will be shortstack-wrapped.
 *
 * For nonterminals whose display text contains spaces (or underscores),
 * the TikZ writer renders them as @c \\shortstack, splitting at spaces.
 * This makes them narrower (and taller).
 *
 * The wrapped width is estimated as @c originalWidth/numLines, clamped
 * to at least @c minsize.  Only adjusts the temporary leafInfo used for
 * auto-wrap; the actual layout uses real bnfnodes.dat sizes.
 */

static void adjustWrappedWidths(ASTNode *n,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  size_t i;
  int numSpaces, numLines;
  float estWidth;
  string display;

  switch(n->kind)
    {
    case ASTKind::Nonterminal:
      {
        auto it = layout.leafInfo.find(n);
        if(it == layout.leafInfo.end())
          return;
        ASTLeafInfo &li = it->second;
        if(li.isTerminal)
          return;

        /* If the node is already wrapped (multi-line in bnfnodes.dat),
           its width is already the correct wrapped width */
        if(li.height > 1.5f * sizes->minsize)
          return;

        display = li.rawText;
        replace(display.begin(), display.end(), '_', ' ');

        numSpaces = 0;
        for(i = 0; i < display.size(); i++)
          {
            if(display[i] == ' ')
              numSpaces++;
          }
        if(numSpaces == 0)
          return;

        numLines = numSpaces + 1;
        /* Conservative estimate: the wrapped width is the widest
           word, which is at least 55% of the total unwrapped width
           (the longest word dominates for 2-3 word names). */
        estWidth = li.width / (float)numLines;
        if(estWidth < li.width * 0.55f)
          estWidth = li.width * 0.55f;
        if(estWidth < sizes->minsize)
          estWidth = sizes->minsize;

        li.width = estWidth;
        return;
      }

    case ASTKind::Terminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        adjustWrappedWidths(seq->children[i], layout, sizes);
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        adjustWrappedWidths(ch->alternatives[i], layout, sizes);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      adjustWrappedWidths(opt->child, layout, sizes);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        adjustWrappedWidths(loop->body, layout, sizes);
      for(i = 0; i < loop->repeats.size(); i++)
        adjustWrappedWidths(loop->repeats[i], layout, sizes);
      return;
    }
}

/**
 * @brief Insert NewlineNodes into sequences whose row width exceeds available width.
 *
 * Recurses into composite nodes (Loop, Choice, Optional), reducing
 * @p availableWidth by the container's rail padding at each nesting level.
 * For SequenceNodes, inserts NewlineNode break points where the
 * accumulated child widths exceed the available width.
 */

static void astAutoWrap(ASTNode *body, float availableWidth,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  float rowWidth, childWidth, innerWidth;
  size_t i;
  pair<float,float> csz;
  int firstOnRow;

  if(availableWidth <= 0)
    return;

  switch(body->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(body);
      innerWidth = availableWidth - 2.0f * sizes->colsep;
      /* If any alternative will need wrapping, use tighter width
         to account for the extra rail padding added around
         wrapped alternatives (see containsNewline checks in
         computeSizeChoicelike / layoutChoicelike). */
      for(i = 0; i < ch->alternatives.size(); i++)
        {
          csz = astComputeSize(ch->alternatives[i], layout, sizes);
          if(csz.first > innerWidth)
            {
              innerWidth = availableWidth - 4.0f * sizes->colsep;
              i = ch->alternatives.size() - 1;
            }
        }
      for(i = 0; i < ch->alternatives.size(); i++)
        astAutoWrap(ch->alternatives[i], innerWidth, layout, sizes);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(body);
      innerWidth = availableWidth - 2.0f * sizes->colsep;
      /* Same tighter width check as Choice above. */
      csz = astComputeSize(opt->child, layout, sizes);
      if(csz.first > innerWidth)
        innerWidth = availableWidth - 4.0f * sizes->colsep;
      astAutoWrap(opt->child, innerWidth, layout, sizes);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(body);
      innerWidth = availableWidth - 2.0f * sizes->colsep;
      /* Same tighter width check as Choice above. */
      if(loop->body != NULL)
        {
          csz = astComputeSize(loop->body, layout, sizes);
          if(csz.first > innerWidth)
            innerWidth = availableWidth - 4.0f * sizes->colsep;
        }
      if(innerWidth == availableWidth - 2.0f * sizes->colsep)
        {
          for(i = 0; i < loop->repeats.size(); i++)
            {
              csz = astComputeSize(loop->repeats[i], layout, sizes);
              if(csz.first > innerWidth)
                {
                  innerWidth = availableWidth - 4.0f * sizes->colsep;
                  i = loop->repeats.size() - 1;
                }
            }
        }
      if(loop->body != NULL)
        astAutoWrap(loop->body, innerWidth, layout, sizes);
      for(i = 0; i < loop->repeats.size(); i++)
        astAutoWrap(loop->repeats[i], innerWidth, layout, sizes);
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(body);
      rowWidth = 0;
      firstOnRow = 1;

      for(i = 0; i < seq->children.size(); i++)
        {
          if(seq->children[i]->kind == ASTKind::Newline)
            {
              rowWidth = 0;
              firstOnRow = 1;
            }
          else if(seq->children[i]->kind == ASTKind::Epsilon)
            {
              /* zero-width */
            }
          else
            {
              csz = astComputeSize(seq->children[i], layout, sizes);
              childWidth = csz.first;
              if(!firstOnRow)
                childWidth += sizes->colsep;

              if(!firstOnRow && rowWidth + childWidth > availableWidth)
                {
                  seq->children.insert(seq->children.begin() + i,
                                       new NewlineNode());
                  i++;
                  rowWidth = csz.first;
                  firstOnRow = 0;
                }
              else
                {
                  rowWidth += childWidth;
                  firstOnRow = 0;
                }

              /* Also recurse into this child in case it contains
                 inner sequences that need wrapping */
              astAutoWrap(seq->children[i], availableWidth, layout, sizes);
            }
        }
      return;
    }
}

/**
 * @brief Split overwide null-body loops into body+repeat pairs wrapped in Optional.
 *
 * When a @c Loop(body=null, repeat=Seq(A,B,C,...)) is too wide, the
 * repeat sequence is split so the first half becomes the loop body
 * and the second half stays as the repeat.  The result is wrapped
 * in Optional to preserve zero-or-more semantics.
 *
 * This creates a boustrophedon layout: body flows forward on top,
 * repeat flows reversed on bottom, connected by loop rails.
 */

static ASTNode* astSplitOverwideLoop(ASTNode *n, float availableWidth,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  SequenceNode *repeatSeq;
  SequenceNode *bodySeq;
  SequenceNode *newRepeatSeq;
  pair<float,float> csz;
  float innerWidth, rowWidth, childWidth;
  size_t i, breakPoint;
  int foundBreak;

  switch(n->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return n;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        seq->children[i] = astSplitOverwideLoop(seq->children[i],
                               availableWidth, layout, sizes);
      return n;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        ch->alternatives[i] = astSplitOverwideLoop(ch->alternatives[i],
                                   availableWidth, layout, sizes);
      return n;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      opt->child = astSplitOverwideLoop(opt->child, availableWidth,
                       layout, sizes);
      return n;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      innerWidth = availableWidth - 2.0f * sizes->colsep;

      /* Recurse into body and repeats first */
      if(loop->body != NULL)
        loop->body = astSplitOverwideLoop(loop->body, innerWidth,
                         layout, sizes);
      for(i = 0; i < loop->repeats.size(); i++)
        loop->repeats[i] = astSplitOverwideLoop(loop->repeats[i],
                               innerWidth, layout, sizes);

      /* Check if this is a splittable null-body loop */
      if(loop->body != NULL)
        return n;
      if(loop->repeats.size() != 1)
        return n;
      if(loop->repeats[0]->kind != ASTKind::Sequence)
        return n;

      repeatSeq = static_cast<SequenceNode*>(loop->repeats[0]);
      csz = astComputeSize(repeatSeq, layout, sizes);
      if(csz.first <= innerWidth)
        return n;

      /* Find break point: iterate children, break when cumulative
         width would exceed innerWidth */
      rowWidth = 0;
      foundBreak = 0;
      breakPoint = 0;
      for(i = 0; i < repeatSeq->children.size() && !foundBreak; i++)
        {
          csz = astComputeSize(repeatSeq->children[i], layout, sizes);
          childWidth = csz.first;
          if(i > 0)
            childWidth += sizes->colsep;

          if(i > 0 && rowWidth + childWidth > innerWidth)
            {
              breakPoint = i;
              foundBreak = 1;
            }
          else
            {
              rowWidth += childWidth;
            }
        }

      /* If no valid break found, leave unchanged */
      if(!foundBreak)
        return n;

      /* Create body sequence (first half) and new repeat sequence
         (second half) */
      bodySeq = new SequenceNode();
      newRepeatSeq = new SequenceNode();
      for(i = 0; i < breakPoint; i++)
        bodySeq->append(repeatSeq->children[i]);
      for(i = breakPoint; i < repeatSeq->children.size(); i++)
        newRepeatSeq->append(repeatSeq->children[i]);

      /* Detach children from original sequence and delete it */
      repeatSeq->children.clear();
      delete repeatSeq;

      /* Assign body and repeat to the loop */
      loop->body = bodySeq;
      loop->repeats[0] = newRepeatSeq;

      /* Wrap in Optional to preserve zero-or-more semantics */
      return new OptionalNode(loop);
    }

  return n;
}

/**
 * @brief Auto-wrap all non-subsumed productions in the grammar.
 *
 * Creates a temporary ASTLayoutContext to assign names (needed for
 * astComputeSize to look up leaf dimensions from bnfnodes.dat).
 * The temporary context assigns names identically to the later
 * layout context because both start counters at 0, traverse in
 * the same order, and NewlineNodes don't consume names.
 *
 * @see astAutoWrapGrammar (declared in ast_layout.hh)
 */

void astAutoWrapGrammar(ASTGrammar *grammar, nodesizes *sizes)
{
  ASTLayoutContext ctx(sizes);
  ASTProductionLayout tempLayout;
  size_t i;
  ASTProduction *prod;
  pair<float,float> bodySize;
  float availableWidth;

  if(sizes == NULL || sizes->textwidth <= 0)
    return;

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];

      /* Skip subsumed productions */
      if(prod->annotations != NULL &&
         (*prod->annotations)["subsume"] != "")
        ;
      else
        {
          tempLayout.leafInfo.clear();
          astAssignNames(prod->body, ctx, tempLayout);

          /* Compute body width.  If it exceeds the available row
             width, mark the production for nonterminal wrapping
             (shortstack) and estimate narrower widths before
             deciding where to insert auto-wrap newlines. */
          bodySize = astComputeSize(prod->body, tempLayout, sizes);
          availableWidth = sizes->textwidth - 2.5f * sizes->colsep;
          if(bodySize.first > availableWidth)
            {
              prod->needsWrap = 1;
              adjustWrappedWidths(prod->body, tempLayout, sizes);
            }

          /* Split overwide null-body loops into body+repeat */
          prod->body = astSplitOverwideLoop(prod->body, availableWidth,
                                             tempLayout, sizes);

          astAutoWrap(prod->body, availableWidth, tempLayout, sizes);

          /* Consume coord names for stubs (4 per production)
             to keep counters in sync with the later layout pass */
          ctx.nextCoord();  /* start1 */
          ctx.nextCoord();  /* start2 */
          ctx.nextCoord();  /* end1 */
          ctx.nextCoord();  /* end2 */
        }
    }
}
