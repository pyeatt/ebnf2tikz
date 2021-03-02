
#include <getopt.h>
#include <fstream>
#include <driver.hh>

/* Create a table of command line option structures, as defined in
   getopt.h You can use "man getopt" ath the command line to learn
   more about this. */
struct option options[] = {
  {"nooptimize", no_argument,  NULL, 'n'},
  {"makefigures", no_argument,  NULL, 'f'},
  {"help", no_argument,  NULL, 'h'},
  {0, 0, 0, 0}
};

char *optstring = (char*)"nfh";

/* description is an array of strings, each of which describes one of
   the command line options.  They are given in the same order as the
   options table, so that usage() can use them.
*/
char *description[] = {
  (char*)"    Do not do any graph transformations.",
  (char*)"    Wrap all tikzpictures in figures and create commands"
         "to place them.",
  (char*)"",
  (char*)""
};


/* Print the command line summary, options, and description of each
   option. */
void usage(char *name)
{
  int i;
  cout<<"\nUSAGE: ";
  cout<<name<<" <options> input_file output_file\n";
  cout<<"  where command is one of:\n\n";
  for(i=0;options[i].name!=NULL;i++)
    {
      if(options[i].has_arg==1)
	cout<< "  --"<<options[i].name<<" <arg> or -"<<
	  options[i].val<<" <arg>\n";
      else
	cout<< "  --"<<options[i].name<<" or -"<<options[i].val<<"\n";
      cout<<description[i]<<endl<<endl;
    }
  exit(1);
}




int main(int argc, char** argv) {
  char *infilename,*outfilename;
  int option_index = 0;
  int c;
  int noopt=0,figures=0;
  
  /* process the command line options */

  while((c = getopt_long (argc, argv, optstring, options,
                          &option_index)) >= 0)
    {
      cout << "got option "<<c<<endl;
      switch(c)
        {
        case 'n':
	  noopt = 1;
          break;
        case 'f':
	  figures = 1;
          break;
        case 'h':
          usage(argv[0]);
          exit(1);
          break;
        }
    }

  if((argc-optind) != 2)
    {
      cerr << "You must provide an input file name and an output file name.\n";
      usage(argv[0]);
    }
	
  infilename = argv[optind];
  outfilename = argv[optind+1];

  ofstream outfile(outfilename);
  driver drv(&outfile);

  node::loadData("bnfnodes.dat");
  
  if (drv.parse (infilename,noopt,figures))
    std::cout << "Parser returned " << drv.result << endl;

  outfile.close();
  
  return drv.result;

}


		
