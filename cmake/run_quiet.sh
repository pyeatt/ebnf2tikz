#!/bin/bash
# Run a command with output suppressed.  On failure, print the captured
# output so the user can diagnose the problem.
#
# Set LATEX_VERBOSE=1 in the environment to see full output:
#   LATEX_VERBOSE=1 make sty-doc
#
# Usage: run_quiet.sh command [args...]

if [ -n "$LATEX_VERBOSE" ]; then
    exec "$@"
fi

LOG=$(mktemp)
if "$@" > "$LOG" 2>&1; then
    rm -f "$LOG"
else
    RC=$?
    echo "ERROR: $1 failed (exit code $RC). Output follows:"
    echo "-----------------------------------------------"
    cat "$LOG"
    echo "-----------------------------------------------"
    echo "Hint: re-run with LATEX_VERBOSE=1 for live output."
    rm -f "$LOG"
    exit $RC
fi
