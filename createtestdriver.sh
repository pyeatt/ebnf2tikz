#!/bin/bash
# Generate testlist.tex from test.tex newsavebox entries.
# Must be run AFTER ebnf2tikz generates test.tex.

LIST=$(grep newsavebox test.tex | grep Box | sed -e 's/[\]newsavebox[{]//g' | sed -e 's/[}]//g' | sort -u)

rm -f testlist.tex

for i in $LIST; do
    shortname=$(echo "$i" | sed -e 's/[\]//g')
    echo "\\par\\bigskip" >> testlist.tex
    echo "\\noindent\\textbf{${shortname}}" >> testlist.tex
    echo "\\par\\medskip" >> testlist.tex
    echo "\\centerline{\\usebox{${i}}}" >> testlist.tex
    echo "" >> testlist.tex
done
