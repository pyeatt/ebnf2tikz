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
 * @file ast_tikzwriter.hh
 * @brief TikZ code emitter for laid-out railroad diagrams.
 *
 * Provides ASTTikzWriter, which emits LaTeX TikZ commands for a single
 * production, and the top-level astPlaceGrammar() function that
 * orchestrates layout and emission for an entire grammar.
 *
 * @see ast_layout.hh for the layout engine that feeds this emitter
 */

#ifndef AST_TIKZWRITER_HH
#define AST_TIKZWRITER_HH

#include "ast.hh"
#include "ast_layout.hh"
#include <fstream>

using namespace std;

/**
 * @brief Emits TikZ commands for laid-out railroad diagram productions.
 *
 * Each production is written as a @c \\newsavebox / @c \\savebox pair
 * containing a @c tikzpicture with node declarations, coordinate
 * declarations, and polyline draw commands.
 */
class ASTTikzWriter {
private:
  ofstream &outs;    /**< Output stream for TikZ commands. */
  nodesizes *sizes;  /**< Node size cache for dimension queries. */

  /**
   * @brief Generate LaTeX display text for a leaf node.
   * @param li The leaf info to format.
   * @return LaTeX string for the node label.
   */
  string texName(ASTLeafInfo &li);

  /**
   * @brief Emit a single TikZ @c \\node or @c \\coordinate for a leaf.
   * @param n    The AST leaf node.
   * @param li   Leaf information (name, style, text).
   * @param geom Placed geometry (position).
   */
  void emitLeafNode(ast::ASTNode *n, ASTLeafInfo &li,
                    ASTNodeGeom &geom);

  /**
   * @brief Emit a TikZ @c \\draw command for a polyline.
   * @param pl The polyline to emit.
   */
  void emitPolyline(const ASTPolyline &pl);

  /**
   * @brief Recursively walk the AST emitting all leaf node declarations.
   * @param n      Root of the subtree to emit.
   * @param layout The production's layout data.
   */
  void emitAllNodes(ast::ASTNode *n, ASTProductionLayout &layout);

public:
  /**
   * @brief Construct a TikZ writer.
   * @param out Output file stream.
   * @param sz  Node size cache.
   */
  ASTTikzWriter(ofstream &out, nodesizes *sz);

  /**
   * @brief Write complete TikZ output for one production.
   *
   * Emits a @c \\newsavebox / @c \\savebox block containing the
   * production name label, stub coordinates, all leaf nodes, and
   * all connection polylines.
   *
   * @param prod   The production to write.
   * @param layout The production's layout data.
   */
  void writeProduction(ASTProduction *prod,
                       ASTProductionLayout &layout);
};

/**
 * @brief Top-level function: layout and write TikZ for all productions.
 *
 * Iterates over all non-subsumed productions, computes layout, optionally
 * wraps overwide nonterminals, and emits TikZ via ASTTikzWriter.
 *
 * @param grammar The grammar to output.
 * @param outs    Output file stream.
 * @param sizes   Node size cache.
 */
void astPlaceGrammar(ASTGrammar *grammar, ofstream &outs,
                     nodesizes *sizes);

#endif
