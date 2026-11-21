#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line, awe.3)
# r98.sh TAG WIN98DIR OPTS [SECS] - AWE32 in a Windows 98 DOS box on 86Box (store-w98qi-86box acpi: P3B-F
# Pentium II 450, SB16 with its Windows driver; ours at 240/7/3/7, MPU 300, EMU8000 620). SC55W2.EXE + SC55W.VXD from
# WIN98DIR copied in by hand, SBOPTS.TXT = OPTS (e.g. "/AWE:620", "" = AWE off).
# A StartUp batch starts SC55W2, waits for the synth, then in a DOS box: IDLEMTR (CPU left, 20 s), AWEDET, AWETIME
# (detection / init / 512 KB upload timings), IDLEMTR again, AWELOAD (16 voices) + IDLEMTR, then Duke 3D with AWE32
# music (attract mode) until SECS. Results in /tmp/w86/a-TAG: IDLE.TXT DET.TXT TIME.TXT, SC55W.LOG, REC.WAV (SC55W's
# own output = the DOS box's sound with the AWE32 mixed in). Needs store-w98qi-86box, store-86rig (86Box), store-86box-roms
# (ROMS=), store-personal (SC-55 ROMs + AWE32.RAW), /tmp/g/duke, nasm (AWETIME/AWEDET/AWELOAD built here).
# env: SCROMS (ROM folder copied to C:\SC55\ROMS; awe.3 used one with only AWE32.RAW: no SC-55 boot in 86Box), WAIT (s
# before the tests), RECSECS (REC.WAV closed after that much sound, default 90: complete on disk), DUKEONLY=1 (no
# IDLEMTR/AWEDET/AWETIME/AWELOAD: REC.WAV starts with Duke's music), SFX=0 (Duke's effects off: music only),
# SHOTEVERY=1 (screenshot every 20 s), CMDARGS (SC55W2.EXE switches, e.g. /AWE:620 instead of OPTS), SERIALLOG (awe.9).
T=$1; SRC=$2; OPTS=$3; SECS=${4:-600}
H=$(cd "$(dirname "$0")" && pwd); V=/home/claude/vms; W=/tmp/w86; D=$W/acpi-$T
export B86=${B86:-/home/claude/86box/src/build/src/86Box} ROMS=${ROMS:-/tmp/dl/roms/roms-6.0}
SCR=${SCROMS:-/home/claude/tmp/p52/NUKEDSB/FILES/ROMS}
mkdir -p $W && rm -rf $D && cp -r --sparse=always $V/w98-86box/acpi $D && cp --sparse=always $V/shared/test.img $W/d-$T.img
sed -i "s#^hdd_02_fn = .*#hdd_02_fn = $W/d-$T.img#" $D/86box.cfg
# awe.9: SERIALLOG=file = COM1 to that file (86Box "File (COM)", buffered: the last <4 KB are lost when the VM is killed);
# with CMDARGS=/DBG:COM1 = the Windows 98 debug link during Duke (DBGLOG.PY -f can read the file afterwards)
[ -n "${SERIALLOG:-}" ] && printf '\n[Ports (COM & LPT)]\nserial1_enabled = 1\nserial1_device = file\n\n[File (COM) #1]\npath = %s\n' "$SERIALLOG" >> $D/86box.cfg
I="$D/win98.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 ::/SC55/ROMS 2>/dev/null
for f in SC55W2.EXE SC55W.VXD; do mcopy -o -i $I $SRC/$f ::/SC55/WIN98/$f; done
mcopy -o -i $I $SCR/* ::/SC55/ROMS/
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:300 /OPL:0 %s\r\n' "$OPTS" > $W/sbopts.txt; mcopy -o -i $I $W/sbopts.txt ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nRecordWav=C:\\REC.WAV\r\nRecordSecs=%s\r\nJoystick=0\r\nJoyBios=0\r\nPadKeys=0\r\n' "${RECSECS:-90}" > $W/sc55w.ini
mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
G=$W/g-$T; rm -rf $G; mkdir -p $G/T/R
cp -r /tmp/g/htic $G/T/HTIC
printf 'snd_musicdevice\t9\r\nsnd_sfxdevice\t3\r\nsnd_sbport\t576\r\nsnd_sbirq\t7\r\nsnd_sbdma\t3\r\nsnd_mport\t768\r\nsnd_musicvolume\t12\r\nsnd_channels\t3\r\nusemouse\t0\r\nusejoystick\t0\r\n' > $G/T/HTIC/HERETIC.CFG
for a in awetime; do nasm -f bin -o $G/T/AWETIME.COM $H/$a.asm; done
nasm -f bin -o $G/T/AWEDET.COM $H/../awe/awedet.asm; nasm -f bin -o $G/T/AWELOAD.COM $H/../awe/aweload.asm
cp $H/../cpu/IDLEMTR.EXE $G/T/
{ echo '@ECHO OFF'; echo "START C:\\SC55\\WIN98\\SC55W2.EXE ${CMDARGS:-}"; echo "CHOICE /T:Y,${WAIT:-100} /C:Y >NUL"
  echo 'D:'; echo 'CD \T'
  if [ -z "${DUKEONLY:-}" ]; then
    echo 'IDLEMTR 20 D:\T\R\IDLE.TXT start'
    echo 'AWEDET > D:\T\R\DET.TXT'; echo 'AWETIME > D:\T\R\TIME.TXT'
    echo 'IDLEMTR 20 D:\T\R\IDLE.TXT after'
    echo 'AWELOAD'; echo 'IDLEMTR 20 D:\T\R\IDLE.TXT voices16'
  fi
  echo 'ECHO DONE > D:\T\R\DONE.TXT'
  echo 'SET BLASTER=A240 I7 D3 H7 P300 E620 T6'; echo 'CD \T\HTIC'; echo 'HERETIC.EXE'; } | sed 's/$/\r/' > $G/T/T3.BAT
mcopy -s -o -i $W/d-$T.img@@32256 $G/T ::/
mcopy -o -i $I $G/T/T3.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T3.BAT"
sh $V/bin/86run.sh $D
n=0; while [ $n -lt "$SECS" ]; do sleep 20; n=$((n + 20))
  [ $n -ge 100 ] && [ $n -le 160 ] && DISPLAY=:86 xdotool search --name "86Box" key --window %@ Return 2>/dev/null
  [ -n "${SHOTEVERY:-}" ] && sh $V/bin/86run.sh $D shot $W/shot-$T-$n.png > /dev/null 2>&1; done
sh $V/bin/86run.sh $D shot $W/shot-$T.png > /dev/null 2>&1; sh $V/bin/86run.sh $D stop
R=$W/a-$T; rm -rf $R; mkdir -p $R
mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG ::/REC.WAV $R/ 2>/dev/null
mcopy -n -i $I ::/SC55/SBOPTS.TXT $R/ 2>/dev/null
mcopy -n -i $W/d-$T.img@@32256 ::/T/R/IDLE.TXT ::/T/R/DET.TXT ::/T/R/TIME.TXT ::/T/R/DONE.TXT $R/ 2>/dev/null
for f in IDLE DET TIME; do echo "== $f"; tr -d '\r' < $R/$f.TXT 2>/dev/null; done
tr -d '\r' < $R/SC55W.LOG | grep -E 'AWE32|activity|attach|failed' | tail -12
rm -rf $D $W/d-$T.img
