#ifndef DRIVER_HH
#define DRIVER_HH
#include <string>
#include <set>
#include <fstream>
#include "graph.hh"
#include "parser.hh"

using namespace std;

// Give Flex the prototype of yylex we want ...
# define YY_DECL \
  yy::parser::symbol_type yylex (driver& drv)
// ... and declare it for the parser's sake.
YY_DECL;

// Conducting the whole scanning and parsing of Calc++.
class driver
{
  set<string> nonterminals;
  set<string> terminals;
  ofstream *outFile;
  int noopt,figures;
public:
  driver(ofstream *out);

  ofstream &outs(){return *outFile;}

  node* addTerminal(string &s)
  {
    terminals.insert(s);
    return new termnode(s);
  };
  node* addString(string &s)
  {
    nonterminals.insert(s);
    return new nontermnode(s);
  };
  
  int result;
  // Run the parser on file F.  Return 0 on success.
  int parse (const char *f,int opt, int fig);
  // The name of the file being parsed.
  std::string file;
  // Whether to generate parser debug traces.
  bool trace_parsing;
  // Handling the scanner.
  void scan_begin ();
  void scan_end ();
  // Whether to generate scanner debug traces.
  bool trace_scanning;
  // The token's location used by the scanner.
  yy::location location;
};
#endif // ! DRIVER_HH
