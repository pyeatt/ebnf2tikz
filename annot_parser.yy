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

/*  This file contains the parser that handles annotations */

%require "3.7"
%language "c++"
%define api.prefix {annot}
%define api.token.raw
%define api.location.file "annot_location.hh"
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%code requires {
  #include <string>
  #include <assert.h>
  #include <map>
  #include <graph.hh>
  using namespace std;
  class annotdriver;
}
// The parsing context.
%param { annotmap *m }
%locations
%define parse.error detailed
%define parse.lac full
%code {
#include <iostream>
using namespace std;
#include "annot_lexer.hh"
#include "nodesize.hh"

annot::location aloc;

// Give Flex the prototype of yylex
#define YY_DECL annot::parser::symbol_type annotlex (annotmap *m)
// declare yylex here
YY_DECL;

annot::location loc;
  
}

%define api.token.prefix {TOK_}

%token
  ASTART    "<<--"			
  AEND      "-->>"
  SEMICOLON ";"
  SUBSUME   "subsume"
  AS        "as"
  CAPTION   "caption"
  SIDEWAYS  "sideways"
;			

%token	<std::string> UNEXP "character"
%token	<std::string> STRING "nonterminal"
%token END 0 "end of file"

%printer { yyo << $$; } <*>;

// Bison works by asking flex to get the next token, which it returns
// as an object of type "yystype".  Initially (by default), yystype is
// merely a typedef of "int", but for non-trivial projects, tokens
// could be of any arbitrary data type.  So, to deal with that, the
// idea is to override yystype's default typedef to be a C union
// instead.  Unions can hold all of the types of tokens that Flex
// could return, and this this means we can return ints or floats or
// strings cleanly.

// Define the "terminal symbol" token types (in CAPS by convention),
// and associate each with a field of the %union:

// declare the types for nonterminals

%nterm <annot_t *>  annot;
%nterm <annotmap *> annots;
%nterm <annotmap *> annotations;

// Currently supports these options for annotations:
//
// sideways           // create a sidewaysfigure instead of figure
// caption "chars"    // set the caption for the LaTeX figure
// subsume            // replace all nonterms matching the name of the
                      // annotated node
// subsume as <regex> // using regex... replace all nonterms matching this
                      // regex with thi production, and do not produce seperate
		      // figure for this production.

// I may add these if I need them:
// make figure        // even if subsumed according to the previous rulse,
                      // produce a figure
// replace x with y   // in the following production, replace notterm x with
                      // production y 

%%

%start annotations;

// Define the grammar: A grammar is a list of productions
annotations :
  ASTART annots AEND {
  
  } ;

annots :
  annot {
    (*m)[$1->first] = $1->second;
    delete $1;
  } |
  annots SEMICOLON annot {
    (*m)[$3->first] = $3->second;
    delete $3;
  } ;

annot :
  SIDEWAYS {
    $$=new annot_t("sideways","true");
  } |
  SUBSUME {
    $$=new annot_t("subsume","emusbussubsume");
  } |
  SUBSUME AS STRING {
    $$=new annot_t("subsume",$3);
  } |
  CAPTION STRING {
  $$=new annot_t("caption",$2);
  } ;



%%

annotmap *scanAnnot(string &s, void *loc)
{
  annot::location *lloc = (annot::location*)loc;
  aloc.initialize(lloc->begin.filename,lloc->begin.line,lloc->begin.column);
  aloc += *(annot::location *)loc;
  annotmap *m = new annotmap;
  annot::parser p(m);
  annot_scan_string(s.c_str());
  p.parse();
  annotlex_destroy();
  return m;
}


    
void annot::parser::error (const annot::location& l, const std::string& m)
{
  cerr << m << " between "
  "line "<<l.begin.line << " col "<<l.begin.column<<" and "<<
  "line "<<l.end.line   << " col "<<l.end.column << " in "<<
  *l.begin.filename<<endl;
}

