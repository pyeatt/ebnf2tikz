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
 * @file ast_layout.hh
 * @brief Layout engine data structures and function declarations.
 *
 * The layout engine operates in several passes:
 * -# **Naming pass** (astAssignNames): assigns TikZ node/coordinate names
 *    and looks up dimensions from @c bnfnodes.dat.
 * -# **Size computation** (astComputeSize): bottom-up measurement of each
 *    subtree's bounding box.
 * -# **Placement** (astLayoutNode): top-down coordinate assignment.
 * -# **Connection routing** (astComputeConnections): generates polyline
 *    paths for rails and inter-node connections.
 *
 * An optional auto-wrap pass (astAutoWrapGrammar) inserts NewlineNodes
 * into wide productions before layout.
 *
 * @see ast_layout.cc for the full implementation
 * @see ast_tikzwriter.hh for TikZ code emission from layout results
 */

#ifndef AST_LAYOUT_HH
#define AST_LAYOUT_HH

#include "ast.hh"
#include <nodesizes.hh>
#include <map>
#include <vector>
#include <string>

/**
 * @brief Information about a leaf node assigned during the naming pass.
 *
 * Populated by astAssignNames() for Terminal, Nonterminal, and Epsilon nodes.
 */
struct ASTLeafInfo {
  std::string tikzName;     /**< TikZ name (e.g., "node1" or "coord1"). */
  float width;         /**< Width in points from bnfnodes.dat. */
  float height;        /**< Height in points from bnfnodes.dat. */
  std::string style;        /**< TikZ style: "terminal", "nonterminal", or "null". */
  std::string format;       /**< LaTeX font command: "railtermname" or "railname". */
  std::string rawText;      /**< Display text (quotes stripped for terminals). */
  int isTerminal;      /**< 1 if terminal, 0 if nonterminal. */
  int wrapped;         /**< 1 if shortstack wrapping is applied for wide names. */
};

/**
 * @brief Computed geometry for an AST subtree after layout.
 *
 * Stores the bounding box and connection points for each node.
 */
struct ASTNodeGeom {
  coordinate origin;   /**< Top-left corner of the bounding box. */
  float width;         /**< Total width in points. */
  float height;        /**< Total height in points. */
  coordinate entry;    /**< Left connection point (input rail). */
  coordinate exit;     /**< Right connection point (output rail). */
  int reversed;        /**< 1 if laid out right-to-left (loop repeats). */
};

/**
 * @brief A polyline: a sequence of points connected with rounded corners.
 *
 * Emitted as a TikZ @c \\draw command with @c rounded @c corners.
 */
struct ASTPolyline {
  std::vector<coordinate> points;  /**< Ordered list of polyline vertices. */
};

/**
 * @brief A named coordinate emitted as a TikZ @c \\coordinate command.
 *
 * Used for entry/exit stub coordinates of a production.
 */
struct ASTNamedCoord {
  std::string name;         /**< TikZ coordinate name (e.g., "stubleft"). */
  coordinate pos;      /**< Position in points. */
};

/**
 * @brief Complete layout result for a single production.
 *
 * Contains all geometry, leaf info, connection polylines, and stub
 * coordinates needed to emit TikZ for the production.
 */
struct ASTProductionLayout {
  std::map<ast::ASTNode*, ASTNodeGeom> geom;       /**< Per-node geometry. */
  std::map<ast::ASTNode*, ASTLeafInfo> leafInfo;    /**< Per-leaf naming/size info. */
  std::vector<ASTPolyline> connections;             /**< Connection polylines. */
  std::vector<ASTNamedCoord> stubs;                 /**< Entry/exit stub coordinates. */
  std::map<ast::ASTNode*, std::pair<float,float>> sizeCache; /**< Cached astComputeSize results. */
};

/**
 * @brief Context for naming and size lookup during layout.
 *
 * Maintains monotonically increasing counters for generating unique
 * TikZ node and coordinate names (e.g., "node1", "coord2").
 * The counters persist across productions so names are globally unique.
 */
struct ASTLayoutContext {
  int nodeCounter;     /**< Counter for TikZ node names. */
  int coordCounter;    /**< Counter for TikZ coordinate names. */
  nodesizes *sizes;    /**< Pointer to the node size cache. */

  /**
   * @brief Construct a layout context.
   * @param s Pointer to the node size cache (from bnfnodes.dat).
   */
  ASTLayoutContext(nodesizes *s);

  /**
   * @brief Generate the next unique TikZ node name.
   * @return A name like "node1", "node2", etc.
   */
  std::string nextNode();

  /**
   * @brief Generate the next unique TikZ coordinate name.
   * @return A name like "coord1", "coord2", etc.
   */
  std::string nextCoord();
};

/**
 * @brief Assign TikZ names and look up sizes for all leaf nodes.
 *
 * Walks the AST and creates an ASTLeafInfo entry for each Terminal,
 * Nonterminal, and Epsilon node, assigning unique TikZ names and
 * querying bnfnodes.dat for dimensions.
 *
 * @param n    Root of the AST subtree.
 * @param ctx  Layout context (provides name counters and size cache).
 * @param info Map to populate with leaf information.
 */
void astAssignNames(ast::ASTNode *n, ASTLayoutContext &ctx,
                    ASTProductionLayout &layout);

/**
 * @brief Compute the width and height of an AST subtree.
 *
 * Bottom-up recursive measurement that accounts for colsep, rowsep,
 * rail padding, and child arrangement (horizontal for sequences,
 * vertical for choices/optionals/loops).
 *
 * @param n      Root of the AST subtree.
 * @param layout Layout result (uses leafInfo for dimensions).
 * @param sizes  Node size cache.
 * @return A (width, height) pair in points.
 */
std::pair<float,float> astComputeSize(ast::ASTNode *n,
                                 ASTProductionLayout &layout,
                                 nodesizes *sizes);

/**
 * @brief Place an AST node at the given origin coordinate.
 *
 * Top-down recursive placement that computes entry/exit connection
 * points and stores geometry in the layout's geom map.
 *
 * @param n      The AST node to place.
 * @param origin Top-left corner for this node's bounding box.
 * @param layout Layout result (populates geom, reads leafInfo).
 * @param sizes  Node size cache.
 * @return The computed geometry for node @p n.
 */
ASTNodeGeom astLayoutNode(ast::ASTNode *n, coordinate origin,
                          ASTProductionLayout &layout,
                          nodesizes *sizes);

/**
 * @brief Compute all connection polylines for a laid-out subtree.
 *
 * Generates the rail connections (horizontal runs, vertical drops,
 * loop-back paths) between nodes based on their placed geometry.
 *
 * @param n      Root of the AST subtree.
 * @param layout Layout result (reads geom/leafInfo, appends connections).
 * @param sizes  Node size cache.
 */
void astComputeConnections(ast::ASTNode *n,
                           ASTProductionLayout &layout,
                           nodesizes *sizes);

/**
 * @brief Layout an entire production (stubs + body + connections).
 *
 * Orchestrates the full layout pipeline for one production:
 * name assignment, size computation, coordinate placement,
 * connection routing, and stub generation.
 *
 * @param prod The production to layout.
 * @param ctx  Layout context (shared across productions for unique naming).
 * @return Complete layout result for the production.
 */
ASTProductionLayout astLayoutProduction(ASTProduction *prod,
                                        ASTLayoutContext &ctx);

/**
 * @brief Auto-wrap wide productions by inserting NewlineNodes.
 *
 * Examines each production's top-level sequence and inserts NewlineNode
 * markers where the accumulated width would exceed the available text
 * width.  Must be called before layout (and before dump if auto-inserted
 * newlines should be visible in the AST dump output).
 *
 * @param grammar The grammar whose productions may be wrapped.
 * @param sizes   Node size cache (provides textwidth and colsep).
 */
void astAutoWrapGrammar(ASTGrammar *grammar, nodesizes *sizes);

/**
 * @brief Apply post-layout shortstack wrapping to overwide nonterminals.
 *
 * Checks whether a production's body is wider than \\textwidth and,
 * if so, marks nonterminal leaf nodes that contain wrappable
 * spaces/underscores for shortstack wrapping.  This only affects
 * TikZ label text, not layout geometry (sizes come from bnfnodes.dat).
 *
 * @param layout   Layout result (modifies leafInfo wrapped flags).
 * @param body     The production body AST node.
 * @param needsWrap Pre-determined wrap flag from the production.
 * @param sizes    Node size cache (provides textwidth, minsize).
 * @param prodName Production name (for overwidth warning).
 */
void astPostLayoutWrap(ASTProductionLayout &layout, ast::ASTNode *body,
                       int needsWrap, nodesizes *sizes,
                       const std::string &prodName);

#endif
