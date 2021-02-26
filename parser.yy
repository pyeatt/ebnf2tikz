%require "3.7"
%language "c++"
%define api.token.raw
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%code requires {
  #include <string>
  #include "graph.hh"
  class driver;
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
#include "nodesize.hh"
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
  SUBSUME   "subsume"
;			
%token	<std::string> UNEXP "charaacter"
%token	<std::string> TERM  "terminal"
%token	<std::string> STRING "nonterminal"
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

// declare the type for nonterminals
%nterm <node*> concat;
%nterm <node*> concats;
%nterm <productionnode*> production;
%nterm <grammar*> productions;
%nterm <node*> choice;
%nterm <node*> choices;
%nterm <grammar*> grammar;
%nterm <int> annotations;

			
%right EQUAL
%left SEMICOLON
%left PIPE
%left COMMA



			
%%

%start grammar;
// Define the grammar: A grammar is a list of productions
grammar : productions
  { // after reading the program...
     grammar *g = $1;
     g->optimize();
     g->subsume();
     g->optimize();
g->dump();
     // let everyoune know their parent, and if they are in a concat,
     // who is previous and who is next.
     g->setParent();
     g->setPrevious();
     g->setNext();
     g->mergeRails();
     g->place(drv.outs());
  };


// a list of statements is a single statement, or a list of statements
// followed by a single statement
productions: productions production 
   {
       // add new production to the grammar
       $1->insert($2);
       $$=$1;
   }
   | production
   {
       // create the grammar
       node *n = (node*)$1;
       $$=new grammar(n);
   };

// Finally getting to the meat.  A single statement is one of these things.
production: annotations STRING EQUAL concats SEMICOLON
  {
      coordinate start;
      $$ = new productionnode($1,$2,$4);

      //$$->dump(0);
      

  };

// I would like to support these syntax options for annotations:
// subsume
// subsume as x y z .*qrest  // using regex... 
// replace x with y
// caption "thttht"
// caption 'thouuot'
annotations : SUBSUME{
    $$=1;
    }
  |
  {
      $$=0;
  }

/* lines : lines NEWLINE concats { */
/*     $$ = new newlinenode("ebnf2tikz manual newline node"); */
/*     } | */
/*     concats */
/*     { */

/*     } */

concats : concats COMMA concat
  {
    $1->insert($3);
    $$ = $1;
  }
  | concat
  {
     $$ = new concatnode($1);	
  };

concat :
   NEWLINE {
       $$ = new newlinenode("ebnf2tikz manual newline node");
   } |
   STRING {
       $$ = drv.addString($1);
   } |
   TERM {
       $$ = drv.addTerminal($1);
   } |
   LBRACK concats RBRACK {
       choicenode *c = new choicenode(new nullnode("ebnf2tikz NULL node"));
       railnode *r,*l;
       l = new railnode(LEFT,DOWN);
       r = new railnode(RIGHT,UP);
       c->setLeftRail(l);
       c->setRightRail(r);
       c->insert($2); 
       $$ = new concatnode(l);
       $$->insert(c);
       $$->insert(r);
   } |
   LBRACE concats RBRACE {
      loopnode *n = new loopnode($2); 
       railnode *r,*l;
       l = new railnode(LEFT,UP);
       r = new railnode(RIGHT,DOWN);
       n->setLeftRail(l);
       n->setRightRail(r);
       $$ = new concatnode(l);
       $$->insert(n);
       $$->insert(r);
   } |
   LPAREN concats RPAREN {
      $$ = $2;
   } |
   choices {
       railnode *r,*l;
       l = new railnode(LEFT,DOWN);
       r = new railnode(RIGHT,UP);
       $1->setLeftRail(l);
       $1->setRightRail(r);
       $$ = new concatnode(l);
       $$->insert($1);
       $$->insert(r);
   } ;

   /* concat COMMA concat { */
   /* } ; */

choices : choices PIPE concats
  {
    $1->insert($3);
    $$ = $1;
  } |
  concats PIPE concats // PIPE concat
  {
     $$ = new choicenode($1);
     $$->insert($3);
  };



    
    %%
    
void
yy::parser::error (const location_type& l, const std::string& m)
{
  cerr << m << " at line "<< l.end.line << " column " <<l.end.column << " in "<<
		*l.begin.filename<<endl;
}

