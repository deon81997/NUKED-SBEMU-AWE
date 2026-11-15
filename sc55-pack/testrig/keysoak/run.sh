#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# keysoak/run.sh [MIN] [EXE_DIR] - idle soak in QEMU (beta.125): MS-DOS 7.1 HIMEM + HDPMI32I + SBEMU /JOY:1 on OHCI with a
# USB keyboard, a USB mouse and testrig/fakepad.py playing idle.txt (noise, drift, trigger chatter, blips); PADUSER.TXT
# puts both sticks and the triggers on keys. SBDIAG SOAK MIN must report 0 keys (KEYSOAK.TXT, last line PASS/FAIL).
# Then KEYS: QEMU types one key on the USB keyboard -> SBDIAG KEYS must list it ("USB keyboard: 1 keys").
# EXE_DIR (default ../../output) lets you run an older SBEMU (beta.123: only BIOS keys are counted).
# One rig at a time: common/run.sh links /tmp/testrig-run-latest (the control key goes to the newest VM).
# JEMM=1: JEMMEX + QPIEMU (ports 60h/64h trapped). Needs store-dos71 (/home/claude/vms/dos71/msdos71.img) and /tmp/rel (an unzipped release for HDPMI32I).
set -u
H=$(cd "$(dirname "$0")" && pwd); R=$(cd "$H/../.." && pwd); MIN=${1:-30}; X=${2:-$R/output}
W=/tmp/keysoak; rm -rf $W; mkdir -p $W; cd $W
cp $X/sbemu.exe SBEMU.EXE; cp $R/output/sbdiag.exe SBDIAG.EXE; cp /tmp/rel/NUKEDSB/FILES/HDPMI/HDPMI32I.EXE .; cp $H/PADUSER.TXT .
JP=(); JL=""
if [ -n "${JEMM:-}" ]; then   # JEMM=1: JEMMEX + QPIEMU (real-mode traps: ports 60h/64h trapped, PS/2 mouse emulation)
  J=/tmp/rel/NUKEDSB/FILES/JEMM; cp $J/JEMMEX.EXE $J/JLOAD.EXE $J/QPIEMU.DLL .
  printf 'DEVICE=C:\\JEMMEX.EXE NOEMS\r\nDOS=HIGH,UMB\r\nFILES=40\r\n' > CONFIG.SYS
  JP=(-p JEMMEX.EXE -p JLOAD.EXE -p QPIEMU.DLL -p CONFIG.SYS); JL='C:\\JLOAD.EXE C:\\QPIEMU.DLL > NUL\r\n'
fi
P=$((4700 + RANDOM % 200))
pkill -f "fakepad.py $P" 2>/dev/null; python3 $R/testrig/fakepad.py $P $H/idle.txt & FP=$!; sleep 0.5
# the second SOAK (1 min, a key typed in it) must FAIL: its KEYSOAK.TXT replaces the first, so keep the first
printf "@ECHO OFF\r\nSET SBEMULOG=C:\\SBLOG.TXT\r\n$JL"'C:\\HDPMI32I.EXE -r -x > NUL\r\nC:\\SBEMU.EXE /JOY:1 > C:\\SB.TXT\r\nC:\\SBDIAG.EXE SOAK %s > NUL\r\nCOPY C:\\KEYSOAK.TXT C:\\SOAK1.TXT > NUL\r\nC:\\SBDIAG.EXE KEYS CLEAR > NUL\r\nECHO KEYNOW> COM1\r\nC:\\SBDIAG.EXE SOAK 1 > NUL\r\nC:\\SBDIAG.EXE KEYS > C:\\KEYS.TXT\r\nTYPE C:\\SB.TXT > C:\\RESULT.LOG\r\nTYPE C:\\KEYSOAK.TXT >> C:\\RESULT.LOG\r\nTYPE C:\\KEYS.TXT >> C:\\RESULT.LOG\r\nTYPE C:\\SOAK1.TXT >> C:\\RESULT.LOG\r\n' "$MIN" > T.BAT
rm -f /tmp/testrig-run-latest
( # host side: wait for KEYNOW on COM1, then type one key (QEMU sends it to the USB keyboard, the newest one)
  for i in $(seq 1 $((MIN * 60 + 400))); do sleep 1; grep -q KEYNOW /tmp/testrig-run-latest/com1.txt 2>/dev/null && break; done
  sleep 10; echo "sendkey a" | socat - UNIX-CONNECT:/tmp/testrig-run-latest/mon.sock >/dev/null 2>&1 ) &
$R/testrig/common/run.sh ${KEEP:+-k} -t $((MIN * 60 + 300)) -p SBEMU.EXE -p SBDIAG.EXE -p HDPMI32I.EXE -p PADUSER.TXT "${JP[@]}" \
  /home/claude/vms/dos71/msdos71.img T.BAT -- -device pci-ohci,id=hc,addr=5 -device usb-kbd,bus=hc.0,port=1 \
  -device usb-mouse,bus=hc.0,port=2 -chardev socket,id=pad,host=127.0.0.1,port=$P -device usb-redir,chardev=pad,bus=hc.0,port=3 \
  -serial file:/tmp/testrig-run-latest/com1.txt | tee $W/out.txt
kill $FP 2>/dev/null
# common/run.sh calls any FAIL line a failure; the control soak FAILs on purpose: the idle soak's own last line decides
RC=1; [ "$(grep -E '^(PASS|FAIL)' $W/out.txt | tail -1)" = PASS ] && RC=0
grep -q "USB keyboard: 1 keys" $W/out.txt || { echo "FAIL: the key QEMU typed was not logged (control)"; exit 1; }
[ $RC = 0 ] && echo "RIG PASS: idle soak $MIN min = 0 keys, typed key logged"; exit $RC
