# ebnf2tikz

Author: Larry D. Pyeatt

February, 2021


An optimizing compiler to convert (possibly annotated) <a href=https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form>Extended Backus–Naur  Form</a> (EBNF) to railroad diagrams expressed as LaTeX <a href=https://en.wikipedia.org/wiki/PGF/TikZ> TikZ</a> commands.

For example, if  you feed a file containing the following annotated EBNF into ebnf2tikz:
```ebnf
case_statement_alternative =
    'when' , choices , '=>', sequence_of_statements;

subsume
choices =
    choice, { '|', choice } ;

subsume
choice =
    simple_expression |
    discrete_range |
    simple_name |
    'others' ;
```
Then it will output the following TikZ code:

```latex
\begin{figure}
\centerline{
\begin{tikzpicture}
\node at (0pt,0pt)[anchor=west](name){\railname{case\_statement\_alternative\strut}};
\coordinate (node42) at (59.3677pt,-21pt);
\coordinate (node42linetop) at (59.3677pt,-27pt);
\coordinate (node42linebottom) at (59.3677pt,-101pt);
\draw [rounded corners=\railcorners] (node42linetop) -- (node42linebottom);
\draw [rounded corners=\railcorners] (node42linetop) -- (node42) -- +(east:8pt);
\coordinate (node45) at (67.3677pt,-21pt);
\coordinate (node45linetop) at (67.3677pt,-27pt);
\coordinate (node45linebottom) at (67.3677pt,-79pt);
\draw [rounded corners=\railcorners] (node45linetop) -- (node45linebottom);
\draw [rounded corners=\railcorners] (node45linetop) -- (node45) -- +(west:8pt);
\coordinate (node51) at (156.118pt,-21pt);
\coordinate (node51linetop) at (156.118pt,-27pt);
\coordinate (node51linebottom) at (156.118pt,-79pt);
\draw [rounded corners=\railcorners] (node51linetop) -- (node51linebottom);
\draw [rounded corners=\railcorners] (node51linetop) -- (node51) -- +(east:8pt);
\coordinate (node53) at (164.118pt,-21pt);
\coordinate (node53linetop) at (164.118pt,-27pt);
\coordinate (node53linebottom) at (164.118pt,-101pt);
\draw [rounded corners=\railcorners] (node53linetop) -- (node53linebottom);
\draw [rounded corners=\railcorners] (node53linetop) -- (node53) -- +(west:8pt);
\node (node1) at (16pt,-21pt)[anchor=west,terminal] {\railtermname{when\strut}};
\writenodesize{node1}
\draw [rounded corners=\railcorners] (node1.east) -- (node42.west);
\node (node47) at (75.3677pt,-21pt)[anchor=west,nonterminal] {\railname{simple\_expression\strut}};
\writenodesize{node47}
\node (node48) at (83.5977pt,-43pt)[anchor=west,nonterminal] {\railname{discrete\_range\strut}};
\writenodesize{node48}
\node (node49) at (84.5176pt,-65pt)[anchor=west,nonterminal] {\railname{simple\_name\strut}};
\writenodesize{node49}
\node (node50) at (92.8288pt,-87pt)[anchor=west,terminal] {\railtermname{others\strut}};
\writenodesize{node50}
\draw [rounded corners=\railcorners] (node45.east) -- (node47.west);
\draw [rounded corners=\railcorners] (node47.east) -- (node51.west);
\node (node52) at (104.743pt,-109pt)[anchor=west,terminal] {\railtermname{|}};
\writenodesize{node52}
\draw [rounded corners=\railcorners] (node42.east) -- (node45.west);
\draw [rounded corners=\railcorners] (node51.east) -- (node53.west);
\node (node4) at (172.118pt,-21pt)[anchor=west,terminal] {\railtermname{=>\strut}};
\writenodesize{node4}
\draw [rounded corners=\railcorners] (node53.east) -- (node4.west);
\node (node5) at (199.775pt,-21pt)[anchor=west,nonterminal] {\railname{sequence\_of\_statements\strut}};
\writenodesize{node5}
\draw [rounded corners=\railcorners] (node4.east) -- (node5.west);
\draw [rounded corners=\railcorners] (node48.west) -- (node48.west-|node45) -- (node45linetop);
\draw [rounded corners=\railcorners] (node49.west) -- (node49.west-|node45) -- (node45linetop);
\draw [rounded corners=\railcorners] (node50.west) -- (node50.west-|node45) -- (node45linetop);
\draw [rounded corners=\railcorners] (node52.west) -- (node52.west-|node42) -- (node42linetop);
\draw [rounded corners=\railcorners] (node48.east) -- (node48.east-|node51) -- (node51linetop);
\draw [rounded corners=\railcorners] (node49.east) -- (node49.east-|node51) -- (node51linetop);
\draw [rounded corners=\railcorners] (node50.east) -- (node50.east-|node51) -- (node51linetop);
\draw [rounded corners=\railcorners] (node52.east) -- (node52.east-|node53) -- (node53linetop);
\end{tikzpicture}
}
\caption{No Caption.}
\label{No Caption.}
\end{figure}
```

You will need a ```\usepackage{ebnf2tikz}``` command in the preamble of your LaTeX document.
Then you can just include the TikZ code in your LaTeX document, and it will draw this:

<img src="./testdriver.png" height="200">


## About the Code

This is a work in progress.  There are still a couple of bugs that I am aware of, but nothing major.
I have not really started working on newlines, so the diagrams can easily become wider than the space available.  



Originally, I planned to have TikZ do most of the work.  However,
while I could get it to do small diagrams, it failed miserably when
the level of nesting went beyond three.  TikZ really does not have the
concept of "sub-images" that have user-defined anchor points. I think
it is possible because, ... look at CircuiTikZ. 

Trying to get TikZ to do all of the work to lay out complex diagrams was a nightmare.  It
does not do recursive structures well. I put in some effort, then gave
up and decided to go another direction.  Ebnf2TikZ does all of the layout, and just uses TikZ to do the drawing. 
This does mean that it needs some information from LaTeX about how big the basic nodes are.  This means that you have to run ebnf2tikz, then LaTeX, then ebnf2tikz again, then LaTeX again.

I have written it so that
you can:

1. Run ebnf2tikz to produce all of the diagrams, but they are not
correct.

2. Run the incorrect diagrams through LaTeX to get the dimensions of
the nodes and the settings for railcolsep and railrowsep.

3. After that, re-run ebnf2tikz and all of the diagrams are correct.

4. Run LaTeX again and everything looks good.

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

All nodes are placed using exact coordinates```\node (nodename) at (exact
coordinate)``` but all lines are drawn using the node names.
