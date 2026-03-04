#!/bin/bash
# Build Free Pascal grammar railroad diagram PDF.
# Runs the multi-pass ebnf2tikz + pdflatex feedback loop.
#
# Usage: build_pdf.sh <path-to-ebnf2tikz>

EBNF2TIKZ="${1:?Usage: build_pdf.sh <path-to-ebnf2tikz>}"
SRCDIR="$(cd "$(dirname "$0")" && pwd)"
EBNF_FILE="$SRCDIR/free-pascal.ebnf"
TEXDRIVER="$SRCDIR/testdriver.tex"

if [ ! -x "$EBNF2TIKZ" ]; then
    echo "Error: ebnf2tikz not found at $EBNF2TIKZ"
    exit 1
fi

# Locate the LaTeX package directory so pdflatex can find ebnf2tikz.sty
STYDIR="$(cd "$SRCDIR/../LaTeX" && pwd)"
if [ ! -f "$STYDIR/ebnf2tikz.sty" ]; then
    echo "Error: ebnf2tikz.sty not found in $STYDIR"
    echo "  Run 'latex ebnf2tikz.ins' in the LaTeX/ directory first"
    exit 1
fi
export TEXINPUTS="$STYDIR::"

if [ ! -f "$EBNF_FILE" ]; then
    echo "Error: free-pascal.ebnf not found at $EBNF_FILE"
    exit 1
fi

# Copy testdriver.tex into the working directory if not already here
if [ "$(pwd)" != "$SRCDIR" ] && [ ! -f testdriver.tex ]; then
    cp "$TEXDRIVER" .
fi

# Clean generated files from previous runs
rm -f all_diagrams.tex testlist.tex bnfnodes.dat
rm -f testdriver.aux testdriver.log testdriver.pdf

# 3 iterations: ebnf2tikz produces TikZ, pdflatex measures nodes, repeat
for iter in 1 2 3; do
    echo "=== Iteration $iter ==="

    # Run ebnf2tikz on the grammar
    if ! "$EBNF2TIKZ" -O "$EBNF_FILE" all_diagrams.tex 2>/dev/null; then
        echo "  WARN: ebnf2tikz returned non-zero"
    fi

    # Generate testlist.tex (display each production)
    rm -f testlist.tex
    LIST=$(grep newsavebox all_diagrams.tex | grep Box | \
           sed -e 's/[\]newsavebox[{]//g' | sed -e 's/[}]//g' | sort -u)
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
    echo "  Running pdflatex ($COUNT productions)..."
    if ! pdflatex -interaction=nonstopmode testdriver > /dev/null 2>&1; then
        echo "  WARN: pdflatex returned non-zero on iteration $iter"
    fi
done

# Clean up intermediate files (keep testdriver.pdf)
rm -f all_diagrams.tex testlist.tex bnfnodes.dat
rm -f testdriver.aux testdriver.log

echo ""
if [ -f testdriver.pdf ]; then
    echo "Success: testdriver.pdf generated with all Free Pascal railroad diagrams"
else
    echo "Error: testdriver.pdf was not generated"
    exit 1
fi
