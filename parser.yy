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

/*  This file contains the main parser.  There is another parser that
    handles annotations.
*/

%require "3.7"
%language "c++"
%define api.token.raw
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%code requires {
  #include <string>
  #include <assert.h>
  #include "ast.hh"
  class driver;
  annotmap *scanAnnot(string &s, void *loc);
}
// The parsing context.
%param { driver& drv }
%locations
%define parse.trace
%define parse.error detailed
%define parse.lac full
%code {
#include <iostream>
using namespace std;
#include "driver.hh"
#include "annot_lexer.hh"
#include "lexer.hh"
#include "nodesizes.hh"
#include "resolver.hh"
#include "ast_optimizer.hh"
#include "ast_dump.hh"
#include "ast_layout.hh"
#include "ast_tikzwriter.hh"

extern yy::location loc;
}

%define api.token.prefix {TOK_}


%token
  COMMA     ","

%token
  EQUAL     "="
  SEMICOLON ";"
  PIPE      "|"
  LBRACK    "["
  RBRACK    "]"
  LPAREN    "("
  RPAREN    ")"
  LBRACE    "{"
  RBRACE    "}"
  NEWLINE   "\\\\"
;
%token	<std::string> UNEXP "character"
%token	<std::string> TERM  "terminal"
%token	<std::string> STRING "nonterminal"
%token	<std::string> ANNOTATION "annotation"
%token END 0 "end of file"

%printer { yyo << $$; } <*>;

// declare the types for nonterminals
%nterm <ASTProduction*> production;
%nterm <ASTGrammar*> productions;
%nterm <ASTGrammar*> grammar;
%nterm <annotmap*> annotations;
%nterm <ast::ASTNode*> expression;
%nterm <ast::ASTNode*> primary;
%nterm <ast::ASTNode*> rows;

// associativity and precedence.  Lowest precedence first.
%right EQUAL
%left SEMICOLON
%left NEWLINE
%left LBRACK RBRACK
%left LBRACE RBRACE
%left LPAREN RPAREN
%left PIPE
%left COMMA

%%

%start grammar;

// Define the grammar: A grammar is a list of productions.
grammar : productions {
     ASTGrammar *astg = $1;

     if(!drv.get_noopt())
       {
         resolver::subsume(astg);
         ast_optimizer::optimize(astg);
       }

     astAutoWrapGrammar(astg, drv.getSizes());

     if(drv.get_dumponly())
       {
         /* Dump mode: print AST directly */
         astDumpGrammar(astg);
         delete astg;
       }
     else
       {
         /* TikZ output: AST-based layout directly */
         astPlaceGrammar(astg, drv.outs(), drv.getSizes());
         delete astg;
       }
  } ;

productions: productions production {
    $1->insert($2);
    $$=$1;
  } |
  production {
    $$=new ASTGrammar();
    $$->insert($1);
  } ;

// A single production: annotations, name, =, body, ;
production: annotations STRING EQUAL rows SEMICOLON
  {
    $$ = new ASTProduction($1, $2, $4);
  } ;


annotations : ANNOTATION {
    map<string,string> *a;
    a = scanAnnot($1,&drv.get_location());
    $$ = a;
  } |
  {
      $$=NULL;
  } ;

// The body of a production may span multiple rows separated by newlines.
rows :
  rows NEWLINE expression {
      ast::SequenceNode *s;
      s = new ast::SequenceNode();
      s->append($1);
      s->append(new ast::NewlineNode());
      s->append($3);
      $$ = s;
     } |
    expression {
      $$ = $1;
     } ;

expression:
  expression PIPE expression // CHOICE node
    {
      ast::ChoiceNode *c;
      // PIPE is left associative, so only need to check $1
      if($1->kind == ast::ASTKind::Choice)
	{
          c = static_cast<ast::ChoiceNode*>($1);
	}
      else
        {
          c = new ast::ChoiceNode();
	  c->addAlternative($1);
	}
      c->addAlternative($3);
      $$ = c;
    } |
  expression COMMA expression // CONCAT node
    {
      ast::SequenceNode *s;
      // COMMA is left associative, so only need to check $1
      if($1->kind == ast::ASTKind::Sequence)
        {
          s = static_cast<ast::SequenceNode*>($1);
        }
      else
        {
          s = new ast::SequenceNode();
          s->append($1);
        }
      s->append($3);
      $$ = s;
    } |
  LPAREN expression RPAREN {
    $$ = $2;
  } |
  LBRACK expression RBRACK {  // OPTIONAL node
    $$ = new ast::OptionalNode($2);
  } |
  LBRACE expression RBRACE {  // LOOP node
    ast::LoopNode *n;
    size_t i;
    n = new ast::LoopNode();
    if($2->kind == ast::ASTKind::Choice)
      {
        ast::ChoiceNode *ch = static_cast<ast::ChoiceNode*>($2);
	for(i = 0; i < ch->alternatives.size(); i++)
	  n->addRepeat(ch->alternatives[i]);
	ch->alternatives.clear();
	delete ch;
      }
    else
      {
	n->addRepeat($2);
      }
    $$ = n;
  } |
  primary {
    $$ = $1;
  } ;



/*  a primary is either a terminal or nonterminal symbol */
primary:
  STRING {
    $$ = drv.addString($1);
  } |
  TERM   {
    $$ = drv.addTerminal($1);
  } ;

%%


void yy::parser::error (const location_type& l, const std::string& m)
{
  cerr << m << " between "
  "line "<<l.begin.line << " col "<<l.begin.column<<" and "<<
  "line "<<l.end.line   << " col "<<l.end.column << " in "<<
  *l.begin.filename<<endl;
}
