
OBJS=lexer.o parser.o driver.o graph.o optimize.o subsume.o output.o main.o
CSRC= $(patsubst %.o,%.cc,$(OBJS))
DEPENDFLAGS=-M
CFLAGS=-I. -c -Wall -g 
CC=g++

ebnf2tikz: $(OBJS)
	$(CC) -g -I. -o ebnf2tikz $(OBJS)

parser.cc: parser.yy
	bison -d --locations -o parser.cc parser.yy 

parser.hh: parser.yy
	bison -d --locations -o parser.cc parser.yy

lexer.cc: lexer.ll parser.cc parser.hh
	flex -o lexer.cc lexer.ll

.cc.o:
	$(CC) $(DEFINES) $(CFLAGS) $(INCLUDES) $<

clean:
	rm -f lexer.cc parser.hh parser.cc ebnf2tikz location.hh $(OBJS) *~ bnfnodes.dat testdriver.aux testdriver.log testdriver.pdf test.tex

realclean: clean
	rm -f .depend

depend: parser.cc lexer.cc parser.hh
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) $(DEPENDFLAGS) $(CSRC) > .depend

# if we have a .depend file, include it

ifeq (.depend,$(wildcard .depend))
include .depend
endif
