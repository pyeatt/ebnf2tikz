#!/bin/bash
# Build a PDF from only the subsume test files.
# Reuses testdriver.tex but with subsume-only input.
# Usage: build_subsume_tests.sh [path-to-ebnf2tikz]

EBNF2TIKZ="${1:-../ebnf2tikz}"

if [ ! -x "$EBNF2TIKZ" ]; then
    echo "Error: ebnf2tikz not found at $EBNF2TIKZ"
    exit 1
fi

# Clean generated files from previous runs
rm -f all_tests.tex all_input.ebnf testlist.tex bnfnodes.dat
rm -f subsume_driver.aux subsume_driver.log subsume_driver.pdf

# Concatenate only the subsume .ebnf files into a single input
EBNF_COUNT=0
for f in *_subsume*.ebnf; do
    if [ -f "$f" ]; then
        cat "$f" >> all_input.ebnf
        echo "" >> all_input.ebnf
        EBNF_COUNT=$((EBNF_COUNT + 1))
    fi
done

if [ $EBNF_COUNT -eq 0 ]; then
    echo "Error: no subsume .ebnf files found"
    exit 1
fi

echo "Found $EBNF_COUNT subsume test files"

# Create a driver that sources the same preamble as testdriver.tex
# but writes output to subsume_driver.pdf
sed 's/all_tests/all_tests/;s/testlist/testlist/' testdriver.tex > subsume_driver.tex

# 3 iterations: ebnf2tikz produces TikZ, pdflatex measures nodes, repeat
for iter in 1 2 3; do
    echo "=== Iteration $iter ==="

    if ! "$EBNF2TIKZ" -O all_input.ebnf all_tests.tex 2>/dev/null; then
        echo "  WARN: ebnf2tikz failed"
    fi

    # Generate testlist.tex (one block per \newsavebox)
    rm -f testlist.tex
    LIST=$(grep newsavebox all_tests.tex | grep Box | sed -e 's/[\]newsavebox[{]//g' | sed -e 's/[}]//g' | sort -u)
    for boxname in $LIST; do
        shortname=$(echo "$boxname" | sed -e 's/[\]//g')
        echo "\\par\\bigskip" >> testlist.tex
        echo "\\noindent\\textbf{${shortname}}" >> testlist.tex
        echo "\\par\\medskip" >> testlist.tex
        echo "\\centerline{\\usebox{${boxname}}}" >> testlist.tex
        echo "" >> testlist.tex
    done

    echo "  Running pdflatex..."
    if ! pdflatex -interaction=nonstopmode -jobname=subsume_driver subsume_driver > /dev/null 2>&1; then
        echo "  WARN: pdflatex returned non-zero on iteration $iter"
    fi
done

# Clean up intermediate files
rm -f all_tests.tex all_input.ebnf testlist.tex bnfnodes.dat
rm -f subsume_driver.aux subsume_driver.log subsume_driver.tex

echo ""
if [ -f subsume_driver.pdf ]; then
    echo "Success: subsume_driver.pdf generated"
else
    echo "Error: subsume_driver.pdf was not generated"
    exit 1
fi
