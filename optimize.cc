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
    tmp = body->mergeChoices(0);
  }while(tmp > 0);

  do{
    tmp = body->mergeConcats(0);
    tmp += body->liftConcats(0);
  }while(tmp > 0);

  do{
    tmp = body->analyzeNonOptLoops(0);
  }while(tmp > 0);

  do{
    tmp = body->analyzeOptLoops(0);
  }while(tmp > 0);
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
		(*j)->setRightRail(rightrail);
	    }
	  
	  sum++;
	}
      else if((*i)->is_choice())
	{
	  ip = dynamic_cast<choicenode*>(*i);
	  vector<node*>* inodes = &(ip->nodes);
	  i = nodes.erase(i);
	  if(inodes->front()->is_null())
	    {
	      i = nodes.insert(i, inodes->begin()+1, inodes->end());
	      delete inodes->front();
	      inodes->erase(inodes->begin());
	    }
	  else
	    i = nodes.insert(i, inodes->begin(), inodes->end());
	  inodes->clear();
	  delete ip;
	  for(auto j = nodes.begin(); j != nodes.end(); j++)
	    {
	      if((*j)->getLeftRail() != NULL)
		(*j)->setLeftRail(leftrail);
	      if((*j)->getRightRail() != NULL)
		(*j)->setRightRail(rightrail);
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


static void clearRailsRecursive(node *n, node *lr, node *rr)
{
  int ci;

  if(lr == NULL || n->getLeftRail() == lr || n->getLeftRail() == rr)
    n->setLeftRail(NULL);
  if(rr == NULL || n->getRightRail() == rr || n->getRightRail() == lr)
    n->setRightRail(NULL);
  for(ci = 0; ci < n->numChildren(); ci++)
    clearRailsRecursive(n->getChild(ci), lr, rr);
}

static int countMatches(node *alt, vector<node*> &parentNodes, int before);

// Analyze optional loops.
// Look for the pattern: ..., rail_UP, choice(null, loop(null, ...)), rail_UP, ...
// The choice wrapping a loop with null body is redundant (a loop with null
// body already represents zero-or-more). Unwrap the choice, and if leading
// elements of each (reversed) repeat alternative match elements preceding
// the rail, extract them as the loop body (one-or-more form).
int concatnode::analyzeOptLoops(int depth)
{
  int sum = 0;
  unsigned int ri;
  loopnode *loop;
  choicenode *choice;
  int minMatch;
  int numAlts;
  int allZero;
  int ai;

  // recurse into all children first
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    sum += (*i)->analyzeOptLoops(depth+1);

  // scan for the pattern: rail_UP, choice(null, loop(null,...)), rail_UP
  ri = 0;
  while(ri + 2 < nodes.size())
    {
      if(nodes[ri]->is_rail() &&
	 ((railnode*)nodes[ri])->getRailDir() == UP &&
	 nodes[ri+1]->is_choice() &&
	 nodes[ri+1]->numChildren() == 2 &&
	 nodes[ri+1]->getChild(0)->is_null() &&
	 nodes[ri+1]->getChild(1)->is_loop() &&
	 nodes[ri+1]->getChild(1)->getChild(0)->is_null() &&
	 nodes[ri+2]->is_rail() &&
	 ((railnode*)nodes[ri+2])->getRailDir() == UP)
	{
	  choice = (choicenode*)nodes[ri+1];
	  loop = (loopnode*)choice->getChild(1);
	  numAlts = loop->numChildren() - 1;

	  // Extract the loop from the choice before we delete it.
	  choice->forgetChild(1);

	  // Apply matching logic (same as analyzeNonOptLoops)
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

	  if(!allZero && minMatch > 0)
	    {
	      // Build the new body from matched parent elements.
	      // Elements at indices [ri-minMatch .. ri-1].
	      node *newBody;
	      if(minMatch == 1)
		{
		  newBody = nodes[ri-1]->clone();
		}
	      else
		{
		  concatnode *bodyConcat =
		    new concatnode(nodes[ri-minMatch]->clone());
		  int bi;
		  for(bi = 1; bi < minMatch; bi++)
		    bodyConcat->insert(nodes[ri-minMatch+bi]->clone());
		  newBody = bodyConcat;
		}
	      // Cloned nodes may have stale rail pointers from the
	      // originals; clear them all.
	      clearRailsRecursive(newBody, NULL, NULL);

	      // Strip minMatch leading elements from each matching
	      // repeat alternative.
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

	      // Move null repeat alternatives to the end
	      stable_partition(loop->nodes.begin() + 1,
			       loop->nodes.end(),
			       [](node *n){ return !n->is_null(); });

	      // Set the loop body (replaces the null)
	      loop->setBody(newBody);
	      newBody->setParent(loop);

	      // Remove minMatch elements from parent concat
	      int di;
	      for(di = 0; di < minMatch; di++)
		{
		  delete nodes[ri - minMatch];
		  nodes.erase(nodes.begin() + (ri - minMatch));
		}
	      ri -= minMatch;
	    }

	  delete[] altMatches;

	  // Capture all rail pointers that will be deleted, then
	  // recursively clear any references to them in the loop subtree.
	  {
	    node *loopLR = loop->getLeftRail();
	    node *loopRR = loop->getRightRail();
	    node *outerLR = nodes[ri];
	    node *outerRR = nodes[ri+2];
	    clearRailsRecursive(loop, loopLR, loopRR);
	    clearRailsRecursive(loop, outerLR, outerRR);
	  }

	  // Delete the loop's internal leftrail/rightrail (orphaned
	  // rail nodes from the parser, not in any concat).
	  if(loop->getLeftRail() != NULL)
	    {
	      delete loop->getLeftRail();
	      loop->setLeftRail(NULL);
	    }
	  if(loop->getRightRail() != NULL)
	    {
	      delete loop->getRightRail();
	      loop->setRightRail(NULL);
	    }

	  // Delete the choice node (now has only the null child)
	  delete choice;

	  // Replace choice position with the loop in parent concat
	  nodes[ri+1] = loop;
	  loop->setParent(this);

	  // Delete the two choice rail nodes from parent concat
	  delete nodes[ri];            // left rail
	  nodes.erase(nodes.begin() + ri);
	  // now nodes[ri] is the loop, nodes[ri+1] is right rail
	  delete nodes[ri+1];          // right rail
	  nodes.erase(nodes.begin() + ri + 1);

	  sum++;
	  // don't advance ri — re-check in case of cascaded patterns
	}
      else
	{
	  ri++;
	}
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
	      // Cloned nodes may have stale rail pointers from the
	      // originals; clear them all.
	      clearRailsRecursive(newBody, NULL, NULL);

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

	      // Recursively clear rail pointers matching the rails
	      // that are about to be deleted
	      clearRailsRecursive(loop, nodes[ri], nodes[ri+2]);

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
