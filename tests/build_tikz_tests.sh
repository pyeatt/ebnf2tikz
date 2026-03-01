#!/bin/bash
# TikZ output regression test builder for ebnf2tikz
# Concatenates all .ebnf files into a single input, runs ebnf2tikz once
# (so that node name counters are unique across all productions),
# generates testlist.tex, and runs pdflatex with the multi-pass feedback loop.
#
# Usage: build_tikz_tests.sh [path-to-ebnf2tikz]

EBNF2TIKZ="${1:-../ebnf2tikz}"

if [ ! -x "$EBNF2TIKZ" ]; then
    echo "Error: ebnf2tikz not found at $EBNF2TIKZ"
    exit 1
fi

# Clean generated files from previous runs
rm -f all_tests.tex all_input.ebnf testlist.tex bnfnodes.dat
rm -f testdriver.aux testdriver.log testdriver.pdf

# Count .ebnf files (after cleaning, so all_input.ebnf is excluded)
EBNF_COUNT=0
for f in *.ebnf; do
    EBNF_COUNT=$((EBNF_COUNT + 1))
done

if [ $EBNF_COUNT -eq 0 ]; then
    echo "Error: no .ebnf files found"
    exit 1
fi

echo "Found $EBNF_COUNT test files"

# Concatenate all .ebnf files into a single input file.
# This ensures node name counters are unique across all productions.
for f in *.ebnf; do
    cat "$f" >> all_input.ebnf
    echo "" >> all_input.ebnf
done

# 3 iterations: ebnf2tikz produces TikZ, pdflatex measures nodes, repeat
for iter in 1 2 3; do
    echo "=== Iteration $iter ==="

    # Run ebnf2tikz once on the concatenated input
    if ! "$EBNF2TIKZ" all_input.ebnf all_tests.tex 2>/dev/null; then
        echo "  WARN: ebnf2tikz failed"
    fi

    # Generate testlist.tex (figure environments for each \newsavebox)
    rm -f testlist.tex
    LIST=$(grep newsavebox all_tests.tex | grep Box | sed -e 's/[\]newsavebox[{]//g' | sed -e 's/[}]//g' | sort -u)
    COUNT=0
    for boxname in $LIST; do
        shortname=$(echo "$boxname" | sed -e 's/[\]//g')
        echo "\\par\\bigskip" >> testlist.tex
        echo "\\noindent\\textbf{${shortname}}" >> testlist.tex
        echo "\\par\\medskip" >> testlist.tex
        echo "\\centerline{\\usebox{${boxname}}}" >> testlist.tex
        echo "" >> testlist.tex
        COUNT=$((COUNT + 1))
        if [ $((COUNT % 10)) -eq 0 ]; then
            echo "\\clearpage" >> testlist.tex
        fi
    done

    # Run pdflatex (produces bnfnodes.dat for the next iteration)
    echo "  Running pdflatex..."
    if ! pdflatex -interaction=nonstopmode testdriver > /dev/null 2>&1; then
        echo "  WARN: pdflatex returned non-zero on iteration $iter"
    fi
done

# Clean up intermediate files (keep testdriver.pdf for inspection)
rm -f all_tests.tex all_input.ebnf testlist.tex bnfnodes.dat
rm -f testdriver.aux testdriver.log

echo ""
if [ -f testdriver.pdf ]; then
    echo "Success: testdriver.pdf generated with all railroad diagrams"
else
    echo "Error: testdriver.pdf was not generated"
    exit 1
fi
