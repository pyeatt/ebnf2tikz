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

#include <graph.hh>
#include <nodesize.hh>
#include <algorithm> 
using namespace std;


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

int singlenode::liftConcats(int depth)
{
  int count = body->liftConcats(depth+1);
  if(body->is_concat() && body->numChildren() == 1)
    {
      node *child = body->getChild(0);
      body->forgetChild(0);
      delete body;
      body = child;
      count++;
    }
  return count;
}

// ------------------------------------------------------------------------
void multinode::mergeRails()
{
  for(auto i=nodes.begin();i!=nodes.end();i++)
    (*i)->mergeRails();
}

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
	  (*i)->forgetChild(0);
	  delete *i;
	  *i = child;
	  count++;
	}
    }
  return count;
}

int multinode::mergeConcats(int depth)
{
  int sum=0;
  for(auto i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->mergeConcats(depth+1);
  return sum;
}    

int multinode::mergeChoices(int depth)
{
  int sum=0;
  for(auto i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->mergeChoices(depth+1);
  return sum;
}

int multinode::analyzeOptLoops(int depth)
{
  int sum=0;
  for(auto i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->analyzeOptLoops(depth+1);
  return sum;
}

int multinode::analyzeNonOptLoops(int depth)
{
  int sum=0;
  for(auto i = nodes.begin();i!=nodes.end();i++)
    sum += (*i)->analyzeNonOptLoops(depth+1);
  return sum;
}

// ------------------------------------------------------------------------

void productionnode::optimize()
{
  int tmp;

  do{
    tmp = body->analyzeNonOptLoops(0);
  }while(tmp > 0);

  // do{
  //   tmp = body->analyzeOptLoops(0);
  // }while(tmp > 0);

  // do{
  //   tmp = body->mergeChoices(0);
  // }while(tmp > 0);

  // do{
  //   tmp = body->mergeConcats(0);
  //   tmp += body->liftConcats(0);
  // }while(tmp > 0);
}

// ------------------------------------------------------------------------

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
	      inodes->erase(inodes->begin());
	    }
	  else
	    i= nodes.insert(i,inodes->begin(),inodes->end());
	  // delete the node that used to be at i
	  inodes->clear();
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
	  ip->nodes.clear();
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
	    ip->nodes.clear();
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

// does not need to be a member, so just make it static.
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

static void flipChoiceRails(node *j){
  concatnode *c=(concatnode*)j;
  if(j->is_concat())
    {
      for(int i = 0; i < c->numChildren(); i++)
	if(c->getChild(i)->is_rail())
	  switch(((railnode*)c->getChild(i))->getRailDir())
	    {
	    case DOWN:
	      ((railnode*)c->getChild(i))->setRailDir(UP);
	      break;
	    case UP:
	      ((railnode*)c->getChild(i))->setRailDir(DOWN);
	      break;
	    }
    }
};


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
      //cout<<"found something to delete\n";
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

  // The previous loop may have deleted nodes. If we have only one
  // node left, then return immediately
  if(nodes.size() < 2)
    return 0;
  
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
	if(gp != NULL && child != NULL)
	  numnodes = findAndDeleteMatches(gp->nodes,prev,child->nodes,j);
	else
	  numnodes = 0;
	// If there were matching nodes, then we can move stuff around
	if(numnodes > 0) {
	  // THERE WERE MATCHING NODES
	  // If there is only one node left in the child concat,
	  if(j-1 == child->nodes.begin()) {
	    // CHILD HAS ONE REMAINING NODE
	    //cout << "CHILD HAS ONE REMAINING NODE"<<endl;
	    // then move it to the repeat part.
	    loop->setRepeat(*(j-1));
	    j = child->nodes.erase(j-1);
	  }
	  else {
	    // CHILD HAS MULTIPLE REMAINING NODES
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER.  But,
	    // if I find a rail, then I need to find the matching
	    // rail and keep them in order.
	    j--;
	    concatnode *c = new concatnode(*j);
	    flipChoiceRails(*j);
	    do {
	      j--;
	      flipChoiceRails(*j);
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
	  // THERE WERE NO MATCHING NODES
	  // If there is only one node left in the child concat,
	  if(child->numChildren()==1) {
	    // CHILD HAS ONE REMAINING NODE
	    cout << "CHILD HAS ONE REMAINING NODE"<<endl;
	    // then swap the body and the repeat.
	    node *tmp = loop->getChild(0);
	    loop->nodes[0] = loop->getChild(1);
	    loop->nodes[1] = tmp;
	  }
	  else {
	    // CHILD HAS MULTIPLE REMAINING NODES
	    int delcount = 0;
	    // If there is more than one node left in the child
	    // concat, then create a new concat node and move all of
	    // the remaining nodes into it IN REVERSE ORDER. 
	    if(gp == NULL)
	      j = (child->nodes.end()-1);
	    if(j == child->nodes.end())
	      j--;
	    concatnode *c = new concatnode(*j);
	    flipChoiceRails(*j);
	    do
	      {
		j--;
		flipChoiceRails(*j);
		c->insert(*j);
		delcount++;
	      }
	    while(j!= child->nodes.begin());
	    // erase the nodes that were moved from the child concat
	    for(int k=0;k<delcount+1;k++)
	      j = child->nodes.erase(j);
	    // replace the loop repeat node with the new concat
	    delete loop->nodes[0];
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
    
  // if our first child is a rail and second child is a loop, and our
  // parent is a row following a newline, then change the rail's
  // beforeskip to zero.
  if(nodes[0]->is_rail() &&
     nodes[1]->is_loop() &&
     parent->is_row() &&
     parent->getPrevious()->is_newline())
    nodes[0]->setBeforeSkip(0);
  
  }
  return sum;
}


// Count how many leading elements of a repeat alternative match elements
// preceding position 'before' in the parent concat, walking backward.
// For a single node, compare directly (match count 0 or 1).
// For a concat, compare leading children of the repeat against parent
// elements walking backward.
static int countMatches(node *alt, vector<node*> &parentNodes, int before)
{
  int matchCount = 0;
  int pi;
  int ci;

  if(alt->is_concat())
    {
      pi = before;
      for(ci = 0; ci < alt->numChildren() && pi >= 0; ci++)
	{
	  if(*(alt->getChild(ci)) == *(parentNodes[pi]))
	    {
	      matchCount++;
	      pi--;
	    }
	  else
	    return matchCount;
	}
    }
  else
    {
      if(before >= 0 && *alt == *(parentNodes[before]))
	matchCount = 1;
    }
  return matchCount;
}

// Analyze non-optional loops.
// Look for the pattern: ..., X, Y, rail_UP, loop(null, repeats...), rail_UP, ...
// where leading elements of each (reversed) repeat alternative match the
// elements preceding the rail. If so, extract matched elements as the loop
// body and strip them from both the parent concat and the repeat alternatives.
int concatnode::analyzeNonOptLoops(int depth)
{
  int sum = 0;
  unsigned int ri;
  loopnode *loop;
  int minMatch;
  int numAlts;
  int allZero;
  int ai;

  // recurse into all children first
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    sum += (*i)->analyzeNonOptLoops(depth+1);

  // scan for the pattern: rail_UP, loop(null, ...), rail_UP
  ri = 0;
  while(ri + 2 < nodes.size())
    {
      if(nodes[ri]->is_rail() &&
	 ((railnode*)nodes[ri])->getRailDir() == UP &&
	 nodes[ri+1]->is_loop() &&
	 nodes[ri+1]->getChild(0)->is_null() &&
	 nodes[ri+2]->is_rail() &&
	 ((railnode*)nodes[ri+2])->getRailDir() == UP &&
	 ri > 0)
	{
	  loop = (loopnode*)nodes[ri+1];
	  numAlts = loop->numChildren() - 1;

	  // Compute match count for each repeat alternative
	  int *altMatches = new int[numAlts];
	  minMatch = -1;
	  allZero = 1;
	  for(ai = 0; ai < numAlts; ai++)
	    {
	      altMatches[ai] = countMatches(loop->getChild(ai+1),
					    nodes, (int)ri - 1);
	      if(altMatches[ai] > 0)
		{
		  allZero = 0;
		  if(minMatch < 0 || altMatches[ai] < minMatch)
		    minMatch = altMatches[ai];
		}
	    }

	  if(allZero || minMatch <= 0)
	    {
	      delete[] altMatches;
	      ri++;
	      /* no transformation possible */
	    }
	  else
	    {
	      // Build the new body from the matched parent elements.
	      // The matched elements are at indices [ri-minMatch .. ri-1]
	      // in the parent concat (original, un-reversed order).
	      node *newBody;
	      if(minMatch == 1)
		{
		  newBody = nodes[ri-1]->clone();
		}
	      else
		{
		  concatnode *bodyConcat = new concatnode(nodes[ri-minMatch]->clone());
		  int bi;
		  for(bi = 1; bi < minMatch; bi++)
		    bodyConcat->insert(nodes[ri-minMatch+bi]->clone());
		  newBody = bodyConcat;
		}

	      // Strip minMatch leading elements from each repeat alternative.
	      // Only strip from alternatives that actually matched.
	      for(ai = 0; ai < numAlts; ai++)
		{
		  if(altMatches[ai] == 0)
		    /* leave non-matching alternatives unchanged */;
		  else if(loop->getChild(ai+1)->is_concat())
		    {
		      node *alt = loop->getChild(ai+1);
		      int ci;
		      for(ci = 0; ci < minMatch; ci++)
			{
			  delete alt->getChild(0);
			  alt->forgetChild(0);
			}
		      if(alt->numChildren() == 1)
			{
			  node *remaining = alt->getChild(0);
			  alt->forgetChild(0);
			  loop->nodes[ai+1] = remaining;
			  remaining->setParent(loop);
			  delete alt;
			}
		      else if(alt->numChildren() == 0)
			{
			  loop->nodes[ai+1] = new nullnode("NULL node");
			  loop->nodes[ai+1]->setParent(loop);
			  delete alt;
			}
		    }
		  else
		    {
		      // single node matched entirely; replace with null
		      delete loop->getChild(ai+1);
		      loop->nodes[ai+1] = new nullnode("NULL node");
		      loop->nodes[ai+1]->setParent(loop);
		    }
		}

	      delete[] altMatches;

	      // Move null repeat alternatives to the end so they
	      // appear at the bottom of the railroad diagram.
	      stable_partition(loop->nodes.begin() + 1,
			       loop->nodes.end(),
			       [](node *n){ return !n->is_null(); });

	      // Set the loop body (replaces the null)
	      loop->setBody(newBody);
	      newBody->setParent(loop);

	      // Remove minMatch elements from parent concat (before the rail)
	      int di;
	      for(di = 0; di < minMatch; di++)
		{
		  delete nodes[ri - minMatch];
		  nodes.erase(nodes.begin() + (ri - minMatch));
		}
	      // ri now points minMatch positions too far; adjust
	      ri -= minMatch;

	      // Clear rail pointers on the loop and its alternatives
	      // before deleting the rail nodes
	      loop->setLeftRail(NULL);
	      loop->setRightRail(NULL);
	      for(ai = 0; ai < loop->numChildren(); ai++)
		{
		  loop->getChild(ai)->setLeftRail(NULL);
		  loop->getChild(ai)->setRightRail(NULL);
		}

	      // Remove the two rail nodes
	      delete nodes[ri];       // left rail
	      nodes.erase(nodes.begin() + ri);
	      // now nodes[ri] is the loop, nodes[ri+1] is right rail
	      delete nodes[ri+1];     // right rail
	      nodes.erase(nodes.begin() + ri + 1);

	      sum++;
	      // don't advance ri — re-check in case of cascaded patterns
	    }
	}
      else
	{
	  ri++;
	}
    }

  return sum;
}

void concatnode::mergeRails(){
  railnode *newrail;
  railnode *oldrail;
  // We can merge rails if this concat begins with a rail followed by
  // a choice or loop, and it is one of the children inside a choice
  // or loop with matching left rail.
  if((*(nodes.begin()))->is_rail() &&
     ((*(nodes.begin()+1))->is_choice() || (*(nodes.begin()+1))->is_loop()))
    {
      oldrail = (railnode*)(*(nodes.begin()));
      newrail = parent->getLeftRail();
      if(newrail != NULL && newrail->getRailDir() == oldrail->getRailDir())
	{
	  delete nodes.front();
	  nodes.erase(nodes.begin());
	  (*(nodes.begin()))->setLeftRail(newrail);
	}
    }
  // We can merge rails if this concat ends with a a choice or loop
  // followed by a rail, and it is one of the children inside a choice
  // or loop with matching right rail.
  if((*(nodes.end()-1))->is_rail() &&
     ((*(nodes.end()-2))->is_choice() || (*(nodes.end()-2))->is_loop()))
    {
      oldrail = (railnode*)(*(nodes.end()-1));
      newrail = parent->getRightRail();
      if(newrail != NULL && newrail->getRailDir() == oldrail->getRailDir())
	{
	  delete nodes.back();
	  nodes.erase(nodes.end()-1);
	  (*(nodes.end()-1))->setRightRail(newrail);
	}
    }
  multinode::mergeRails();
}
