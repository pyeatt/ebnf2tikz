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

// Larry Pyeatt
#include <graph.hh>
#include <sstream>
#include <nodesize.hh>
#include <cstring>
using namespace std;


string latexwrite(string fontspec,string s);
string nextCoord();
string nextNode();
string nextChain();
string nextFit();
string stripSpecial(string s);

nodesizes* node::sizes;

string stripSpecial(string s)
{
  stringstream outs;
  for(auto i = s.begin(); i != s.end(); i++)
    {
      if(strchr("&%$#_{}~^\\",*i)==NULL)
	outs << *i;
    }
  return outs.str();
}

string latexwrite(string fontspec,string s)
{
  stringstream outs;
  outs << "\\"<<fontspec<<"{";
  for(auto i = s.begin(); i != s.end(); i++)
    {
      if(strchr("&%$#_{}~^\\",*i)!=NULL)
	outs << "\\";
      outs << *i;
    }
  if(s != "|")
    outs << "\\strut}";
  else
    outs << "}";
  return outs.str();
}

string nextCoord(){
  static int i = 0;
  string s = "coord";
  s += to_string(++i);
  return s;
}

string nextNode(){
  static int i = 0;
  string s = "node";
  s += to_string(++i);
  return s;
};

string nextFit(){
  static int i = 0;
  string s = "fit";
  s += to_string(++i);
  return s;
};

// ------------------------------------------------------------------------

// assign node names as we create the nodes
node::node(){
  type = UNKNOWN;
  nodename=nextNode();
  ea=nodename+".east";
  wa=nodename+".west";
  myWidth = 0;
  myHeight = 0;
  parent = NULL;
  previous = NULL;
  next = NULL;
  beforeskip = sizes->colsep;
  drawtoprev = 1;
  leftrail = NULL;
  rightrail = NULL;
  dead = 0;
};

node::node(const node &original){
  type = original.type;
  nodename = nextNode();;
  ea   = nodename+".east";
  wa   = nodename+".west";
  myWidth = original.myWidth;
  myHeight = original.myHeight;
  parent = NULL;
  previous = NULL;
  next = NULL;
  beforeskip = original.beforeskip;
  drawtoprev = original.drawtoprev;
  leftrail = original.leftrail;
  rightrail = original.leftrail;
  dead = 0;
}

node::~node(){};

void node::dump(int depth) const
{
  cout << " " <<nodename <<" :";
  if(leftrail != NULL)
    cout<<" leftrail="<<((node*)leftrail)->rawName();
  if(rightrail != NULL)
    cout<<" rightrail="<<((node*)rightrail)->rawName();
  cout<<" drawtoprev="<<drawtoprev<<" beforeskip="<<beforeskip<<" width="<<myWidth<< " height="<<myHeight<<" "<<ea<<" "<<wa<<endl;
      
      //
      //" 
}

// ------------------------------------------------------------------------

singlenode::singlenode(node *p):node()
{
  body=p;
  ea=p->east(); 
  wa=p->west();
}

singlenode::singlenode(const singlenode &original):node(original)
{
  body = original.body->clone();
  ea = body->east();
  wa = body->west();
  body->setParent(this); 
}

singlenode* singlenode::clone() const
{
  return new singlenode(*this);
}

void singlenode::forgetChild(int n)
{
  if(n==0)
    body=NULL;
  else
    cout<<"singlenode forgetChild bad index\n";
}

int singlenode::operator ==(node &r)
{
  if(same_type(r))
    return *body == *(r.getChild(0));
  return 0;
}

void singlenode::setParent(node* p)
{
  parent = p;
  body->setParent(this);
}

// ------------------------------------------------------------------------

multinode::multinode(const multinode &original):node(original)
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

multinode::~multinode()
{
  for(auto i = nodes.begin();i!=nodes.end();i++)
    delete (*i);
}

multinode* multinode::clone() const
{
  multinode*m=new multinode(*this);
  return m;
}

void multinode::forgetChild(int n)
{
  vector<node*>::iterator i=nodes.begin()+n;
  nodes.erase(i);
}

multinode::multinode(node *p):node(){
  nodes.push_back(p);
  if(p!=NULL)
    {
      ea=p->east(); 
      wa=p->west();
    }
  drawtoprev=0;
};

void multinode::insert(node *node)
{
  nodes.push_back(node);
  ea = node->east();
}

void multinode::insertFirst(node *node)
{
  node->setPrevious(nodes.front()->getPrevious());
  node->setNext(nodes.front());
  node->setParent(this);
  nodes.front()->setPrevious(node);
  nodes.insert(nodes.begin(),node);
  wa = node->west();
}

void multinode::setParent(node* p)
{
  node::setParent(p);
  for(auto i=nodes.begin(); i!=nodes.end(); i++)
    (*i)->setParent(this);
}

void multinode::setPrevious(node* p)
{
  node::setPrevious(p);
  for(auto i=nodes.begin(); i!=nodes.end(); i++) {
    (*i)->setDrawToPrev(0);
    (*i)->setPrevious(NULL);
  }
}

void multinode::setNext(node* p)
{
  node::setNext(p);
  for(auto i=nodes.begin(); i!=nodes.end(); i++) {
    (*i)->setDrawToPrev(0);
    (*i)->setNext(NULL);
  }
}

int multinode::operator == (node &r)
{
  if(same_type(r) && (numChildren() == r.numChildren()))
    {
      multinode rt = (multinode&)(r);
      // if any child of this is not equal to the matching child of
      // r, return 0.
      vector <node*>::iterator mychild, rchild;
      for(mychild = nodes.begin(), rchild = rt.nodes.begin();
	  mychild != nodes.end() && rchild != rt.nodes.end();
	  mychild++, rchild++)
	{
	  if(**mychild != **rchild)
	    return 0;
	}
      if((mychild == nodes.end() && rchild == rt.nodes.end()))
	return 1;  // all children match
    }
  return 0;
}

// ------------------------------------------------------------------------

rownode::rownode(node *p):singlenode(p)
{
  type=ROW;
  drawtoprev=0;
  beforeskip=sizes->colsep;
  p->setDrawToPrev(0);
}

rownode::rownode(const rownode &original):singlenode(original)
{
  drawtoprev=original.drawtoprev;
}

rownode* rownode::clone() const
{
  return new rownode(*this);
}

void rownode::dump(int depth) const
{
  int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout << "row";
  node::dump(depth);
  body->dump(depth+1);
}

// ------------------------------------------------------------------------

productionnode::productionnode(annotmap *a,string s,node *p):
  singlenode(p){
  name = s;
  type=PRODUCTION;
  subsume_spec=NULL;
  annotations=NULL;
  if(a != NULL)
    {
      annotations = a;
      string reg = (*a)["subsume"];
      if(reg != "")
	{
	  
	  if(reg == "emusbussubsume")
	    reg = name;
	  cout<<"creating regex: "<<reg<<" for "<<name<<endl;
	  subsume_spec = new regex_t;
	  if(regcomp(subsume_spec,reg.c_str(),REG_EXTENDED))
	    {
	      cerr<<"Unable to interpret regular expression '"<<reg<<"'\n";
	      exit(2);
	    }
	}
    }
}

productionnode::productionnode(const productionnode &original):
  singlenode(original)
{
  annotations = original.annotations;
  subsume_spec = original.subsume_spec;
  name = original.name;
}

productionnode* productionnode::clone() const
{
  return new productionnode(*this);
}

void productionnode::dump(int depth) const
{ 
  depth=0;
  cout << "production: "<<name<<endl;
  if(!subsume_spec)
    body->dump(1);
}

node* productionnode::createRows()
{
  if(body->is_concat())
    {
      concatnode *tmp = (concatnode*)((concatnode*)body)->createRows();
      delete body;
      body = tmp;
    }
  return NULL;
}
  

// ------------------------------------------------------------------------
loopnode::loopnode(node *node):multinode(node)
{
  type=LOOP;
  // initialize repeat part to nullnode
  nodes.push_back(new nullnode(nextNode()));
}

// should make deep copy of body
loopnode::loopnode(const loopnode &original):multinode(original)
{
}

loopnode* loopnode::clone() const
{
  return new loopnode(*this);
}

node* loopnode::getRepeat()
{
  return nodes[0];
}

void loopnode::setRepeat(node *r)
{
  if(nodes[1] != NULL)
    delete nodes[1];
  nodes[1]=r;
} 

node* loopnode::getBody()
{
  return nodes[0];
}

void loopnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout << "loop";
  node::dump(depth);
  for(auto j=nodes.begin();j!=nodes.end();j++)
    (*j)->dump(depth+1);
}

  void loopnode::setBody(node *r)
{
  if(nodes[0] != NULL)
    delete nodes[0];
  nodes[0]=r;
}

// ------------------------------------------------------------------------

nontermnode::nontermnode(string s):node()
{
  str = s;
  style="nonterminal";
  format="railname";
  sizes->getSize(nodename,myWidth,myHeight);
  ea = nodename+".east";
  wa = nodename+".west";
  type=NONTERM;
}

nontermnode::nontermnode(const nontermnode &original):node(original)
{
  style = original.style;
  format = original.format;
  str = original.str;
  sizes->getSize(nodename,myWidth,myHeight);
}

nontermnode* nontermnode::clone() const
{
  return new nontermnode(*this);
}

void nontermnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout<<latexwrite("railnonterm",str)<<" -> ";
  node::dump(depth);
}  

int nontermnode::operator ==(node &r)
{
  if(same_type(r)) {
    nontermnode &n=(nontermnode&) r ;
    return (style == n.style) && (format == n.format) && (str == n.str);
  }
  return 0;
}

// ------------------------------------------------------------------------

railnode::railnode():node()
{
  type=RAIL;
}

railnode::railnode(vrailside s,vraildir d):node()
{
  type=RAIL;
  side = s;
  direction = d;
}

railnode::railnode(const railnode &original):node(original)
{
  side=original.side;
  direction=original.direction;
  top = original.top;
  bottom = original.bottom;
}

void railnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout<<"rail ";
  cout<<" "<<vrailStr(direction);
 node::dump(depth);
  
}  

int railnode::operator ==(node &r)
{
  if(!same_type(r))
    return 0;
  railnode &rt = dynamic_cast<railnode&>(r);
  return (side == rt.side && direction == rt.direction);
}


// ------------------------------------------------------------------------
termnode::termnode(string s):nontermnode(s)
{
  str.pop_back();         // strip off trailing single ro double quote
  str.erase(str.begin()); // strip off initial single ro double quote
  style="terminal";
  format="railtermname";
  type = TERMINAL;
}

termnode::termnode(const termnode &original):nontermnode(original)
{
}

termnode* termnode::clone() const
{
  return new termnode(*this);
}


// ------------------------------------------------------------------------
nullnode::nullnode(string s):nontermnode(s)
{
  type = NULLNODE;
  style = "null";
  format = "null";
  nodename = nextCoord();
  myWidth=0;
  myHeight=1.5*sizes->rowsep;
   ea = nodename;
  wa = nodename;
}

nullnode::nullnode(const nullnode &original):nontermnode(original)
{
}

nullnode* nullnode::clone() const
{
  return new nullnode(*this);
}

// ------------------------------------------------------------------------

choicenode::choicenode(node *p):multinode(p)
{
  type = CHOICE;
}

choicenode::choicenode(const choicenode &original):multinode(original)
{
}

choicenode* choicenode::clone() const
{
  return new choicenode(*this);
}

void choicenode::insert(node *node)
{
  nodes.push_back(node);
  node->setDrawToPrev(0);
}

void choicenode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout << "choices";
  node::dump(depth);
  for(auto j=nodes.begin();j!=nodes.end();j++)
    (*j)->dump(depth+1);
}

// ------------------------------------------------------------------------

newlinenode::newlinenode():railnode()
{
  type = NEWLINE;
  nodename = nextCoord();
  ea = nodename;
  wa = nodename;
  drawtoprev = 0;
  //beforeskip = 0;
}

newlinenode::newlinenode(const newlinenode &original):
  railnode(original)
{
  drawtoprev=0;
  lineheight=original.lineheight;
}

newlinenode* newlinenode::clone() const
{
  return new newlinenode(*this);
}

void newlinenode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout<<"newline";
  node::dump(depth);
}  


// ------------------------------------------------------------------------

concatnode::concatnode(node *p):multinode(p)
{
  type=CONCAT;
  drawtoprev=0;
}

concatnode::concatnode(const concatnode &original):multinode(original)
{
  ea=nodes.back()->east();
}

concatnode* concatnode::clone() const
{
  return new concatnode(*this);
}

void concatnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout << "concat";
  node::dump(depth);
  for(auto j=nodes.begin();j!=nodes.end();j++)
    (*j)->dump(depth+1);
}

void concatnode::setPrevious(node* p)
{
  node::setPrevious(p);
  for(auto i=nodes.begin()+1; i!=nodes.end(); i++)
    (*i)->setPrevious(*(i-1));
  nodes.front()->setPrevious(getPrevious());
}
void concatnode::setNext(node* p)
{
  node::setNext(p);
  for(auto i=nodes.begin(); i!=nodes.end()-1; i++)
    (*i)->setNext(*(i+1));
  nodes.back()->setNext(getNext());
}

void concatnode::insert(node* p)
{
  multinode::insert(p);
  if(p->is_null() || p->is_nonterm() || p->is_terminal())
    p->setDrawToPrev(1);
  else
    p->setDrawToPrev(0);
}

node* concatnode::createRows()
{
  // create a replacement for ourself (we will erase the NULL later)
  concatnode *rep=new concatnode(NULL);

  rep->beforeskip = beforeskip;
  rep->drawtoprev = drawtoprev;
  rep->leftrail = leftrail;
  rep->rightrail = rightrail;
  
  while(nodes.size() > 0)
    {
      auto i = nodes.begin();
      concatnode *c = new concatnode(*i);
      i = nodes.erase(i);
      while(i != nodes.end() && !(*i)->is_newline())
	{
	  c->insert(*i);
	  i = nodes.erase(i);
	}
      rownode *r = new rownode(c);
      rep->insert(r);
      if(i!=nodes.end() && (*i)->is_newline())
	{
	  rep->insert(*i);
	  i = nodes.erase(i);
	}
    }
  rep->nodes.erase(rep->nodes.begin());
  return rep;
}

// ------------------------------------------------------------------------

grammar::~grammar()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    delete (*i);
}
  
void grammar::dump() const
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->dump(0);
}

void grammar::setParent()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->setParent(NULL);
}

void grammar::setPrevious()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->setPrevious(NULL);
}

void grammar::setNext()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->setNext(NULL);
}

void grammar::createRows()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->createRows();
}
