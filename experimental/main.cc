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


#include <getopt.h>
#include <fstream>
#include <driver.hh>
#include <nodesizes.hh>
#include <diagnostics.hh>

/* Create a table of command line option structures, as defined in
   getopt.h You can use "man getopt" at the command line to learn
   more about this. */
struct option options[] = {
  {"optimize", no_argument, NULL, 'O'},
  {"nooptimize", no_argument, NULL, 'n'},
  {"makefigures", no_argument, NULL, 'f'},
  {"dumponly", no_argument, NULL, 'd'},
  {"help", no_argument, NULL, 'h'},
  {0, 0, 0, 0}
};

const char *optstring = "Onfhd";

/* description is an array of strings, each of which describes one of
   the command line options.  They are given in the same order as the
   options table, so that usage() can use them.
*/
const char *description[] = {
  "    Enable graph transformations (optimize, subsume).",
  "    Do not do any graph transformations (default).",
  "    Wrap all tikzpictures in figures and create commands"
  "to place them.",
  "    Parse and dump the AST only; do not generate TikZ output.",
  "",
  ""
};


/* Print the command line summary, options, and description of each
   option. */
void usage(const char *name)
{
  int i;
  cout << "\nUSAGE: ";
  cout << name << " <options> input_file output_file\n";
  cout << "  where command is one of:\n\n";
  for(i = 0; options[i].name != NULL; i++)
    {
      if(options[i].has_arg == 1)
	cout << "  --" << options[i].name << " <arg> or -"
	     << char(options[i].val) << " <arg>\n";
      else
	cout << "  --" << options[i].name << " or -"
	     << char(options[i].val) << "\n";
      cout << description[i] << endl << endl;
    }
  exit(1);
}




int main(int argc, char** argv) {
  char *infilename, *outfilename;
  int option_index = 0;
  int c;
  int noopt = 1, figures = 0, dumponly = 0;

  /* process the command line options */

  while((c = getopt_long(argc, argv, optstring, options,
			 &option_index)) >= 0)
    {
      switch(c)
	{
	case 'O':
	  noopt = 0;
	  break;
	case 'n':
	  break;
	case 'f':
	  figures = 1;
	  break;
	case 'd':
	  dumponly = 1;
	  break;
	case 'h':
	  usage(argv[0]);
	  exit(1);
	  break;
	}
    }

  if((argc - optind) != 2)
    {
      cerr << "You must provide an input file name and an output file name.\n";
      usage(argv[0]);
    }

  infilename = argv[optind];
  outfilename = argv[optind + 1];

  ofstream outfile(outfilename);
  nodesizes sizes;

  sizes.loadData("bnfnodes.dat");

  driver drv(&outfile, &sizes);

  if(drv.parse(infilename, noopt, figures, dumponly))
    std::cout << "Parser returned " << drv.get_result() << endl;

  outfile.close();

  if(diagnostics.hasErrors())
    diagnostics.emit(cerr);

  return drv.get_result();

}
