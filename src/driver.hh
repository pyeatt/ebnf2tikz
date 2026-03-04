/* 
ebnf2tikz
    An optimizing compiler to convert (possibly annotated) Extended
    Backus–Naur Form (EBNF) to railroad diagrams expressed as LaTeX
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
 * @file driver.hh
 * @brief Parser driver class that coordinates scanning and parsing.
 *
 * The driver owns the output stream and node size cache references,
 * manages Flex/Bison scanner/parser lifecycle, and tracks terminal
 * and nonterminal symbol sets encountered during parsing.
 *
 * @see driver.cc for the implementation
 * @see parser.yy for the Bison grammar that uses this driver
 */

#ifndef DRIVER_HH
#define DRIVER_HH
#include <set>
#include <fstream>
#include <sstream>
#include "ast.hh"
#include "nodesizes.hh"
#include "parser.hh"

/** @brief Flex scanner function prototype, receives a driver reference. */
#define YY_DECL yy::parser::symbol_type yylex (driver& drv)
YY_DECL;

/**
 * @brief Coordinates scanning and parsing of EBNF input files.
 *
 * Acts as the interface between main(), the Flex scanner, and the
 * Bison parser.  Holds command-line options (optimize, figures, dump)
 * and provides factory methods for creating AST leaf nodes during parsing.
 */
class driver
{
  int result;                  /**< Parser return code. */
  std::string file;            /**< Current input filename. */
  std::set<std::string> nonterminals;    /**< Set of nonterminal names seen. */
  std::set<std::string> terminals;       /**< Set of terminal strings seen. */
  std::ofstream *outFile;           /**< Output file stream for TikZ. */
  nodesizes *sizes;            /**< Node size cache from bnfnodes.dat. */
  int noopt;                   /**< 1 to skip optimization. */
  int figures;                 /**< 1 to wrap tikzpictures in figures. */
  int dumponly;                /**< 1 to dump AST only (no TikZ output). */
  int dump_before;             /**< 1 to dump AST before optimization. */
  int dump_after;              /**< 1 to dump AST after optimization. */
  bool trace_parsing;          /**< Enable Bison debug tracing. */
  bool trace_scanning;         /**< Enable Flex debug tracing. */
  yy::location location;       /**< Current source location for error reporting. */
public:
  /**
   * @brief Construct a driver.
   * @param out Output file stream for TikZ output.
   * @param sz  Node size cache.
   */
  driver(std::ofstream *out, nodesizes *sz);

  /** @brief Get the output file stream. */
  std::ofstream &outs() {return *outFile;}

  /** @brief Get the node size cache pointer. */
  nodesizes *getSizes() const {return sizes;}

  /**
   * @brief Create a TerminalNode and record the terminal string.
   * @param s Terminal text (with quotes).
   * @return Newly allocated TerminalNode.
   */
  ast::ASTNode* addTerminal(std::string &s)
  {
    terminals.insert(s);
    return new ast::TerminalNode(s);
  };

  /**
   * @brief Create a NonterminalNode and record the nonterminal name.
   * @param s Nonterminal name.
   * @return Newly allocated NonterminalNode.
   */
  ast::ASTNode* addString(std::string &s)
  {
    nonterminals.insert(s);
    return new ast::NonterminalNode(s);
  };

  /**
   * @brief Parse an input file.
   * @param f       Input filename.
   * @param opt     1 to skip optimization, 0 to optimize.
   * @param fig     1 to wrap in figures.
   * @param dump    1 for AST dump mode.
   * @param dbefore 1 to dump AST before optimization.
   * @param dafter  1 to dump AST after optimization.
   * @return Parser return code (0 = success).
   */
  int parse (const char *f, int opt, int fig, int dump,
             int dbefore, int dafter);

  /** @brief Get whether dump-only mode is enabled. */
  int get_dumponly() const {return dumponly;}

  /** @brief Get whether optimization is disabled. */
  int get_noopt() const {return noopt;}

  /** @brief Get whether to dump AST before optimization. */
  int get_dump_before() const {return dump_before;}

  /** @brief Get whether to dump AST after optimization. */
  int get_dump_after() const {return dump_after;}

  /** @brief Get whether to wrap productions in figure environments. */
  int get_figures() const {return figures;}

  /** @brief Begin scanning from the current file.
   *  @return true on success, false if the file cannot be opened.
   */
  bool scan_begin ();

  /**
   * @brief Begin scanning from a string stream.
   * @param s The string stream to scan.
   */
  void scan_begin (std::stringstream &s);

  /** @brief End scanning and close the input. */
  void scan_end ();

  /** @brief Get the parser return code. */
  int get_result() const {return result;}

  /** @brief Get a reference to the current source location. */
  yy::location &get_location() {return location;}

};
#endif
