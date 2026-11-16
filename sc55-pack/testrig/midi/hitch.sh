#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# hitch.sh TAG [HC] - beta.128 USB MIDI timing: Duke 3D (attract mode: music + digital sound) with SBEMU /MUSB:1
# /DBG:COM1 on QEMU + the fake Uno (UART pacing), COM1 -> TCP -> DBGLOG.PY; prints the "usbmidi" stats summary.
#   env G=/tmp/g (Duke unpacked in $G/duke with DUKE3D.CFG: MusicDevice 4, MidiPort 0x330, FXDevice 0), SECS (90),
#   SBX (more SBEMU options), OLD=dir with an older SBEMU.EXE (its /DBG stats have no usbmidi line), PORT (5700)
H=$(cd "$(dirname "$0")" && pwd); G=${G:-/tmp/g}; T=$1; HC=${2:-uhci}; SECS=${SECS:-90}; PORT=${PORT:-5700}
SB="SBEMU /VMPU /JOY:1 /MUSB:1 /DBG:COM1:115200:1 $SBX"; DIRS="$G/duke:DUKE"
[ -n "$OLD" ] && { SB="C:\\OLD\\SBEMU /VMPU /JOY:1 /MUSB:1 /DBG:COM1:115200:1 $SBX"; DIRS="$DIRS,$OLD:OLD"; }
L=/tmp/hitch-$T.log; rm -f $L
( for i in $(seq 60); do python3 -I "$H/../../pack/FILES/SC55/DBGLOG.PY" "socket://127.0.0.1:$PORT" -o $L -q --timeout $((SECS + 60)) 2>/dev/null && break; sleep 1; done ) &
D=$!
RIGSERIAL=$PORT RIGTAG=-hitch-$T python3 "$H/rig.py" $HC "$SB|CD \\DUKE|DUKE3D" $SECS --dir "$DIRS" > /dev/null 2>&1
sleep 2; kill $D 2>/dev/null; pkill -f "DBGLOG.PY socket://127.0.0.1:$PORT"
python3 "$H/hitchsum.py" $L
