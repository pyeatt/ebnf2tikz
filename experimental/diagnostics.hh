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

#ifndef DIAGNOSTICS_HH
#define DIAGNOSTICS_HH

#include <string>
#include <vector>
#include <iostream>

using namespace std;

enum class Severity { Note, Warning, Error };

struct SourceLocation {
  string filename;
  int line;
  int column;
  SourceLocation() : line(0), column(0) {}
  SourceLocation(const string &f, int l, int c)
    : filename(f), line(l), column(c) {}
};

struct Diagnostic {
  Severity severity;
  SourceLocation loc;
  string message;
};

class DiagnosticEngine {
private:
  vector<Diagnostic> diags;
  int errorCount;
  int warningCount;
public:
  DiagnosticEngine();

  /* Report a diagnostic with source location */
  void report(Severity sev, const SourceLocation &loc, const string &msg);

  /* Report a diagnostic without source location */
  void report(Severity sev, const string &msg);

  /* Check if any errors have been reported */
  bool hasErrors() const;

  int getErrorCount() const;
  int getWarningCount() const;

  /* Emit all collected diagnostics to the given stream */
  void emit(ostream &out) const;

  /* Get the raw diagnostics list */
  const vector<Diagnostic>& getDiagnostics() const;

  /* Clear all diagnostics */
  void clear();
};

/* Global diagnostic engine instance */
extern DiagnosticEngine diagnostics;

#endif
