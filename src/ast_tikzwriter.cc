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
 * @file ast_tikzwriter.cc
 * @brief Implementation of TikZ code emission for railroad diagrams.
 *
 * Contains the ASTTikzWriter methods and the top-level astPlaceGrammar()
 * function.  Also includes post-layout heuristics for wrapping overwide
 * nonterminal labels using LaTeX @c \\shortstack.
 *
 * @see ast_tikzwriter.hh for class and function declarations
 * @see ast_layout.hh for the layout engine
 */

#include "ast_tikzwriter.hh"
#include "ast_visitor.hh"
#include <util.hh>
#include <sstream>
#include <algorithm>
#include <cstring>

using namespace std;
using namespace ast;

/**
 * @brief Escape LaTeX special characters and wrap in a font command.
 *
 * Prepends a backslash to LaTeX-special characters (&, %, $, #, _, {, }, ~, ^, \\)
 * and wraps the result in @c \\fontspec{...\\strut}.
 *
 * @param fontspec The LaTeX font command name (e.g., "railtermname").
 * @param s        The raw text to format.
 * @return Formatted LaTeX string.
 */
static string astLatexwrite(const string &fontspec, const string &s)
{
  stringstream outs;

  outs << "\\" << fontspec << "{";
  for(auto i = s.begin(); i != s.end(); i++)
    {
      if(strchr("&%$#_{}~^\\", *i) != nullptr)
        outs << "\\";
      outs << *i;
    }
  if(s != "|")
    outs << "\\strut}";
  else
    outs << "}";
  return outs.str();
}

/** @name ASTTikzWriter implementation */
/** @{ */

ASTTikzWriter::ASTTikzWriter(ofstream &out, nodesizes *sz)
  : outs(out), sizes(sz)
{
}

/** @} */

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


void ASTTikzWriter::emitLeafNode(ASTNode *, ASTLeafInfo &li,
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
  outs << "\\ebnf@writenodesize{" << li.tikzName << "}\n";
}


/**
 * @brief Visitor that emits TikZ nodes for all leaf nodes in the AST.
 */
class NodeEmitter : public DefaultASTVisitor {
public:
  ASTTikzWriter &writer;
  ASTProductionLayout &layout;

  NodeEmitter(ASTTikzWriter &w, ASTProductionLayout &l)
    : writer(w), layout(l) {}

  void visitTerminal(TerminalNode *n) override { emitIfPresent((ASTNode*)n); }
  void visitNonterminal(NonterminalNode *n) override { emitIfPresent((ASTNode*)n); }
  void visitEpsilon(EpsilonNode *n) override { emitIfPresent((ASTNode*)n); }

private:
  void emitIfPresent(ASTNode *n)
  {
    auto leafIt = layout.leafInfo.find(n);
    auto geomIt = layout.geom.find(n);
    if(leafIt != layout.leafInfo.end() &&
       geomIt != layout.geom.end())
      writer.emitLeafNode(n, leafIt->second, geomIt->second);
  }
};

void ASTTikzWriter::emitAllNodes(ASTNode *n,
                                  ASTProductionLayout &layout)
{
  NodeEmitter emitter(*this, layout);
  n->accept(emitter);
}


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


void ASTTikzWriter::writeProduction(ASTProduction *prod,
                                     ASTProductionLayout &layout,
                                     bool figures)
{
  string boxname;
  string caption;
  string label;
  bool sideways;
  unsigned int i;

  boxname = camelcase(prod->name);
  sideways = false;

  /* extract annotation values */
  if(prod->annotations != nullptr)
    {
      caption = (*prod->annotations)["caption"];
      label = (*prod->annotations)["label"];
      sideways = (*prod->annotations)["sideways"] != "";
    }

  outs << "\n\\newsavebox{\\" << boxname << "Box}\n\n";
  outs << "\\savebox{\\" << boxname << "Box}{";
  outs << "\\begin{tikzpicture}[raildiagram]\n";

  /* production name label — replace underscores with spaces */
  {
    string prodDisplayName = prod->name;
    replace(prodDisplayName.begin(), prodDisplayName.end(), '_', ' ');
    outs << "\\node at (0pt,0pt)[anchor=west](name){"
         << astLatexwrite("railprodname", prodDisplayName) << "};\n";
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

  /* when -f is active, emit a figure environment that uses the savebox */
  if(figures)
    {
      if(sideways)
        outs << "\\begin{sidewaysfigure}[htbp]\n";
      else
        outs << "\\begin{figure}[htbp]\n";
      outs << "\\centering\n";
      outs << "\\usebox{\\" << boxname << "Box}\n";
      if(!caption.empty())
        outs << "\\caption{" << caption << "}\n";
      if(!label.empty())
        outs << "\\label{" << label << "}\n";
      if(sideways)
        outs << "\\end{sidewaysfigure}\n";
      else
        outs << "\\end{figure}\n";
    }
}

/** @brief Layout and write TikZ for all productions. @see astPlaceGrammar() in ast_tikzwriter.hh */

void astPlaceGrammar(ASTGrammar *grammar, ofstream &outs,
                     nodesizes *sizes, bool figures)
{
  ASTLayoutContext ctx(sizes);
  ASTTikzWriter writer(outs, sizes);
  ASTProductionLayout layout;
  size_t i;
  ASTProduction *prod;

  outs << "\\makeatletter\n";

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];

      /* skip subsumed productions */
      if(prod->isSubsumed())
        ;  /* subsumed: skip */
      else
        {
          /* Layout the production (assigns names + computes geometry) */
          layout = astLayoutProduction(prod, ctx);

          /* Post-layout wrapping check: wrapping only affects texName
             output, not layout geometry (sizes come from bnfnodes.dat).
             So we can check and set wrapped flags after layout. */
          astPostLayoutWrap(layout, prod->body, prod->needsWrap,
                            sizes, prod->name);

          writer.writeProduction(prod, layout, figures);
        }
    }

  outs << "\\makeatother\n";
}
