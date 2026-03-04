#!/bin/bash
# AST-dump regression test runner for ebnf2tikz
# Usage: run_tests.sh [path-to-ebnf2tikz]

EBNF2TIKZ="${1:-../ebnf2tikz}"
PASS=0
FAIL=0
ERRORS=""

for ebnf in *.ebnf; do
    expected="expected/${ebnf%.ebnf}.expected"
    testname="${ebnf%.ebnf}"

    if [ ! -f "$expected" ]; then
        echo "SKIP $testname (no .expected file)"
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS  $testname: missing .expected file\n"
    else
        flagsfile="flags/${ebnf%.ebnf}.flags"
        if [ -f "$flagsfile" ]; then
            FLAGS=$(cat "$flagsfile")
        else
            FLAGS="-n -d"
        fi
        actual=$("$EBNF2TIKZ" $FLAGS "$ebnf" /dev/null 2>/dev/null)
        if echo "$actual" | diff -u "$expected" - > /dev/null 2>&1; then
            echo "PASS $testname"
            PASS=$((PASS + 1))
        else
            echo "FAIL $testname"
            echo "$actual" | diff -u "$expected" - | head -20
            FAIL=$((FAIL + 1))
            ERRORS="$ERRORS  $testname\n"
        fi
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed"
if [ $FAIL -gt 0 ]; then
    echo -e "Failed tests:\n$ERRORS"
    exit 1
fi
exit 0
