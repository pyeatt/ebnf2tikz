ebnf2tikz
A compiler to convert (annotated) Extended Backus-Naur Form into
railroad diagrams (using tikz) for LaTeX.

Author: Larry D. Pyeatt
February, 2021

Originally, I planned to have tikz do most of the work.  However,
while I could get it to do small diagrams, it failed miserably when
the level of nesting went beyond three.  tikz really does not have the
concept of "sub-images" that have user-defined anchor points. I think
it is possible, because look at circuitikz. But I don't know enough
about tikz.

Trying to get tikz to lay out complex diagrams was a nightmare.  It
does not do recursive structures well. I put in some effort, then gave
up and decided to go another direction.  Now I have written it so that
you can:

1) Run ebnf2tikz to produce all of the diagrams, but they are not
correct.

2) Run the incorrect diagrams through LaTeX to get the dimensions of
the nodes and the settings for railcolsep and railrowsep.

3) After that, re-run ebnf2tikz and all of the diagrams are correct.

4) Run LaTeX again and everything looks good.

Any change to the ebnf file requires these four steps to get
everything looking good again. It is not so different from bibtex,
makeindex, etc.  The bottom line is that you may have to run ebnf2tikz
twice if you change its input files, and you will have to run it at
least once if you change the input file or change railrowsep or
railcolsep or any other layout settings.

The good news is that this approach makes the diagrams as concise as
they can possibly be. All of the layout is handled by ebnf2tikz, so
LaTeX does not spend a lot of time on them. Also, I may be able to do
some sort of "auto-newline" thing.

All nodes are placed using exact coordinates "\node (nodename) at (exat
coordinates) ..." but all lines are drawn using the node names.
