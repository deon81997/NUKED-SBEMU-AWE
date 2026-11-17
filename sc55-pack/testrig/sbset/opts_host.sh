#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sbset)
# opts_host.sh: SBSET's SBOPTS.TXT line builder on the host (every field/value, AWE32 options, kept options, red line,
# Win98 opts.cpp reading it), with ASan/UBSan. Prints PASS/FAIL as its last line.
set -e
H=$(cd $(dirname $0); pwd); O=/tmp/sbset-host; mkdir -p $O
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -c $H/w98glue.cpp -o $O/glue.o
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -c $H/../../win98/helper/opts.cpp -o $O/opts.o
gcc -O1 -g -Wall -DHOSTTEST -fsanitize=address,undefined -fno-sanitize-recover=undefined -c $H/opts_host.c -o $O/t.o
g++ -fsanitize=address,undefined -o $O/t $O/t.o $O/glue.o $O/opts.o
ASAN_OPTIONS=detect_leaks=0 $O/t
