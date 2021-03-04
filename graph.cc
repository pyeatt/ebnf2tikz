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
//node* node::lastPlaced;

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
  //lastPlaced=NULL;
  nodename=nextNode();
  ea=nodename+".east";
  wa=nodename+".west";
  parent = NULL;
  previous = NULL;
  next = NULL;
  leftrail = NULL;
  rightrail = NULL;
  type = UNKNOWN;
  myWidth = 0;
  myHeight = 0;
  beforeskip = sizes->colsep;
  drawtoprev = 1;
  dead = 0;
};
node::~node(){};


// ------------------------------------------------------------------------

singlenode::singlenode(node *p):node()
{
  body=p;
  ea=p->east(); 
  wa=p->west();
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



multinode::multinode(node *p):node(){
  nodes.push_back(p);
  ea=p->east(); 
  wa=p->west();
  drawtoprev=0;
};


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

productionnode::productionnode(int subsumptionspec,string s,node *p):singlenode(p){
  name = s;
  subsume_spec = subsumptionspec;
  type=PRODUCTION;
}

void productionnode::dump(int depth) const
{ 
  depth=0;
  cout << "production: "<<name<<endl;
  body->dump(1);
}


void productionnode::optimize()
{
  int changes=0,tmp;
  //  do {
    changes = 0;
    tmp = body-> analyzeNonOptLoops(0);
    cout <<"-----------------\n";
    cout << latexwrite("emph",name) <<endl;
    cout<<tmp<<" non-optional loops modified\n";
    changes += tmp;
    //}while(tmp > 0);

    
    //  do{
    // tmp = body-> analyzeOptLoops(0);
    // cout<<tmp<<" optional loops modified\n";
    // changes += tmp;
    //  }while(tmp > 0);
      
  do{
    // if a child of a choice is a choice, merge it with the parent
    tmp = body->mergeChoices(0);
    changes += tmp;
    cout<<tmp<<" choices merged\n";
  }while(tmp > 0);
      
  do{
    // combine consecutive concants
    tmp = body->mergeConcats(0);
    changes += tmp;
    cout<<tmp<<" concats merged\n";
    // if a child of a concat is a concat, lift the child
    tmp += body->liftConcats(0);
    changes += tmp;
    cout<<tmp<<" concats lifted\n";	
  }while(tmp > 0);
  
cout <<"-----------------\n";
};

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

void nontermnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout<<latexwrite("railnonterm",str)<<" -> ";
  node::dump(depth);
}  

// ------------------------------------------------------------------------

void railnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout<<"rail";
  node::dump(depth);
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

// ------------------------------------------------------------------------
nullnode::nullnode(string s):nontermnode(s)
{
  type = NULLNODE;
  style = "null";
  format = "null";
  nodename = nextCoord();
  myWidth=0;
  //myHeight=
  myHeight=1.5*sizes->rowsep;
  //myHeight=0.5*sizes->minsize;
  ea = nodename;
  wa = nodename;
}

// ------------------------------------------------------------------------

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
  
  // lineheight=0;
  // myWidth=0;
  // myHeight=0;
  ea = nodename;
  wa = nodename;
  drawtoprev = 0;
  beforeskip = 0;
}

void newlinenode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout<<"newline";
  node::dump(depth);
}  


// ------------------------------------------------------------------------

void concatnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout << "concat";
  node::dump(depth);
  for(auto j=nodes.begin();j!=nodes.end();j++)
    (*j)->dump(depth+1);
}

