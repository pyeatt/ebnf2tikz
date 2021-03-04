// Larry Pyeatt
#include <graph.hh>
#include <nodesize.hh>
using namespace std;

// ------------------------------------------------------------------------

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

node* singlenode::subsume(string name, node *replacement){
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

// ------------------------------------------------------------------------

node* multinode::subsume(string name, node *replacement){
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

// ------------------------------------------------------------------------

node* productionnode::subsume(string name, node *replacement) {
  node *tmp;
  // Production nodes always contain a concat with two leading
  // nullnodes followed by a rownode We only want to take the actual
  // production, which is the child of the third child.
  replacement = new concatnode(replacement->getChild(2)->getChild(0));
  tmp = body->subsume(name,replacement);
  replacement->forgetChild(0);
  delete replacement;
  if(tmp != body)
    {
      delete body;
      body = tmp;
    }
  return this;
}

// ------------------------------------------------------------------------

node* nontermnode::subsume(string name, node *replacement){
  if(name == str)
    return replacement->clone(); // return deep copy of replacement
  else
    return this;                 // or pointer to this
}

// ------------------------------------------------------------------------

