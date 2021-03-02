
#include <graph.hh>
#include <sstream>
#include <cstdarg>
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <nodesize.hh>

using namespace std;


// Variadic function for drawing lines
template <class ... Args>
void node::line(ofstream &outs,const int numpts, Args ... args)
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
  
  top = start + coordinate(0,-sizes.rowsep);

  if(side == LEFT)
    {
      if(direction == STARTNEWLINE)
	{
	  // left newline rails have top and bottom inverted... makes
	  // everything easier
	  top = start + coordinate(0, sizes.colsep);
	  bottom = start + coordinate(0,sizes.rowsep);
	}
      else
	{
	  top = start - coordinate(0,sizes.colsep);
	  bottom = start -  coordinate(0, sizes.rowsep);
	}
    }
  else
    {
      bottom = start - coordinate(0, sizes.colsep);
      // bottom = start - coordinate(0,previous->height());
      // bottom = bottom +
      // 	coordinate(0, sizes.colsep +
      // 		   previous->getChild(previous->numChildren()-1)->height());
    }

  if(drawrails)
    {
      outs<<"\\coordinate ("<<nodename<<") at "<<start<<";\n";
      outs<<"\\coordinate ("<<nodename+"linetop"<<") at "<<top<<";\n";
      outs<<"\\coordinate ("<<nodename+"linebottom"<<") at "<<bottom<<";\n";
      line(outs,2,nodename+"linetop",nodename+"linebottom");
      stringstream s;
      // if(side==LEFT)
      // 	{
      if(direction==DOWN)
	{
	  s<<"+west:"<<sizes.colsep<<"pt";
	  line(outs,3,nodename+"linetop",nodename,s.str());
	}
      else
	{
	  s<<"+east:"<<sizes.colsep<<"pt";
	  line(outs,3,nodename+"linetop",nodename,s.str());
	}
      // }
      // else
      // 	{
      // 	  if(direction==DOWN)
      // 	    {
      // 	      s<<"+east:"<<sizes.colsep<<"pt";
      // 	      line(outs,3,nodename+"linetop",nodename,s.str());
      // 	    }
      // 	  else
      // 	    {
      // 	      s<<"+west:"<<sizes.colsep<<"pt";
      // 	      line(outs,3,nodename+"linetop",nodename,s.str());
      // 	    }
      // 	}
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
      (*i)->dump(1);
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
  nc=body->place(outs,draw,drawrails,start,this,0);
  myWidth = body->width();
  myHeight = body->height();
  return nc;
}

// ------------------------------------------------------------------------

coordinate productionnode::place(ofstream &outs,int draw, int drawrails,
				 coordinate start,node *parent, int depth)
{
  coordinate c;
  string coord=nextCoord();
  string coord2=nextCoord();

  c = start+coordinate(0.0,-1.5*sizes.minsize);

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
      body->drawToLeftRail(outs,NULL,UP);
      body->drawToRightRail(outs,NULL,UP);
      outs<<"\\end{tikzpicture}\n";
      outs<<"}\n";
      outs<<"\\caption{No Caption.}\n";
      outs<<"\\label{No Caption.}\n";
      outs<<"\\end{figure}\n";
    }
  lastPlaced=this;
  return c;
}

// ------------------------------------------------------------------------

coordinate nontermnode::place(ofstream &outs,int draw, int drawrails,
			      coordinate start,node *parent, int depth)
{
  coordinate c;
  if(draw)
    {
      outs<<"\\node ("<<nodename<<") at "<<start<<"[anchor=west,"<<
	style<<"] {"<<texName()<<"};\n";
      outs<<"\\writenodesize{"<<nodename<<"}\n";
    }
  c.x = start.x+myWidth;
  c.y = start.y;
  lastPlaced=this;
  return c;
}

// ------------------------------------------------------------------------

coordinate nullnode::place(ofstream &outs,int draw, int drawrails,
			   coordinate start,node *parent, int depth)
{
  coordinate c;
  if(draw)
    outs<<"\\coordinate ("<<nodename<<") at "<<start<<";\n";
  lastPlaced=this;
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
      height = (*i)->height() + sizes.rowsep;
      totalheight += height;
    }

  setheight(totalheight - sizes.rowsep);
  setwidth(maxwidth);
  current = start;
  for(auto i=nodes.begin();i!=nodes.end();i++)
    {
      c = current + coordinate((maxwidth-(*i)->width())/2,0);
      (*i)->place(outs, draw, drawrails, c, this, depth+1);
      current = current - coordinate(0,sizes.rowsep+(*i)->height());
    }

  lastpoint=start + coordinate(widest->width(),0);

  ea = nodes[0]->east();
  wa = nodes[0]->west();

  lastPlaced=this;
  return lastpoint;
}

// ------------------------------------------------------------------------
 

coordinate newlinenode::place(ofstream &outs,int draw, int drawrails,
			      coordinate start,node *parent, int depth)
{


  cout << "placing newline "<<previous->getPrevious()->height() <<endl;

  myWidth = -previous->getPrevious()->width() + sizes.colsep;
  myHeight = previous->getPrevious()->height();

  // for newline nodes, top is actually the right-hand side, and
  // bottom is the left-hand side.  The previous and next will always
  // be rails.
  top = start - coordinate(0, previous->getPrevious()->height());
  bottom = coordinate(sizes.colsep,top.y);
  
  if(draw)
    {
      outs<<"\\coordinate ("<<nodename<<") at "<<start<<";\n";
      outs<<"\\coordinate ("<<nodename+"linetop"<<") at "<<top<<";\n";
      outs<<"\\coordinate ("<<nodename+"linebottom"<<") at "<<bottom<<";\n";

      line(outs,4,
	   ((railnode*)previous)->rawName()+"linebottom",
	   nodename+"linetop",
	   nodename+"linebottom",
	   ((railnode*)next)->rawName()+"linebottom");
	   
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

  if(depth==0)
    cout<< "placing concat at "<<start<<endl;
  
  for(auto j=nodes.begin();j!=nodes.end();j++)
    {      
      // place the node and update the current coordinate
      if(j != nodes.begin())
	{
	  current.x += (*j)->getBeforeSkip();
	  linewidth += (*j)->getBeforeSkip();
	}
 
      (*j)->place(outs,draw,drawrails,current,this,depth+1);
      current = current + coordinate((*j)->width(),0);
      linewidth += (*j)->width();      

      if((*j)->height() > rowheight)
	rowheight = (*j)->height();

      if((*j)->is_newline())
	{
	  current = current - coordinate(linewidth,
					 (*j)->height() + 3*sizes.rowsep - 2);

	  if(linewidth > myWidth)
	    myWidth = linewidth;
	  cout << "adding "<<rowheight<<" to myHeight\n";
	  myHeight += rowheight;
	  linewidth = 0;
	  rowheight = 0;
				       
	}

     // connect to previous node
      if(draw && (*j)->getDrawToPrev() && (*j)->getPrevious() != NULL)
	{
	  if(j==nodes.begin())
	    {
	      outs << "% drawing to previous\n";
	      line(outs,2,previous->east(),(*j)->west());
	    }
	  else
	    {
	      outs << "% drawing to previous\n";
	      line(outs,2,(*j)->getPrevious()->east(),(*j)->west());
	    }
	}
     // connect to previous node
      // cout << " pointer to previous :"<< (*j)->getPrevious()<<endl;
      // if(draw && j!=nodes.begin() &&
      // 	  (*j)->getDrawToPrev())
      // 	{
      // 	  outs << "% drawing to previous\n";
      // 	  line(outs,2,(*j)->getPrevious()->east(),(*j)->west());
      // 	}
    }
  if(linewidth > myWidth)
    myWidth = linewidth;
  myHeight += rowheight;

  cout << "width: "<<myWidth<<"  height: "<<myHeight<<endl;

  //  na=(*prev)->north();
  //  sa=(*prev)->south();
  ea=nodes.back()->east();
  wa=nodes.front()->west();
  // if(nodes.back()->is_loop())
  //   current.x -= sizes.colsep;
  return current;
}

// ------------------------------------------------------------------------

void node::drawToLeftRail(ofstream &outs, railnode* p,vraildir join){
  //cout << "default drawToLeftRail used for ";
  //dump(0);
}

void node::drawToRightRail(ofstream &outs, railnode* p,vraildir join){
  //cout << "default drawTorRghtRail used for ";
  //dump(0);
}
  
void nontermnode::drawToLeftRail(ofstream &outs,railnode* p, vraildir join){
  //cout << "basic node "<<nodename<<" drawing to left rail "<<p<<' '<<join<<"\n";
  if(p != NULL)
    {
      // stringstream s;
      // s<<"+up:"<<sizes.colsep<<"pt";
      // line(outs,3,wa,wa+"-|"+p->rawName(),s.str());
      line(outs,3,wa,wa+"-|"+p->rawName(),p->rawName()+"linetop");
    }
}

void nontermnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  //cout << "basic node "<<nodename<<" drawing to right rail "<<p<<' '<<join<<"\n";
  if(p != NULL)
    {
      // stringstream s;
      // s<<"+up:"<<sizes.colsep<<"pt";
      // line(outs,3,ea,ea+"-|"+p->rawName(),s.str());
      line(outs,3,ea,ea+"-|"+p->rawName(),p->rawName()+"linetop");
    }
}


void multinode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join){
  // if p is NULL then override with our own rails, and do straight
  // entry for first child
  if(p==NULL)
    {
      p = leftrail;
      join = leftrail->getRailDir();
      line(outs,2,nodes.front()->west(),nodes.front()->west()+"-|"+leftrail->east());
      nodes.front()->drawToLeftRail(outs,NULL,join);
    }
  else
    nodes.front()->drawToLeftRail(outs,p,join);
  // skip first child
  for(auto i = nodes.begin()+1;i!=nodes.end();i++)
    (*i)->drawToLeftRail(outs,p,join);

}

void multinode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  // if p is NULL then override with our own rails, and do straight
  // entry for first child
  if(p==NULL)
    {
      p = rightrail;
      join = rightrail->getRailDir();
      line(outs,2,nodes.front()->east(),nodes.front()->east()+"-|"+rightrail->west());
      nodes.front()->drawToRightRail(outs,NULL,join);
    }
  else
    nodes.front()->drawToRightRail(outs,p,join);
  // skip first child
  for(auto i = nodes.begin()+1;i!=nodes.end();i++)
    (*i)->drawToRightRail(outs,p,join);
}

void concatnode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join){
  // draw from first child to my rail
  if(p != NULL)
    nodes.front()->drawToLeftRail(outs,p,join);
  else
    {
      if(leftrail != NULL)
	{
	  join = leftrail->getRailDir();
	  nodes.front()->drawToLeftRail(outs,leftrail,join);
	}
    }
    // have all remaining children connect their internal rails
  for(auto i = nodes.begin()+1;i != nodes.end();i++)
    (*i)->drawToLeftRail(outs,NULL,join);
}

void concatnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  // draw from last child to my rail
  if( p!= NULL)
    nodes.back()->drawToRightRail(outs,p,join);
  else
    {
      if(rightrail != NULL)
	{
	  join = rightrail->getRailDir();
	  nodes.back()->drawToRightRail(outs,rightrail,join);
	}
    }
  for(auto i = nodes.begin();i != nodes.end();i++)
    (*i)->drawToRightRail(outs,NULL,join);
}

void singlenode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join){
  if(leftrail != NULL)
    {
      p = leftrail;
      join = leftrail->getRailDir();
    }
  body->drawToLeftRail(outs,p,join);
}

void singlenode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  if(rightrail != NULL)
    {
      p = rightrail;
      join = rightrail->getRailDir();
    }
  body->drawToRightRail(outs,p,join);
}
