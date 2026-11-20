#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# desckeys.sh LOG N PLAY - drives Descent 1.4 shareware in the QEMU Win98 VM (hmi98.sh desc, QSOCK=/tmp/w98t/q.sock):
# for each of N starts (the "runs ...DCNTSHR" line in the debug log): pilot name, New Game, level 1, PLAY s of play
# (shot every 60 s), Esc + Y (abort), Esc (quit to DOS). Shots /tmp/w98t/shots/dk-<run>-<step>.png.
LOG=$1; N=${2:-3}; PLAY=${3:-120}; export QSOCK=/tmp/w98t/q.sock SHOTS=/tmp/w98t/shots
Q="python3 /home/claude/vms/bin/q.py"
k() { $Q key "$@" > /dev/null 2>&1; }
s() { $Q shot "dk-$r-$1" > /dev/null 2>&1; }
for r in $(seq "$N"); do
  t=0; while [ "$(cat "$LOG" 2>/dev/null | grep -c 'runs .*DCNTSHR')" -lt "$r" ] && [ $t -lt 900 ]; do sleep 2; t=$((t + 2)); done
  sleep 12; s a; $Q type test > /dev/null 2>&1; k ret; sleep 5; s b
  k ret; sleep 4; s c; k ret; sleep 6; s d; k spc; sleep 2; k spc; sleep 2; k spc; sleep 2; k esc; sleep 4; s e
  p=0; while [ $p -lt "$PLAY" ]; do sleep 60; p=$((p + 60)); s "p$p"; done
  k esc; sleep 3; s f; k y; sleep 5; s g; k ret; sleep 2; k esc; sleep 3; s h; k esc; sleep 2; k y; sleep 5; s i
done
