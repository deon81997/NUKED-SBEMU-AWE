#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
# mpu.sh NAME SBEMU.EXE "SBEMU options" "DOS commands; ..." [SECS] [himem|jemm] : AWE32 + MPU-401 cases (beta.121-awe.2).
# 86Box P2B-LS PII-400 (p2bls-uhci.cfg, ES1371, UHCI), MS-DOS 7.1 (store-dos71), HIMEM only (default) or JEMMEX+QPIEMU.
# C:\SC55 gets SBEMU, HDPMI32I, SBDIAG, MIDPLAY, SBSET, MPUTEST, AWELOAD.COM, T.MID (genmid.py busy song) and ROMS
# (store-personal ROMs: SC-55 sets + AWEROM.BIN). AUTOEXEC: HDPMI32I -r, SBEMU opts > C:\SB.TXT, SBDIAG START, the
# commands (from C:\SC55), SBDIAG > C:\DIAG.TXT. Records 86Box audio for SECS (default 40) of emulated time.
# Out: /tmp/mpu-NAME/{SB,DIAG,OUT}.TXT (OUT = what the commands printed, if they redirect to C:\OUT.TXT), aud/main.wav.
# ENV: ENG=qemu (QEMU TCG + AC97 instead of 86Box: fast enough for the SC-55); EXTRA="file ..." more files into C:\SC55; PRE="DOS line" before SBEMU (e.g. SET SBEMULOG=...).
set -e
H=$(cd $(dirname $0); pwd); TR=$(dirname $H); P=$(dirname $TR); R=/home/claude/rel/NUKEDSB/FILES; O=$P/output
N=$1; EXE=$2; OPTS=$3; CMDS=$4; SECS=${5:-40}; MODE=${6:-himem}; V=/tmp/mpu-$N; rm -rf $V; mkdir -p $V/c/SC55/ROMS
cp --sparse=always /home/claude/vms/dos71/msdos71.img $V/hd.img
cp $R/HDPMI/HDPMI32I.EXE $R/JEMM/JEMMEX.EXE $R/JEMM/JLOAD.EXE $R/JEMM/QPIEMU.DLL $V/c/SC55/
cp $EXE $V/c/SC55/SBEMU.EXE; cp $O/sbdiag.exe $V/c/SC55/SBDIAG.EXE; cp $O/midplay.exe $V/c/SC55/MIDPLAY.EXE
cp $O/sbset.exe $V/c/SC55/SBSET.EXE; cp $O/mputest.exe $V/c/SC55/MPUTEST.EXE
nasm -f bin -o $V/c/SC55/AWELOAD.COM $H/aweload.asm
for f in /home/claude/tmp/p52/NUKEDSB/FILES/ROMS/*; do case $(basename $f) in ROMHASH.CAC) ;; *) cp $f $V/c/SC55/ROMS/ ;; esac; done
for f in ${EXTRA:-}; do cp $f $V/c/SC55/; done
python3 $TR/midplay/genmid.py $V/c/SC55/T.MID 60 2 > /dev/null
if [ $MODE = jemm ]; then C='DEVICE=C:\SC55\JEMMEX.EXE NOEMS'; Q='C:\SC55\JLOAD C:\SC55\QPIEMU.DLL'; else C='DEVICE=C:\DOS\HIMEM.SYS'; Q=REM; fi
printf '%s\r\nDOS=HIGH\r\nFILES=40\r\n' "$C" > $V/c/CONFIG.SYS
{ printf '@ECHO OFF\r\nPATH C:\\DOS\r\n%s\r\nC:\\SC55\\HDPMI32I -r\r\nCD \\SC55\r\n%s\r\n' "$Q" "${PRE:-REM}"
  printf 'SBEMU %s > C:\\SB.TXT\r\nSBDIAG START > NUL\r\n' "$OPTS"
  echo "$CMDS" | tr ';' '\n' | sed 's/^ *//; s/$/\r/'
  printf 'SBDIAG > C:\\DIAG.TXT\r\nECHO DONE > C:\\DONE.TXT\r\n'; } > $V/c/AUTOEXEC.BAT
mcopy -o -s -i $V/hd.img@@32256 $V/c/* ::/
if [ "${ENG:-86box}" = qemu ]; then   # ENG=qemu: fast TCG (the SC-55 runs in real time), AC97, audio to aud/main.wav
  mkdir -p $V/aud
  timeout $((SECS + 60)) qemu-system-i386 -machine pc -cpu pentium3 -m 64 -vga std -display none -no-reboot \
    -drive file=$V/hd.img,format=raw,if=ide,index=0,cache=writethrough -boot c \
    -audiodev wav,id=snd,path=$V/aud/main.wav,timer-period=2000 -device AC97,audiodev=snd > $V/qemu.log 2>&1 &
  for i in $(seq $(( (SECS + 60) / 2 ))); do sleep 2; mtype -i $V/hd.img@@32256 ::/DONE.TXT > /dev/null 2>&1 && break; done
  sleep 2; pkill -f "qemu-system-i386.*$V/hd.img" || true; sleep 1
else
sed -e 's#^hdd_01_fn = .*#hdd_01_fn = hd.img#' -e 's/^hdd_01_parameters = .*/hdd_01_parameters = 63, 16, 2080, 0, ide/' $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
bash $TR/86box/run.sh $V > /dev/null
python3 $H/drv.py $V $SECS > $V/drv.log 2>&1 || true
python3 $TR/86box/a.py $V/a.sock release > /dev/null 2>&1 || true
for i in $(seq 60); do sleep 2; mtype -i $V/hd.img@@32256 ::/DONE.TXT > /dev/null 2>&1 && break; done
pkill -f "86Box.*$V" || true; sleep 1
fi
for f in SB DIAG OUT; do mtype -i $V/hd.img@@32256 ::/$f.TXT 2>/dev/null | tr -d '\r' > $V/$f.TXT || true; done
grep -aiE 'awe|mpu|sc-55|synth|trap|error|fail' $V/SB.TXT | head -10
grep -a "Measured\|interrupts (\|uses .*CPU\|needs .*real time" $V/DIAG.TXT || echo "no SBDIAG report"
[ -s $V/OUT.TXT ] && head -5 $V/OUT.TXT
python3 $H/lev.py $V/aud/main.wav 2>/dev/null | cut -c1-200 || true
