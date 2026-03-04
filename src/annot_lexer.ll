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
 * @file annot_lexer.ll
 * @brief Flex tokenizer for annotation blocks.
 *
 * Tokenizes annotation content between @c \\<\\<-- and @c --\\>\\>.
 * Recognizes keywords (@c subsume, @c as, @c caption, @c label, @c sideways),
 * quoted strings, and semicolons.  Uses the @c annot prefix to
 * avoid symbol collisions with the main lexer.
 *
 * @see annot_parser.yy for the Bison grammar that consumes these tokens
 * @see lexer.ll for the main EBNF tokenizer
 */



%{ /* -*- C++ -*- */
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <cstring>
# include <string>
# include "ast.hh"

# include "annot_parser.hh"

// Give Flex the prototype of yylex
#define YY_DECL annot::parser::symbol_type annotlex (annotmap *m)
// declare yylex here
YY_DECL;

using namespace std;

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
  # define YY_USER_ACTION  aloc.columns (yyleng);
%}

%%

%{
  // A handy shortcut to the location held by the driver.
  extern annot::location aloc;
  // Code run each time yylex is called.
  aloc.step ();
  (void)m;  /* m is used by the parser, not directly in lexer rules */
%}

<<EOF>>           {return annot::parser::make_END(aloc);}

[ ]	          {aloc.step();}
[\t]              {aloc.step();}
[\n\r]            {aloc.lines(yyleng); aloc.step();}

\/\/.*\n          {}
---.*\n           {}

"<<--"            {return annot::parser::make_ASTART(aloc);}
"-->>"            {return annot::parser::make_AEND(aloc);}

subsume 	  {return annot::parser::make_SUBSUME (aloc);}
as	   	  {return annot::parser::make_AS (aloc);}
caption           {return annot::parser::make_CAPTION (aloc);}
label             {return annot::parser::make_LABEL (aloc);}
sideways          {return annot::parser::make_SIDEWAYS (aloc);}

\;                {return annot::parser::make_SEMICOLON (aloc);}
\"[^"]*\"         {return annot::parser::make_STRING(stripquotes(yytext),aloc);}

.                 {return annot::parser::make_UNEXP(yytext,aloc);}

%%


