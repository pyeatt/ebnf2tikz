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
 * @file diagnostics.cc
 * @brief Implementation of the diagnostic reporting engine.
 *
 * @see diagnostics.hh for the class declarations
 */

#include "diagnostics.hh"

/** @brief Global diagnostic engine instance used throughout the compiler. */
DiagnosticEngine diagnostics;

DiagnosticEngine::DiagnosticEngine()
  : errorCount(0), warningCount(0)
{
}

void DiagnosticEngine::report(Severity sev, const SourceLocation &loc,
			      const string &msg)
{
  Diagnostic d;
  d.severity = sev;
  d.loc = loc;
  d.message = msg;
  diags.push_back(d);
  switch(sev)
    {
    case Severity::Error:
      errorCount++;
      break;
    case Severity::Warning:
      warningCount++;
      break;
    case Severity::Note:
      break;
    }
}

void DiagnosticEngine::report(Severity sev, const string &msg)
{
  report(sev, SourceLocation(), msg);
}

bool DiagnosticEngine::hasErrors() const
{
  return errorCount > 0;
}

int DiagnosticEngine::getErrorCount() const
{
  return errorCount;
}

int DiagnosticEngine::getWarningCount() const
{
  return warningCount;
}

void DiagnosticEngine::emit(ostream &out) const
{
  int i;
  int n;
  n = diags.size();
  for(i = 0; i < n; i++)
    {
      switch(diags[i].severity)
	{
	case Severity::Note:
	  out << "note: ";
	  break;
	case Severity::Warning:
	  out << "warning: ";
	  break;
	case Severity::Error:
	  out << "error: ";
	  break;
	}
      if(!diags[i].loc.filename.empty())
	out << diags[i].loc.filename << ":"
	    << diags[i].loc.line << ":"
	    << diags[i].loc.column << ": ";
      out << diags[i].message << endl;
    }
}

const vector<Diagnostic>& DiagnosticEngine::getDiagnostics() const
{
  return diags;
}

void DiagnosticEngine::clear()
{
  diags.clear();
  errorCount = 0;
  warningCount = 0;
}
