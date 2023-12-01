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

#ifndef GRAPH_HH
#define GRAPH_HH

#include <map>
#include <vector>
#include <regex.h>
#include <nodesize.hh>

using namespace std;

// choicenode and loopnode have associated left and right railnodes.
// When the parser creates a choicenode or a loopnode, it also
// creates the railnodes and inserts them into the current concat.
// During optimization, adjacent rails can be merged if they are
// compatible.  We can merge consecutive left choice rails,
// Consecutive right choice rails, and right choice rails followed by
// right loop rails if the first choice is nullnode and the loop
// repeat is nullnode.
string latexwrite(string fontspec,string s);
string nextCoord();
string nextNode();
string nextChain();
string nextFit();
string stripSpecial(string s);

// ------------------------------------------------------------------------

typedef map<string,string> annotmap; 
typedef pair<string,string> annot_t ;
typedef enum {LEFT,RIGHT} vrailside;
//typedef enum {UP,DOWN,STARTNEWLINEUP,STARTNEWLINEDOWN} vraildir;
typedef enum {UP,DOWN} vraildir;

class railnode;

// Base class for all nodes in the parse tree.
class node {
protected:
  static string vrailStr(vraildir d)
  {
    switch (d)
      {
      case UP:
	return "UP";
      case DOWN:
	return "DOWN";
      default:
	cerr<<"BAD VALUE for vraildir"<<endl;
	exit(4);
      }
    return "";
  }
  typedef enum {GRAMMAR, CHOICE, TERMINAL, NONTERM, CONCAT,
    NULLNODE, LOOP, NEWLINE, PRODUCTION, RAIL, ROW, UNKNOWN} nodetype;
  static nodesizes* sizes;
  nodetype type;
  string nodename;
  string ea,wa;   // east and west attachment points
  float myWidth,myHeight;
  node* parent;
  node* previous;
  node* next;
  float beforeskip;
  int drawtoprev;
  railnode *leftrail,*rightrail;
  int dead;
  coordinate location; // where to draw the east point of the thing
  // use a variadic function to draw lines.  first argument is the
  // number of points in the line, The remaining arguments are the
  // tikz coordinates of things you want connected (eg "node1.east",
  // "coord3", "10,10", etc
  template <class ... Args>
  void line(ofstream &outs, Args ... args);
  // Returns 0 if two nodes do not have the same type.
  int same_type(node &r){return type == r.type;}
public:
  // Constructors
  node();
  node(const node &original);
  // Method to clone a node
  virtual node* clone() const = 0;
  // Destructor
  virtual ~node();
  // Set a node's parent pointer. Some nodes have multiple children
  virtual void setParent(node* p){parent = p;}
  node* getParent(){return parent;}
  // Set a node's previous pointer. The top level is a concat, and
  // every node in the concat has a pointer to the previous and next
  // nodes.
  virtual void setPrevious(node* p){previous = p;}
  node* getPrevious(){return previous;}
  virtual void setNext(node* p){next = p;}
  node* getNext(){return next;}
  // Most nodes will have a left and right rail
  virtual void setLeftRail(railnode* p){leftrail = p;}
  virtual void setRightRail(railnode* p){rightrail = p;}
  railnode* getLeftRail(){return leftrail;}
  railnode* getRightRail(){return rightrail;}
  // Functions to draw connections to the left and right rails.
  virtual void drawToLeftRail(ofstream &outs, railnode* p,
			      vraildir join,int drawself);
  virtual void drawToRightRail(ofstream &outs, railnode* p,
			       vraildir join, int drawself);
  // If something is optimized out, it is marked as dead
  void makeDead(){dead = 1;}
  int isDead(){return dead;}
  // set the beforeskip value in points  
  void setBeforeSkip(float s){beforeskip=s;}
  float getBeforeSkip(){return beforeskip;}
  // Should this node draw a rail to the previous node?
  void setDrawToPrev(int d){drawtoprev=d;}
  int getDrawToPrev(){return drawtoprev;}
  // call this static member function to load the row and colum widths
  // and the node sizes before calling the place(...) function on the
  // top level (graph) node.
  static void loadData(string filename) {
    sizes = new nodesizes();
    sizes->loadData(filename);
  }
  static void deleteData() {
    delete sizes;
  }
  static float getColSep(){return sizes->colsep;}
  int is_choice(){return type==CHOICE;}
  int is_terminal(){return type==TERMINAL;}
  int is_nonterm(){return type==NONTERM;}
  int is_concat(){return type==CONCAT;}
  int is_null(){return type==NULLNODE;}
  int is_loop(){return type==LOOP;}
  int is_row(){return type==ROW;}
  int is_production(){return type==PRODUCTION;}
  int is_newline(){return type==NEWLINE;}
  int is_rail(){return type==RAIL;}
  string east(){return ea;}
  string west(){return wa;}
  // start at start, lay yourself out, and return the coordinate that
  // you end at.  The "draw" argument tells whether or not to actually
  // emit tikz code.  The compiler will call this method twice for
  // every object.  The first time is just to calculate the size of
  // the object.  The second time will be to actually emit code.
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth){
    return start;}

  virtual void fixSkips(){}
  
  virtual void insert(node*){}
  virtual void mergeRails(){}
  virtual void dump(int depth) const;
  virtual string texName() { return "";};
  virtual string rawName() { return nodename;};
  float width(){return myWidth;}
  void setwidth(float w){myWidth=w;}
  float height(){return myHeight;}
  void setheight(float h){myHeight=h;}
  virtual int mergeConcats(int depth);
  virtual int liftConcats(int depth);
  virtual int analyzeOptLoops(int depth);
  virtual int analyzeNonOptLoops(int depth);
  virtual int mergeChoices(int depth);
  virtual int numChildren(){return 0;}
  virtual node* getChild(int n){return NULL;}
  virtual int operator ==(node &r){return 0;}
  virtual int operator !=(node &r){return 1;}
  virtual node* subsume(regex_t* name, node *replacement){return this;}
  virtual void forgetChild(int n){};
  virtual node* createRows(){return NULL;}
};


// ------------------------------------------------------------------------
// Base class for nodes with a single child
class singlenode:public node{
protected:
  node* body;
public:
  singlenode(node *p);
  singlenode(const singlenode &original);
  virtual singlenode* clone() const;
  virtual void forgetChild(int n);
  virtual void drawToLeftRail(ofstream &outs, railnode* p,
			      vraildir join, int drawself);
  virtual void drawToRightRail(ofstream &outs, railnode* p,
			       vraildir join, int drawself);
  virtual void mergeRails(){body->mergeRails();}
  virtual ~singlenode(){if(body != NULL) delete body;}
  virtual int mergeConcats(int depth){
    return body->mergeConcats(depth+1);}
  virtual int liftConcats(int depth);
  virtual int mergeChoices(int depth){
    return body->mergeChoices(depth+1);}
  virtual int analyzeOptLoops(int depth){
    return body->analyzeOptLoops(depth+1);}
  virtual int analyzeNonOptLoops(int depth){
    return body->analyzeNonOptLoops(depth+1);}
  virtual int numChildren(){return 1;}
  virtual node* getChild(int n){return body;}
  virtual int operator ==(node &r);
  virtual int operator !=(node &r){return  !(*this == r);} 
  virtual node* subsume(regex_t* name, node *replacement);
  virtual void setParent(node* p);
  virtual void setPrevious(node *n){previous = n;body->setPrevious(n);}
  virtual void setNext(node *n) {next = n;body->setNext(n);}
  virtual void fixSkips();
  virtual string texName() { return "singlenode";};
};

// -------------------------------------------------------
// Class for rail nodes
// -------------------------------------------------------
/* downgoing rails are drawn  ----
                               \ 
                                |
 
   entries from left are drawn \  (except for bottom entry)
                                
   entries from right are drawn / (except for bottom entry)
  --------------------------------------------------------
   upgoing rails are drawn    ----
                                /
                               |
   
   entries from left are drawn /  (except for bottom entry)
                                
   entries from right are drawn \ (except for bottom entry)
  -------------------------------------------------
   newrow rails are drawn  -   at the end of the row
                            \
                             |
  
   and are drawn      -  at the beginning of the next row
                     /
                     |
   a choice at the end of a row can use the downgoing row rail.
   final choice also goes down instead of up
   a choice at the beginning of a row can use the upgoing row rail.
   first choice goes up instead of split down
*/
class railnode:public node{     
protected:
  //  vrailside side;    // LEFT or RIGHT: Which side of choice/loop is it on?
  vraildir direction;   // UP or DOWN: Direction of travel down this rail.
  vraildir bottomdir;   // LEFT or RIGHT: whether to turn left or right at bottom
                        
  coordinate top,bottom;  // top is even with bottom of top child
                          // if UP, then bottom is even with top
                          // of bottom child.  Otherwise it is even with
                          // bottom of bottom child.
public:
  railnode();
  railnode(vrailside s,vraildir d);
  railnode(const railnode &original);
  virtual railnode* clone() const {return new railnode(*this);}
  virtual ~railnode(){};
  virtual void setBottom(coordinate b){bottom = b;}
  virtual coordinate getBottom(){return bottom;}
  virtual void dump(int depth) const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int operator ==(node &r);
  virtual int operator !=(node &r){return !(*this == r);}
  virtual vraildir getRailDir(){return direction;}
  virtual void setRailDir(vraildir d){direction = d;}
  virtual string texName() { return "railnode";};
};

// ------------------------------------------------------------------------
// Node to draw a nonterminal

class nontermnode:public node{
protected:
  string style;
  string format;
  string str;
public:
  nontermnode(string s);
  nontermnode(const nontermnode &original);
  virtual nontermnode* clone() const;
  virtual ~nontermnode(){}
  virtual void forgetChild(int n){}
  virtual void dump(int depth) const;
  virtual string texName() {return latexwrite(format,str);}
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int mergeConcats(int depth){return 0;}
  virtual int mergeChoices(int depth){return 0;}
  virtual int liftConcats(int depth){return 0;}
  virtual node* getChild(int n){return NULL;}
  virtual int analyzeOptLoops(int depth){return 0;}
  virtual int analyzeNonOptLoops(int depth){return 0;}
  virtual void drawToLeftRail(ofstream &outs, railnode* p,
			      vraildir join, int drawself);
  virtual void drawToRightRail(ofstream &outs, railnode* p,
			       vraildir join, int drawself);
  virtual int operator ==(node &r);
  virtual int operator !=(node &r){return  !(*this == r);} 
  virtual node* subsume(regex_t* name, node *replacement);
};  

// ------------------------------------------------------------------------
// Node to draw a terminal
class termnode:public nontermnode{
public:
  termnode(string s);
  termnode(const termnode &original);
  virtual termnode* clone() const;
  virtual ~termnode(){}
};

// ------------------------------------------------------------------------
// this node type provides a placeholder to indicate that I need to
// draw a line, but there is nothing really on it.  It is a node with
// zero width and height. When drawn, it just emits a coordinate.  Its
// north(), south(), east() and west() just refer to that coordinate.
// This simplifies the code in choicenode.place() and loopnode.place().
class nullnode:public nontermnode{
public:
  nullnode(string s);
  nullnode(const nullnode &original);
  virtual nullnode* clone() const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual string texName() { return "nullnode";};
  virtual void dump(int depth) const;
};

// ------------------------------------------------------------------------
// Node for a newline rail
class newlinenode:public railnode{
  float lineheight;
public:
  newlinenode();
  newlinenode(const newlinenode &original);
  virtual newlinenode* clone() const;
  virtual ~newlinenode(){}
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int rail_left(){return 1;}
  virtual int rail_right(){return 1;}
  virtual void setLineHeight(float h){lineheight=h;}
  virtual void dump(int depth) const;
};

// ------------------------------------------------------------------------
// A rownode contains expressions that are drawn across the page.  The
// child is usually a concatnode.
class rownode:public singlenode{
private:
  string coordname;
public:
  rownode(node *p);
  rownode(const rownode &original);
  virtual rownode* clone() const;
  virtual ~rownode(){}
  virtual void dump(int depth) const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual string texName() { return "rownode";};
};

// ------------------------------------------------------------------------

// ------------------------------------------------------------------------
// nodes with multiple children
class multinode:public node{
  friend class concatnode; // allow concatnode to access nodes
protected:
  vector<node*> nodes;
public:
  multinode(node *p);
  multinode(const multinode &original);
  virtual multinode* clone() const;
  virtual void forgetChild(int n);
  virtual ~multinode();
  virtual void mergeRails();
  virtual void insert(node *node);
  virtual void insertFirst(node *node);
  virtual int numChildren(){return nodes.size();}
  virtual node* getChild(int n){return nodes[n];}
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int operator ==(node &r);
  virtual int operator !=(node &r){return  !(*this == r);} 
  virtual node* subsume(regex_t* name, node *replacement);
  virtual void setParent(node* p);
  virtual void setPrevious(node* p);
  virtual void setNext(node* p);
  virtual int liftConcats(int depth);
  virtual int mergeConcats(int depth);
  virtual int mergeChoices(int depth);
  virtual int analyzeOptLoops(int depth);
  virtual int analyzeNonOptLoops(int depth);
  virtual void fixSkips();
  virtual string texName() { return "multinode";};
};

// ------------------------------------------------------------------------
// Node for a choice

class choicenode:public multinode{
public:
  choicenode(node *p);
  choicenode(const choicenode &original);
  virtual choicenode* clone() const;
  virtual void insert(node *node);
  virtual ~choicenode(){}
  virtual int rail_left(){return 1;}
  virtual int rail_right(){return 1;}
  virtual void drawToLeftRail(ofstream &outs, railnode* p,
			      vraildir join, int drawself);
  virtual void drawToRightRail(ofstream &outs, railnode* p,
			       vraildir join, int drawself);
  virtual void dump(int depth) const;
  virtual int mergeChoices(int depth);
  virtual void insertFirst(node *node);
  virtual void fixSkips();
  virtual string texName() { return "choicenode";};
};

// ------------------------------------------------------------------------
// Node for a loop

class loopnode:public multinode{
public:
  loopnode(node *node);
  loopnode(const loopnode &original);
  loopnode* clone() const;
  virtual ~loopnode(){}
  virtual void dump(int depth) const;
  virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join, int drawself);
  virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join, int drawself);
  node* getRepeat();
  void setRepeat(node *r);
  node* getBody();
  void setBody(node *r);
  virtual void fixSkips();
  virtual string texName() { return "loopnode";};
};

// ------------------------------------------------------------------------
// Node for a concat

class concatnode:public multinode{
private:
  int findAndDeleteMatches(vector<node*> &parentnodes,
			   vector<node*>::iterator &wherep,
			   vector<node*> &childnodes,
			   vector<node*>::iterator &wherec);
public:
  concatnode(node *p);
  concatnode(const concatnode &original);
  virtual concatnode* clone() const;
  virtual ~concatnode(){}
  virtual void dump(int depth) const;
  virtual void insert(node* p);
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int mergeConcats(int depth);
  virtual int analyzeOptLoops(int depth);
  virtual int analyzeNonOptLoops(int depth);
  virtual void drawToLeftRail(ofstream &outs, railnode* p,
			      vraildir join, int drawself);
  virtual void drawToRightRail(ofstream &outs, railnode* p,
			       vraildir join, int drawself);
  virtual void mergeRails();
  virtual void setPrevious(node* p);
  virtual void setNext(node* p);
  virtual void fixSkips();
  virtual node* createRows();
};

// ------------------------------------------------------------------------
// A productionnode contains lines, rails, and nemlines
class productionnode:public singlenode{
private:
  string name;
  annotmap *annotations;
  regex_t *subsume_spec;
public:
  productionnode(annotmap *subsumespec,string s,node *p);
  productionnode(const productionnode &original);
  virtual productionnode* clone() const;
  virtual ~productionnode(){
    if(annotations != NULL)
      delete annotations;
    if(subsume_spec != NULL)
      {
	regfree(subsume_spec);
	delete subsume_spec;
      }
  }
  virtual regex_t* getSubsume(){return subsume_spec;}
  virtual string getName(){return name;}
  void optimize();
  virtual node* subsume(regex_t* name, node *replacement);
  virtual void dump(int depth) const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual void fixSkips(){body->fixSkips();}
  virtual node* createRows();
  virtual string texName() {return "production node '"+name+"'";};
};

// ------------------------------------------------------------------------
// All of the productions are collected into a grammar
class grammar{
private:
  vector<productionnode*> productions;
public:
  grammar(node *p){productions.push_back((productionnode*)p);}
  ~grammar();
  void insert(productionnode *node) {productions.push_back(node);}
  void dump() const;
  void optimize();
  void subsume();
  void place(ofstream &outs);
  void mergeRails();
  void setParent();
  void setPrevious();
  void setNext();
  void fixSkips(){for(auto i=productions.begin();i!=productions.end();i++)(*i)->fixSkips();}
  void createRows();
};

#endif


    
