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
  #include "graph.hh"
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
#include "nodesize.hh"

extern yy::location loc;

/* node* addRails(node *n) { */
/*   if(n->is_choice()) { */
/*     railnode *r,*l; */
/*     concatnode *c; */
/*     l = new railnode(LEFT,DOWN); */
/*     r = new railnode(RIGHT,UP); */
/*     n->setLeftRail(l); */
/*     n->setRightRail(r); */
/*     c = new concatnode(l); */
/*     c->insert(n);  */
/*     c->insert(r); */
/*     n=c; */
/*   } */
/*   return n; */
/* } */

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

// Bison fundamentally works by asking flex to get the next token, which it
// returns as an object of type "yystype".  Initially (by default), yystype
// is merely a typedef of "int", but for non-trivial projects, tokens could
// be of any arbitrary data type.  So, to deal with that, the idea is to
// override yystype's default typedef to be a C union instead.  Unions can
// hold all of the types of tokens that Flex could return, and this this means
// we can return ints or floats or strings cleanly.  

// Define the "terminal symbol" token types (in CAPS by convention),
// and associate each with a field of the %union:

// declare the types for nonterminals
%nterm <productionnode*> production;
%nterm <grammar*> productions;
%nterm <grammar*> grammar;
%nterm <annotmap*> annotations;
%nterm <node*> expression;
%nterm <node*> primary;
%nterm <node*> rows;

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

// Define the grammar: A grammar is a list of productions
grammar : productions {
     grammar *g = $1;

        // g->dump();

	/* g->setParent(); */
	/* g->setPrevious();  */
    	/* g->setNext(); */

//        g->optimize();

//        g->dump();

     
     /* g->setParent(); */
     /* g->setPrevious(); */
     /* g->setNext(); */

//      g->setParent();
//      g->setPrevious();
//      g->setNext();
//      g->subsume();

//      g->setParent();
//      g->setPrevious();
//      g->setNext();
//      g->optimize();

     /* g->setParent(); */
     /* g->setPrevious(); */
     /* g->setNext(); */
     /* g->mergeRails(); */

//      // g->setParent();
//      // g->setPrevious();
//      // g->setNext();
//      // g->optimize();

/*      g->setParent(); */
/*      g->setPrevious(); */
/*      g->setNext(); */
/* g->dump(); */
/*        g->createRows(); */

//      g->setParent();
//      g->setPrevious();
//      g->setNext();
//      g->fixSkips();


     /* g->setParent(); */
     g->setPrevious();
     g->setNext();
     g->place(drv.outs());

        g->dump();


     delete g;
  } ;

productions: productions production {
    // add new production to the grammar
    $1->insert($2);
    $$=$1;
  } |
  production {
    // create the grammar
    node *n = (node*)$1;
    $$=new grammar(n);
  } ;

// Finally getting to the meat.  A single production is described
// as annotations, followed by a production name, followed by an
// equal sign, followed by rows, followed by a semicolon.
production: annotations STRING EQUAL rows SEMICOLON
  {
    concatnode *c = new concatnode(new nullnode("start1"));
    c->setBeforeSkip(0);
    c->setDrawToPrev(0);
    c->getChild(0)->setBeforeSkip(0);
    c->getChild(0)->setDrawToPrev(0);
    c->insert(new nullnode("start2"));
    coordinate start;
    if(!$4->is_concat())
       {
         $4->setBeforeSkip(node::getColSep());
         $4->setDrawToPrev(1);
         c->insert($4);
       }
    else
      {
        // move all children to new concat
	while($4->numChildren())
         {
// SECOND PLACE WHERE THE INSERT IS DONE
           c->insert($4->getChild(0));
           $4->forgetChild(0);
         }
        delete $4;
      }
    
    c->insert(new nullnode("end1"));
    c->insert(new nullnode("end2"));
    
    $$ = new productionnode($1,$2,c);
  } ;


annotations : ANNOTATION {
    map<string,string> *a;
    a = scanAnnot($1,&drv.get_location());
    $$ = a;
  } |
  {
      $$=NULL;
  } ;

// The body of a production is a concat of rows separated by newlines.
// It may contain only one row.
rows :
  // in the first case, we have a concat containing at least one row,
  // and need to add a newline node and an expression (row) to it.
  rows NEWLINE expression {
 //     $3 = addRails($3);
      newlinenode *n = new newlinenode();
      //rownode *row = new rownode($3);
      concatnode *row = new concatnode($3);
      row->setBeforeSkip(0);
      $3->setBeforeSkip(0);
      // if($3->getChild(0) != NULL)
      //   $3->getChild(0)->setBeforeSkip(0);
      if($1->is_concat())
        {
	   $$ = $1;
 	   // if the previous row endend in a rail, then set the
 	   // beforeskip for the newline to zero
	   node *lr = $1->getChild($1->numChildren()-1)->getChild(0);
	   if(lr != NULL && lr->is_concat() &&
	       lr->getChild(lr->numChildren()-1)->is_rail())
	     n->setBeforeSkip(0);
	}
      else
        {
	  $$ = new concatnode($1);
          $$->setBeforeSkip(0);
	}
	
	// if the next row begins in a rail, then set beforeskips
	// appropriately
       if($3->is_concat())
       	{ 
          // beforskip for first child of new row is zero
	   row->setBeforeSkip(0); // beforeskip for its row is zero
	   $3->setBeforeSkip(0); // beforeskip for its concat is zero
	  if($3->getChild(0)->is_rail())
	    {
	      $3->getChild(0)->setBeforeSkip(0);
	      $3->getChild(1)->setBeforeSkip(0);
	      $3->getChild(1)->getChild(0)->setBeforeSkip(0);
	    }
	}
       else
         $3->setBeforeSkip(node::getColSep());

      $$->insert(n);
      $$->insert(row);
      $$->setDrawToPrev(1);
     } |
    // in this case, we need to create the initial concat, and add the
    // row (expression) to it
    expression {
      if($1->is_choice())
	{
	    $1->setLeftRail(new railnode(LEFT,UP));
	    $1->setRightRail(new railnode(RIGHT,UP));
            /* if($1->is_choice()) */
	    /* 	{ */
	    /* 	  $1->setDrawToPrev(1); */
            /*       $1->getChild(0)->setDrawToPrev(1); */
            /*     } */
	}
      /* else */
      /*   if($1->is_loop()) */
      /* 	  { */
      /* 	    $1->setLeftRail(new railnode(LEFT,UP)); */
      /* 	    $1->setRightRail(new railnode(RIGHT,DOWN)); */
      /* 	  } */
      //$1 = addRails($1);
      //$$ = new rownode($1);
      //$$ = new concatnode($1);
      $1->setBeforeSkip(0);
      $1->setDrawToPrev(1);
      $$ = $1;
      /* $$->setBeforeSkip(0); */
      /* $$->setDrawToPrev(1); */
     } ;
  
expression:
  // NEWLINE
  //   {
  //     railnode *r,*l;
  //     newlinenode *n;
  //     l = new railnode(RIGHT,DOWN);
  //     r = new railnode(LEFT,UP);
  //     n = new newlinenode("ebnf2tikz manual newline node");
  //     n->setLeftRail(l);
  //     n->setRightRail(r);
  //     $$ = new concatnode(l);
  //     $$->insert(n);
  //     $$->insert(r);
  //   } |
  expression PIPE expression // CHOICE node
    {
      // PIPE is left associative, so only need to check $1
      $1->setBeforeSkip(0);
      $3->setBeforeSkip(0);
      if($3->is_concat())
	($3->getChild(0))->setBeforeSkip(0);
      if($1->is_concat())
	($1->getChild(0))->setBeforeSkip(0);
      if($1->is_choice())
	{
          $$ = $1;
	}
      else
        {
          $$ = new choicenode($1);
	  $1->setDrawToPrev(1);
	  $$->setDrawToPrev(1);
	}
      $$->insert($3);
    } |
  expression COMMA expression // CONCAT node
    {
      //      $1=addRails($1);
      //    $3=addRails($3);
      // COMMA is left associative, so only need to check $1
      if(!$1->is_concat())
        {
          $$ = new concatnode($1);
            /* if($1->is_choice()) */
	    /* 	{ */
	    /* 	  $1->setDrawToPrev(1); */
            /*       $1->getChild(0)->setDrawToPrev(1); */
            /*     } */

        }
      else 
 	$$ = $1;

      /* if($3->is_concat()) */
      /* 	{ */
      /*     // move children from second concat to first concat */
      /*     while($3->numChildren()) */
      /* 	    { */
      /*         ($3->getChild(0))->setBeforeSkip(node::getColSep()); */
      /*         $$->insert($3->getChild(0)); */
      /*         $3->forgetChild(0); */
      /*       } */
      /*     delete $3; */
      /*   } */
      /* else */
// FIRST PLACE WHERE THE INSERT IS DONE
        $$->insert($3);
            if($3->is_choice())
		{
		  $3->setDrawToPrev(1);
                  $3->getChild(0)->setDrawToPrev(1);
                }


    } |
  LPAREN expression RPAREN {
      // if it is a choice, then we need to add rails
      if($2->is_choice())
	{
          railnode *l = new railnode(LEFT,UP);
          railnode *r = new railnode(RIGHT,UP);
          $2->setLeftRail(l);
          $2->setRightRail(r);
	}

//    $2 = addRails($2);
    $$ = $2;
  } |
  LBRACK expression RBRACK {  // OPTIONAL node (choice with first child NULL) 
//		$2 = addRails($2);
		choicenode *c; 
      $2->setBeforeSkip(0);
      if($2->is_concat())
	($2->getChild(0))->setBeforeSkip(0);
		// If $2 is a choice node, then insert a 
		// nullnode as its first child.
		if($2->is_choice())
		{
 		/* cout << "-----------------------------\n"; */
		/* cout << "Building optional node from choice node\n"; */
		/* ((concatnode*)$2)->dump(0); */
		/* cout << "-----------------------------\n"; */
		c = (choicenode*)$2;
                c->insertFirst(new nullnode("NULL node"));
	        /* c->dump(0); */
		/* cout << "-----------------------------\n"; */
		}
		// otherwise create a choice node with the first
		// child being a null node and the second child $2
		else  
		{
		/* cout << "-----------------------------\n"; */
		/* cout << "Building optional node from other node\n"; */
		/* ((concatnode*)$2)->dump(0); */
		/* cout << "-----------------------------\n"; */
		c = new choicenode(new nullnode("NULL node"));
		c->insert($2);
		}
    // add the rail nodes
    railnode *r,*l;
    l = new railnode(LEFT,UP);
    r = new railnode(RIGHT,UP);
    c->setLeftRail(l);
    c->setRightRail(r);
    $$ = c;
//    c->insert($2);
//    $$ = c;
//    $$ = new concatnode(l);
//    $$->insert(c);
//    $$->insert(r);
  } |
  LBRACE expression RBRACE {  // LOOP node
    // cout << "creating loop around ";$2->dump(1);
    loopnode *n;
    railnode *r,*l;
    // reverse all of the concats contained in $2
    $2->reverse();
    n = new loopnode(new nullnode("NULL node"));
    l = new railnode(LEFT,UP);
    r = new railnode(RIGHT,UP);
    n->setLeftRail(l);
    n->setRightRail(r);
    // if $2 is a choice node, then delete its rails, move all
    // of its children to the new loop node, and set their rails.
    if($2->is_choice())
      {
cout << "converting choice to loop\n";
        while($2->numChildren())
	 {
cout << "moving child \n";
($2->getChild(0))->dump(0);
	   ($2->getChild(0))->setLeftRail(l);
	   ($2->getChild(0))->setLeftRail(r);
	   ($2->getChild(0))->setParent(n);
	   ($2->getChild(0))->setBeforeSkip(0);
($2->getChild(0))->dump(0);
l->dump(0);
	   n->insert($2->getChild(0));
           $2->forgetChild(0);
         }
        delete $2->getLeftRail();          
        delete $2->getRightRail();          
        delete $2;          
      }
    else
      {
cout << "creating new loop\n";
    //    n = new loopnode(new nullnode("NULL node"));
        if($2->is_concat())
          ($2->getChild(0))->setBeforeSkip(0);
        n->insert($2);
      }
    $$ = n;
//    $$ = new concatnode(l);
//    $$->insert(n);
//    $$->insert(r);
  } |
  primary {
//    cout << "creating primary \n";
//    $1->dump(0);
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

