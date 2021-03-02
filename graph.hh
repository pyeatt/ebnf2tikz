#ifndef GRAPH_HH
#define GRAPH_HH

#include <cstdarg>
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <nodesize.hh>

using namespace std;

// choicenode and loopnode have associated left and right railnodes.
// When the parser creates a choicenode or a loopnode,, it also
// creates the railnodes and inserts them into the current concat.
// During optimization, adjacent rails can be merged if they are
// compatible. During placement, the choicenode or loopnode will place
// all of its children, then connect them to the rails.  We can merge
// consecutive left choice rails, Consecutive right choice rails, and
// right choice rails followed by right loop rails.

string latexwrite(string fontspec,string s);
string nextCoord();
string nextNode();
string nextChain();
string nextFit();
string stripSpecial(string s);

// ------------------------------------------------------------------------

typedef enum {LEFT,RIGHT} vrailside;
typedef enum {UP,DOWN,STARTNEWLINEUP,STARTNEWLINEDOWN} vraildir;
class railnode;
// base class for all nodes in the parse tree
class node {
protected:
  typedef enum {GRAMMAR, CHOICE, TERMINAL, NONTERM, CONCAT,
    NULLNODE, LOOP, NEWLINE, PRODUCTION, RAIL, ROW, UNKNOWN} nodetype;

  nodetype type;
  
  static node *lastPlaced; // the last thing that was drawn
  string ea,wa,nodename;   // east and west attachment points, and node name
  static nodesizes sizes;
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
  void line(ofstream &outs, const int numpts, Args ... args);

  int same_type(node &r){return type == r.type;}

public:
  node();
  node(const node &original){
    type = original.type;
    //    nodename = original.nodename;
    nodename = nextNode();;
    ea   = nodename+".east";
    wa   = nodename+".west";
    myWidth = original.myWidth;
    myHeight = original.myHeight;
    parent = NULL;
    previous = NULL;
    next = NULL;
    leftrail = NULL;
    rightrail = NULL;
    beforeskip = original.beforeskip;
    drawtoprev = original.drawtoprev;
    dead = 0;
    //parent = original.parent;
  }
  virtual node* clone() const = 0;
  virtual ~node();

  virtual void setParent(node* p){parent = p;}
  virtual void setPrevious(node* p){previous = p;}
  virtual void setNext(node* p){next = p;}
  virtual void setLeftRail(railnode* p){leftrail = p;}
  virtual void setRightRail(railnode* p){rightrail = p;}
  railnode* getLeftRail(){return leftrail;}
  railnode* getRightRail(){return rightrail;}


  virtual void drawToLeftRail(ofstream &outs, railnode* p,vraildir join);
  virtual void drawToRightRail(ofstream &outs, railnode* p,vraildir join);

  void makeDead(){dead = 1;}
  int isDead(){return dead;}

  void setBeforeSkip(float s){beforeskip=s;}
  void setDrawToPrev(int d){drawtoprev=d;}
  float getBeforeSkip(){return beforeskip;}
  int getDrawToPrev(){return drawtoprev;}

  node* getParent(){return parent;}
  node* getNext(){return next;}
  node* getPrevious(){return previous;}
  
  // call this static member function to load the row and colum widths
  // and the node sizes before calling the place(...) function on the
  // top level (graph) node.
  static void loadData(string filename) {sizes.loadData(filename);}

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
  
  virtual int rail_left(){return 0;}
  virtual int rail_right(){return 0;}

  // start at start, lay yourself out, and return the coordinate that
  // you end at.  The "draw" argument tells whether or not to actually
  // emit tikz code.  The compiler will call this method twice for
  // every object.  The first time is just to calculate the size of
  // the object.  The second time will be to actually emit code.
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth){
    lastPlaced=this;return start;}
  
  virtual void insert(node*){}
  virtual void mergeRails(){}
    
  virtual void dump(int depth) const {
    cout << " " <<nodename <<" :";
    if(leftrail != NULL)
      cout<<" leftrail="<<((node*)leftrail)->rawName();
    if(rightrail != NULL)
      cout<<" rightrail="<<((node*)rightrail)->rawName();
    cout<<" drawtoprev="<<drawtoprev<<endl;
      //<<ea<<" "<<wa<<
      //" width="<<myWidth<<" height="<<myHeight<<" beforeskip="<<beforeskip<<
  }

  virtual string texName() { return "";};
  virtual string rawName() { return nodename;};
  float width(){return myWidth;}
  void setwidth(float w){myWidth=w;}
  float height(){return myHeight;}
  void setheight(float h){myHeight=h;}
  virtual int mergeConcats(int depth);
  virtual int liftConcats(int depth);
  //  virtual int liftOptionChoice(int depth);
  virtual int analyzeOptLoops(int depth);
  virtual int analyzeNonOptLoops(int depth);
  virtual int mergeChoices(int depth);
  virtual int numChildren(){return 0;}
  virtual node* getChild(int n){return NULL;}
  virtual int operator ==(node &r){return 0;}
  virtual int operator !=(node &r){return 1;}
  // subsume will either give back the original node pointer, or a new pointer
  virtual node* subsume(string name, node *replacement){return this;}
  virtual void forgetChild(int n){};
};


// ------------------------------------------------------------------------
// nodes with a single child
class singlenode:public node{
protected:
  node* body;
public:
  singlenode(node *p);
  singlenode(const singlenode &original):node(original)
  {
    body = original.body->clone();
    ea = body->east();
    wa = body->west();
  }
  virtual singlenode* clone() const {
    return new singlenode(*this);
  }
  virtual void forgetChild(int n){
    if(n==0)
      body=NULL;
    else
      cout<<"singlenode forgetChild bad index\n";
  }

  virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join);
  virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join);
  virtual void mergeRails(){
    body->mergeRails();
  }

  virtual ~singlenode(){if(body != NULL) delete body;}
  virtual int mergeConcats(int depth){return body->mergeConcats(depth+1);}
  virtual int liftConcats(int depth);
  virtual int mergeChoices(int depth){return body->mergeChoices(depth+1);}
  virtual int analyzeOptLoops(int depth){return body->analyzeOptLoops(depth+1);}
  virtual int analyzeNonOptLoops(int depth){return body->analyzeNonOptLoops(depth+1);}

  //  virtual int liftOptionChoice(int depth);
  virtual int numChildren(){return 1;}
  virtual node* getChild(int n){return body;}
  virtual int operator ==(node &r){
    if(same_type(r))
      return *body == *(r.getChild(0));
    return 0;
  }
  virtual int operator !=(node &r){return  !(*this == r);} // not efficient
  virtual node* subsume(string name, node *replacement){
    node* tmp;
    tmp = body->subsume(name,replacement);
    if(tmp != body)
      {
	tmp->setParent(this);
	tmp->setPrevious(body->getPrevious());
	tmp->setNext(body->getNext());
	delete body;
	body = tmp;
      }
    return this;
  }
  virtual void setParent(node* p){
    parent = p;
    body->setParent(this);
  }
  virtual void setPrevious(node *n){previous = n;body->setPrevious(n);}
  virtual void setNext(node *n) {next = n;body->setNext(n);}



};

// ------------------------------------------------------------------------

class railnode:public node{
protected:
  vrailside side;       // LEFT or RIGHT: Which side of choice/loop is it on?
  vraildir direction;  // UP or DOWN: Direction of travel down this rail.
  coordinate top,bottom;  // top is even with bottom of top child
                          // if UP, then bottom is even with top
                          // of bottom child.  Otherwise it is even with
                          // bottom of bottom child.
public:
  railnode():node(){type=RAIL;}//beforeskip=0;}
  railnode(vrailside s,vraildir d):node(){
    type=RAIL;side = s; direction = d;}
  railnode(const railnode &original):node(original){
    side=original.side;
    direction=original.direction;
    top = original.top;
    bottom = original.bottom;
  }
  virtual void setBottom(coordinate b){bottom = b;}
  virtual coordinate getBottom(){return bottom;}
  virtual railnode* clone() const {
    return new railnode(*this);
  }
  virtual void dump(int depth) const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual ~railnode(){};
  virtual int operator ==(node &r){
    if(!same_type(r))
      return 0;
    railnode &rt = dynamic_cast<railnode&>(r);
    return (side == rt.side && direction == rt.direction);
  }
  virtual int operator !=(node &r){return !(*this == r);}
  virtual vraildir getRailDir(){return direction;}
  virtual void setRailDir(vraildir d){direction = d;}

};


// ------------------------------------------------------------------------
// nodes with multiple children
class multinode:public node{
  friend class concatnode; // allow concatnode to access nodes
protected:
  vector<node*> nodes;
public:
  multinode(node *p);
  multinode(const multinode &original):node(original)
  { // clone all of the child nodes;
    for(auto i=original.nodes.begin();i!=original.nodes.end();i++)
      {
	nodes.push_back((*i)->clone());
	if(i != original.nodes.begin() &&
	   ((nodes.back()->is_choice() || nodes.back()->is_loop() ||
	     nodes.back()->is_concat()))
	   && (*(i-1))->is_rail())
	  nodes.back()->setLeftRail((railnode*)*(nodes.end()-2));
	if(i != original.nodes.begin() && nodes.back()->is_rail() &&
	   ((*(i-1))->is_choice() || (*(i-1))->is_loop() ||
	    (*(i-1))-> is_concat()))
	  (*(nodes.end()-2))->setRightRail((railnode*)nodes.back());
      }
    ea = nodes.front()->east();
    wa = nodes.front()->west();
  }

  virtual void mergeRails(){
    for(auto i=nodes.begin();i!=nodes.end();i++)
      (*i)->mergeRails();
  }
  
  virtual multinode* clone() const {
    multinode*m=new multinode(*this);
    return m;
  }
  virtual void forgetChild(int n){
    vector<node*>::iterator i=nodes.begin()+n;
    nodes.erase(i);
  }
  virtual ~multinode(){
    //for(auto i = nodes.begin();i!=nodes.end();i++)delete (*i);}
  }
  virtual void insert(node *node){
    nodes.push_back(node);
    ea = node->east();
  }
  virtual void insertFirst(node *node){
    node->setPrevious(nodes.front()->getPrevious());
    node->setNext(nodes.front());
    node->setParent(this);
    nodes.front()->setPrevious(node);
    nodes.insert(nodes.begin(),node);
    wa = node->west();
  }
  virtual int numChildren(){return nodes.size();}
  virtual node* getChild(int n){return nodes[n];}

  // virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join){
  //   for(auto i = nodes.begin();i!=nodes.end();i++)
  //     (*i)->drawToLeftRail(p, join);
  // }
  // virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  //   for(auto i = nodes.begin();i!=nodes.end();i++)
  //     (*i)->drawToRightRail(p, join);
  // }

  virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join);
  virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join);
  
  virtual int mergeConcats(int depth){
    int sum=0;
    for(auto i = nodes.begin();i!=nodes.end();i++)
      sum += (*i)->mergeConcats(depth+1);
    return sum;
  }    
  virtual int liftConcats(int depth);
  virtual int mergeChoices(int depth){
    int sum=0;
    for(auto i = nodes.begin();i!=nodes.end();i++)
      sum += (*i)->mergeChoices(depth+1);
    return sum;
  }
  virtual int analyzeOptLoops(int depth){
    int sum=0;
    for(auto i = nodes.begin();i!=nodes.end();i++)
      sum += (*i)->analyzeOptLoops(depth+1);
    return sum;
  }
  virtual int analyzeNonOptLoops(int depth){
    int sum=0;
    for(auto i = nodes.begin();i!=nodes.end();i++)
      sum += (*i)->analyzeNonOptLoops(depth+1);
    return sum;
  }
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  //  virtual int liftOptionChoice(int depth);
  virtual int operator ==(node &r);
  virtual int operator !=(node &r){return  !(*this == r);} // not efficient
  virtual node* subsume(string name, node *replacement){
    node *tmp;
    for(auto i = nodes.begin();i!=nodes.end();i++)
      {
	tmp = (*i)->subsume(name,replacement);
	if(tmp != (*i))
	  {
	    tmp->setParent(this);
	    tmp->setPrevious((*i)->getPrevious());
	    tmp->setNext((*i)->getNext());
	    tmp->setDrawToPrev((*i)->getDrawToPrev());
	    delete (*i);
	    (*i) = tmp;
	  }
      }
    return this;
  }
  virtual void setParent(node* p){
    node::setParent(p);
    for(auto i=nodes.begin(); i!=nodes.end(); i++)
      (*i)->setParent(this);
  }
  virtual void setPrevious(node* p){
    node::setPrevious(p);
    for(auto i=nodes.begin(); i!=nodes.end(); i++) {
      (*i)->setDrawToPrev(0);
      (*i)->setPrevious(NULL);
    }
  }
  virtual void setNext(node* p){
    node::setNext(p);
    for(auto i=nodes.begin(); i!=nodes.end(); i++) {
      (*i)->setDrawToPrev(0);
      (*i)->setNext(NULL);
    }
  }
};

// ------------------------------------------------------------------------

class nontermnode:public node{
protected:
  string style;
  string format;
  string str;
public:
  nontermnode(string s);
  nontermnode(const nontermnode &original):node(original)
  {
    style = original.style;
    format = original.format;
    str = original.str;
  }
  virtual nontermnode* clone() const {
    return new nontermnode(*this);
  }
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
  //  virtual int liftOptionChoice(int depth){return 0;}
  virtual int analyzeOptLoops(int depth){return 0;}
  virtual int analyzeNonOptLoops(int depth){return 0;}

  virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join);
  virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join);

  virtual int operator ==(node &r) {
    if(same_type(r)) {
	nontermnode &n=(nontermnode&) r ;
	return (style == n.style) && (format == n.format) && (str == n.str);
    }
    return 0;
  }
  virtual int operator !=(node &r){return  !(*this == r);} // not efficient
  virtual node* subsume(string name, node *replacement){
    if(name == str)
      return replacement->clone(); // return deep copy of replacement
    else
      return this;                 // or pointer to this
  }
};  


// ------------------------------------------------------------------------

class termnode:public nontermnode{
public:
  termnode(string s);
  termnode(const termnode &original):nontermnode(original){}
  virtual termnode* clone() const {
    return new termnode(*this);
  }
  virtual ~termnode(){}
};

// ------------------------------------------------------------------------
// this node type provides a placeholder to indicate that I need to
// draw a line, but there is nothing really on it.  It is a node with
// zeo width and height. When drawn, in just emits a coordinate.  Its
// north(), south(), east() and west() just refer to that coordinate.
// This simplifies the code in choicenode.place() and loopnode.place().
class nullnode:public nontermnode{
public:
  nullnode(string s);
  nullnode(const nullnode &original):nontermnode(original){}
  virtual nullnode* clone() const {
    return new nullnode(*this);
  }
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
};

// ------------------------------------------------------------------------
class newlinenode:public railnode{
  float lineheight;
  
public:
  newlinenode();
  newlinenode(const newlinenode &original):railnode(original){drawtoprev=0;
    lineheight=original.lineheight;}
  virtual newlinenode* clone() const {
    return new newlinenode(*this);
  }
  virtual ~newlinenode(){}
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int rail_left(){return 1;}
  virtual int rail_right(){return 1;}
  virtual void setLineHeight(float h){lineheight=h;}
  virtual void dump(int depth) const;
};

// ------------------------------------------------------------------------
// A rownode contains expressions that are drawn across the page.
class rownode:public singlenode{
public:
  rownode(node *p):singlenode(p){
    type=ROW;drawtoprev=0;beforeskip=sizes.colsep;p->setDrawToPrev(0);}
  rownode(const rownode &original):singlenode(original){
    drawtoprev=original.drawtoprev;}
  virtual rownode* clone() const {
    return new rownode(*this);
  }
  virtual ~rownode(){}

  virtual void dump(int depth) const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  
  // virtual void setPrevious(node *p) {
  //   body->setPrevious(getPrevious());
  // }
  // virtual void setNext(node *p) {
  //   body->setNext(getNext());
  // }

};

// ------------------------------------------------------------------------

class choicenode:public multinode{
public:
  choicenode(node *p):multinode(p){type=CHOICE;p->setDrawToPrev(0);drawtoprev=0;}
  choicenode(const choicenode &original):multinode(original){}
  virtual choicenode* clone() const {
    return new choicenode(*this);
  }
  virtual void insert(node *node){
    nodes.push_back(node);
    node->setDrawToPrev(0);
  }
  virtual ~choicenode(){}
  virtual int rail_left(){return 1;}
  virtual int rail_right(){return 1;}
  virtual void dump(int depth) const;
  virtual int mergeChoices(int depth);
};

// ------------------------------------------------------------------------

class loopnode:public multinode{
public:
  loopnode(node *node):multinode(node){
    type=LOOP;
    // initialize repeat part to nullnode
    nodes.push_back(new nullnode(nextNode()));
  }
  // should make deep copy of body
  loopnode(const loopnode &original):multinode(original) {}
  virtual loopnode* clone() const {
    return new loopnode(*this);
  }
  virtual ~loopnode(){}
  
  //  virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join);
  // virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join);

  virtual void dump(int depth) const;
  node* getRepeat(){
    return nodes[1];
  }
  void setRepeat(node *r){
    if(nodes[1] != NULL)
      delete nodes[1];
    nodes[1]=r;
  } 
  node* getBody(){
    return nodes[0];
  }
  void setBody(node *r){
    if(nodes[0] != NULL)
      delete nodes[0];
    nodes[0]=r;
  }
};

// ------------------------------------------------------------------------
  
class concatnode:public multinode{
private:
  int findAndDeleteMatches(vector<node*> &parentnodes,
			   vector<node*>::iterator &wherep,
			   vector<node*> &childnodes,
			   vector<node*>::iterator &wherec);
public:
  concatnode(node *p):multinode(p){type=CONCAT;drawtoprev=0;}//beforeskip=0;}
  concatnode(const concatnode &original):multinode(original){
    ea=nodes.back()->east();}
  virtual concatnode* clone() const {
    return new concatnode(*this);
  }
  virtual ~concatnode(){}
  
  virtual void dump(int depth) const;
  virtual void insert(node* p){
    multinode::insert(p);
    //drawtoprev = 1;
    if(p->is_null() || p->is_nonterm() || p->is_terminal())
      p->setDrawToPrev(1);
    else
      p->setDrawToPrev(0);
  }
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  virtual int rail_left(){return nodes.front()->rail_left();};
  virtual int rail_right(){return nodes.back()->rail_right();};
  virtual int mergeConcats(int depth);
  virtual int analyzeOptLoops(int depth);
  virtual int analyzeNonOptLoops(int depth);

  virtual void drawToLeftRail(ofstream &outs, railnode* p, vraildir join);
  virtual void drawToRightRail(ofstream &outs, railnode* p, vraildir join);
  virtual void mergeRails();

  virtual void setPrevious(node* p){
    node::setPrevious(p);
    for(auto i=nodes.begin()+1; i!=nodes.end(); i++)
      {
	(*i)->setPrevious(*(i-1));
	//	(*i)->setDrawToPrev(1);
      }
    nodes.front()->setPrevious(getPrevious());
    //   nodes.front()->setDrawToPrev(1);
  }
  virtual void setNext(node* p){
    node::setNext(p);
    for(auto i=nodes.begin(); i!=nodes.end()-1; i++)
      (*i)->setNext(*(i+1));
    nodes.back()->setNext(getNext());
  }
};

// ------------------------------------------------------------------------
// A productionnode contains lines, rails, and nelines
class productionnode:public singlenode{
private:
  string name;
  int subsume_spec;
public:
  productionnode(int subsumespec,string s,node *p);
  productionnode(const productionnode &original):singlenode(original)
  {
    name = original.name;
    subsume_spec = original.subsume_spec;
  }
  virtual productionnode* clone() const {
    return new productionnode(*this);
  }
  virtual ~productionnode(){}
  virtual int getSubsume(){return subsume_spec;}
  virtual string getName(){return name;}
  void optimize();
  virtual node* subsume(string name, node *replacement) {
    replacement = new concatnode(replacement->getChild(2)->getChild(0));
    return body->subsume(name,replacement);
  }
  virtual void dump(int depth) const;
  virtual coordinate place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth);
  // void setParent(node *n){parent = n;body->setParent(this);}
  // void setNext(){body->setNext(NULL);}
  // void setPrevious(){body->setPrevious(NULL);}
};

// ------------------------------------------------------------------------
// All of the productions are collected into a grammar
class grammar{
private:
  vector<productionnode*> productions;
public:
  grammar(node *p){productions.push_back((productionnode*)p);}
  ~grammar() {
    for(auto i=productions.begin();i!=productions.end();i++)
      delete (*i);
  }
  void insert(productionnode *node) {
    productions.push_back(node);
  }
  void dump() const {
    for(auto i=productions.begin();i!=productions.end();i++)
      (*i)->dump(0);
  }
  void optimize();
  void subsume();
  void place(ofstream &outs);
  void mergeRails();
  void setParent() {
    for(auto i=productions.begin();i!=productions.end();i++)
      (*i)->setParent(NULL);
  }
  void setPrevious() {
    for(auto i=productions.begin();i!=productions.end();i++)
      (*i)->setPrevious(NULL);
  }
  void setNext() {
    for(auto i=productions.begin();i!=productions.end();i++)
      (*i)->setNext(NULL);
  }
  
};





#endif


    
