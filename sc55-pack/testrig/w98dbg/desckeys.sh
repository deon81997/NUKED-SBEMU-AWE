#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# desckeys.sh LOG N PLAY - drives Descent 1.4 shareware in the QEMU Win98 VM (hmi98.sh desc, QSOCK=/tmp/w98t/q.sock):
# for each of N starts (the "runs ...DCNTSHR" line in the debug log): pilot (new on the 1st), New Game, Rookie, skip
# the briefing, level 1 for PLAY s
# (shot every 60 s), Esc + Enter (abort), up + Enter + Space (Quit, order screen) = back to DOS.
# Keys checked by hand in the VM 11/20 (QEMU sendkey: 'y' is ignored, gray up may come twice: kp_8). Shots /tmp/w98t/shots/dk-<run>-<step>.png.
LOG=$1; N=${2:-3}; PLAY=${3:-120}; export QSOCK=/tmp/w98t/q.sock SHOTS=/tmp/w98t/shots
Q="python3 /home/claude/vms/bin/q.py"
k() { $Q key "$@" > /dev/null 2>&1; }
s() { $Q shot "dk-$r-$1" > /dev/null 2>&1; }
for r in $(seq "$N"); do
  t=0; while [ "$(cat "$LOG" 2>/dev/null | grep -c 'runs .*DCNTSHR')" -lt "$r" ] && [ $t -lt 900 ]; do sleep 2; t=$((t + 2)); done
  sleep 12; s a
  if [ $r = 1 ]; then $Q type test > /dev/null 2>&1; k ret; sleep 5; s b; k ret; sleep 4   # new pilot, keyboard only
  else k ret; sleep 4; fi                                                                   # pilot TEST
  s c; k ret; sleep 4; s d; k ret; sleep 8; s d2; k esc; sleep 8; s e                     # New Game, Rookie, skip briefing
  p=0; while [ $p -lt "$PLAY" ]; do sleep 60; p=$((p + 60)); s "p$p"; done
  k esc; sleep 3; s f; k ret; sleep 6; s g                                                 # Abort game? YES
  k kp_8; sleep 1; k ret; sleep 3; s h; k spc; sleep 5; s i                                # menu: up = Quit, order screen
done
