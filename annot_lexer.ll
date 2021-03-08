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




%{ /* -*- C++ -*- */
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <cstring>
# include <string>
# include "graph.hh"

# include "annot_parser.hh"

// Give Flex the prototype of yylex
#define YY_DECL annot::parser::symbol_type annotlex (annotmap *m)
// declare yylex here
YY_DECL;

string stripquotes(string s)
{
  s.erase(s.begin());
  s.pop_back();
  return s;
}
  
%}

%option noyywrap nounput noinput batch 
%option prefix="annot"

%{
  // Code run each time a pattern is matched.
  # define YY_USER_ACTION  loc.columns (yyleng);
%}

%%

%{
  // A handy shortcut to the location held by the driver.
  extern annot::location loc;
  // Code run each time yylex is called.
  loc.step ();
%}

<<EOF>>           {return annot::parser::make_END(loc);}

[ ]	          {loc.step();}
[\t]              {loc.step();}
[\n\r]            {loc.lines(yyleng); loc.step();}

\/\/.*\n          {}
---.*\n           {}

"<<--"            {return annot::parser::make_ASTART(loc);}
"-->>"            {return annot::parser::make_AEND(loc);}

subsume 	  {return annot::parser::make_SUBSUME (loc);}
as	   	  {return annot::parser::make_AS (loc);}
caption           {return annot::parser::make_CAPTION (loc);}

\;                {return annot::parser::make_SEMICOLON (loc);}
\".*\"            {return annot::parser::make_STRING(stripquotes(yytext),loc);}

.                 {return annot::parser::make_UNEXP(yytext,loc);}

%%


