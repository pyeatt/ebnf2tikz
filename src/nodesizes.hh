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
 * @file nodesizes.hh
 * @brief Coordinate class and node size cache for TikZ layout.
 *
 * Provides a 2D coordinate type used throughout the layout engine,
 * and the nodesizes class that loads actual rendered node dimensions
 * from the @c bnfnodes.dat file produced by LaTeX.
 *
 * @see nodesizes.cc for the implementation
 */

#ifndef NODESIZES_HH
#define NODESIZES_HH

#include <map>
#include <string>
#include <iostream>

/**
 * @brief A 2D point in the TikZ coordinate system (in points).
 *
 * Used for node positions, sizes, and connection points throughout
 * the layout engine.  Supports addition, subtraction, assignment,
 * and formatted output as TikZ coordinate syntax @c (Xpt,Ypt).
 */
class coordinate {
public:
  float x;  /**< Horizontal position in points. */
  float y;  /**< Vertical position in points (positive = down in TikZ). */

  /** @brief Default constructor: origin (0, 0). */
  coordinate() { x = y = 0; }

  /**
   * @brief Construct a coordinate at (nx, ny).
   * @param nx X position in points.
   * @param ny Y position in points.
   */
  coordinate(float nx, float ny) { x = nx; y = ny; }

  /** @brief Component-wise addition. */
  coordinate operator+(coordinate r) {
    coordinate c;
    c.x = x + r.x;
    c.y = y + r.y;
    return c;
  }

  /** @brief Component-wise equality. */
  bool operator==(const coordinate &r) const {
    return x == r.x && y == r.y;
  }

  /** @brief Component-wise inequality. */
  bool operator!=(const coordinate &r) const {
    return x != r.x || y != r.y;
  }

  /** @brief Component-wise subtraction. */
  coordinate operator-(coordinate r) {
    coordinate c;
    c.x = x - r.x;
    c.y = y - r.y;
    return c;
  }

  /**
   * @brief Output in TikZ coordinate syntax: @c (Xpt,Ypt).
   * @param out Output stream.
   * @param c   Coordinate to print.
   * @return The output stream.
   */
  friend std::ostream& operator<<(std::ostream& out, const coordinate &c) {
    out << '(' << c.x << "pt," << c.y << "pt)";
    return out;
  }
};

/**
 * @brief Cache of TikZ node dimensions read from @c bnfnodes.dat.
 *
 * On the first ebnf2tikz run, no @c bnfnodes.dat exists and default
 * sizes are used.  After pdflatex runs, the file contains actual
 * rendered widths and heights for each named node, enabling accurate
 * layout on subsequent runs.
 */
class nodesizes {
private:
  std::map<std::string, coordinate> sizemap;  /**< Map from node name to (width, height). */
public:
  float rowsep;      /**< Vertical separation between rows (default 6pt). */
  float colsep;      /**< Horizontal separation between nodes (default 8pt). */
  float minsize;     /**< Minimum node dimension (default 14pt). */
  float textwidth;   /**< Available text width from LaTeX (0 if unknown). */

  /** @brief Construct with default layout parameters. */
  nodesizes();

  /** @brief Destructor. */
  ~nodesizes();

  /**
   * @brief Load node dimensions from a bnfnodes.dat file.
   *
   * Reads entries of the form @c "nodename width , height" and
   * a special @c "textwidth value" entry.  If the file cannot be
   * opened, a message is printed and defaults are retained.
   *
   * @param filename Path to the bnfnodes.dat file.
   */
  void loadData(const std::string &filename);

  /**
   * @brief Look up the size of a named node.
   *
   * If the node is found in the cache, @p width and @p height are
   * set to its dimensions.  Otherwise, they are set to colsep and
   * rowsep as defaults.
   *
   * @param nodename TikZ node name to look up.
   * @param width    Output: node width in points.
   * @param height   Output: node height in points.
   * @return 1 if found, 0 if defaults were used.
   */
  int getSize(const std::string &nodename, float &width, float &height);
};

#endif
