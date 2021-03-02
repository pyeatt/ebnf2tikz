#ifndef NODESIZE_HH
#define NODESIZE_HH

#include <map>
#include <string>
#include <fstream>
#include <iostream>     // std::cout

using namespace std;


class coordinate{
public:
  float x;
  float y;
  coordinate(){x=y=0;}
  coordinate(float nx,float ny){x=nx;y=ny;}
  coordinate operator+(coordinate r){
    coordinate c;
    c.x = x+r.x;
    c.y = y+r.y;
    return c;
  }
  coordinate operator-(coordinate r){
    coordinate c;
    c.x = x-r.x;
    c.y = y-r.y;
    return c;
  }
  coordinate& operator=(coordinate r){
    x = r.x;
    y = r.y;
    return *this;
  }
  friend ostream& operator<<(std::ostream& out, const coordinate &c)
  {
    out << '('<<c.x<<"pt,"<<c.y<<"pt)";
    return out;
  }

};

class nodesizes{
private:
  map <string,coordinate> sizemap;
public:
  float rowsep,colsep,minsize;
  nodesizes(){};

  void loadData(string filename) {
    string nodename;
    char ch;
    coordinate size;
    ifstream inf;
    rowsep = 6;
    colsep = 8;
    minsize=14;
    inf.open(filename);
    if(!inf.is_open())
      cout << "Unable to open input file "<<filename<<". Continuing\n";
    else
      {
	do{
	  inf>>nodename;
	  inf >> size.x;
	  do
	    inf >> ch;
	  while(ch != ',' && !inf.eof());
	  inf >> size.y;
	  if(inf)
	    sizemap.insert ( pair<string,coordinate>(nodename,size) );
	}while(inf);
	inf.close();
      }
  }

  int getSize(string nodename, float &width, float &height)
  {
    auto i = sizemap.find(nodename);
    if(i == sizemap.end())
      {
	width=colsep;
	height=rowsep;
	return 0;
      }
    width = i->second.x;
    height = i->second.y;
    return 1;
  }

  
};


#endif
