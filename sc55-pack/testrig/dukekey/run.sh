#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# dukekey/run.sh [SECS] [EXE_DIR] [SEED] - stuck-arrow test (beta.130): QEMU dos71 HIMEM + HDPMI32I + SBEMU /JOY:1 (OHCI,
# no USB device - like the D810) /VSF (busy sound interrupt) /DBG:COM1:2; DKEY.EXE = Duke 3D's INT 9 keyhandler; keys.py
# types held/repeated arrows with Ctrl/Alt over QMP on the PS/2 keyboard. PASS = no key left down. COM1 (key lines) is
# kept in /tmp/dukekey/com1.txt. Env: USBKB=1 (keys through a USB keyboard), HOTKEY=1 (Ctrl+Alt+F12 x2), SBARGS, DBGL. Needs store-dos71, /tmp/rel (unzipped release) and TimGM6mb.sf2 (/tmp/dl/timgm6mb).
set -u
H=$(cd "$(dirname "$0")" && pwd); R=$(cd "$H/../.." && pwd); S=${1:-60}; X=${2:-$R/output}; SEED=${3:-1}
W=/tmp/dukekey; rm -rf $W; mkdir -p $W; cd $W
export PATH=/tmp/dl/djgpp/djgpp/bin:$PATH
i586-pc-msdosdjgpp-gcc -O2 -o DKEY.EXE $H/dkey.c || exit 2
cp $X/sbemu.exe SBEMU.EXE; cp $R/output/sbdiag.exe SBDIAG.EXE; cp /tmp/rel/NUKEDSB/FILES/HDPMI/HDPMI32I.EXE .
cp ${SF2:-/tmp/dl/timgm6mb/x/usr/share/sounds/sf2/TimGM6mb.sf2} TIMGM6MB.SF2
printf "@ECHO OFF\r\nC:\\\\HDPMI32I.EXE -r -x > NUL\r\nC:\\\\SBEMU.EXE /JOY:1 /VSF:C:\\\\TIMGM6MB.SF2 /DBG:COM1:${DBGL:-2} ${SBARGS:-} > C:\\\\SB.TXT\r\nC:\\\\DKEY.EXE $S > NUL\r\nC:\\\\SBDIAG.EXE KEYS > C:\\\\K.TXT\r\nTYPE C:\\\\K.TXT > C:\\\\RESULT.LOG\r\nTYPE C:\\\\DKEY.TXT >> C:\\\\RESULT.LOG\r\n" > T.BAT
rm -f /tmp/testrig-run-latest
( for i in $(seq 1 240); do sleep 1; [ -S /tmp/testrig-run-latest/qmp.sock ] && break; done
  HOTKEY=${HOTKEY:-} python3 $H/keys.py /tmp/testrig-run-latest/qmp.sock /tmp/testrig-run-latest/com2.txt $((S - 5)) $SEED > $W/keys.txt 2>&1 ) &
QAUDIO=${QAUDIO:-none,id=snd,timer-period=2000} $R/testrig/common/run.sh -k -t $((S + 240)) -p DKEY.EXE -p SBEMU.EXE -p SBDIAG.EXE -p HDPMI32I.EXE -p TIMGM6MB.SF2 \
  /home/claude/vms/dos71/msdos71.img T.BAT -- -device pci-ohci,id=hc,addr=5 ${USBKB:+-device usb-kbd,bus=hc.0,port=1} -serial file:/tmp/testrig-run-latest/com1.txt \
  -serial file:/tmp/testrig-run-latest/com2.txt -qmp unix:/tmp/testrig-run-latest/qmp.sock,server=on,wait=off > $W/out.txt 2>$W/err.txt
RC=$?; wait; cp /tmp/testrig-run-latest/com1.txt $W/ 2>/dev/null
grep -v "^[0-9A-F][0-9A-F] " $W/out.txt | tail -25; cat $W/keys.txt; exit $RC
