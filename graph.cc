
#include <graph.hh>
#include <cstdarg>
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <sstream>
#include <nodesize.hh>
#include <algorithm>    // std::find
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
    if((*i)->getSubsume()) {
      name = (*i)->getName();
      for(auto j=productions.begin();j!=productions.end();j++)
	if(j != i)
	  (*j)->subsume(name,(*i)->getChild(0));
    }
}

// ------------------------------------------------------------------------
// assign node names as we create the nodes
node::node(){
  lastPlaced=NULL;
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

int node::analyzeOptLoops(int depth)
{
  // cout<<"No implementation given for analyzeOptLoops\n";
  return 0;
}

int node::analyzeNonOptLoops(int depth)
{
  // cout<<"No implementation given for analyzeNonOptLoops\n";
  return 0;
}

// ------------------------------------------------------------------------

singlenode::singlenode(node *p):node()
{
  body=p;
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

// ------------------------------------------------------------------------

multinode::multinode(node *p):node(){
  nodes.push_back(p);
  ea=p->east(); 
  wa=p->west();
  drawtoprev=0;
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
    tmp = body-> analyzeOptLoops(0);
    cout<<tmp<<" optional loops modified\n";
    changes += tmp;
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
  sizes.getSize(nodename,myWidth,myHeight);
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

int concatnode::mergeConcats(int depth){
  int sum=0;
  concatnode *ip,*currp;
  int no_newlines=1;
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

  // check to see if this parent has any newlinenodes
  i = nodes.begin();
  while(i != nodes.end() && no_newlines)
    if((*i)->is_newline())
      no_newlines = 0;
    else
      i++;

  if(no_newlines) {
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

  //if(wherep != parentnodes.begin())
  //{
  wherec = childnodes.end()-1; // get iterator to last node in child nodes
  
  while(wherec != childnodes.begin() &&
	wherep != parentnodes.begin() &&
	*(*wherep) == *(*wherec)) {
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
  // The previous loop may have ended because of a mismatch. If
  // so, then move both forward one item.
  if(*(*wherep) != *(*wherec)) {
      wherep++;
      wherec++;
    }	  
  // delete matching nodes from this concat
  for(int count=0;count<numnodes;count++) {
    (*wherep)->makeDead();
    wherep++;
  }
  //}
  return numnodes;
}

static void moveLoop(loopnode *loop, vector<node*>::iterator i)
{
  choicenode *option = (choicenode *)*i;

  // When we delete the option node, it will delete everything under
  // it, recursively.  When the concat holding the loop is deleted, we
  // don't want it to take the loop with it.  Tell the concat to
  // forget about the loop. We have to do this now, because we are
  // about to change the loop's parent.
  loop->getParent()->forgetChild(1);	    
  // link the loop to its new parent and siblings
  loop->setParent(option->getParent());
  loop->setPrevious(option->getPrevious());
  loop->setNext(option->getNext());
  // link siblings to the loop
  if(loop->getPrevious() != NULL)
    loop->getPrevious()->setNext(loop);
  if(loop->getNext() != NULL)
    loop->getNext()->setPrevious(loop);
  // Fix up the rails
  loop->setLeftRail(option->getLeftRail());
  loop->setRightRail(option->getRightRail());
  loop->getLeftRail()->setRailDir(UP);
  loop->getRightRail()->setRailDir(DOWN);
  // replace the option node with the loop node
  *i = loop;	    
  // free the option node
  delete option;
}

// analyze optionloops
// nodea followed by optionnode followed by loop
int concatnode::analyzeOptLoops(int depth)
{
  int sum = 0;
  vector<node*>::iterator i, prev, j, child_last,oleft,oright;
  concatnode *child;
  loopnode *loop;
  concatnode *gp;
  int numnodes;
  // do analyzeLoops on every child of this concat
  for(i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->analyzeOptLoops(depth+1);
  // bury the dead
  i = nodes.begin();
  while(i!=nodes.end()) {
    if((*i)->isDead()) {
      if(i != nodes.end()-1)
	(*(i+1))->setLeftRail((*i)->getLeftRail());
      if(i != nodes.begin())
	(*(i-1))->setRightRail((*i)->getRightRail());
      delete (*i);
      i = nodes.erase(i);
    }
    else
      i++;
  }
  // find loops and try to make them better
  for(i = nodes.begin()+1;i!=nodes.end();i++) {
    // An optionnode is a choicenode with exactly two children, where
    // the first child is a nullnode. We want to find optionnodes where
    // the second child is a concat containing a loop between two rails..
    if( i !=nodes.begin() &&
	(*i)->is_choice()  && (*i)->numChildren() == 2 &&
	(*i)->getChild(0)->is_null()  &&  //&&
	(*i)->getChild(1)->is_concat() &&
	(*i)->getChild(1)->numChildren() == 3 &&
	(*i)->getChild(1)->getChild(0)->is_rail() &&
	(*i)->getChild(1)->getChild(1)->is_loop() &&
	(*i)->getChild(1)->getChild(2)->is_rail()) { 
      // Found an optionnode containing a loop node.  Can we rebuild it?
      sum++;
      // get an iterator to the node preceding "this" in its parent's nodes
      if(parent->is_concat())
	{
	  gp = (concatnode*)parent;
	  prev = find(gp->nodes.begin(),gp->nodes.end(),this);
	  prev--;  
	}
      else
	gp = NULL;
      // Get the loopnode that we are interested in;
      loop = (loopnode*)(*i)->getChild(1)->getChild(1);
      // is loop body a concat?
      if(loop->getChild(0)->is_concat()) {
	// Loop body is a concat.  Working back from the end of the
	// loop body, find the first node that does not match a
	// corresponding node in the nodes of the parent of "this".
	// Call findAndDelete to do all of that and delete matching
	// nodes the parent
	child = (concatnode*)loop->getChild(0);
	if(gp != NULL)
	  numnodes = findAndDeleteMatches(gp->nodes,prev,child->nodes,j);
	else
	  numnodes = 0;
	// If there were matching nodes, then we can move stuff around
	if(numnodes > 0) {
	  // If there is only one node left in the child concat,
	  if(j-1 == child->nodes.begin()) {
	    // then move it to the repeat part.
	    loop->nodes[1]=(*(j-1));
	    child = (concatnode*)loop->getChild(0);
	    j = child->nodes.erase(j-1);
	    // if child only has one remaining node, then replace it
	    // with its one child and delete it.
	    if(j == child->nodes.begin()) {
	      loop->setBody(child->getChild(0));
	      //child->forgetChild(0);
	      // delete child;
	    }
	  }
	// if(numnodes > 0) {
	//   // If there is only one node left in the child concat,
	//   if(j-1 == child->nodes.begin()) {
	//     // then move it to the repeat part.
	//     loop->setRepeat(*(j-1));
	//     j = child->nodes.erase(j-1);
	//   }
	  else {
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER.  But,
	    // if I find a rail, then I need to find the matching
	    // rail and keep them in order.
	    j--;
	    concatnode *c = new concatnode(*j);
	    do {
	      j--;
	      c->insert(*j);
	      delcount++;
	    } while(j!= child->nodes.begin());
	    // replace the loop repeat node with the new concat
	    loop->setRepeat(c);
	    // erase the nodes that were moved from the child concat
	    for(int i=0;i<delcount+1;i++)
	      j = child->nodes.erase(j);
	  }
	}
	else {
	  // If there is only one node left in the child concat,
	  if(child->numChildren()==1) {
	    // then swap the body and the repeat.
	    node *tmp = loop->getChild(0);
	    loop->nodes[0] = loop->getChild(1);
	    loop->nodes[1] = tmp;
	  }
	  else {
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER.  But,
	    // if I find a rail, then I need to find the matching
	    // rail and keep them in order.
	    if(gp == NULL)
	      j = (child->nodes.end()-1);
	    if(j == child->nodes.end())
	      j--;
	    concatnode *c = new concatnode(*j);
	    do
	      {
		j--;
		c->insert(*j);
		delcount++;
	      }
	    while(j!= child->nodes.begin());
	    // erase the nodes that were moved from the child concat
	    for(int k=0;k<delcount+1;k++)
	      j = child->nodes.erase(j);
	    // replace the loop repeat node with the new concat
	    loop->nodes[0] = loop->nodes[1];
	    loop->nodes[1] = c;
	  }
	}
      }
      else {
	// loop body is NOT a concat.
	// if loop body matches previous item in this concat
	if(gp != NULL && prev != gp->nodes.end() &&
	   *(loop->getChild(0)) == **prev) {
	  // mark matching node for deletion
	  (*prev)->makeDead();
	  //   replace the option node with the loop node
	}
	else {
	  // does not match previous item in this concat
	  // swap the repeat and the body
	  node *tmp = loop->getChild(0);
	  loop->nodes[0] = loop->getChild(1);
	  loop->nodes[1] = tmp;
	  //   replace the option node with the loop node
	}
      }
      moveLoop(loop,i);
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
    sum += (*i)->analyzeNonOptLoops(depth+1);
  
  // find loops and try to make them better
  for(i = nodes.begin()+1;i!=nodes.end();i++) {
    // looking for concat containing a loop
    if((*i)->is_concat() &&
       (*i)->numChildren() == 3 &&
       (*i)->getChild(0)->is_rail() &&
       (*i)->getChild(1)->is_loop() &&
       (*i)->getChild(2)->is_rail()) { 
      // Found a loop node.  Can we rebuild it?
      sum++;
      // get an iterator to the node preceding "this" in its parent's nodes
      prev = i-1;	            
      loop = (loopnode*)(*i)->getChild(1);
      // is loop body a conca?
      if(loop->getChild(0)->is_concat()) {
	// Loop body is a concat.  Working back from the end of the
	// child, find the first node that does not match a
	// corresponding node in this concat. call findAndDelete... to
	// do all of that and delete matching fram the parent
	child = (concatnode*)loop->getChild(0);
	numnodes = findAndDeleteMatches(nodes,prev,child->nodes,j);
	// If there were matching nodes, then we can move stuff around
	if(numnodes > 0) {
	  // If there is only one node left in the child concat,
	  if(j-1 == child->nodes.begin()) {
	    // then move it to the repeat part.
	    loop->nodes[1]=(*(j-1));
	    child = (concatnode*)loop->getChild(0);
	    j = child->nodes.erase(j-1);
	    // if child only has one remaining node, then replace it
	    // with its one child and delete it.
	    if(j == child->nodes.begin()) {
	      loop->setBody(child->getChild(0));
	      //child->forgetChild(0);
	      // delete child;
	    }
	  }
	  else {
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER.
	    j--;
	    concatnode *c = new concatnode(*j);
	    do {
	      j--;
	      c->insert(*j);
	      delcount++;
	    } while(j!= child->nodes.begin());
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
  
  // bury the dead
  // i = nodes.begin();
  // while(i!=nodes.end())
  //   {
  //     if((*i)->isDead())
  // 	{
  // 	  if(i != nodes.end()-1)
  // 	    (*(i+1))->setLeftRail((*i)->getLeftRail());
  // 	  if(i != nodes.begin())
  // 	    (*(i-1))->setRightRail((*i)->getRightRail());
  // 	  delete (*i);
  // 	  i = nodes.erase(i);
  // 	}
  //     else
  // 	i++;
  //   }
  return sum;
}



void concatnode::mergeRails(){
  railnode *newrail;
  railnode *oldrail;
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
	  newrail->setBottom(oldrail->getBottom());
	  nodes.erase(nodes.end()-1);
	  (*(nodes.end()-1))->setRightRail(newrail);
	}
    }

  if(parent->is_row() && parent->getLeftRail() != NULL &&
     nodes.front()->is_rail())
    {
      oldrail = (railnode*)nodes.front();
      newrail = parent->getLeftRail();
      if(oldrail->getRailDir() == UP)
	newrail->setRailDir(STARTNEWLINEUP);
      else
      	newrail->setRailDir(STARTNEWLINEDOWN);
      nodes.erase(nodes.begin());
      (*(nodes.begin()))->setLeftRail(newrail);
      setLeftRail(newrail);
    }

  if(parent->is_row() && parent->getRightRail() != NULL &&
     nodes.back()->is_rail())
    {
      oldrail = (railnode*)nodes.back();
      newrail = parent->getRightRail();
      // if(oldrail->getRailDir() == UP)
      // 	newrail->setRailDir(STARTNEWLINEUP);
      // else
      // 	newrail->setRailDir(STARTNEWLINEDOWN);
      //      newrail->setRailDir(oldrail->getRailDir());
      if(oldrail->getRailDir() == UP)
	newrail->setRailDir(STARTNEWLINEUP);
      else
      	newrail->setRailDir(STARTNEWLINEDOWN);

      newrail->setDrawToPrev(0);
      nodes.erase(nodes.end()-1);
      nodes.back()->setRightRail(newrail);
      setRightRail(newrail);
    }

  multinode::mergeRails();
}
