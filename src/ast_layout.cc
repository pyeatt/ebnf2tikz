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
#include "ast_visitor.hh"
#include "diagnostics.hh"
#include <algorithm>

using namespace std;
using namespace ast;

/* Forward declarations */

/** @brief Distinguishes Choice from Optional in shared choicelike functions. */
enum ChoicelikeKind { CK_Choice, CK_Optional };

static pair<float,float> computeSizeSequence(SequenceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);
static pair<float,float> computeSizeChoicelike(ASTNode *self,
    ChoicelikeKind kind,
    ASTProductionLayout &layout, nodesizes *sizes);
static pair<float,float> computeSizeLoop(LoopNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);

static ASTNodeGeom layoutLeaf(ASTNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);
static ASTNodeGeom layoutEpsilon(ASTNode *n, coordinate origin,
    ASTProductionLayout &layout);
static ASTNodeGeom layoutSequence(SequenceNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes, int reversed);
static ASTNodeGeom layoutChoicelike(ASTNode *self,
    ChoicelikeKind kind, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);
static ASTNodeGeom layoutLoop(LoopNode *n, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes);

static void connectSequence(SequenceNode *n,
    ASTProductionLayout &layout, nodesizes *sizes);
static void connectChoicelike(ASTNode *self, ChoicelikeKind kind,
    ASTProductionLayout &layout, nodesizes *sizes);
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

/**
 * @brief Visitor that assigns TikZ names to leaf nodes during layout.
 */
class NameAssigner : public DefaultASTVisitor {
public:
  ASTLayoutContext &ctx;
  ASTProductionLayout &layout;

  NameAssigner(ASTLayoutContext &c, ASTProductionLayout &l)
    : ctx(c), layout(l) {}

  void visitTerminal(TerminalNode *n) override
  {
    ASTLeafInfo li;

    li.tikzName = ctx.nextNode();
    li.rawText = n->text;
    /* strip surrounding quotes */
    if(li.rawText.size() >= 2)
      li.rawText = li.rawText.substr(1, li.rawText.size() - 2);
    li.style = "terminal";
    li.format = "railtermname";
    li.isTerminal = 1;
    li.wrapped = 0;
    ctx.sizes->getSize(li.tikzName, li.width, li.height);
    layout.leafInfo[(ASTNode*)n] = li;
  }

  void visitNonterminal(NonterminalNode *n) override
  {
    ASTLeafInfo li;

    li.tikzName = ctx.nextNode();
    li.rawText = n->name;
    li.style = "nonterminal";
    li.format = "railname";
    li.isTerminal = 0;
    li.wrapped = 0;
    ctx.sizes->getSize(li.tikzName, li.width, li.height);
    layout.leafInfo[(ASTNode*)n] = li;
  }

  void visitEpsilon(EpsilonNode *n) override
  {
    ASTLeafInfo li;

    li.tikzName = ctx.nextCoord();
    li.rawText = "";
    li.style = "null";
    li.format = "";
    li.isTerminal = 0;
    li.wrapped = 0;
    li.width = 0;
    li.height = 0;
    layout.leafInfo[(ASTNode*)n] = li;
  }
};

/** @brief Walk AST assigning TikZ names to leaf nodes. @see astAssignNames() in ast_layout.hh */

void astAssignNames(ASTNode *n, ASTLayoutContext &ctx,
                    ASTProductionLayout &layout)
{
  NameAssigner assigner(ctx, layout);
  n->accept(assigner);
}

/** @brief Measure an AST subtree. @see astComputeSize() in ast_layout.hh */

pair<float,float> astComputeSize(ASTNode *n,
                                 ASTProductionLayout &layout,
                                 nodesizes *sizes)
{
  float w, h;
  ASTLeafInfo *li;
  pair<float,float> result;
  auto cacheIt = layout.sizeCache.find(n);
  auto it = layout.leafInfo.find(n);

  /* Return cached result if available */
  if(cacheIt != layout.sizeCache.end())
    return cacheIt->second;

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
          result = make_pair(w, h);
        }
      else
        result = make_pair(sizes->colsep, sizes->rowsep);
      layout.sizeCache[n] = result;
      return result;

    case ASTKind::Epsilon:
      return make_pair(0.0f, 0.0f);

    case ASTKind::Newline:
      return make_pair(0.0f, 0.0f);

    case ASTKind::Sequence:
      result = computeSizeSequence(static_cast<SequenceNode*>(n),
                                   layout, sizes);
      layout.sizeCache[n] = result;
      return result;

    case ASTKind::Choice:
      result = computeSizeChoicelike(n, CK_Choice, layout, sizes);
      layout.sizeCache[n] = result;
      return result;

    case ASTKind::Optional:
      result = computeSizeChoicelike(n, CK_Optional, layout, sizes);
      layout.sizeCache[n] = result;
      return result;

    case ASTKind::Loop:
      result = computeSizeLoop(static_cast<LoopNode*>(n), layout, sizes);
      layout.sizeCache[n] = result;
      return result;
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
          /* 3x rowsep: gap above rail + rail height + gap below rail */
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

/**
 * @brief Compute size for a Choice or Optional node.
 *
 * For CK_Choice, iterates over the alternatives vector.
 * For CK_Optional, treats the layout as an implicit epsilon row
 * followed by the single child.
 */

static pair<float,float> computeSizeChoicelike(ASTNode *self,
    ChoicelikeKind kind,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  float maxWidth, totalHeight;
  size_t i, numAlts;
  int widestIsRailed;
  ASTNode *child;
  pair<float,float> csz;
  ChoiceNode *ch;
  OptionalNode *opt;

  ch = nullptr;
  opt = nullptr;
  if(kind == CK_Choice)
    {
      ch = static_cast<ChoiceNode*>(self);
      numAlts = ch->alternatives.size();
    }
  else
    {
      opt = static_cast<OptionalNode*>(self);
      numAlts = 1;
    }

  maxWidth = 0;
  totalHeight = 0;
  widestIsRailed = 0;

  /* For Optional: implicit epsilon row at top */
  if(kind == CK_Optional)
    totalHeight = sizes->rowsep;

  for(i = 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      csz = astComputeSize(child, layout, sizes);
      if(csz.first > maxWidth)
        {
          maxWidth = csz.first;
          widestIsRailed = (child->kind != ASTKind::Loop && isRailed(child));
        }
      if(i > 0 || kind == CK_Optional)
        totalHeight += sizes->rowsep;
      totalHeight += csz.second;
      if(csz.second < sizes->rowsep)
        totalHeight += sizes->rowsep - csz.second;
    }

  if(!widestIsRailed)
    maxWidth += 2.0f * sizes->colsep;

  /* When any alternative has been auto-wrapped (contains newlines),
     add extra rail padding so the branching rails don't overlap
     the row wrap-around polylines. */
  for(i = 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      if(containsNewline(child))
        {
          maxWidth += 2.0f * sizes->colsep;
          i = numAlts - 1;
        }
    }

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
  if(n->body != nullptr)
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
  if(n->body != nullptr && containsNewline(n->body))
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
        g.reversed = 0;
        layout.geom[n] = g;
        return g;
      }

    case ASTKind::Sequence:
      return layoutSequence(static_cast<SequenceNode*>(n), origin,
                            layout, sizes, 0);

    case ASTKind::Choice:
      return layoutChoicelike(n, CK_Choice, origin, layout, sizes);

    case ASTKind::Optional:
      return layoutChoicelike(n, CK_Optional, origin, layout, sizes);

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
    g.reversed = 0;
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
  g.reversed = 0;
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
  g.reversed = 0;
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
          nlg.reversed = 0;
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

  g.reversed = reversed;
  layout.geom[n] = g;
  return g;
}

/**
 * @brief Shared layout for choice-like constructs.
 *
 * Stacks alternatives vertically and centers them horizontally
 * within the widest alternative's bounding box.
 */

/**
 * @brief Shared layout for Choice and Optional constructs.
 *
 * For CK_Choice, stacks alternatives vertically and centers them
 * horizontally within the widest alternative's bounding box.
 * For CK_Optional, adds an implicit epsilon row at the top followed
 * by the single child.
 */

static ASTNodeGeom layoutChoicelike(ASTNode *self,
    ChoicelikeKind kind, coordinate origin,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  ASTNodeGeom g, childg;
  float maxWidth, totalHeight, cursorY, childWidth, childHeight;
  float railPad;
  size_t i, numAlts;
  int widestIsRailed;
  ASTNode *child;
  pair<float,float> csz;
  coordinate childOrigin;
  ChoiceNode *ch;
  OptionalNode *opt;

  ch = nullptr;
  opt = nullptr;
  if(kind == CK_Choice)
    {
      ch = static_cast<ChoiceNode*>(self);
      numAlts = ch->alternatives.size();
    }
  else
    {
      opt = static_cast<OptionalNode*>(self);
      numAlts = 1;
    }

  /* first pass: find max width */
  maxWidth = 0;
  widestIsRailed = 0;
  for(i = 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      csz = astComputeSize(child, layout, sizes);
      if(csz.first > maxWidth)
        {
          maxWidth = csz.first;
          widestIsRailed = (child->kind != ASTKind::Loop && isRailed(child));
        }
    }

  if(!widestIsRailed)
    railPad = sizes->colsep;
  else
    railPad = 0;

  /* Extra padding when any alternative is auto-wrapped */
  for(i = 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      if(containsNewline(child))
        {
          railPad += sizes->colsep;
          i = numAlts - 1;
        }
    }

  /* second pass: place children vertically */
  cursorY = origin.y;
  totalHeight = 0;

  /* For Optional: implicit epsilon row at top */
  if(kind == CK_Optional)
    {
      totalHeight = sizes->rowsep;
      cursorY -= sizes->rowsep;
    }

  for(i = 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      csz = astComputeSize(child, layout, sizes);
      childWidth = csz.first;
      childHeight = csz.second;
      if(childHeight < sizes->rowsep)
        childHeight = sizes->rowsep;

      if(i > 0 || kind == CK_Optional)
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

  if(kind == CK_Optional)
    {
      /* entry and exit at the epsilon (top) level */
      g.entry = coordinate(origin.x, origin.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           origin.y);
    }
  else if(numAlts > 0 &&
          layout.geom.find(ch->alternatives[0]) != layout.geom.end())
    {
      g.entry = coordinate(origin.x,
                            layout.geom[ch->alternatives[0]].entry.y);
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           layout.geom[ch->alternatives[0]].exit.y);
    }
  else
    {
      g.entry = origin;
      g.exit = coordinate(origin.x + maxWidth + 2.0f * railPad,
                           origin.y);
    }

  g.reversed = 0;
  layout.geom[self] = g;
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
  if(n->body != nullptr)
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
  if(n->body != nullptr && containsNewline(n->body))
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

  if(n->body != nullptr)
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
  if(n->body != nullptr && layout.geom.find(n->body) != layout.geom.end())
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

  g.reversed = 0;
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
      connectChoicelike(n, CK_Choice, layout, sizes);
      return;

    case ASTKind::Optional:
      connectChoicelike(n, CK_Optional, layout, sizes);
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
  prevContent = nullptr;
  rowMaxBelow = 0;

  /* Read the reversed flag stored during layout */
  reversed = 0;
  if(layout.geom.find((ASTNode*)n) != layout.geom.end())
    reversed = layout.geom[(ASTNode*)n].reversed;

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
          if(prevContent != nullptr &&
             layout.geom.find(prevContent) != layout.geom.end() &&
             layout.geom.find((ASTNode*)n) != layout.geom.end())
            {
              nextContent = nullptr;
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

              if(nextContent != nullptr)
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
          prevContent = nullptr;
          rowMaxBelow = 0;
        }
      else
        {
          /* content or epsilon node: connect to previous */
          if(prevContent != nullptr &&
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

/**
 * @brief Generate connections for a Choice or Optional node.
 *
 * For CK_Choice, connects the first alternative with straight horizontal
 * entry/exit and routes rail connections for remaining alternatives.
 * For CK_Optional, draws a straight epsilon bypass and routes the
 * child through rail connections.
 */

static void connectChoicelike(ASTNode *self, ChoicelikeKind kind,
    ASTProductionLayout &layout, nodesizes *sizes)
{
  size_t i, numAlts;
  ASTNode *child;
  ASTNodeGeom selfGeom, firstGeom, childGeom;
  ASTPolyline pl;
  float railX, exitRailX, halfCol;
  ChoiceNode *ch;
  OptionalNode *opt;

  ch = nullptr;
  opt = nullptr;
  if(kind == CK_Choice)
    {
      ch = static_cast<ChoiceNode*>(self);
      numAlts = ch->alternatives.size();
    }
  else
    {
      opt = static_cast<OptionalNode*>(self);
      numAlts = 1;
    }

  if(numAlts < 1)
    return;
  if(layout.geom.find(self) == layout.geom.end())
    return;

  selfGeom = layout.geom[self];

  /* recurse into children */
  for(i = 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      astComputeConnections(child, layout, sizes);
    }

  railX = selfGeom.origin.x;
  exitRailX = selfGeom.origin.x + selfGeom.width;

  /* First alternative connections */
  if(kind == CK_Choice)
    {
      /* First child: straight horizontal entry/exit */
      if(layout.geom.find(ch->alternatives[0]) == layout.geom.end())
        return;
      firstGeom = layout.geom[ch->alternatives[0]];

      pl.points.clear();
      pl.points.push_back(coordinate(railX, firstGeom.entry.y));
      pl.points.push_back(firstGeom.entry);
      addPolyline(layout.connections, pl);

      pl.points.clear();
      pl.points.push_back(firstGeom.exit);
      pl.points.push_back(coordinate(exitRailX, firstGeom.exit.y));
      addPolyline(layout.connections, pl);
    }
  else
    {
      /* Optional: epsilon path (straight through at top) */
      pl.points.clear();
      pl.points.push_back(coordinate(railX, selfGeom.entry.y));
      pl.points.push_back(coordinate(exitRailX, selfGeom.exit.y));
      addPolyline(layout.connections, pl);
    }

  /* Remaining alternatives: rail connections.
     For Choice, start at index 1 (first child handled above).
     For Optional, start at index 0 (the child is the "second" alt).
     Reference Y for rail alignment is selfGeom.entry/exit. */
  for(i = (kind == CK_Choice) ? 1 : 0; i < numAlts; i++)
    {
      child = (kind == CK_Choice) ? ch->alternatives[i] : opt->child;
      if(layout.geom.find(child) == layout.geom.end())
        return;
      childGeom = layout.geom[child];
      halfCol = 0.5f * sizes->colsep;

      if(childGeom.entry.x == railX)
        {
          /* child's entry aligns with rail (nested railed node) */
          pl.points.clear();
          pl.points.push_back(
              coordinate(railX - halfCol, selfGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, selfGeom.entry.y));
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
              coordinate(exitRailX, selfGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX + halfCol, selfGeom.exit.y));
          addPolyline(layout.connections, pl);
        }
      else
        {
          /* 4-point: extend past rail for inward curve */
          pl.points.clear();
          pl.points.push_back(
              coordinate(railX - halfCol, selfGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, selfGeom.entry.y));
          pl.points.push_back(
              coordinate(railX, childGeom.entry.y));
          pl.points.push_back(childGeom.entry);
          addPolyline(layout.connections, pl);

          pl.points.clear();
          pl.points.push_back(childGeom.exit);
          pl.points.push_back(
              coordinate(exitRailX, childGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX, selfGeom.exit.y));
          pl.points.push_back(
              coordinate(exitRailX + halfCol, selfGeom.exit.y));
          addPolyline(layout.connections, pl);
        }
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
  if(n->body != nullptr)
    astComputeConnections(n->body, layout, sizes);
  for(i = 0; i < n->repeats.size(); i++)
    astComputeConnections(n->repeats[i], layout, sizes);

  railX = loopGeom.origin.x;
  exitRailX = loopGeom.origin.x + loopGeom.width;

  /* Body entry/exit coordinates */
  if(n->body != nullptr && layout.geom.find(n->body) != layout.geom.end())
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
        {
          diagnostics.report(Severity::Warning,
            "loop repeat node has no geometry; skipping connection routing");
          return;
        }
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

  /* Production body starts below the name label.
     1.5x minsize provides clearance: 1x for the label height
     plus 0.5x padding between the label baseline and the first rail. */
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
      if(loop->body != nullptr)
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
      if(loop->body != nullptr)
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
      if(loop->body != nullptr)
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
      if(loop->body != nullptr)
        loop->body = astSplitOverwideLoop(loop->body, innerWidth,
                         layout, sizes);
      for(i = 0; i < loop->repeats.size(); i++)
        loop->repeats[i] = astSplitOverwideLoop(loop->repeats[i],
                               innerWidth, layout, sizes);

      /* Check if this is a splittable null-body loop */
      if(loop->body != nullptr)
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

  if(sizes == nullptr || sizes->textwidth <= 0)
    return;

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];

      /* Skip subsumed productions */
      if(prod->isSubsumed())
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

/* ------------------------------------------------------------------ */
/** @name Post-layout wrapping helpers */
/** @{ */

/**
 * @brief Check if a leaf node's display text contains wrappable spaces.
 * @param li The leaf info to check.
 * @return 1 if the node is a nonterminal with spaces/underscores, 0 otherwise.
 */
static int hasWrappableSpaces(ASTLeafInfo &li)
{
  string display;

  if(li.isTerminal)
    return 0;

  display = li.rawText;
  replace(display.begin(), display.end(), '_', ' ');
  if(display.find(' ') != string::npos)
    return 1;
  return 0;
}

/**
 * @brief Visitor that collects nonterminal nodes eligible for shortstack wrapping.
 */
class WrappableCollector : public DefaultASTVisitor {
public:
  map<ASTNode*, ASTLeafInfo> &info;
  vector<ASTNode*> &result;

  WrappableCollector(map<ASTNode*, ASTLeafInfo> &i, vector<ASTNode*> &r)
    : info(i), result(r) {}

  void visitNonterminal(NonterminalNode *n) override
  {
    auto it = info.find((ASTNode*)n);
    if(it != info.end() && !it->second.wrapped &&
       hasWrappableSpaces(it->second))
      result.push_back((ASTNode*)n);
  }
};

/**
 * @brief Visitor that estimates extra width from unwrapped tall nonterminals.
 */
class UnwrappedWidthEstimator : public DefaultASTVisitor {
public:
  map<ASTNode*, ASTLeafInfo> &info;
  nodesizes *sizes;
  float extra;

  UnwrappedWidthEstimator(map<ASTNode*, ASTLeafInfo> &i, nodesizes *s)
    : info(i), sizes(s), extra(0) {}

  void visitNonterminal(NonterminalNode *n) override
  {
    int numLines;
    auto it = info.find((ASTNode*)n);
    if(it != info.end() && it->second.height > 1.5f * sizes->minsize &&
       hasWrappableSpaces(it->second))
      {
        numLines = (int)(it->second.height / sizes->minsize + 0.5f);
        if(numLines < 2)
          numLines = 2;
        extra += (float)(numLines - 1) * it->second.width;
      }
  }
};

/** @} */

/** @brief Apply post-layout shortstack wrapping. @see astPostLayoutWrap() in ast_layout.hh */

void astPostLayoutWrap(ASTProductionLayout &layout, ASTNode *body,
                       int needsWrap, nodesizes *sizes,
                       const string &prodName)
{
  float bodyWidth, extraWidth;
  size_t j;
  vector<ASTNode*> wrappable;
  auto git = layout.geom.find(body);

  if(sizes->textwidth <= 0)
    return;

  if(git != layout.geom.end())
    bodyWidth = git->second.width;
  else
    bodyWidth = 0;

  if(bodyWidth > sizes->textwidth)
    needsWrap = 1;

  if(!needsWrap)
    {
      UnwrappedWidthEstimator estimator(layout.leafInfo, sizes);
      body->accept(estimator);
      extraWidth = estimator.extra;
      if(extraWidth > 0 && bodyWidth + extraWidth > sizes->textwidth)
        needsWrap = 1;
    }

  if(needsWrap)
    {
      WrappableCollector collector(layout.leafInfo, wrappable);
      body->accept(collector);
      for(j = 0; j < wrappable.size(); j++)
        {
          auto it = layout.leafInfo.find(wrappable[j]);
          if(it != layout.leafInfo.end())
            it->second.wrapped = 1;
        }
    }

  /* Warn if still overwide */
  if(bodyWidth > sizes->textwidth)
    cerr << "Warning: production '"
         << prodName
         << "' width (" << bodyWidth
         << "pt) exceeds \\textwidth ("
         << sizes->textwidth << "pt) by "
         << bodyWidth - sizes->textwidth << "pt"
         << endl;
}
