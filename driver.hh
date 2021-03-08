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

#ifndef DRIVER_HH
#define DRIVER_HH
#include <string>
#include <set>
#include <fstream>
#include "graph.hh"
#include "parser.hh"

using namespace std;

// Give Flex the prototype of yylex 
# define YY_DECL yy::parser::symbol_type yylex (driver& drv)

// declare yylex here
YY_DECL;

// Conducting the whole scanning and parsing of Calc++.
class driver
{
  int result;
  std::string file;
  set<string> nonterminals;
  set<string> terminals;
  ofstream *outFile;
  int noopt,figures;
  bool trace_parsing;
  bool trace_scanning;
  yy::location location;
public:
  driver(ofstream *out);

  ofstream &outs(){return *outFile;}

  node* addTerminal(string &s)
  {
    terminals.insert(s);
    return new termnode(s);
  };
  node* addString(string &s)
  {
    nonterminals.insert(s);
    return new nontermnode(s);
  };
  
  int parse (const char *f,int opt, int fig);
  void scan_begin ();
  void scan_end ();
  int get_result(){return result;}
  yy::location &get_location(){return location;}
  
};
#endif
