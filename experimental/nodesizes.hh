/*
ebnf2tikz
    An optimizing compiler to convert (possibly annotated) Extended
    Backus-Naur Form (EBNF) to railroad diagrams expressed as LaTeX
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

#ifndef NODESIZES_HH
#define NODESIZES_HH

#include <map>
#include <string>
#include <iostream>

using namespace std;

class coordinate {
public:
  float x;
  float y;
  coordinate() { x = y = 0; }
  coordinate(float nx, float ny) { x = nx; y = ny; }
  coordinate operator+(coordinate r) {
    coordinate c;
    c.x = x + r.x;
    c.y = y + r.y;
    return c;
  }
  coordinate operator-(coordinate r) {
    coordinate c;
    c.x = x - r.x;
    c.y = y - r.y;
    return c;
  }
  coordinate& operator=(coordinate r) {
    x = r.x;
    y = r.y;
    return *this;
  }
  friend ostream& operator<<(ostream& out, const coordinate &c) {
    out << '(' << c.x << "pt," << c.y << "pt)";
    return out;
  }
};

class nodesizes {
private:
  map<string, coordinate> sizemap;
public:
  float rowsep, colsep, minsize, textwidth;

  nodesizes();
  ~nodesizes();

  void loadData(const string &filename);
  int getSize(const string &nodename, float &width, float &height);
};

#endif
