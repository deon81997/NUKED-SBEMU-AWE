#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# m98.sh TAG WIN98DIR [SECS] - the "AWE32 MIDI Synth" Windows MIDI device on 86Box (store-w98qi-86box acpi: P3B-F
# Pentium II 450, SB16 with its driver; ours at 240/7/3/7, MPU 300, EMU8000 620, 8 MB sample RAM). From WIN98DIR:
# SC55W2.EXE, SC55W.VXD, SC55MIDI.DRV (to WINDOWS\SYSTEM), WSEQ.EXE (test sequencer: i686-w64-mingw32-gcc -O2 -s
# -mwindows -o WSEQ.EXE sc55-pack/win98/test/wseq.c -lwinmm). StartUp batch: MIDI Mapper ->
# "AWE32 MIDI Synth" (REGEDIT), SC55W2, then: IDLEMTR (CPU left, idle), Media Player (MPLAYER /play /close) song via
# the Mapper, WSEQ song at volume 100 then 25 (midiOutSetVolume), WSEQ in the background + IDLEMTR (CPU while it
# plays), WSEQ dense note test (DENSE.MID, voice stealing), WSEQ + AWELOAD in this DOS box (a DOS box takes the
# EMU8000; 10 s later the device has it back), WSEQ + AWELOAD in a second DOS box that closes (back at once).
# env: SBK1/SBK2 = banks hidden under C:\SC55\EXTRA (found by content, copied to ROMS), BANK (GM|GS|MT32),
#      SONG (MIDI file, default avmidi 0A.mid), SCROMS (ROM folder: only AWE32.RAW, no SC-55 boot), OPTS.
# Results in /tmp/w86/m-TAG: SC55W.LOG, REC.WAV, WSEQ.LOG, IDLE.TXT, ROMS.TXT (ROMS folder listing), shot.
T=$1; SRC=$2; SECS=${3:-900}
H=$(cd "$(dirname "$0")" && pwd); V=/home/claude/vms; W=/tmp/w86; D=$W/acpi-$T
export B86=${B86:-/home/claude/86box/src/build/src/86Box} ROMS=${ROMS:-/tmp/dl/roms/roms-6.0}
SCR=${SCROMS:-/tmp/awerom}; SONG=${SONG:-$H/../../upstream/Nuked-SC55/test/integration/avmidi/0A.mid}
mkdir -p $W && rm -rf $D && cp -r --sparse=always $V/w98-86box/acpi $D && cp --sparse=always $V/shared/test.img $W/d-$T.img
sed -i "s#^hdd_02_fn = .*#hdd_02_fn = $W/d-$T.img#" $D/86box.cfg
I="$D/win98.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 ::/SC55/ROMS ::/SC55/EXTRA ::/SC55/EXTRA/BANKS 2>/dev/null
for f in SC55W2.EXE SC55W.VXD; do mcopy -o -i $I $SRC/$f ::/SC55/WIN98/$f; done
mcopy -o -i $I $SRC/SC55MIDI.DRV ::/WINDOWS/SYSTEM/SC55MIDI.DRV
# register the driver as SC55W /INSTALL does (SYSTEM.INI [drivers] midi1=)
mtype -i $I ::/WINDOWS/SYSTEM.INI | python3 -c "import sys; s=sys.stdin.read(); sys.stdout.write(s if 'sc55midi' in s.lower() else s.replace('[drivers]\r\n','[drivers]\r\nmidi1=sc55midi.drv\r\n',1))" > $W/system.ini
grep -qi sc55midi $W/system.ini && mcopy -o -i $I $W/system.ini ::/WINDOWS/SYSTEM.INI
mcopy -o -i $I $SCR/* ::/SC55/ROMS/
[ -n "$SBK1" ] && mcopy -o -i $I "$SBK1" ::/SC55/EXTRA/BANKS/BANK1.DAT
[ -n "$SBK2" ] && mcopy -o -i $I "$SBK2" ::/SC55/EXTRA/OTHER.BIN
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:300 /OPL:0 %s\r\n' "${OPTS:-/AWE:620 /AWERAM:8192}" > $W/sbopts.txt; mcopy -o -i $I $W/sbopts.txt ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nRecordWav=C:\\REC.WAV\r\nRecordSecs=%s\r\nJoystick=0\r\nJoyBios=0\r\nPadKeys=0\r\n[AWE32 MIDI]\r\nSynth=1\r\nBank=%s\r\nReverb=4\r\nChorus=2\r\n' "${RECSECS:-140}" "${BANK:-GM}" > $W/sc55w.ini
mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
G=$W/g-$T; rm -rf $G; mkdir -p $G/T/R
cp "$SONG" $G/T/SONG.MID; python3 $H/dense.py $G/T/DENSE.MID 30 > /dev/null
cp $SRC/WSEQ.EXE $G/T/; cp $H/../cpu/IDLEMTR.EXE $G/T/
nasm -f bin -o $G/T/AWELOAD.COM $H/../awe/aweload.asm
printf 'REGEDIT4\r\n\r\n[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Multimedia\\MIDIMap]\r\n"CurrentInstrument"="AWE32 MIDI Synth"\r\n"UseScheme"=dword:00000000\r\n"AutoScheme"=dword:00000000\r\n\r\n' > $G/T/MAP.REG
printf '@ECHO OFF\r\nD:\\T\\AWELOAD\r\nC:\\WINDOWS\\COMMAND\\CHOICE /T:Y,8 /C:Y >NUL\r\n' > $G/T/AWEL.BAT
{ echo '@ECHO OFF'; echo 'REGEDIT /S D:\T\MAP.REG'
  echo 'START C:\SC55\WIN98\SC55W2.EXE'; echo "CHOICE /T:Y,${WAIT:-60} /C:Y >NUL"
  echo 'D:'; echo 'CD \T'
  echo 'DIR /B C:\SC55\ROMS > D:\T\R\ROMS.TXT'
  echo 'IDLEMTR 15 D:\T\R\IDLE.TXT idle'
  echo 'START /W C:\WINDOWS\MPLAYER.EXE /play /close D:\T\SONG.MID'
  echo 'CHOICE /T:Y,5 /C:Y >NUL'
  echo 'START /W D:\T\WSEQ.EXE AWE32 D:\T\SONG.MID 100 30'
  echo 'START /W D:\T\WSEQ.EXE AWE32 D:\T\SONG.MID 25 30'
  echo 'CHOICE /T:Y,5 /C:Y >NUL'
  echo 'START D:\T\WSEQ.EXE AWE32 D:\T\SONG.MID 100 25'
  echo 'CHOICE /T:Y,3 /C:Y >NUL'
  echo 'IDLEMTR 15 D:\T\R\IDLE.TXT song'
  echo 'CHOICE /T:Y,10 /C:Y >NUL'
  echo 'START D:\T\WSEQ.EXE AWE32 D:\T\DENSE.MID 100 32'
  echo 'CHOICE /T:Y,3 /C:Y >NUL'
  echo 'IDLEMTR 15 D:\T\R\IDLE.TXT dense'
  echo 'CHOICE /T:Y,20 /C:Y >NUL'
  echo 'START D:\T\WSEQ.EXE AWE32 D:\T\SONG.MID 100 45'
  echo 'CHOICE /T:Y,8 /C:Y >NUL'
  echo 'AWELOAD'
  echo 'CHOICE /T:Y,25 /C:Y >NUL'
  echo 'START D:\T\WSEQ.EXE AWE32 D:\T\SONG.MID 100 40'
  echo 'CHOICE /T:Y,8 /C:Y >NUL'
  echo 'START /W C:\COMMAND.COM /C D:\T\AWEL.BAT'
  echo 'CHOICE /T:Y,30 /C:Y >NUL'
  echo 'ECHO DONE > D:\T\R\DONE.TXT'; } | sed 's/$/\r/' > $G/T/T5.BAT
mcopy -s -o -i $W/d-$T.img@@32256 $G/T ::/
mcopy -o -i $I $G/T/T5.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T5.BAT"
sh $V/bin/86run.sh $D; T0=$(date +%s)
n=0; while [ $n -lt "$SECS" ]; do sleep 20; n=$((n + 20))
  mcopy -n -i $W/d-$T.img@@32256 ::/T/R/DONE.TXT /tmp/m98done-$T 2>/dev/null && break
  # Media Player's first start asks "settings have been changed by another program": answer Yes (Enter)
  if [ $n -ge 100 ] && sh $V/bin/86run.sh $D shot $W/mp-$T.png > /dev/null 2>&1 && python3 -c "
import sys; from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); w, h = im.size
n = sum(1 for y in range(180, 205) for x in range(140, 520, 4) if (lambda p: p[2] > 100 and p[0] < 40 and p[1] < 150)(im.getpixel((x, y))))
sys.exit(0 if n > 600 else 1)" $W/mp-$T.png; then sh $V/bin/86run.sh $D key Return; echo "Media Player dialog answered at $n s"; fi
  [ -n "${SHOTEVERY:-}" ] && sh $V/bin/86run.sh $D shot $W/shot-$T-$n.png > /dev/null 2>&1; done
echo "host seconds from VM start to DONE: $(( $(date +%s) - T0 )) (compare SC55W.LOG's last time stamp: 86Box speed)"
sh $V/bin/86run.sh $D shot $W/shot-$T.png > /dev/null 2>&1; sh $V/bin/86run.sh $D stop
R=$W/m-$T; rm -rf $R; mkdir -p $R; rm -f /tmp/m98done-$T
mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG ::/REC.WAV ::/WSEQ.LOG $R/ 2>/dev/null
mcopy -n -i $W/d-$T.img@@32256 ::/T/R/IDLE.TXT ::/T/R/ROMS.TXT ::/T/R/DONE.TXT $R/ 2>/dev/null
cp $W/shot-$T.png $R/ 2>/dev/null
echo "== ROMS"; tr -d '\r' < $R/ROMS.TXT 2>/dev/null | tr '\n' ' '; echo; echo "== IDLE"; tr -d '\r' < $R/IDLE.TXT 2>/dev/null
echo "== WSEQ"; tr -d '\r' < $R/WSEQ.LOG 2>/dev/null
tr -d '\r' < $R/SC55W.LOG | grep -E 'AWE32|activity|attach|failed' | tail -30
rm -rf $D $W/d-$T.img
