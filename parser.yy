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

node* wrapChoice(node *n) {
  if(n->is_choice()) {
    railnode *r,*l;
    concatnode *c;
    l = new railnode(LEFT,DOWN);
    r = new railnode(RIGHT,UP);
    n->setLeftRail(l);
    n->setRightRail(r);
    c = new concatnode(l);
    c->insert(n); 
    c->insert(r);
    n=c;
  }
  return n;
}

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
// %nterm <node*> concat;
// %nterm <node*> concats;
%nterm <productionnode*> production;
%nterm <grammar*> productions;
//%nterm <node*> choice;
//%nterm <node*> choices;
%nterm <grammar*> grammar;
%nterm <int> annotations;
%nterm <node*> expression;
%nterm <node*> primary;

%right EQUAL
%left SEMICOLON
%left LBRACK RBRACK
%left LBRACE RBRACE
%left LPAREN RPAREN
%left PIPE
%left COMMA

%%

%start grammar;

// Define the grammar: A grammar is a list of productions
grammar : productions {
     // after reading the grammar...
     grammar *g = $1;
     g->dump();
     g->setParent();
     g->setPrevious();
     g->setNext();
     g->optimize();
     g->setParent();
     g->setPrevious();
     g->setNext();
     g->subsume();
//     g->optimize();

     g->setParent();
     g->setPrevious();
     g->setNext();
     g->dump();
     // let everyoune know their parent, and if they are in a concat,
     // who is previous and who is next.
     g->mergeRails();
     g->place(drv.outs());
  } ;

// "productions" is either a single production or a list of
// productions
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
// equal sign, followed by an expression, followed by a semicolon.
production: annotations STRING EQUAL expression SEMICOLON
  {
    coordinate start;
    $4=wrapChoice($4);
    $$ = new productionnode($1,$2,$4);
  } ;

// I would like to support these options for annotations:
//
// subsume            // replace all nonterms exactly matching this productinon
                      // with this production, and do not produce seperate figure
		      // for this production.
// subsume as <regex> // using regex... replace all nonterms matching this
                      // regex with thi production, and do not produce seperate
		      // figure for this production.
// make figure        // even if subsumed according to the previous rulse,
                      // produce a figure
// replace x with y   // in the following production, replace notterm x with
                      // production y 
// caption "chars"    // set the caption for the LaTeX figure
annotations : SUBSUME{
    $$=1;
  } |
  {
      $$=0;
  } ;

/* lines : lines NEWLINE concats { */
/*     $$ = new newlinenode("ebnf2tikz manual newline node"); */
/*     } | */
/*     concats */
/*     { */
/*     } */
  
expression:
  expression PIPE expression
    {
      // PIPE is left associative, so only need to check $1
      if(!$1->is_choice())
        $$ = new choicenode($1);
      else
        $$ = $1;    
      $$->insert($3);
    } |
  expression COMMA expression
    {
      $1=wrapChoice($1);
      $3=wrapChoice($3);
      // COMMA is left associative, so only need to check $1
      if($1->is_terminal() || $1->is_nonterm()) {
        $$ = new concatnode($1);
        $$->insert($3);
      } else {
 	$$ = $1;
	$$->insert($3);
      }
    } |
  LPAREN expression RPAREN {
    $2 = wrapChoice($2);
    $$ = $2;
  } |
  LBRACK expression RBRACK {
    $2 = wrapChoice($2);
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
  LBRACE expression RBRACE {
    $2=wrapChoice($2);
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
  cerr << m << " at line "<< l.end.line << " column " <<
		l.end.column << " in "<<*l.begin.filename<<endl;
}

