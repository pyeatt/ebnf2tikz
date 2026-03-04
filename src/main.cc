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

/**
 * @file main.cc
 * @brief CLI entry point for the ebnf2tikz compiler.
 *
 * Parses command-line options with @c getopt_long, loads node sizes
 * from @c bnfnodes.dat, invokes the parser driver, and reports
 * any diagnostics on exit.
 *
 * @see driver.hh for the parser driver
 * @see nodesizes.hh for the node size cache
 */

#include <getopt.h>
#include <fstream>
#include <driver.hh>
#include <nodesizes.hh>
#include <diagnostics.hh>

using namespace std;

/** @brief Command-line option table for getopt_long. */
struct option options[] = {
  {"optimize", no_argument, nullptr, 'O'},
  {"nooptimize", no_argument, nullptr, 'n'},
  {"makefigures", no_argument, nullptr, 'f'},
  {"dumponly", no_argument, nullptr, 'd'},
  {"dump-before", no_argument, nullptr, 'B'},
  {"dump-after", no_argument, nullptr, 'A'},
  {"help", no_argument, nullptr, 'h'},
  {0, 0, 0, 0}
};

/** @brief Short option string for getopt_long. */
const char *optstring = "OnfhdBAh";

/** @brief Descriptions for each option, parallel to the options table. */
const char *description[] = {
  "    Enable optimization (default, accepted for compatibility).",
  "    Disable optimization and subsumption.",
  "    Wrap all tikzpictures in figures and create commands"
  "to place them.",
  "    Parse and dump the AST only; do not generate TikZ output.",
  "    Dump the AST before optimization (to stdout).",
  "    Dump the AST after optimization (to stdout).",
  "",
  ""
};


/**
 * @brief Print command-line usage summary and exit.
 * @param name Program name (argv[0]).
 */
void usage(const char *name)
{
  int i;
  cout << "\nUSAGE: ";
  cout << name << " <options> input_file output_file\n";
  cout << "  where command is one of:\n\n";
  for(i = 0; options[i].name != nullptr; i++)
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




/**
 * @brief Program entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, nonzero on parse error.
 */
int main(int argc, char** argv) {
  char *infilename, *outfilename;
  int option_index = 0;
  int c;
  int noopt = 0, figures = 0, dumponly = 0;
  int dump_before = 0, dump_after = 0;

  /* process the command line options */

  while((c = getopt_long(argc, argv, optstring, options,
			 &option_index)) >= 0)
    {
      switch(c)
	{
	case 'O':
	  break;
	case 'n':
	  noopt = 1;
	  break;
	case 'f':
	  figures = 1;
	  break;
	case 'd':
	  dumponly = 1;
	  break;
	case 'B':
	  dump_before = 1;
	  break;
	case 'A':
	  dump_after = 1;
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

  if(drv.parse(infilename, noopt, figures, dumponly, dump_before, dump_after))
    std::cout << "Parser returned " << drv.get_result() << endl;

  outfile.close();

  if(diagnostics.hasErrors())
    diagnostics.emit(cerr);

  return drv.get_result();

}
