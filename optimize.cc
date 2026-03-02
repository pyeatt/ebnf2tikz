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

int node::flattenLoopChoices()
{
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

int multinode::flattenLoopChoices()
{
  int sum = 0;
  int ni;

  // Recurse into all children first
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    sum += (*i)->flattenLoopChoices();

  // For loops: flatten choice repeat children into direct alternatives
  if(is_loop())
    {
      ni = 1;
      while(ni < numChildren())
	{
	  if(getChild(ni)->is_choice())
	    {
	      node *ch = getChild(ni);
	      int nch = ch->numChildren();
	      int ci;
	      forgetChild(ni);
	      for(ci = nch - 1; ci >= 0; ci--)
		{
		  node *alt = ch->getChild(ci);
		  ch->forgetChild(ci);
		  alt->setParent(this);
		  nodes.insert(nodes.begin() + ni, alt);
		}
	      delete ch;
	      ni += nch;
	      sum++;
	    }
	  else
	    {
	      ni++;
	    }
	}

      // Remove null repeat alternatives only when at least
      // one non-null repeat exists.
      {
	int hasNonNull = 0;
	for(ni = 1; ni < numChildren(); ni++)
	  {
	    if(!getChild(ni)->is_null())
	      hasNonNull = 1;
	  }
	if(hasNonNull)
	  {
	    ni = 1;
	    while(ni < numChildren())
	      {
		if(getChild(ni)->is_null())
		  {
		    delete getChild(ni);
		    forgetChild(ni);
		    sum++;
		  }
		else
		  {
		    ni++;
		  }
	      }
	  }
      }
    }

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
    tmp += body->mergeConcats(0);
    tmp += body->liftConcats(0);
    tmp += body->analyzeOptLoops(0);
    tmp += body->flattenLoopChoices();
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

// Replace all references to oldrail with newrail throughout a subtree.
// Handles the parser.yy bug where leftrail may actually point to the
// right rail, so we check both leftrail and rightrail against oldrail.
static void replaceRailRecursive(node *n, railnode *oldrail, railnode *newrail)
{
  int ci;

  if(n->getLeftRail() == oldrail)
    n->setLeftRail(newrail);
  if(n->getRightRail() == oldrail)
    n->setRightRail(newrail);
  for(ci = 0; ci < n->numChildren(); ci++)
    replaceRailRecursive(n->getChild(ci), oldrail, newrail);
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
		      else if(alt->numChildren() == 3 &&
			      alt->getChild(0)->is_rail() &&
			      (alt->getChild(1)->is_choice() ||
			       alt->getChild(1)->is_loop()) &&
			      alt->getChild(2)->is_rail())
			{
			  node *inner = alt->getChild(1);
			  clearRailsRecursive(inner,
					      alt->getChild(0),
					      alt->getChild(2));
			  alt->forgetChild(1);
			  delete alt->getChild(0);
			  alt->forgetChild(0);
			  delete alt->getChild(0);
			  alt->forgetChild(0);
			  delete alt;
			  loop->nodes[ai+1] = inner;
			  inner->setParent(loop);
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

	  // Clear the loop's internal rail pointers (orphaned from
	  // the parser) and delete the corresponding rail nodes.
	  {
	    node *loopLR = loop->getLeftRail();
	    node *loopRR = loop->getRightRail();
	    clearRailsRecursive(loop, loopLR, loopRR);
	  }

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

	  // Keep the outer rail nodes and wire them to the loop
	  loop->setLeftRail((railnode*)nodes[ri]);
	  loop->setRightRail((railnode*)nodes[ri+2]);

	  // Flatten bare choice nodes among repeat children into
	  // direct loop alternatives (choiceloop form).
	  {
	    int ni = 1;
	    while(ni < loop->numChildren())
	      {
		if(loop->getChild(ni)->is_choice())
		  {
		    node *ch = loop->getChild(ni);
		    int nch = ch->numChildren();
		    int ci;
		    // Detach all children from the choice
		    loop->forgetChild(ni);
		    for(ci = nch - 1; ci >= 0; ci--)
		      {
			node *alt = ch->getChild(ci);
			ch->forgetChild(ci);
			alt->setParent(loop);
			loop->nodes.insert(loop->nodes.begin() + ni, alt);
		      }
		    delete ch;
		    ni += nch;
		  }
		else
		  {
		    ni++;
		  }
	      }

	    // Remove null repeat alternatives only when at least
	    // one non-null repeat exists — the non-null alternatives
	    // already provide feedback paths, so the null is redundant.
	    // A loop whose only repeat is null needs it for feedback.
	    {
	      int hasNonNull = 0;
	      ni = 1;
	      while(ni < loop->numChildren())
		{
		  if(!loop->getChild(ni)->is_null())
		    hasNonNull = 1;
		  ni++;
		}
	      if(hasNonNull)
		{
		  ni = 1;
		  while(ni < loop->numChildren())
		    {
		      if(loop->getChild(ni)->is_null())
			{
			  delete loop->getChild(ni);
			  loop->forgetChild(ni);
			}
		      else
			{
			  ni++;
			}
		    }
		}
	    }
	  }

	  // When the loop body is a bare choice and the only repeat
	  // is null, convert to choiceloop form: body becomes null,
	  // choice children become repeat alternatives.  This is safe
	  // inside analyzeOptLoops because the outer optional already
	  // provides the zero-repetitions bypass.
	  if(loop->getChild(0)->is_choice() &&
	     loop->numChildren() == 2 &&
	     loop->getChild(1)->is_null())
	    {
	      node *bodyChoice = loop->getChild(0);
	      node *nullRepeat = loop->getChild(1);
	      int nch = bodyChoice->numChildren();
	      int ci;

	      // Remove old body and null repeat
	      loop->forgetChild(1);
	      loop->forgetChild(0);

	      // New null body
	      node *newBody = new nullnode("NULL node");
	      newBody->setParent(loop);
	      loop->nodes.insert(loop->nodes.begin(), newBody);

	      // Add choice children as repeat alternatives
	      for(ci = 0; ci < nch; ci++)
		{
		  node *alt = bodyChoice->getChild(0);
		  bodyChoice->forgetChild(0);
		  alt->setParent(loop);
		  loop->nodes.push_back(alt);
		}

	      delete bodyChoice;
	      delete nullRepeat;
	    }

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
// elements walking backward.  Stop at rail nodes — they are structural
// markers for choice/loop parents and should not be matched.
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
	  if(alt->getChild(ci)->is_rail())
	    return matchCount;
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
      if(before >= 0 && !alt->is_rail() &&
	 *alt == *(parentNodes[before]))
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

	  // Compute match count for each repeat alternative.
	  // All alternatives must match for the transformation
	  // to be semantics-preserving.
	  int *altMatches = new int[numAlts];
	  minMatch = -1;
	  allZero = 1;
	  int anyZero = 0;
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
	      else
		{
		  anyZero = 1;
		}
	    }

	  if(allZero || anyZero || minMatch <= 0)
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
		      else if(alt->numChildren() == 3 &&
			      alt->getChild(0)->is_rail() &&
			      (alt->getChild(1)->is_choice() ||
			       alt->getChild(1)->is_loop()) &&
			      alt->getChild(2)->is_rail())
			{
			  // Strip redundant rail, choice/loop, rail
			  // wrapper — the loop provides its own rails.
			  node *inner = alt->getChild(1);
			  clearRailsRecursive(inner,
					      alt->getChild(0),
					      alt->getChild(2));
			  alt->forgetChild(1);
			  delete alt->getChild(0);
			  alt->forgetChild(0);
			  delete alt->getChild(0);
			  alt->forgetChild(0);
			  delete alt;
			  loop->nodes[ai+1] = inner;
			  inner->setParent(loop);
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

	      // Clear stale rail pointers in the loop subtree, then
	      // re-wire the outer rails to the loop.
	      clearRailsRecursive(loop, nodes[ri], nodes[ri+2]);
	      loop->setLeftRail((railnode*)nodes[ri]);
	      loop->setRightRail((railnode*)nodes[ri+2]);

	      // Flatten bare choice nodes among repeat children into
	      // direct loop alternatives (choiceloop form).
	      {
		int ni = 1;
		while(ni < loop->numChildren())
		  {
		    if(loop->getChild(ni)->is_choice())
		      {
			node *ch = loop->getChild(ni);
			int nch = ch->numChildren();
			int ci;
			loop->forgetChild(ni);
			for(ci = nch - 1; ci >= 0; ci--)
			  {
			    node *alt = ch->getChild(ci);
			    ch->forgetChild(ci);
			    alt->setParent(loop);
			    loop->nodes.insert(loop->nodes.begin() + ni, alt);
			  }
			delete ch;
			ni += nch;
		      }
		    else
		      {
			ni++;
		      }
		  }

	      }

	      // When the only repeat is null, convert to choiceloop
	      // form if the loop is inside an optional wrapper
	      // (choice with null bypass).  Move the body to become
	      // a repeat alternative (or flatten it if it is a
	      // choice) and set body to null.
	      if(loop->numChildren() == 2 &&
		 loop->getChild(1)->is_null())
		{
		  // Check whether this concat is inside an optional
		  int isOptional = 0;
		  node *par = parent;
		  if(par != NULL && par->is_choice())
		    {
		      int pi;
		      for(pi = 0; pi < par->numChildren(); pi++)
			{
			  if(par->getChild(pi)->is_null())
			    {
			      isOptional = 1;
			      pi = par->numChildren();
			    }
			}
		    }
		  if(isOptional)
		    {
		      node *oldBody = loop->getChild(0);
		      node *nullRepeat = loop->getChild(1);

		      loop->forgetChild(1);
		      loop->forgetChild(0);

		      node *newNullBody = new nullnode("NULL node");
		      newNullBody->setParent(loop);
		      loop->nodes.insert(loop->nodes.begin(), newNullBody);

		      if(oldBody->is_choice())
			{
			  int nch = oldBody->numChildren();
			  int ci;
			  for(ci = 0; ci < nch; ci++)
			    {
			      node *alt = oldBody->getChild(0);
			      oldBody->forgetChild(0);
			      alt->setParent(loop);
			      loop->nodes.push_back(alt);
			    }
			  delete oldBody;
			}
		      else
			{
			  oldBody->setParent(loop);
			  loop->nodes.push_back(oldBody);
			}

		      delete nullRepeat;

		      // The loop's rails were the inner concat's
		      // flanking rails.  Delete them so the concat
		      // collapses to a single child, then liftConcats
		      // and analyzeOptLoops can finish the job.
		      clearRailsRecursive(loop, nodes[ri], nodes[ri+2]);
		      loop->setLeftRail(NULL);
		      loop->setRightRail(NULL);
		      delete nodes[ri+2];
		      nodes.erase(nodes.begin() + ri + 2);
		      delete nodes[ri];
		      nodes.erase(nodes.begin() + ri);
		    }
		}

	      // Remove null repeat alternatives only when at least
	      // one non-null repeat exists.
	      {
		int ni = 1;
		int hasNonNull = 0;
		while(ni < loop->numChildren())
		  {
		    if(!loop->getChild(ni)->is_null())
		      hasNonNull = 1;
		    ni++;
		  }
		if(hasNonNull)
		  {
		    ni = 1;
		    while(ni < loop->numChildren())
		      {
			if(loop->getChild(ni)->is_null())
			  {
			    delete loop->getChild(ni);
			    loop->forgetChild(ni);
			  }
			else
			  {
			    ni++;
			  }
		      }
		  }
	      }

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
	  replaceRailRecursive(*(nodes.begin()+1), oldrail, newrail);
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
	  replaceRailRecursive(*(nodes.end()-2), oldrail, newrail);
	  delete nodes.back();
	  nodes.erase(nodes.end()-1);
	  (*(nodes.end()-1))->setRightRail(newrail);
	}
    }
  multinode::mergeRails();
}
