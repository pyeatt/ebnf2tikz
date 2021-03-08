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
#include <sstream>
#include <nodesize.hh>

using namespace std;


// Variadic function for drawing lines
template <class ... Args>
void node::line(ofstream &outs, Args ... args)
{
  vector<string> list = {args...};
  if(list.size() > 1)
    {
      auto i = list.begin();
      outs<<"\\draw [rounded corners=\\railcorners] ";
      while(i != list.end())
	{
	  while((*i)[0] == '+')
	    {
	      outs << '+';
	      (*i).erase(0,1);
	    }
	  outs << '('<<(*i);
	  outs << ')';
	  i++;
	  if(i != list.end())
	    outs<<" -- ";
	}
      outs << ";\n";
    }
}


// ------------------------------------------------------------------------
coordinate railnode::place(ofstream &outs, int draw, int drawrails,
			   coordinate start,node *parent, int depth)
{
  
  top = start + coordinate(0,-sizes->rowsep);

  if(side == LEFT)
    {
      if(previous != NULL && previous->is_newline())
      {  
	// left newline rails have top and bottom inverted... makes
	// everything easier
	top = start + coordinate(0, sizes->colsep);
	bottom = start +  coordinate(0, sizes->rowsep);
      }
      else
       	{
	  top = start - coordinate(0,sizes->colsep);
	  bottom = start -  coordinate(0, sizes->rowsep);
	}
    }
  else
    if(next != NULL && next->is_newline())
      {
	top = start - coordinate(0,sizes->rowsep);
	bottom = start - coordinate(0, sizes->colsep);
      }
    else
      {
	top = start - coordinate(0,sizes->rowsep);
	bottom = start - coordinate(0, sizes->colsep);
      }
  
  if(drawrails)
    {
      outs<<"\\coordinate ("<<nodename<<") at "<<start<<";\n";
      outs<<"\\coordinate ("<<nodename+"linetop"<<") at "<<top<<";\n";
      outs<<"\\coordinate ("<<nodename+"linebottom"<<") at "<<bottom<<";\n";
    }
  return start;
}

// ------------------------------------------------------------------------


// start at start, lay yourself out, and return the coordinat that
// you end at.  The "draw" argument tells whether or not to actually
// emit code.  The compiler will call this method twice.  The first
// time is just to calculate the size of this object.  The second
// time will be to actually emit code.


// ------------------------------------------------------------------------
void grammar::place(ofstream &outs)
{
  for(auto i=productions.begin();i!=productions.end();i++)
    {
      // place everything
      (*i)->place(outs, 0, 0, coordinate(0,0), NULL, 0);
      // draw rails
      (*i)->place(outs, 0, 1, coordinate(0,0), NULL, 0);
      // draw everything else
      (*i)->place(outs, 1, 0, coordinate(0,0), NULL, 0);
    }
  
}

// ------------------------------------------------------------------------

coordinate rownode::place(ofstream &outs,int draw, int drawrails,
			  coordinate start,node *parent, int depth)
{
  coordinate nc;
  string childwest;
  if(!body->is_concat())
    {
      nc=start + coordinate(sizes->colsep,0);
      nc = body->place(outs,draw,drawrails,nc,this,depth+1);
      myWidth = body->width() + sizes->colsep;
    }
  else
    {
      nc=body->place(outs,draw,drawrails,start,this,depth+1);
      myWidth = body->width();
    }
  myHeight = body->height();
  if(previous->is_newline() && drawrails)
    {
      // output a coordinate for the newline to attach to, and draw an
      // entry line
      outs<<"% drawn by rownode "<<nodename<<endl;
      coordname = nextCoord();
      coordinate attach = start - coordinate(0,-sizes->colsep);
      wa = coordname;
      outs<<"\\coordinate ("<<coordname<<") at "<<attach<<";\n";
      
    }
  if(previous->is_newline() && draw)
    {
      outs<<"% drawn by rownode "<<nodename<<endl;
      if(body->is_concat()  && body->getChild(0)->is_rail())
	childwest = body->getChild(1)->west();
      else
	childwest = body->west();
      line(outs,childwest,childwest+"-|"+coordname,coordname);
    }
  ea = body->east();
  return nc + coordinate(sizes->colsep,0);
}

// ------------------------------------------------------------------------

coordinate productionnode::place(ofstream &outs,int draw, int drawrails,
				 coordinate start,node *parent, int depth)
{
  coordinate c;
  string coord=nextCoord();
  string coord2=nextCoord();

  c = start+coordinate(0.0,-1.5*sizes->minsize);

  if(drawrails)
    {
      outs<<"\n\\begin{figure}\n";
      outs<<"\\centerline{\n";
      outs<<"\\begin{tikzpicture}\n";
      outs<<"\\node at "<<start<<"[anchor=west](name){";
      outs << latexwrite("railname",name);
      outs<<"};\n";
    }
  c=body->place(outs,draw,drawrails,c,this,0);
  if(draw)
    {
      body->drawToLeftRail(outs,NULL,UP,1);
      body->drawToRightRail(outs,NULL,UP,1);
      outs<<"\\end{tikzpicture}\n";
      outs<<"}\n";
      outs<<"\\caption{No Caption.}\n";
      outs<<"\\label{No Caption.}\n";
      outs<<"\\end{figure}\n";
    }
  //  lastPlaced=this;
  return c;
}

// ------------------------------------------------------------------------

coordinate nontermnode::place(ofstream &outs,int draw, int drawrails,
			      coordinate start,node *parent, int depth)
{
  coordinate c;
  if(drawrails)
    {
      outs<<"\\node ("<<nodename<<") at "<<start<<"[anchor=west,"<<
	style<<"] {"<<texName()<<"};\n";
      outs<<"\\writenodesize{"<<nodename<<"}\n";
    }
  c.x = start.x+myWidth;
  c.y = start.y;
  //  lastPlaced=this;
  return c;
}

// ------------------------------------------------------------------------

coordinate nullnode::place(ofstream &outs,int draw, int drawrails,
			   coordinate start,node *parent, int depth)
{
  coordinate c;
  if(drawrails)
    outs<<"\\coordinate ("<<nodename<<") at "<<start<<";\n";
  //  lastPlaced=this;
  return start;
}


// ------------------------------------------------------------------------
// first tells if it is the first node in a list. parent can be used
// to find what type of list. prevPT is the label of the point that
// this should connect to (above or to right depending on the list
// type).  If first node in the list is NULL, then the first option
// is to choose none, so we draw a line straight through.
coordinate multinode::place(ofstream &outs,int draw, int drawrails,
			    coordinate start, node *parent, int depth)
{
  float curwidth,maxwidth=0,height,totalheight=0;
  coordinate lastpoint,c,current=start;
  node *widest;
  
  // find the widest object
  for(auto i=nodes.begin();i!=nodes.end();i++)
    {
      // make them all calculate width and height
      (*i)->place(outs, 0, 0, current, this, depth+1);
      curwidth = (*i)->width();
      // keep the maximum width
      if(curwidth > maxwidth)
	{
	  maxwidth = curwidth;
	  widest = (*i);
	}
      height = (*i)->height() + sizes->rowsep;
      totalheight += height;
    }

  setheight(totalheight - sizes->rowsep);
  setwidth(maxwidth);
  current = start;
  for(auto i=nodes.begin();i!=nodes.end();i++)
    {
      c = current + coordinate((maxwidth-(*i)->width())/2,0);
      (*i)->place(outs, draw, drawrails, c, this, depth+1);
      current = current - coordinate(0,sizes->rowsep+(*i)->height());
    }

  lastpoint=start + coordinate(widest->width(),0);

  ea = nodes[0]->east();
  wa = nodes[0]->west();

  //  lastPlaced=this;
  return lastpoint;
}

// ------------------------------------------------------------------------
 

coordinate newlinenode::place(ofstream &outs,int draw, int drawrails,
			      coordinate start,node *parent, int depth)
{
  myWidth = -previous->width(); // + sizes->colsep;
  myHeight = previous->height();
  // for newline nodes, top is actually the right-hand side, and
  // bottom is the left-hand side.  The previous and next will always
  // be rails.
  top = start - coordinate(0, previous->getPrevious()->height());
  bottom = start - coordinate(0,myHeight);
  if(drawrails) {
    outs<<"% drawn by newlinenode "<<nodename<<endl;

    outs<<"\\coordinate ("<<nodename<<") at "<<start<<";\n";
    outs<<"\\coordinate ("<<nodename+"linetop"<<") at "<<top<<";\n";
    outs<<"\\coordinate ("<<nodename+"linebottom"<<") at "<<bottom<<";\n";
  }
  if(draw)
    {
      outs<<"% drawn by newlinenode "<<nodename<<endl;
      stringstream s;
      s<<"+right:"<<0.5*sizes->colsep<<"pt";
      line(outs,previous->east(),
	   previous->east()+"-|"+nodename,
	   nodename+"linebottom",
	   nodename+"linebottom-|"+next->west(),
	   next->west());
    }
  return bottom;
}

// ------------------------------------------------------------------------

coordinate concatnode::place(ofstream &outs,int draw, int drawrails,
			     coordinate start, node *parent, int depth)
{
  // go through all of the nodes and place them, then draw the lines
  // between them.
  float linewidth = 0, rowheight = 0;;
  coordinate current = start;
  myWidth = 0;
  myHeight = 0;  
  for(auto j=nodes.begin();j!=nodes.end();j++) {      

    // place the node and update the current coordinate
    //if(j != nodes.begin()) {
    // cout<<"placing "<<(*j)->rawName()<<" with beforeskip="<<(*j)->getBeforeSkip()<<endl;
    
      current.x += (*j)->getBeforeSkip();
      linewidth += (*j)->getBeforeSkip();
      //}
    
    (*j)->place(outs,draw,drawrails,current,this,depth+1);

    current = current + coordinate((*j)->width(),0);


    linewidth += (*j)->width();      
    if((*j)->height() > rowheight)
      rowheight = (*j)->height();

    if((*j)->is_newline()) {
      //linewidth -= sizes->colsep;
    // adjust line starting position  to account for the newline coming in
      current = current - coordinate(linewidth,
				     (*j)->height() + 3*sizes->rowsep - 2);
      // current = current - coordinate(0,
      // 				     (*j)->height() + 3*sizes->rowsep - 2);

      // current = coordinate(sizes->colsep,current.y);
			   

      
      if(linewidth > myWidth)
	myWidth = linewidth;
      myHeight += rowheight;
      linewidth = 0;
      rowheight = 0;
    }
    
    // connect to previous node
    if(draw)
      {
	if(j==nodes.begin())
	  {
	    if(previous != NULL && drawtoprev)
	      {
		outs<<"% drawn by concatnode "<<nodename<<endl;
		line(outs,previous->east(),(*j)->west());
	      }
	  }
	else
	  if((*j)->getDrawToPrev() && (*j)->getPrevious() != NULL)
	    {
		outs<<"% drawn by concatnode "<<nodename<<endl;
	      line(outs,(*j)->getPrevious()->east(),(*j)->west());
	    }
      }
  }
    
  if(linewidth > myWidth)
    myWidth = linewidth;
  myHeight += rowheight;
  auto i = nodes.begin();
  while(i != nodes.end()-1 && (*i)->is_rail())
    i++;
  wa=(*i)->west();
  i = nodes.end()-1;
  while(i != nodes.begin() && (*i)->is_rail())
    i--;
  ea=(*i)->east();

  return current;
}

// ------------------------------------------------------------------------

void node::drawToLeftRail(ofstream &outs, railnode* p,vraildir join,
			  int drawself){
  //cout << "default drawToLeftRail used for ";
  //dump(0);
}

void node::drawToRightRail(ofstream &outs, railnode* p,vraildir join,
			  int drawself){
  //cout << "default drawTorRghtRail used for ";
  //dump(0);
}
  
void nontermnode::drawToLeftRail(ofstream &outs,railnode* p, vraildir join,
			  int drawself){
  if(p != NULL && drawself && !parent->is_concat())
    {
      stringstream s;
      if(join == UP)
	s<<"+down:"<<sizes->colsep<<"pt";
      else
	s<<"+up:"<<sizes->colsep<<"pt";
      line(outs,wa,wa+"-|"+p->rawName(),s.str());
    }
}

void nontermnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){

  if(p != NULL && drawself && !parent->is_concat())
    {
      stringstream s;
      if(join == UP)
	s<<"+up:"<<sizes->colsep<<"pt";
      else
	s<<"+down:"<<sizes->colsep<<"pt";
      line(outs,ea,ea+"-|"+p->rawName(),s.str());
    }
}



void choicenode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  // if p is NULL then override with our own rails
  stringstream s,s2;
  if(p==NULL)
    {
      p = leftrail;
      if(p != NULL)
	join = p->getRailDir();
    }

  if(drawself)
    {
      if(p->getRailDir()==UP)
	line(outs,nodes.back()->west(),
	       nodes.back()->west()+"-|"+p->rawName(),p->rawName()+"linetop");
      else
      //TODO: make it so that only the last child of the last child will
      //actually draw to the top of the rail.
      if(parent->is_concat() &&
	 this == parent->getChild(1) &&
	 parent->getParent() != NULL &&
	 parent->getParent()->is_row() &&
	 parent->getParent()->getPrevious() != NULL &&
	 parent->getParent()->getPrevious()->is_newline())
	{ // if this choice follows newline, draw first child's rail entry up
	  s<<"+up:"<<sizes->colsep<<"pt";
	  line(outs,nodes.front()->west(),p->rawName(),s.str());
	  // and draw last child up into the newline
	  line(outs,nodes.back()->west(),
	       nodes.back()->west()+"-|"+p->rawName(),p->rawName()+"linetop");
	}
      else
	{
	  if(parent->is_concat() &&
	     this == parent->getChild(0) &&
	     parent->getParent() != NULL &&
	     parent->getParent()->is_choice())
	    { // if this is in a concat contained in a choice, draw first
	      //  child's rail entry up
	      line(outs,nodes.front()->west(),
		   nodes.front()->west()+"-|"+p->rawName(),p->rawName()+"linetop");
	      // draw last child to linetop of the rail
	      line(outs,nodes.back()->west(),
		   nodes.back()->west()+"-|"+p->rawName(),p->rawName()+"linetop");
	    }
	  else
	    { 
	      // otherwise, use straight rail entry
	      s<<"+left:"<<0.5*sizes->colsep<<"pt";
	      line(outs,nodes.front()->west(),p->rawName(),s.str());
	      line(outs,nodes.back()->west(),
		   nodes.back()->west()+"-|"+p->rawName(),p->rawName(),s.str());
	    }
	}
    }
  
  // skip first and last child
  for(auto i = nodes.begin()+1;i!=nodes.end()-1;i++)
    (*i)->drawToLeftRail(outs,p,join,1);
  
  if(nodes.back()->is_concat())
    nodes.back()->drawToLeftRail(outs,p,join,0);
  if(nodes.front()->is_concat())
    nodes.front()->drawToLeftRail(outs,p,join,0);
  
}

void loopnode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  // if p is NULL then override with our own rails
  stringstream s,s2;
  if(p==NULL)
    {
      p = leftrail;
      if(p != NULL)
	join = p->getRailDir();
    }

  if(drawself)
    {
      //TODO: make it so that only the last child of the last child will
      //actually draw to the top of the rail.
      if(parent->is_concat() &&
	 this == parent->getChild(1) &&
	 parent->getParent() != NULL &&
	 parent->getParent()->is_row() &&
	 parent->getParent()->getPrevious() != NULL &&
	 parent->getParent()->getPrevious()->is_newline())
	{ // if this choice follows newline, draw first child's rail entry up
	  s<<"+up:"<<sizes->colsep<<"pt";
	  line(outs,nodes.front()->west(),p->rawName(),s.str());
	}
      else
	{
	  if(parent->is_concat() &&
	     this == parent->getChild(0) &&
	     parent->getParent() != NULL &&
	     parent->getParent()->is_choice())
	    { // if this is in a concat contained in a choice, draw first
	      //  child's rail entry up
	      line(outs,nodes.front()->west(),
		   nodes.front()->west()+"-|"+p->rawName(),p->rawName()+"linetop");
	    }
	  else
	    { 
	      // otherwise, use straight rail entry
	      s<<"+left:"<<0.5*sizes->colsep<<"pt";
	      line(outs,nodes.front()->west(),p->rawName(),s.str());
	    }
	}
    }
  
  line(outs,nodes.back()->west(),
       nodes.back()->west()+"-|"+p->rawName(),
       p->rawName()+"|-"+nodes.front()->west(),
       nodes.front()->west());
  
  nodes.back()->drawToLeftRail(outs,p,join,0);
  nodes.front()->drawToLeftRail(outs,p,join,0);
}



void concatnode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself)
{
  stringstream s;
  // draw from first child to my rail
  if(p==NULL)
    {
      p = leftrail;
      if(p != NULL)
	join = p->getRailDir();
    }
  if(drawself && p != NULL)
    {
      if(parent != NULL && parent->is_choice() && this != parent->getChild(0))
	{ // if this is concat contained in a choice, draw first
	  //  child's rail entry up
	  line(outs,nodes.front()->west(),
	       nodes.front()->west()+"-|"+p->rawName(),p->rawName()+"linetop");
	  
	}
      else
       	{
       	  s<<"+left:"<<0.5*sizes->colsep<<"pt";
 	  line(outs,nodes.front()->west(),p->rawName(),s.str());
	}
    }
  
  nodes.front()->drawToLeftRail(outs,p,join,1);

    // have all remaining children connect their internal rails
  for(auto i = nodes.begin()+1;i != nodes.end();i++)
    (*i)->drawToLeftRail(outs,NULL,join,1);

  // if(nodes.back()->is_concat())
  //   nodes.back()->drawToLeftRail(outs,p,join,0);
  // if(nodes.front()->is_concat())
  //   nodes.front()->drawToLeftRail(outs,p,join,0);

}

void choicenode::drawToRightRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  stringstream s;
  // if p is NULL then override with our own rails, and do straight
  // entry for first child
  if(p==NULL)
    {
      p = rightrail;
      if(p != NULL)
	join = rightrail->getRailDir();
    }

  
  //if(next != NULL && next->is_newline())
  if(parent->is_concat() &&
     this == parent->getChild(1) &&
     parent->getParent() != NULL &&
     parent->getParent()->is_row() &&
     parent->getParent()->getNext() != NULL &&
     parent->getParent()->getNext()->is_newline())
    { // if this choice precedes newline, draw first child's rail entry down
      s<<"+down:"<<sizes->colsep<<"pt";
      line(outs,nodes.front()->east(),p->rawName(),s.str());
      //      line(outs,nodes.front()->east(),p->rawName(),s.str());
      rightrail->setRailDir(DOWN);
      join = DOWN;
      line(outs,nodes.back()->east(),
	   nodes.back()->east()+"-|"+p->rawName(),
	   p->rawName()+"|-"+nodes.front()->east(),
	  s.str());
    }
  else
    {
      if(parent->is_concat() &&
	 this == parent->getChild(parent->numChildren()-1) &&
	 parent->getParent() != NULL &&
	 parent->getParent()->is_choice())
	{ // if this is in a concat contained in a choice, draw first
	  //  child's rail entry up
	  line(outs,nodes.front()->east(),
	       nodes.front()->east()+"-|"+p->rawName(),p->rawName()+"linetop");
	  // draw last child to linetop of the rail
	      line(outs,nodes.back()->east(),
		   nodes.back()->east()+"-|"+p->rawName(),p->rawName()+"linetop");
	}
      else
	{
	  join=UP;
	  rightrail->setRailDir(UP);
	  line(outs,nodes.front()->east(),rightrail->west());
	  s<<"+right:"<<0.5*sizes->colsep<<"pt";
	  line(outs,nodes.back()->east(),
	       nodes.back()->east()+"-|"+p->rawName(),
	       p->rawName()+"|-"+nodes.front()->east(),
	       s.str());
	}
    }
  
  // skip first and last child.  They are cannected it this function,
  // and their methods are called separately so that they will not try
  // to connect themselves, but will only fill in their children.
  for(auto i = nodes.begin()+1;i!=nodes.end()-1;i++)
    (*i)->drawToRightRail(outs,p,join,1);
  
  if(nodes.back()->is_concat())
    nodes.back()->drawToRightRail(outs,p,join,0);
  if(nodes.front()->is_concat())
    nodes.front()->drawToRightRail(outs,p,join,0);
  
}

void loopnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  // if p is NULL then override with our own rails, and do straight
  // entry for first child
  stringstream s,s2;
  if(p==NULL)
    {
      p = rightrail;
      if(p != NULL)
	join = p->getRailDir();
    }

  if(drawself)
    {
      //TODO: make it so that only the last child of the last child will
      //actually draw to the top of the rail.
      if(parent->is_concat() &&
	 this == parent->getChild(1) &&
	 parent->getParent() != NULL &&
	 parent->getParent()->is_row() &&
	 parent->getParent()->getNext() != NULL &&
	 parent->getParent()->getNext()->is_newline())
	{ // if this choice pre newline, draw first child's rail entry up
	  //s<<"+down:"<<sizes->colsep<<"pt";
	  //line(outs,nodes.front()->east(),p->rawName(),s.str());
	}
      else
	{
	  if(parent->is_concat() &&
	     this == parent->getChild(0) &&
	     parent->getParent() != NULL &&
	     parent->getParent()->is_choice())
	    { // if this is in a concat contained in a choice, draw first
	      //  child's rail entry up
	      //line(outs,nodes.front()->east(),
	      //   nodes.front()->east()+"-|"+p->rawName(),p->rawName()+"linetop");
	    }
	  else
	    { 
	      // otherwise, use straight rail entry
	      s<<"+right:"<<0.5*sizes->colsep<<"pt";
	      line(outs,nodes.front()->east(),p->rawName(),s.str());
	    }
	}
	
      line(outs,nodes.back()->east(),
	   nodes.back()->east()+"-|"+p->rawName(),
	   p->rawName()+"|-"+nodes.front()->east(),
	   nodes.front()->east());
    }
  
  nodes.back()->drawToRightRail(outs,p,join,0);
  nodes.front()->drawToRightRail(outs,p,join,0);

  
}

void concatnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  stringstream s;
  if(p==NULL)
    {
      p = rightrail;
      if(p != NULL)
	join = p->getRailDir();
    }

  if(drawself && p != NULL)
    {
      if(join == DOWN)
	{
	  s<<"+down:"<<sizes->colsep<<"pt";
	  line(outs,nodes.back()->east(),
	       nodes.back()->east()+"-|"+p->rawName(),s.str());
	}
      else
	if(parent != NULL && parent->is_choice() && this != parent->getChild(0))
	  { // if this is concat contained in a choice, draw last
	    //  child's rail entry up, unless join type is DOWN
	    line(outs,nodes.back()->east(),
		 nodes.back()->east()+"-|"+p->rawName(),p->rawName()+"linetop");
	    
	  }
	else
	  {
	    s<<"+right:"<<0.5*sizes->colsep<<"pt";
	    line(outs,nodes.back()->east(),p->rawName(),s.str());
	  }
    }
  
  nodes.back()->drawToRightRail(outs,p,join,0);

    // have all remaining children connect their internal rails
  for(auto i = nodes.begin();i != nodes.end()-1;i++)
    (*i)->drawToRightRail(outs,NULL,join,1);
}

void singlenode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  if(leftrail != NULL)
    {
      p = leftrail;
      join = leftrail->getRailDir();
    }
  body->drawToLeftRail(outs,p,join,1);
}

void singlenode::drawToRightRail(ofstream &outs, railnode* p, vraildir join,
			  int drawself){
  if(rightrail != NULL)
    {
      p = rightrail;
      join = rightrail->getRailDir();
    }
  body->drawToRightRail(outs,p,join,1);
}


