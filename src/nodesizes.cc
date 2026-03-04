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
 * @file nodesizes.cc
 * @brief Implementation of the node size cache (bnfnodes.dat loading).
 *
 * @see nodesizes.hh for the class declarations
 */

#include "nodesizes.hh"
#include "diagnostics.hh"
#include <fstream>

using namespace std;

nodesizes::nodesizes()
  : rowsep(6), colsep(8), minsize(14), textwidth(0)
{
}

nodesizes::~nodesizes()
{
  sizemap.clear();
}

void nodesizes::loadData(const string &filename)
{
  string nodename;
  char ch;
  coordinate size;
  ifstream inf;

  rowsep = 6;
  colsep = 8;
  minsize = 14;
  textwidth = 0;

  inf.open(filename);
  if(!inf.is_open())
    {
      /* Original code prints to cout; keeping this for backward
	 compatibility with test output.  Future phases should
	 switch to diagnostics.report(). */
      cout << "Unable to open input file " << filename << ". Continuing\n";
      return;
    }

  while(inf >> nodename)
    {
      if(nodename == "textwidth")
	{
	  inf >> textwidth;
	  if(!inf || textwidth < 0)
	    {
	      diagnostics.report(Severity::Warning,
		"Invalid textwidth in " + filename + "; ignoring");
	      textwidth = 0;
	    }
	}
      else
	{
	  inf >> size.x;
	  ch = 0;
	  while(ch != ',' && !inf.eof())
	    inf >> ch;
	  inf >> size.y;
	  if(!inf)
	    {
	      diagnostics.report(Severity::Warning,
		"Malformed size entry for '" + nodename +
		"' in " + filename + "; skipping");
	      inf.clear();
	    }
	  else if(size.x < 0 || size.y < 0)
	    {
	      diagnostics.report(Severity::Warning,
		"Negative dimension for node '" + nodename +
		"' in " + filename + "; skipping");
	    }
	  else
	    {
	      sizemap.insert(pair<string, coordinate>(nodename, size));
	    }
	}
    }
  inf.close();
}

int nodesizes::getSize(const string &nodename, float &width, float &height)
{
  auto i = sizemap.find(nodename);
  if(i == sizemap.end())
    {
      width = colsep;
      height = rowsep;
      return 0;
    }
  width = i->second.x;
  height = i->second.y;
  return 1;
}
