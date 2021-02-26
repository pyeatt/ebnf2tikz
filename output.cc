
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
      bottom = start - coordinate(0,next->height());
      bottom = bottom +
	coordinate(0, sizes.colsep + 
		   next->getChild(next->numChildren()-1)->height());
    }
  else
    {
      bottom = start - coordinate(0,previous->height());
      bottom = bottom +
	coordinate(0, sizes.colsep +
		   previous->getChild(previous->numChildren()-1)->height());
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
	  //line(outs,2,nodename,s.str());
	}
      else
	{
	  s<<"+east:"<<sizes.colsep<<"pt";
	  line(outs,3,nodename+"linetop",nodename,s.str());
	  //line(outs,2,nodename,s.str());
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
      // draw rails
      (*i)->place(outs, 0, 1, coordinate(0,0), NULL, 0);
      // draw everything else
      (*i)->place(outs, 1, 0, coordinate(0,0), NULL, 0);
    }
}

// ------------------------------------------------------------------------

coordinate productionnode::place(ofstream &outs,int draw, int drawrails,
				 coordinate start,node *parent, int depth)
{
  coordinate nc;
  coordinate c;
  float length;
  string coord=nextCoord();
  string coord2=nextCoord();

  nullnode *null1, *null2;
  
  c = start+coordinate(0.0,-1.5*sizes.minsize);
  if(body->is_choice())
    length = sizes.colsep;
  else
    length = 2*sizes.colsep;
  nc = c + coordinate(length,0);

  if(drawrails)
    {
      outs<<"\n\\begin{figure}\n";
      outs<<"\\centerline{\n";
      outs<<"\\begin{tikzpicture}\n";
      outs<<"\\node at "<<start<<"[anchor=west](name){";
      outs << latexwrite("railname",name);
      outs<<"};\n";
    }
  nc=body->place(outs,draw,drawrails,nc,this,0);
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
  // na = nodename+".north";
  // sa = nodename+".south";
  // ea = nodename+".east";
  // wa = nodename+".west";
  lastPlaced=this;
  return nc;
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
  coordinate nc,c2,c3,c4,c5,sc;
  string coord1,coord2,coord3,coord4;
  // place coordinates
  coord1=nextCoord();
  coord2=nextCoord();
  coord3=nextCoord();
  coord4=nextCoord();
  nc = start;
  // if(rightfancy==NONE)
  //   c2 = start + coordinate(sizes.colsep,0);
  // else
  //   c2 = start + coordinate(sizes.colsep,0);
  
  // if(getPrevious() != NULL && getPrevious()->rail_right())
  //   c3 = c2 - coordinate(0,sizes.minsize);
  // else
  //   c3 = c2 - coordinate(0,lineheight);

  // c4 = coordinate(0,c3.y);
  // c5 = c4 - coordinate(0,sizes.minsize+2);
  // sc = c5 + coordinate(sizes.colsep,0);
  // if(draw)
  //   {
  //     if(rightfancy==NONE)
  // 	outs<<"\\coordinate ("<<na<<") at "<<nc<<";\n";
  //     outs<<"\\coordinate ("<<coord1<<") at "<<c2<<";\n";
  //     outs<<"\\coordinate ("<<coord2<<") at "<<c3<<";\n";
  //     outs<<"\\coordinate ("<<coord3<<") at "<<c4<<";\n";
  //     outs<<"\\coordinate ("<<coord4<<") at "<<c5<<";\n";
  //     if(leftfancy==NONE) 
  // 	outs<<"\\coordinate ("<<sa<<") at "<<sc<<";\n";

  //     if(rightfancy == NONE && leftfancy==NONE)
  // 	line(outs,6,ea,coord1,coord2,coord3,coord4,sa);
  //     else
  // 	if(rightfancy == NONE)
  // 	  line(outs,5,na,coord1,coord2,coord3,coord4);
  // 	else
  // 	  if(leftfancy == NONE)
  // 	    line(outs,5,coord1,coord2,coord3,coord4,sa);
  // 	  else
  // 	    line(outs,4,coord1,coord2,coord3,coord4);
  //   }
  //ea=na=coord2;
  //wa=sa=coord4;

  return sc;
}

// ------------------------------------------------------------------------

coordinate concatnode::place(ofstream &outs,int draw, int drawrails,
			     coordinate start, node *parent, int depth)
{
  // go through all of the nodes and place them, then draw the lines
  // between them.
  coordinate current = start;
  float rowheight=0,totalheight=0;
  float mywidth=0;
  
  auto prev=nodes.begin();
  for(auto j=nodes.begin();j!=nodes.end();j++)
    {      
      // place the node and update the current coordinate
      if(j != nodes.begin())
	current.x += (*j)->getBeforeSkip();
 
      (*j)->place(outs,draw,drawrails,current,this,depth+1);
      current = current + coordinate((*j)->width(),0);
      
      mywidth += (*j)->width();
      
      if(j != nodes.begin())
	mywidth += (*j)->getBeforeSkip();

      if(draw && j!=prev)
	line(outs,2,(*prev)->east(),(*j)->west());
      
      if((*j)->height() > rowheight)
	rowheight = (*j)->height();
      
      prev=j;
    }  

  totalheight += rowheight;
  // update our width, north, south, east, and west anchors
  setheight(totalheight);

  setwidth(mywidth);
  
  na=(*prev)->north();
  sa=(*prev)->south();
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
  // if we have our own rails, override with it
  //cout << "multinode "<<nodename<<" drawing to left rail "<<p<<' '<<join<<"\n";
  nodes.front()->drawToLeftRail(outs,p,join);
  if(leftrail != NULL)
    {
      p = leftrail;
      join = leftrail->getRailDir();
    }
  // skip first child
  for(auto i = nodes.begin()+1;i!=nodes.end();i++)
    (*i)->drawToLeftRail(outs,p,join);
  // last child always goes up unless it is a newlinerail
}

void multinode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  //cout << "multinode "<<nodename<<" drawing to right rail "<<p<<' '<<join<<"\n";
  // if we have our own rails, override with it
  nodes.front()->drawToRightRail(outs,p,join);
  if(rightrail != NULL)
    {
      p = rightrail;
      join = rightrail->getRailDir();
    }
  // skip first child
  for(auto i = nodes.begin()+1;i!=nodes.end();i++)
    (*i)->drawToRightRail(outs,p,join);
}


//   void loopnode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join){
//   for(auto i = nodes.begin();i!=nodes.end();i++)
//     (*i)->drawToLeftRail(outs,leftrail, join);
// }
// void loopnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
//   for(auto i = nodes.begin();i!=nodes.end();i++)
//     (*i)->drawToRightRail(outs,rightrail, join);
// }

void concatnode::drawToLeftRail(ofstream &outs, railnode* p, vraildir join){
  // draw from first child to my rail
  //  if(p != NULL)
  //cout << "concatnode "<<nodename<<" drawing to left rail "<<p<<' '<<join<<"\n";
  if(p != NULL)
    {
      // stringstream s;
      // s<<"+up:"<<sizes.colsep<<"pt";
      // line(outs,3,wa,wa+"-|"+p->rawName(),s.str());
      line(outs,2,wa,wa+"-|"+p->rawName(),p->rawName()+"linetop");
    }
  nodes.front()->drawToLeftRail(outs,p,join);
  if(leftrail != NULL)
    {
      p = leftrail;
      join = leftrail->getRailDir();
    }
  // have all children connect their internal rails
  for(auto i = nodes.begin();i != nodes.end();i++)
    (*i)->drawToLeftRail(outs,NULL,join);
}

void concatnode::drawToRightRail(ofstream &outs, railnode* p, vraildir join){
  // draw from last child to my rail
  //if(p != NULL)
  //cout << "concatnode "<<nodename<<" drawing to right rail "<<p<<' '<<join<<"\n";
  if(p != NULL)
    {
      // stringstream s;
      // s<<"+up:"<<sizes.colsep<<"pt";
      // line(outs,3,ea,ea+"-|"+p->rawName(),s.str());
      line(outs,3,ea,ea+"-|"+p->rawName(),p->rawName()+"linetop");
    }
  nodes.back()->drawToRightRail(outs,p,join);
  if(rightrail != NULL)
    {
      p = rightrail;
      join = rightrail->getRailDir();
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
