#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# r86.sh TAG WIN98DIR SECS [SWITCH] - Win98 in 86Box (VM store store-w98qi-86box: vms/w98-86box/acpi, P3B-F PII-450,
# SB16 with its Windows driver = a real Windows wave out + "SB16 MIDI Out"), SC55W from WIN98DIR (SC55W2.EXE +
# SC55W.VXD) copied in by hand (no installer: the SB16 driver keeps 220/330/388, so ours is /A:240 /I:7 /D:3 /H:7
# /P:300 /OPL:0), a StartUp batch starts SC55W, waits for the synth, (SWITCH=1: MSWITCH.EXE switches the MIDI output
# every 15 s) and runs Duke 3D's attract mode (music 300, effects SB16 240) for SECS. Results: /tmp/w86/r-TAG
# (SC55W.LOG, MSWITCH.LOG, REC.WAV). Needs store-86rig (86Box), store-86box-roms, store-personal (ROMs), /tmp/g/duke,
# /tmp/tb/MSWITCH.EXE. The activity lines' "load" = the speed A/B metric (TAG old / new, same SECS).
T=$1; SRC=$2; SECS=${3:-600}; SW=${4:-0}; V=/home/claude/vms; W=/tmp/w86; D=$W/acpi
ROMS=${ROMS:-/home/claude/tmp/p52/NUKEDSB/FILES/ROMS}
mkdir -p $W && rm -rf $D && cp -r --sparse=always $V/w98-86box/acpi $D && cp --sparse=always $V/shared/test.img $W/d.img
sed -i "s#^hdd_02_fn = .*#hdd_02_fn = $W/d.img#" $D/86box.cfg
I="$D/win98.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 ::/SC55/ROMS 2>/dev/null
for f in SC55W.EXE SC55W2.EXE SC55W.VXD; do [ -f $SRC/$f ] && mcopy -o -i $I $SRC/$f ::/SC55/WIN98/$f; done
mcopy -o -i $I $ROMS/* ::/SC55/ROMS/
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:300 /OPL:0\r\n' > $W/sbopts.txt; mcopy -o -i $I $W/sbopts.txt ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nRecordWav=C:\\REC.WAV\r\nJoystick=0\r\nJoyBios=0\r\nPadKeys=0\r\nMidiOut=${MOUT:-synth}\r\n' > $W/sc55w.ini; mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
mkdir -p $W/d/T && rm -rf $W/d/T/DUKE && cp -r /tmp/g/duke $W/d/T/DUKE && cp /tmp/tb/MSWITCH.EXE $W/d/T/
sed -i 's/BlasterAddress = .*/BlasterAddress = 0x240/; s/BlasterInterrupt = .*/BlasterInterrupt = 7/; s/BlasterDma8 = .*/BlasterDma8 = 3/; s/BlasterDma16 = .*/BlasterDma16 = 7/; s/MidiPort = .*/MidiPort = 0x300/' $W/d/T/DUKE/DUKE3D.CFG
{ echo '@ECHO OFF'; echo 'START C:\SC55\WIN98\SC55W2.EXE'; echo "CHOICE /T:Y,${WAIT:-120} /C:Y >NUL"
  [ "$SW" = 1 ] && echo 'START D:\T\MSWITCH.EXE 12 20 40 NEXT'
  echo 'D:'; echo 'CD \T\DUKE'; echo 'DUKE3D.EXE'; } | sed 's/$/\r/' > $W/d/T/T3.BAT
mcopy -s -o -i $W/d.img@@32256 $W/d/T ::/
mcopy -o -i $I $W/d/T/T3.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T3.BAT"
sh $V/bin/86run.sh $D; sleep "$SECS"; sh $V/bin/86run.sh $D shot $W/shot-$T.png > /dev/null 2>&1; sh $V/bin/86run.sh $D stop
R=$W/r-$T; rm -rf $R; mkdir -p $R
mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG ::/REC.WAV $R/ 2>/dev/null; mcopy -n -i $I ::/MSWITCH.LOG $R/ 2>/dev/null
tr -d '\r' < $R/SC55W.LOG | grep -E 'MIDI out [0-9]|synth:|activity|MIDI output|failed|attach' | tail -40
