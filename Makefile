
OBJS=lexer.o parser.o driver.o graph.o optimize.o subsume.o output.o annot_lexer.o annot_parser.o main.o util.o
CSRC= $(patsubst %.o,%.cc,$(OBJS))
DEPENDFLAGS=-M
CFLAGS=-I. -c -Wall -g  
CC=g++

Debug: ebnf2tikz

ebnf2tikz: $(OBJS)
	$(CC) -g -I. -o ebnf2tikz $(OBJS)

parser.cc: parser.yy 
	bison -d --locations -o parser.cc parser.yy 

parser.hh: parser.yy
	bison -d --locations -o parser.cc parser.yy

lexer.cc: lexer.ll
	flex --header-file=lexer.hh -o lexer.cc lexer.ll

lexer.hh: lexer.ll 
	flex --header-file=lexer.hh -o lexer.cc lexer.ll

annot_parser.cc: annot_parser.yy
	bison -d --locations -o annot_parser.cc annot_parser.yy 

annot_parser.hh: annot_parser.yy
	bison -d --locations -o annot_parser.cc annot_parser.yy

annot_lexer.cc: annot_lexer.ll
	flex --header-file=annot_lexer.hh -P annot -o annot_lexer.cc annot_lexer.ll

annot_lexer.hh: annot_lexer.ll 
	flex --header-file=annot_lexer.hh -P annot -o annot_lexer.cc annot_lexer.ll

.cc.o:
	$(CC) $(DEFINES) $(CFLAGS) $(INCLUDES) $<


test: ebnf2tikz
	./ebnf2tikz test.ebnf test.tex
	pdflatex testdriver
	./ebnf2tikz test.ebnf test.tex
	pdflatex testdriver
	./ebnf2tikz test.ebnf test.tex
	pdflatex testdriver



clean:
	rm -f lexer.cc lexer.hh parser.hh parser.cc annot_lexer.hh annot_lexer.cc annot_parser.hh annot_parser.cc ebnf2tikz location.hh annot_location.hh $(OBJS) *~ bnfnodes.dat testdriver.aux testdriver.log testdriver.pdf test.tex

realclean: clean
	rm -f .depend

depend: parser.cc lexer.cc parser.hh annot_parser.cc annot_lexer.cc annot_parser.hh
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) $(DEPENDFLAGS) $(CSRC) > .depend

# if we have a .depend file, include it

ifeq (.depend,$(wildcard .depend))
include .depend
endif
