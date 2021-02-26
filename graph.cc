
#include <graph.hh>
#include <cstdarg>
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <sstream>
#include <nodesize.hh>

using namespace std;


string latexwrite(string fontspec,string s);
string nextCoord();
string nextNode();
string nextChain();
string nextFit();
string stripSpecial(string s);

nodesizes node::sizes;
node* node::lastPlaced;

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

void grammar::optimize()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->optimize();
}

void grammar::mergeRails()
{
  for(auto i=productions.begin();i!=productions.end();i++)
    (*i)->mergeRails();
}

void grammar::subsume()
{
  string name;
  // look for productions that are marked for subsumption
  for(auto i=productions.begin();i!=productions.end();i++)
    if((*i)->getSubsume())
      {
	name = (*i)->getName();
	for(auto j=productions.begin();j!=productions.end();j++)
	  if(j != i)
	    {
	      //cout << "SUBSUMING\n";
	      //(*j)->dump(0);
	      (*j)->subsume(name,(*i)->getChild(0));
	      //(*j)->dump(0);
	      //cout << "-------------\n";
	    }
      }
  
}

// ------------------------------------------------------------------------
// assign node names as we create the nodes
node::node(){
  lastPlaced=NULL;
  nodename=nextNode();
  na=nodename+".north";
  sa=nodename+".south";
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
  beforeskip = sizes.colsep;
  drawtoprev = 1;
};
node::~node(){};

int node::mergeConcats(int depth)
{
  //cout<<"No implementation given for mergeConcats\n";
  return 0;
}


int node::mergeChoices(int depth)
{
  //cout<<"No implementation given for mergeChoices\n";
  return 0;
}

int node::liftConcats(int depth)
{
  //cout<<"No implementation given for liftConcats\n";
  return 0;
}

int node::liftOptionChoice(int depth)
{
  //cout<<"No implementation given for liftOptionChoices\n";
  return 0;
}

int node::analyzeLoops(int depth)
{
  // cout<<"No implementation given for analyzeLoops\n";
  return 0;
}

// ------------------------------------------------------------------------

singlenode::singlenode(node *p):node()
{
  body=p;
  na=p->north(); // not correct
  sa=p->south(); // not correct should be based on # rows in tallest item
  ea=p->east(); 
  wa=p->west();
}

int singlenode::liftConcats(int depth)
{
  int count = body->liftConcats(depth+1);
  if(body->is_concat() && body->numChildren() == 1)
    {
      node *i = body->getChild(0);
      delete body;
      body = i;
      count++;
    }
  return count;
}

// this optimization is never used, because it is handled by
// mergeChoices.
int singlenode::liftOptionChoice(int depth)
{
  int count = body->liftOptionChoice(depth+1);
  // if my child is an option and their child is a choice, then
  // lift the choice and make it an optional choice
  //  if(body->is_option() && body->getChild(0)->is_choice())
  if(body->is_choice() &&
     body->numChildren()==2 &&
     body->getChild(0)->is_null() &&
     body->getChild(1)->is_choice())
    {    
      node *i = body->getChild(1);
      // inserting a nullnode at the front makes it an optional choice
      if( ! i->getChild(0)->is_null())
	i->insert(new nullnode("ebnf2tikz NULL node"));
      body->forgetChild(1); // don't let body delete the choicenode
      delete body;
      body = i;
      count++;
    }
  return count;
}

// ------------------------------------------------------------------------

multinode::multinode(node *p):node(){
  nodes.push_back(p);
  na=p->north(); // not correct
  sa=p->south(); // not correct should be based on # rows in tallest item
  ea=p->east(); 
  wa=p->west();
  drawtoprev=0;
  //  beforeskip=0;
};

int multinode::liftConcats(int depth)
{
  int count = 0;

  for(auto i = nodes.begin(); i != nodes.end(); i++)
    count += (*i)->liftConcats(depth+1);

  
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    {
      if((*i)->is_concat() && (*i)->numChildren() == 1)
	{
	  node *child = (*i)->getChild(0);
	  delete *i;
	  *i = child;
	  count++;
	}
    }
  return count;
}

int multinode::liftOptionChoice(int depth)
{

  int count = 0;
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    count += (*i)->liftOptionChoice(depth+1);
  
  // if my child is an option and their child is a choice, then
  // lift the choice and make it an optional choice
  for(auto i = nodes.begin(); i != nodes.end(); i++)

    if((*i)->is_choice() &&
       (*i)->numChildren()==2 &&
       (*i)->getChild(0)->is_null() &&
       (*i)->getChild(1)->is_choice())

      //if((*i)->is_option() && (*i)->getChild(0)->is_choice())
      {
	choicenode *k = (choicenode*)(*i);
	choicenode *j = (choicenode*)(*i)->getChild(1);
	// inserting a NULL at the front makes it an optional choice
	if(!j->getChild(0)->is_null())
	  j->insert(new nullnode("ebnf2tikz NULL node"));
	k->forgetChild(1); // don't want (*i) to delete the choicenode
	delete *i;;
	*i = j;
	count++;
      }
  return count;
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

productionnode::productionnode(int subsumptionspec,string s,node *p):singlenode(p){
  name = s;
  subsume_spec = subsumptionspec;
  type=PRODUCTION;
}

void productionnode::dump(int depth) const
{ 
  depth=0;
  cout << "SUBSUME: "<<subsume_spec<<"\n";
  cout<<latexwrite("railname",name);
  cout << endl;
  body->dump(1);
}


void productionnode::optimize()
{
  int changes=0,tmp;
  //cout << latexwrite("emph",name) <<endl;
  do
    {
      changes = 0;
      do{
	// combine consecutive concants
	tmp = body->mergeConcats(0);
	changes += tmp;
	//cout<<tmp<<" concats merged\n";
	// if a child of a concat is a concat, lift the child
	tmp += body->liftConcats(0);
	changes += tmp;
	//cout<<tmp<<" concats lifted\n";	
      }while(tmp > 0);
      
      do{
	// if a child of a choice is a choice, merge it with the parent
        tmp = body->mergeChoices(0);
        changes += tmp;
	//cout<<tmp<<" choices merged\n";
      }while(tmp > 0);
      
      do{
	// if an option is parent to a choice, make it a choice with
	// the first child being null
        tmp = body->liftOptionChoice(0);
        changes += tmp;
	//cout<<tmp<<" optionchoices lifted\n";
      }while(tmp > 0);

      do{
	//	body->dump(0);
        tmp = body-> analyzeLoops(0);
	//cout <<"-----------------\n";
	//body->dump(0);
        changes += tmp;
	//cout<<tmp<<" loops modified\n";
      }while(tmp > 0);
      //cout <<"-----------------\n";
    } while (changes);
  
};

// ------------------------------------------------------------------------

nontermnode::nontermnode(string s):node()
{
  str = s;
  style="nonterminal";
  format="railname";
  sizes.getSize(nodename,myWidth,myHeight);
  na = nodename+".north";
  sa = nodename+".south";
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
  myHeight=1.5*sizes.rowsep;
  //myHeight=0.5*sizes.minsize;
  na = nodename;
  sa = nodename;
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

int choicenode::mergeChoices(int depth)
{
  int sum=0;
  auto i = nodes.begin();
  choicenode *ip;
  concatnode *cp;
  // do mergeChoices on all nodes beneath this one
  for(i=nodes.begin();i != nodes.end();i++)
    sum += (*i)->mergeChoices(depth+1);

  // find children that can be merged with this parent
  i = nodes.begin();
  while(i != nodes.end())
    {
      if((*i)->is_concat() && (*i)->numChildren()==3 &&
	 (*i)->getChild(0)->is_rail() &&
	 (*i)->getChild(1)->is_choice() &&
	 (*i)->getChild(2)->is_rail())
	{
	  // get pointed to node at i
	  cp = dynamic_cast<concatnode*>(*i);
	  ip = dynamic_cast<choicenode*>(cp->getChild(1));
	  // get pointer to its nodes
	  vector<node*>* inodes = &(ip->nodes);
	  // erase i and set i to its replacement
	  i = nodes.erase(i);
	  // insert nodes of i into our nodes, at position where i
	  // used to be, and update i to reference first item inserted
	  if(inodes->front()->is_null())
	    { // don't copy leading null nodes
	      i= nodes.insert(i,inodes->begin()+1,inodes->end());
	      delete inodes->front();
	    }
	  else
	    i= nodes.insert(i,inodes->begin(),inodes->end());
	  // delete the node that used to be at i
	  delete cp;
	  for(auto j = nodes.begin(); j != nodes.end(); j++)
	    {
	      if((*j)->getLeftRail() != NULL)
		(*j)->setLeftRail(leftrail);
	      if((*j)->getRightRail() != NULL)
		(*j)->setRightRail(leftrail);
	    }	  
	  
	  sum++;
	}
      else
	i++;
    }

  return sum;
}

// ------------------------------------------------------------------------

void loopnode::dump(int depth) const
{ int i;
  for(i=0;i<depth;i++)
    cout<<"  ";
  cout << "loop";
  node::dump(depth);
  for(auto j=nodes.begin();j!=nodes.end();j++)
    (*j)->dump(depth+1);
}

// ------------------------------------------------------------------------

newlinenode::newlinenode(string s):nontermnode(s)
{
  type = NEWLINE;
  style = "newline";
  format = "newline";
  nodename = nextCoord();
  lineheight=0;
  myWidth=0;
  myHeight=0;
  na = nextCoord();
  sa = nextCoord();
  ea = nodename;
  wa = nodename;
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

int concatnode::mergeConcats(int depth){
  int sum=0;
  concatnode *ip,*currp;

  auto curr = nodes.begin();
  auto i = nodes.begin();
    
  // do mergeConcats on all children of this concat
  for(i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->mergeConcats(depth+1);

  // look for pairs of children to merge
  i=nodes.begin();  
  i++;                // advance to second node in vector
  while(i!=nodes.end())
    { 
      curr = i-1;     // get iterator to node coming before i
      // if they are both concats, then we can combine them by
      // appending nodes of i to nodes of curr and erasing i
      if((*i)->is_concat() && (*curr)->is_concat())
	{
	  ip = dynamic_cast<concatnode*>(*i);  // get pointer to body of i
	  currp = dynamic_cast<concatnode*>(*curr);  // get pointer to body of curr
	  // get pointer to nodes in i
	  vector<node*>* inodes = &(ip->nodes);
	  // get pointer to nodes in curr 
	  vector<node*> *currnodes = &(currp->nodes);
	  // append inodes to currnodes
	  currnodes->insert(currnodes->end(),inodes->begin(),inodes->end());
	  // erase i from our nodes and free it
	  //	  (*curr)->setRightRail((*i)->getRightRail());
	  i = nodes.erase(i); // make sure to set i to its replacement 
	  delete ip;
	  sum++;
	}
      else
	i++;
    }

  // find children that can be merged with this parent
  i = nodes.begin();
  while(i != nodes.end())
    {
      if((*i)->is_concat())
	{
	  // get pointer to node at i ( we know it is a concat )
	  ip = (concatnode*)(*i);
	  // get pointer to its nodes
	  vector<node*>* inodes = &(ip->nodes);
	  // erase i and set i to its replacement
	  i = nodes.erase(i);
	  // insert nodes of i into our nodes, at position where i
	  // used to be, and update i to reference first item inserted
	  i = nodes.insert(i,inodes->begin(),inodes->end());
	  // delete the node that used to be at i beacuse we don't need it
	  delete ip;
	  sum++;
	}
      else
	i++;
    }

  return sum;
}


// Give reference to parent node vector, iterator to the child's
// location, and child node vector, and iterator that will be set by
// this function to reference the first mismatched node.  Matching
// nodes will be deleted from the parent, and wherep will be returned
// still referencing the location of the child.  The function returns
// the muber of nodes deleted from the parent.
int concatnode::findAndDeleteMatches(vector<node*> &parentnodes,
				     vector<node*>::iterator &wherep,
				     vector<node*> &childnodes,
				     vector<node*>::iterator &wherec)
{
  int numnodes = 0;
  wherec = childnodes.end()-1; // get iterator to last node in child nodes
  
  while(wherec != childnodes.begin() &&
	wherep != parentnodes.begin() &&
	*(*wherep) == *(*wherec))
    {
      wherep--;
      wherec--;
      numnodes++;
    }
  // The previous loop could have ended because it hit the end
  // of one list or the other. Adjust count, if needed.
  if((wherec == childnodes.begin() ||
      wherep == parentnodes.begin()) &&
     *(*wherep) == *(*wherec))
    numnodes++;
  // The previous loop either ended because of a mismatch. If it
  // was a mismatch, then move both forward one item.
  if(*(*wherep) != *(*wherec))
    {
      wherep++;
      wherec++;
    }	  
  // delete matching nodes from this concat
  for(int count=0;count<numnodes;count++) {
    delete *wherep;
    wherep = nodes.erase(wherep);
  }
  return numnodes;
}

// analyze optionloops
// nodea followed by optionnode followed by loop
int concatnode::analyzeOptLoops(int depth)
{
  int sum = 0;
  vector<node*>::iterator i, prev, j, child_last,oleft,oright;
  concatnode *child;
  loopnode *loop;
  concatnode* option;
  int numnodes;
  // do analyzeLoops on every child of this concat
  for(i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->analyzeLoops(depth+1);
  
  // find loops and try to make them better
  for(i = nodes.begin()+1;i!=nodes.end();i++) {
    // looking for an optionnode containing a loop, but not the first
    // node of this concat

    //if((*i)->is_option()  &&
    // (*i)->getChild(0)->is_loop() ) { //&&

    // An optionnode is a choicenode with exactly two children, where
    // the first child is a nullnode. We want to find optionnodes where
    // the second child is a concat containing a loop between two rails..
    if((i-1)!=nodes.begin() &&
       (*i)->is_choice()  && (*i)->numChildren() == 2 &&
       (*i)->getChild(0)->is_null()  &&  //&&
       (*i)->getChild(1)->is_concat() &&
       (*i)->getChild(1)->numChildren() == 3 &&
       (*i)->getChild(1)->getChild(0)->is_rail() &&
       (*i)->getChild(1)->getChild(1)->is_loop() &&
       (*i)->getChild(1)->getChild(2)->is_rail()) { 

      // Found an optionnode containing a loop node.  Can we rebuild it?
      loop = (loopnode*)(*i)->getChild(1)->getChild(1);
      prev = i-2;
      //      if((*prev)->is_rail())
      // prev--;

      loop->setLeftRail((railnode*)*(i-1));
      loop->getLeftRail()->setRailDir(UP);
      loop->setRightRail((railnode*)*(i+1));
      loop->getRightRail()->setRailDir(DOWN);
      
      // is loop body a concat?
      if(loop->getChild(0)->is_concat()) {
	// Loop body is a concat or a choice.  Working back from the
	// end of the child, find the first node that does not match a
	// corresponding node in this concat. call findAndDelete... to
	// do all of that and delete matching fram the parent
	child = (concatnode*)loop->getChild(0);
	numnodes = findAndDeleteMatches(nodes,prev,child->nodes,j);
	i=prev+1;
	if((*prev)->is_rail())
	  prev++;
	// If there were matching nodes, then we can move stuff around
	if(numnodes > 0) {
	  // If there is only one node left in the child concat,
	  if(j-1 == child->nodes.begin()) {
	    // then move it to the repeat part.
	    loop->setRepeat(*(j-1));
	    j = child->nodes.erase(j-1);
	  }
	  else {
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER.
	    j--;
	    concatnode *c = new concatnode(*j);
	    do
	      {
		j--;
		c->insert(*j);
		delcount++;
	      }
	    while(j!= child->nodes.begin());
	    // replace the loop repeat node with the new concat
	    loop->setRepeat(c);
	    
	    // erase the nodes that were moved from the child concat
	    for(int i=0;i<delcount+1;i++)
	      j = child->nodes.erase(j);
	  }
	  // replace the optionnode with the loop node
	  option = (concatnode*)*i;
	  *i = loop;
	  // free the option node
	  option->forgetChild(1);
	  delete option;
	}
      }
      else {
	// loop body is NOT a concat.
	// if loop body matches previous item in this concat
	if(*(loop->getChild(0)) == **prev)
	  {
	    //   erase previous item in this concat
	    delete *prev;
	    i = nodes.erase(prev);
	    //   replace the option node with the loop node
	    i++;
	    option = (concatnode*)*i;
	    *i = loop;
	    // free the option node
	    option->forgetChild(1);
	    delete option;
	  }
      }

    }
  }
  
  return sum;
}



// sequence_a followed by loop containing optional separator and sequence_a
// analyze non optionloops
int concatnode::analyzeNonOptLoops(int depth)
{
  int sum = 0;
  vector<node*>::iterator i, prev, j, child_last;
  concatnode *child;
  loopnode *loop;
  int numnodes;
  // do analyzeLoops on everything beneath this concat
  for(i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->analyzeLoops(depth+1);
  
  // find loops and try to make them better
  for(i = nodes.begin()+1;i!=nodes.end();i++) {
    // looking for a loop, but not the first node of this concat
    if((i-1)!=nodes.begin() && (*i)->is_loop() && (*i)->getChild(1)->is_null() ) {
      // Found a loop node.  Can we rebuild it?
      loop = (loopnode*)(*i);
      prev = i-2;

      loop->setLeftRail((railnode*)*(i-1));
      loop->getLeftRail()->setRailDir(UP);
      loop->setRightRail((railnode*)*(i+1));
      loop->getRightRail()->setRailDir(DOWN);
      
      // is loop body a conca?
      if(loop->getChild(0)->is_concat()) {
	// Loop body is a concat.  Working back from the end of the
	// child, find the first node that does not match a
	// corresponding node in this concat. call findAndDelete... to
	// do all of that and delete matching fram the parent
	child = (concatnode*)loop->getChild(0);
	numnodes = findAndDeleteMatches(nodes,prev,child->nodes,j);
	i=prev+1;

	// If there were matching nodes, then we can move stuff around
	if(numnodes > 0) {
	  // If there is only one node left in the child concat,
	  if(j-1 == child->nodes.begin()) {
	    // then move it to the repeat part.
	    loop->setRepeat(*(j-1));
	    j = child->nodes.erase(j-1);
	  }
	  else {
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER.
	    j--;
	    concatnode *c = new concatnode(*j);
	    do
	      {
		j--;
		c->insert(*j);
		delcount++;
	      }
	    while(j!= child->nodes.begin());

	    // replace the loop repeat node with the new concat
	    loop->setRepeat(c);
	    
	    // erase the nodes that were moved from the child concat
	    for(int i=0;i<delcount+1;i++)
	      j = child->nodes.erase(j);
	  }
	}
      }
      else {
	// loop body is NOT a concat.
	// if loop body matches previous item in this concat
	if(*(loop->getChild(0)) == **prev)
	  {
	    //   erase previous item in this concat
	    delete *prev;
	    i = nodes.erase(prev);
	  }
      }

    }
  }
  return sum;
}

int concatnode::analyzeLoops(int depth)
{
  int sum = 0;
  sum += analyzeNonOptLoops(depth);
  sum += analyzeOptLoops(depth);
  return sum;
}


void concatnode::mergeRails(){
  railnode *newrail;
  railnode *oldrail;
  node *p;
  // if(nodes[0]->is_rail() && nodes[1]->is_choice())
  //   {
  //     cout << "searching for left rail\n";
  //     oldrail = (railnode*)nodes[0];
  //     p = parent;
  //     while(p != NULL && p->getLeftRail() == NULL)
  // 	{
  // 	  cout << "checking parent "<<p<<endl;
  // 	  p = p->getParent();
  // 	}
      
  //     //newrail = findLeftRail(parent);
  //     if(p != NULL && p->getLeftRail() != NULL)
  // 	{
  // 	  newrail = p->getLeftRail();
  // 	  if(newrail->getRailDir() == oldrail->getRailDir())
  // 	    {
  // 	      cout <<"replacing left rail\n";
  // 	      nodes.erase(nodes.begin());
  // 	      nodes[0]->setLeftRail(p->getLeftRail());
  // 	    }
  // 	}
  //   }
  
  if((*(nodes.begin()))->is_rail() && (*(nodes.begin()+1))->is_choice())
    {
      oldrail = (railnode*)(*(nodes.begin()));
      newrail = parent->getLeftRail();
      if(newrail != NULL && newrail->getRailDir() == oldrail->getRailDir())
	{
	  cout <<"replacing left rail\n";
	  newrail->setBottom(oldrail->getBottom());
	  nodes.erase(nodes.begin());
	  (*(nodes.begin()))->setLeftRail(newrail);
	}
    }

  if((*(nodes.end()-1))->is_rail() && (*(nodes.end()-2))->is_choice())
    {
      oldrail = (railnode*)(*(nodes.end()-1));
      newrail = parent->getRightRail();
      if(newrail != NULL && newrail->getRailDir() == oldrail->getRailDir())
	{
	  cout <<"replacing right rail\n";
	  newrail->setBottom(oldrail->getBottom());
	  nodes.erase(nodes.end()-1);
	  (*(nodes.end()-1))->setRightRail(newrail);
	}
    }
  multinode::mergeRails();
}
