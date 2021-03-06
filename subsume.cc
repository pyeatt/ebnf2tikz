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
#include <nodesize.hh>
using namespace std;

// ------------------------------------------------------------------------

void grammar::subsume()
{
  regex_t *name;
  // look for productions that are marked for subsumption
  for(auto i=productions.begin();i!=productions.end();i++)
    if((name = (*i)->getSubsume()) != NULL) {
      for(auto j=productions.begin();j!=productions.end();j++)
	if(*j != *i)
	  (*j)->subsume(name,(*i)->getChild(0));
    }
}

// ------------------------------------------------------------------------

node* singlenode::subsume(regex_t* name, node *replacement){
  node* tmp;
  tmp = body->subsume(name,replacement);
  if(tmp != body)
    {
      tmp->liftConcats(0);
      tmp->setParent(this);
      tmp->setPrevious(body->getPrevious());
      tmp->setNext(body->getNext());
      delete body;
      body = tmp;
    }
  return this;
}

// ------------------------------------------------------------------------

node* multinode::subsume(regex_t* name, node *replacement){
  node *tmp;
  for(auto i = nodes.begin();i!=nodes.end();i++)
    {
      tmp = (*i)->subsume(name,replacement);
      if(tmp != (*i))
	{
	  tmp->liftConcats(0);
	  tmp->setParent(this);
	  tmp->setPrevious((*i)->getPrevious());
	  tmp->setNext((*i)->getNext());
	  tmp->setDrawToPrev((*i)->getDrawToPrev());
	  tmp->setBeforeSkip(0);
	  delete (*i);
	  (*i) = tmp;
	}
    }
    return this;
}

// ------------------------------------------------------------------------

node* productionnode::subsume(regex_t* name, node *replacement) {
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
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

node* nontermnode::subsume(regex_t* name, node *replacement){
  regmatch_t  pmatch[1];
  if(!regexec(name, str.c_str(), ARRAY_SIZE(pmatch), pmatch, 0))
    return replacement->clone(); // return deep copy of replacement
  else
    return this;                 // or pointer to this
}

// ------------------------------------------------------------------------

void singlenode::fixSkips()
{
  body->fixSkips();
}


void multinode::fixSkips()
{
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    (*i)->fixSkips();
}

void choicenode::fixSkips()
{
  multinode::fixSkips();
  
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    (*i)->setBeforeSkip(0);
  // if the loop follows a newline, then our rail must have a
  // beforeskip check the loops rail.  If its beforeskip is zero, then
  // our rail needs a beforeskip.
  if(parent != NULL &&
     parent->is_concat() &&
     parent->numChildren()==3 &&
     this == parent->getChild(1) &&
     parent->getParent() != NULL &&
     (parent->getParent()->is_choice() || parent->getParent()->is_loop()) &&
     parent->getParent()->getLeftRail() != NULL &&
     parent->getParent()->getLeftRail()->getBeforeSkip() > 0)
    previous->setBeforeSkip(0);
}

void loopnode::fixSkips()
{
  multinode::fixSkips();
  
  for(auto i = nodes.begin(); i != nodes.end(); i++)
    (*i)->setBeforeSkip(0);

  if(parent != NULL &&
     parent->is_concat() &&
     parent->numChildren()==3 &&
     this == parent->getChild(1) &&
     parent->getParent() != NULL &&
     (parent->getParent()->is_choice() || parent->getParent()->is_loop()) &&
     parent->getParent()->getLeftRail() != NULL &&
     parent->getParent()->getLeftRail()->getBeforeSkip() > 0)
    previous->setBeforeSkip(0);
}


void concatnode::fixSkips()
{
  multinode::fixSkips();

  node *gp=NULL,*ggp=NULL;
  if(parent != NULL)
    gp = parent->getParent();
  if(gp != NULL)
    ggp = gp->getParent();
  
  if(parent != NULL && (parent->is_loop() || parent->is_choice()) &&
     gp != NULL && gp->is_concat() && ggp != NULL && ggp->is_row() &&
     (ggp->getPrevious() == NULL || !ggp->getPrevious()->is_newline()))
    {
      setBeforeSkip(0);
      nodes[0]->setBeforeSkip(0);
    }

  if(parent != NULL && (parent->is_loop() || parent->is_choice()) &&
     gp != NULL && gp->is_concat() && ggp != NULL &&
     (ggp->is_loop() || ggp->is_choice()))
    {
      setBeforeSkip(0);
      nodes[0]->setBeforeSkip(0);
    }

}
