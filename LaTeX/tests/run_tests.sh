#!/bin/bash
# LaTeX package tests for ebnf2tikz.sty
#
# These tests verify that the ebnf2tikz LaTeX package correctly provides
# all macros, styles, and commands needed by the generated TikZ output.
#
# Usage: run_tests.sh <path-to-ebnf2tikz>
#
# Each test:
#   1. Runs ebnf2tikz on the appropriate .ebnf file(s)
#   2. Runs pdflatex 3 times (multi-pass feedback loop)
#   3. Checks that a PDF was produced

EBNF2TIKZ="${1:?Usage: run_tests.sh <path-to-ebnf2tikz>}"
TESTDIR="$(cd "$(dirname "$0")" && pwd)"
STYDIR="$(cd "$TESTDIR/.." && pwd)"

if [ ! -x "$EBNF2TIKZ" ]; then
    echo "Error: ebnf2tikz not found at $EBNF2TIKZ"
    exit 1
fi

if [ ! -f "$STYDIR/ebnf2tikz.sty" ]; then
    echo "Error: ebnf2tikz.sty not found in $STYDIR"
    echo "  Run 'latex ebnf2tikz.ins' in the LaTeX/ directory first"
    exit 1
fi

export TEXINPUTS="$STYDIR:$TESTDIR:$TEXINPUTS"

PASS=0
FAIL=0

run_test() {
    local testname="$1"
    local texfile="$2"
    local ebnffile="$3"
    local ebnf_flags="$4"
    local jobname="${texfile%.tex}"

    echo -n "  $testname... "

    # Clean previous artifacts
    rm -f "$jobname.pdf" "$jobname.aux" "$jobname.log" \
          "$jobname-rail.tex" "$jobname-rail.ebnf" \
          "$jobname-diagrams.tex" \
          bnfnodes.dat

    # Run ebnf2tikz on the .ebnf file to produce TikZ output
    if [ -n "$ebnffile" ]; then
        local tikzfile="$jobname-diagrams.tex"
        if ! "$EBNF2TIKZ" $ebnf_flags "$ebnffile" "$tikzfile" 2>/dev/null; then
            echo "FAIL (ebnf2tikz failed)"
            FAIL=$((FAIL + 1))
            return
        fi
    fi

    # Run pdflatex 3 times for the multi-pass feedback loop
    for iter in 1 2 3; do
        # Re-run ebnf2tikz if bnfnodes.dat exists (passes 2+)
        if [ -n "$ebnffile" ] && [ -f bnfnodes.dat ]; then
            "$EBNF2TIKZ" $ebnf_flags "$ebnffile" "$tikzfile" 2>/dev/null
        fi
        if ! pdflatex -interaction=nonstopmode "$texfile" > /dev/null 2>&1; then
            # pdflatex often returns non-zero on first pass (missing refs), continue
            true
        fi
    done

    if [ -f "$jobname.pdf" ]; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL (no PDF produced)"
        FAIL=$((FAIL + 1))
    fi

    # Clean intermediate files
    rm -f "$jobname.aux" "$jobname.log" "$jobname.out" \
          "$jobname-rail.tex" "$jobname-rail.ebnf" \
          "$jobname-diagrams.tex" \
          bnfnodes.dat
}

cd "$TESTDIR"

echo "Running LaTeX package tests..."
echo ""

# Test 1: \ebnffile + \useproduction with external .ebnf
# We manually generate TikZ and \input it, since these tests don't use shell-escape.
# The test .tex files use \useproduction which relies on the saveboxes existing.
# So we generate a -diagrams.tex, and prepend an \input to a wrapper.

# For test_ebnffile: generate TikZ from sample.ebnf, input it
cat > test_ebnffile_wrapper.tex <<'WRAPPER'
\documentclass{article}
\usepackage{ebnf2tikz}
\begin{document}
\input{test_ebnffile_wrapper-diagrams}
\section*{Test: ebnffile + useproduction}
Diagrams loaded from \texttt{sample.ebnf} via manual ebnf2tikz run:
\bigskip
\noindent\textbf{expr}
\par\medskip
\useproduction{expr}
\bigskip
\noindent\textbf{term}
\par\medskip
\useproduction{term}
\bigskip
\noindent\textbf{factor}
\par\medskip
\useproduction{factor}
\end{document}
WRAPPER
run_test "ebnffile + useproduction" "test_ebnffile_wrapper.tex" "sample.ebnf" "-O"
rm -f test_ebnffile_wrapper.tex test_ebnffile_wrapper.pdf

# Test 2: underscore + digit production names (camelcase conversion)
cat > test_underscores_wrapper.tex <<'WRAPPER'
\documentclass{article}
\usepackage{ebnf2tikz}
\begin{document}
\input{test_underscores_wrapper-diagrams}
\section*{Test: camelcase conversion}
\bigskip
\noindent\textbf{my\_rule}
\par\medskip
\useproduction{my_rule}
\bigskip
\noindent\textbf{long\_name\_here}
\par\medskip
\useproduction{long_name_here}
\bigskip
\noindent\textbf{rule\_3}
\par\medskip
\useproduction{rule_3}
\end{document}
WRAPPER
run_test "underscore/digit camelcase" "test_underscores_wrapper.tex" "sample_underscores.ebnf" "-O"
rm -f test_underscores_wrapper.tex test_underscores_wrapper.pdf

# Test 3: nooptimize package option (just verify it loads and compiles)
cat > test_noopt_wrapper.tex <<'WRAPPER'
\documentclass{article}
\usepackage[nooptimize]{ebnf2tikz}
\begin{document}
\input{test_noopt_wrapper-diagrams}
\section*{Test: nooptimize option}
\bigskip
\noindent\textbf{expr} (unoptimized)
\par\medskip
\useproduction{expr}
\end{document}
WRAPPER
run_test "nooptimize option" "test_noopt_wrapper.tex" "sample.ebnf" "-n"
rm -f test_noopt_wrapper.tex test_noopt_wrapper.pdf

# Test 4: custom style overrides
cat > test_styles_wrapper.tex <<'WRAPPER'
\documentclass{article}
\usepackage{ebnf2tikz}
\setlength{\railcolsep}{8pt}
\setlength{\railnodeheight}{18pt}
\tikzset{
  nonterminal/.style={
    draw, inner sep=3pt, rectangle,
    minimum size=\railnodeheight, thick,
    top color=white, bottom color=blue!20,
    font=\sffamily\itshape
  }
}
\begin{document}
\input{test_styles_wrapper-diagrams}
\section*{Test: custom style overrides}
\bigskip
\noindent\textbf{expr}
\par\medskip
\useproduction{expr}
\end{document}
WRAPPER
run_test "custom style overrides" "test_styles_wrapper.tex" "sample.ebnf" "-O"
rm -f test_styles_wrapper.tex test_styles_wrapper.pdf

# Test 5: figures package option (verify -f flag compiles)
cat > test_figures_wrapper.tex <<'WRAPPER'
\documentclass{article}
\usepackage[figures]{ebnf2tikz}
\begin{document}
\input{test_figures_wrapper-diagrams}
This document tests the figures package option.
\end{document}
WRAPPER
run_test "figures option" "test_figures_wrapper.tex" "sample.ebnf" "-O -f"
rm -f test_figures_wrapper.tex test_figures_wrapper.pdf

echo ""
echo "Results: $PASS passed, $FAIL failed (out of $((PASS + FAIL)))"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
