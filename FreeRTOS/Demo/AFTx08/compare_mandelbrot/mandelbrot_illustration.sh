#!/bin/bash
# Replay the Mandelbrot frame from the Verilator runs.
#   ./mandelbrot_replay.sh single [seconds_per_char]
#   ./mandelbrot_replay.sh smp    [seconds_per_char]
#
# single prints cell 0, 1, 2, ... in order.
# smp prints cell 0, then the halfway cell, then cell 1, then halfway+1, ...
# Halfway is row 10. Rows 0-9 are hart 0. Rows 10-19 are hart 1.
# Record the terminal. This is a playback, not the chip.

MODE="${1:-}"
DELAY="${2:-0.012}"
COLS=40
ROWS=20
HALF=$((ROWS / 2))
ORIGIN=2

frame=(
"                  ......++@...          "
"                .......+*+.....         "
"              .........*@@*.....        "
"             .....++.+++@@+++....       "
"           .......+@@@@@@@@@@+*+..      "
"         ........*+@@@@@@@@@@@*...      "
"     .....+......@@@@@@@@@@@@@@*+..     "
"  ........+@*@*++@@@@@@@@@@@@@@@...     "
" ........+@@@@@@*@@@@@@@@@@@@@@*...     "
" ..+...+*@@@@@@@@@@@@@@@@@@@@@@....     "
" ..+...++@@@@@@@@@@@@@@@@@@@@@@....     "
" ........+@@@@@@*@@@@@@@@@@@@@@@...     "
"  ........+@*@@++@@@@@@@@@@@@@@@...     "
"     .....+..+...@@@@@@@@@@@@@@*+..     "
"        .........@+@@@@@@@@@@@*...      "
"           .......+@@@@@@@@@@+@*..      "
"             .....++.+++@@+++..*.       "
"              .........*@@*.....        "
"                .......+*+.....         "
"                  ......++@...          "
)

if [[ "$MODE" != "single" && "$MODE" != "smp" ]]; then
    echo "usage: $0 single|smp [seconds_per_char]" >&2
    exit 1
fi

cell() {
    local i="$1"
    local row=$((i / COLS))
    local col=$((i % COLS))
    local ch="${frame[$row]:$col:1}"
    tput cup $((ORIGIN + row)) "$col"
    printf '%s' "$ch"
}

cleanup() {
    tput cnorm
    tput cup $((ORIGIN + ROWS + 1)) 0
}
trap cleanup EXIT

tput civis
clear

if [[ "$MODE" == "single" ]]; then
    printf 'single-core    compute_cycles 489630\n'
else
    printf 'SMP            compute_cycles 312083\n'
fi

count=$((ROWS * COLS))
half=$((HALF * COLS))

if [[ "$MODE" == "single" ]]; then
    for ((i = 0; i < count; i++)); do
        cell "$i"
        sleep "$DELAY"
    done
else
    for ((i = 0; i < half; i++)); do
        cell "$i"
        sleep "$DELAY"
        cell $((i + half))
        sleep "$DELAY"
    done
fi
