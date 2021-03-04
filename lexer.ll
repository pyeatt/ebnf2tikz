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
# include <cstring> // strerror
# include <string>
# include "graph.hh"
# include "driver.hh"
# include "parser.hh"
%}



%option noyywrap nounput noinput batch debug


%{
  // Code run each time a pattern is matched.
  # define YY_USER_ACTION  loc.columns (yyleng);
%}

%%

%{
  // A handy shortcut to the location held by the driver.
  yy::location& loc = drv.location;
  // Code run each time yylex is called.
  loc.step ();
%}

<<EOF>>                   {return yy::parser::make_END(loc);}

[ ]	                  {loc.step();}
[\t]                      {loc.step();}
[\n\r]                    {loc.lines(yyleng); loc.step();}

\/\/.*\n                  {}
---.*\n                   {}

subsume			  {return yy::parser::make_SUBSUME(loc);}

"\\\\"                    {return yy::parser::make_NEWLINE(loc);}

\'[^']*\'		  {return yy::parser::make_TERM (yytext,loc);}
\"[^"]*\"		  {return yy::parser::make_TERM (yytext,loc);}


"="                        {return yy::parser::make_EQUAL(loc);}
";"                        {return yy::parser::make_SEMICOLON(loc);}
"\|"                       {return yy::parser::make_PIPE(loc);}
"\["                       {return yy::parser::make_LBRACK(loc);}
"\]"                       {return yy::parser::make_RBRACK(loc);}
"\("                       {return yy::parser::make_LPAREN(loc);}
"\)"                       {return yy::parser::make_RPAREN(loc);}
"\{"                       {return yy::parser::make_LBRACE(loc);}
"\}"                       {return yy::parser::make_RBRACE(loc);}
","                        {return yy::parser::make_COMMA(loc);}


[a-zA-Z][a-zA-Z0-9_]*     {return yy::parser::make_STRING(yytext,loc);}

.                         {return yy::parser::make_UNEXP(yytext,loc);}

%%


void driver::scan_begin ()
{
  yy_flex_debug = trace_scanning;
  if (file.empty () || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen (file.c_str (), "r")))
    {
      std::cerr << "cannot open " << file << ": " << strerror (errno) << '\n';
      exit (EXIT_FAILURE);
    }
}


void driver::scan_end ()
{
  fclose (yyin);
}
