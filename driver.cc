

#include "driver.hh"
#include "parser.hh"

driver::driver (ofstream *out)
  : trace_parsing (false), trace_scanning (false)
{
  outFile=out;
}

int driver::parse (const char *f,int opt, int fig)
{
  file = f;
  noopt=opt;
  figures=fig;
  location.initialize (&file);
  scan_begin ();
  yy::parser parse (*this);
  parse.set_debug_level (trace_parsing);
  result = parse ();
  scan_end ();
  return result;
}
