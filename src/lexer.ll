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
 * @file lexer.ll
 * @brief Main Flex tokenizer for EBNF input.
 *
 * Tokenizes EBNF grammar files, recognizing:
 * - Terminal strings (single/double quoted and @c ?special sequences?)
 * - Nonterminal identifiers
 * - Operators: @c = @c ; @c | @c , @c ( @c ) @c [ @c ] @c { @c }
 * - Newline directive @c \\\\
 * - Annotations delimited by @c \\<\\<-- and @c --\\>\\>
 * - Comments: @c // line comments, @c --- line comments, @c (* block comments *)
 *
 * @see parser.yy for the Bison grammar that consumes these tokens
 * @see annot_lexer.ll for the annotation sub-tokenizer
 */


%{ /* -*- C++ -*- */
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <cstring> // strerror
# include <string>
# include "ast.hh"
# include "diagnostics.hh"
# include "driver.hh"
# include "parser.hh"
using namespace std;
%}



%option noyywrap nounput noinput batch debug


%{
#define YY_USER_ACTION \
    loc.begin.line = loc.end.line; \
    loc.begin.column = loc.end.column; \
    for(int i = 0; yytext[i] != '\0'; i++) { \
        if(yytext[i] == '\n') { \
            loc.end.line++; \
            loc.end.column = 0; \
        } \
        else { \
            loc.end.column++; \
        } \
    }
  // Code run each time a pattern is matched.
  // # define YY_USER_ACTION  loc.columns (yyleng);
%}


%Start	A
%x	C

%%

%{
  // A handy shortcut to the location held by the driver.
  yy::location& loc = drv.get_location();
  yy::location subloc;
  // Code run each time yylex is called.
  loc.step ();
%}

<<EOF>>                   {return yy::parser::make_END(loc);}

[ ]	                  {}
[\t]                      {loc.end.column+=8;loc.end.column -= loc.end.column % 8;}
[\n\r]                    {}

\/\/.*\n                  {}
---.*\n                   {}

"(*"                      {BEGIN(C);}
<C>[^*\n]*                {}
<C>"*"+[^*)\n]*           {}
<C>\n                     {}
<C>"*"+")"                {BEGIN(INITIAL);}

"\\\\"                    {return yy::parser::make_NEWLINE(loc);}

\'[^']*\'		  {return yy::parser::make_TERM (yytext,loc);}
\"[^"]*\"		  {return yy::parser::make_TERM (yytext,loc);}

"?"[^?\n]*"?"             {
                            std::string raw(yytext+1, yyleng-2);
                            size_t start = raw.find_first_not_of(" \t");
                            size_t end = raw.find_last_not_of(" \t");
                            std::string trimmed;
                            if (start != std::string::npos)
                              trimmed = raw.substr(start, end - start + 1);
                            std::string quoted = "'" + trimmed + "'";
                            return yy::parser::make_TERM(quoted.c_str(), loc);
                          }


"="                       {return yy::parser::make_EQUAL(loc);}
";"                       {return yy::parser::make_SEMICOLON(loc);}
"\|"                      {return yy::parser::make_PIPE(loc);}
"\["                      {return yy::parser::make_LBRACK(loc);}
"\]"                      {return yy::parser::make_RBRACK(loc);}
"\("                      {return yy::parser::make_LPAREN(loc);}
"\)"                      {return yy::parser::make_RPAREN(loc);}
"\{"                      {return yy::parser::make_LBRACE(loc);}
"\}"                      {return yy::parser::make_RBRACE(loc);}
","                       {return yy::parser::make_COMMA(loc);}


[a-zA-Z][a-zA-Z0-9_]*     {return yy::parser::make_STRING(yytext,loc);}

"<<--"	      {BEGIN A;subloc=loc;yymore();}
<A>[^-]*      {yymore();}
<A>"-"[^-]    {yymore();}
<A>"--"[^>]   {yymore();}
<A>"-->"[^>]  {yymore();}
<A>"-->>"     {BEGIN 0;return yy::parser::make_ANNOTATION(yytext,loc);}

.             {return yy::parser::make_UNEXP(yytext,loc);}

%%


void driver::scan_begin ()
{
  yy_flex_debug = trace_scanning;
  if (file.empty () || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen (file.c_str (), "r")))
    {
      diagnostics.report(Severity::Error,
        string("cannot open ") + file + ": " + strerror(errno));
      diagnostics.emit(std::cerr);
      exit (EXIT_FAILURE);
    }
}


void driver::scan_end ()
{
  fclose (yyin);
}
