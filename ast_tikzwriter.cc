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

#include "ast_tikzwriter.hh"
#include <util.hh>
#include <sstream>
#include <algorithm>
#include <cstring>

using namespace std;
using namespace ast;

/* ----------------------------------------------------------------
   Local text formatting helpers (mirrors graph.cc latexwrite)
   ---------------------------------------------------------------- */

static string astLatexwrite(const string &fontspec, const string &s)
{
  stringstream outs;

  outs << "\\" << fontspec << "{";
  for(auto i = s.begin(); i != s.end(); i++)
    {
      if(strchr("&%$#_{}~^\\", *i) != NULL)
        outs << "\\";
      outs << *i;
    }
  if(s != "|")
    outs << "\\strut}";
  else
    outs << "}";
  return outs.str();
}

/* ----------------------------------------------------------------
   ASTTikzWriter constructor
   ---------------------------------------------------------------- */

ASTTikzWriter::ASTTikzWriter(ofstream &out, nodesizes *sz)
  : outs(out), sizes(sz)
{
}

/* ----------------------------------------------------------------
   texName - generate the LaTeX display text for a leaf node
   ---------------------------------------------------------------- */

string ASTTikzWriter::texName(ASTLeafInfo &li)
{
  string display;
  stringstream ss;
  string::size_type pos, last;

  display = li.rawText;

  /* For nonterminals, replace underscores with spaces */
  if(!li.isTerminal)
    replace(display.begin(), display.end(), '_', ' ');

  /* Wrapped nonterminals with spaces: shortstack */
  if(li.wrapped && !li.isTerminal &&
     display.find(' ') != string::npos)
    {
      ss << "\\shortstack{";
      last = 0;
      pos = display.find(' ');
      while(pos != string::npos)
        {
          /* strut on first line for space above; omit on middle lines */
          if(last == 0)
            ss << "\\" << li.format << "{"
               << display.substr(last, pos - last) << "\\strut}";
          else
            ss << "\\" << li.format << "{"
               << display.substr(last, pos - last) << "}";
          ss << "\\\\[-3pt]";
          last = pos + 1;
          pos = display.find(' ', last);
        }
      ss << "\\" << li.format << "{"
         << display.substr(last) << "\\strut}";
      ss << "}";
      return ss.str();
    }

  return astLatexwrite(li.format, display);
}

/* ----------------------------------------------------------------
   emitLeafNode - emit a single TikZ \node or \coordinate
   ---------------------------------------------------------------- */

void ASTTikzWriter::emitLeafNode(ASTNode *n, ASTLeafInfo &li,
                                  ASTNodeGeom &geom)
{
  if(li.style == "null")
    {
      /* epsilon: coordinate only */
      outs << "\\coordinate (" << li.tikzName << ") at "
           << geom.origin << ";\n";
      return;
    }

  if(li.isTerminal)
    {
      outs << "\\node (" << li.tikzName << ") at "
           << geom.origin << "[anchor=west,"
           << li.style << "] {" << texName(li) << "};\n";
    }
  else
    {
      outs << "\\node (" << li.tikzName << ") at "
           << geom.origin << "[anchor=west,"
           << li.style << "] {" << texName(li)
           << "\\hspace*{1pt}};\n";
    }
  outs << "\\writenodesize{" << li.tikzName << "}\n";
}

/* ----------------------------------------------------------------
   emitAllNodes - walk AST emitting all leaf nodes
   ---------------------------------------------------------------- */

void ASTTikzWriter::emitAllNodes(ASTNode *n,
                                  ASTProductionLayout &layout)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  size_t i;
  auto leafIt = layout.leafInfo.find(n);
  auto geomIt = layout.geom.find(n);

  switch(n->kind)
    {
    case ASTKind::Terminal:
    case ASTKind::Nonterminal:
    case ASTKind::Epsilon:
      if(leafIt != layout.leafInfo.end() &&
         geomIt != layout.geom.end())
        emitLeafNode(n, leafIt->second, geomIt->second);
      return;

    case ASTKind::Newline:
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        emitAllNodes(seq->children[i], layout);
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        emitAllNodes(ch->alternatives[i], layout);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      emitAllNodes(opt->child, layout);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        emitAllNodes(loop->body, layout);
      for(i = 0; i < loop->repeats.size(); i++)
        emitAllNodes(loop->repeats[i], layout);
      return;
    }
}

/* ----------------------------------------------------------------
   emitPolyline
   ---------------------------------------------------------------- */

void ASTTikzWriter::emitPolyline(const ASTPolyline &pl)
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
   writeProduction - write TikZ for one production
   ---------------------------------------------------------------- */

void ASTTikzWriter::writeProduction(ASTProduction *prod,
                                     ASTProductionLayout &layout)
{
  string boxname;
  unsigned int i;

  boxname = camelcase(prod->name);

  outs << "\n\\newsavebox{\\" << boxname << "Box}\n\n";
  outs << "\\savebox{\\" << boxname << "Box}{";
  outs << "\\begin{tikzpicture}\n";

  /* production name label — replace underscores with spaces */
  {
    string prodDisplayName = prod->name;
    replace(prodDisplayName.begin(), prodDisplayName.end(), '_', ' ');
    outs << "\\node at (0pt,0pt)[anchor=west](name){"
         << astLatexwrite("railname", prodDisplayName) << "};\n";
  }

  /* emit stub coordinates */
  for(i = 0; i < layout.stubs.size(); i++)
    {
      outs << "\\coordinate (" << layout.stubs[i].name
           << ") at " << layout.stubs[i].pos << ";\n";
    }

  /* emit all TikZ nodes for the body */
  emitAllNodes(prod->body, layout);

  /* emit all connection polylines */
  for(i = 0; i < layout.connections.size(); i++)
    emitPolyline(layout.connections[i]);

  outs << "\\end{tikzpicture}\n";
  outs << "}\n\n";
}

/* ----------------------------------------------------------------
   Overwide wrapping helpers
   ---------------------------------------------------------------- */

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

static void collectWrappableNodes(ASTNode *n,
    map<ASTNode*, ASTLeafInfo> &info,
    vector<ASTNode*> &result)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  size_t i;
  auto it = info.find(n);

  switch(n->kind)
    {
    case ASTKind::Nonterminal:
      if(it != info.end() && !it->second.wrapped &&
         hasWrappableSpaces(it->second))
        result.push_back(n);
      return;

    case ASTKind::Terminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        collectWrappableNodes(seq->children[i], info, result);
      return;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        collectWrappableNodes(ch->alternatives[i], info, result);
      return;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      collectWrappableNodes(opt->child, info, result);
      return;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        collectWrappableNodes(loop->body, info, result);
      for(i = 0; i < loop->repeats.size(); i++)
        collectWrappableNodes(loop->repeats[i], info, result);
      return;
    }
}

static float estimateUnwrappedExtra(ASTNode *n,
    map<ASTNode*, ASTLeafInfo> &info, nodesizes *sizes)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  size_t i;
  float extra;
  int numLines;
  auto it = info.find(n);

  switch(n->kind)
    {
    case ASTKind::Nonterminal:
      if(it != info.end() && it->second.height > 1.5f * sizes->minsize &&
         hasWrappableSpaces(it->second))
        {
          numLines = (int)(it->second.height / sizes->minsize + 0.5f);
          if(numLines < 2)
            numLines = 2;
          return (float)(numLines - 1) * it->second.width;
        }
      return 0;

    case ASTKind::Terminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return 0;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      extra = 0;
      for(i = 0; i < seq->children.size(); i++)
        extra += estimateUnwrappedExtra(seq->children[i], info, sizes);
      return extra;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      extra = 0;
      for(i = 0; i < ch->alternatives.size(); i++)
        extra += estimateUnwrappedExtra(ch->alternatives[i], info, sizes);
      return extra;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      return estimateUnwrappedExtra(opt->child, info, sizes);

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      extra = 0;
      if(loop->body != NULL)
        extra += estimateUnwrappedExtra(loop->body, info, sizes);
      for(i = 0; i < loop->repeats.size(); i++)
        extra += estimateUnwrappedExtra(loop->repeats[i], info, sizes);
      return extra;
    }

  return 0;
}

/* ----------------------------------------------------------------
   astPlaceGrammar - top-level: layout and write all productions
   ---------------------------------------------------------------- */

void astPlaceGrammar(ASTGrammar *grammar, ofstream &outs,
                     nodesizes *sizes)
{
  ASTLayoutContext ctx(sizes);
  ASTTikzWriter writer(outs, sizes);
  ASTProductionLayout layout;
  size_t i, j;
  ASTProduction *prod;
  int needsWrap;
  pair<float,float> bodySize;
  float extraWidth, bodyWidth;
  vector<ASTNode*> wrappable;

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];

      /* skip subsumed productions */
      if(prod->annotations != NULL &&
         (*prod->annotations)["subsume"] != "")
        ;  /* subsumed: skip */
      else
        {
          /* Layout the production (assigns names + computes geometry) */
          layout = astLayoutProduction(prod, ctx);

          /* Post-layout wrapping check: wrapping only affects texName
             output, not layout geometry (sizes come from bnfnodes.dat).
             So we can check and set wrapped flags after layout. */
          needsWrap = prod->needsWrap;
          if(sizes->textwidth > 0)
            {
              auto git = layout.geom.find(prod->body);
              if(git != layout.geom.end())
                bodyWidth = git->second.width;
              else
                bodyWidth = 0;

              if(bodyWidth > sizes->textwidth)
                needsWrap = 1;

              if(!needsWrap)
                {
                  extraWidth = estimateUnwrappedExtra(
                      prod->body, layout.leafInfo, sizes);
                  if(extraWidth > 0 &&
                     bodyWidth + extraWidth > sizes->textwidth)
                    needsWrap = 1;
                }

              if(needsWrap)
                {
                  wrappable.clear();
                  collectWrappableNodes(prod->body,
                                        layout.leafInfo, wrappable);
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
                     << prod->name
                     << "' width (" << bodyWidth
                     << "pt) exceeds \\textwidth ("
                     << sizes->textwidth << "pt) by "
                     << bodyWidth - sizes->textwidth << "pt"
                     << endl;
            }

          writer.writeProduction(prod, layout);
        }
    }
}
