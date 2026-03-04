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
 * @file diagnostics.hh
 * @brief Structured diagnostic reporting for errors, warnings, and notes.
 *
 * Provides a DiagnosticEngine that collects diagnostic messages during
 * compilation and emits them at the end.  A global @c diagnostics
 * instance is available throughout the codebase.
 *
 * @see diagnostics.cc for the implementation
 */

#ifndef DIAGNOSTICS_HH
#define DIAGNOSTICS_HH

#include <string>
#include <vector>
#include <iostream>

using namespace std;

/**
 * @brief Diagnostic severity levels.
 */
enum class Severity {
  Note,     /**< Informational note. */
  Warning,  /**< Warning (non-fatal). */
  Error     /**< Error (fatal, causes nonzero exit). */
};

/**
 * @brief Source file location for diagnostic messages.
 */
struct SourceLocation {
  string filename;  /**< Source file name (empty if unknown). */
  int line;         /**< Line number (0 if unknown). */
  int column;       /**< Column number (0 if unknown). */

  /** @brief Default constructor: unknown location. */
  SourceLocation() : line(0), column(0) {}

  /**
   * @brief Construct a source location.
   * @param f Filename.
   * @param l Line number.
   * @param c Column number.
   */
  SourceLocation(const string &f, int l, int c)
    : filename(f), line(l), column(c) {}
};

/**
 * @brief A single diagnostic message with severity and optional location.
 */
struct Diagnostic {
  Severity severity;     /**< Severity level. */
  SourceLocation loc;    /**< Source location (may be empty). */
  string message;        /**< Diagnostic message text. */
};

/**
 * @brief Collects and emits compiler diagnostic messages.
 *
 * Diagnostics are accumulated during parsing, resolution, and
 * layout, then emitted to stderr at the end of compilation.
 */
class DiagnosticEngine {
private:
  vector<Diagnostic> diags;  /**< Accumulated diagnostic messages. */
  int errorCount;            /**< Number of error-level diagnostics. */
  int warningCount;          /**< Number of warning-level diagnostics. */
public:
  /** @brief Construct an empty diagnostic engine. */
  DiagnosticEngine();

  /**
   * @brief Report a diagnostic with a source location.
   * @param sev Severity level.
   * @param loc Source location.
   * @param msg Message text.
   */
  void report(Severity sev, const SourceLocation &loc, const string &msg);

  /**
   * @brief Report a diagnostic without a source location.
   * @param sev Severity level.
   * @param msg Message text.
   */
  void report(Severity sev, const string &msg);

  /**
   * @brief Check if any errors have been reported.
   * @return True if at least one Error-level diagnostic exists.
   */
  bool hasErrors() const;

  /** @brief Get the number of error-level diagnostics. */
  int getErrorCount() const;

  /** @brief Get the number of warning-level diagnostics. */
  int getWarningCount() const;

  /**
   * @brief Emit all collected diagnostics to an output stream.
   * @param out The stream to write diagnostics to (typically stderr).
   */
  void emit(ostream &out) const;

  /**
   * @brief Get the raw list of collected diagnostics.
   * @return Const reference to the diagnostics vector.
   */
  const vector<Diagnostic>& getDiagnostics() const;

  /** @brief Clear all accumulated diagnostics and reset counters. */
  void clear();
};

/** @brief Global diagnostic engine instance used throughout the compiler. */
extern DiagnosticEngine diagnostics;

#endif
