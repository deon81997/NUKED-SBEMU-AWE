#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# b98.sh TAG WIN98DIR [SECS] - AWE32 bank manager on 86Box (store-w98qi-86box acpi: P3B-F Pentium II 450, SB16 with
# its driver; ours at 240/7/3/7, MPU 300, EMU8000 620, 8 MB sample RAM, /AWEMPU:GM). One boot:
#  SC55W.INI [AWE32 MIDI] Bank1 = TimGM6mb.sf2, Bank2 = stand-in SBK (12 programs), Preset0_0 = the SF2's
#    piano; WSEQ plays BANKS.MID (banksmid.py: CC0/CC32 user banks, empty bank, drum kit; 80 user-bank notes),
#    SONGU.MID (a song on bank 1) + IDLEMTR (CPU), AWELOAD in the DOS box while WSEQ plays (chip handover), BANKS.MID
#    again (banks back), then Settings opens (SC55W2 /SETTINGS) and the host drives the bank dialog by keys:
#    BIG.SBK (5.6 MB) as bank 3 -> refused (RAM full), STANDIN2.SBK as bank 3 -> loaded + SC55W.INI; then Duke 3D
#    with General MIDI on MPU 300 in the DOS box -> the AWE32 MIDI Synth (GM bank) until SECS.
#  then SC55W2 /EXIT + start again: the banks come back from SC55W.INI (bank 3 from the dialog too), BANKS.MID.
# WIN98DIR: SC55W2.EXE SC55W.VXD SC55MIDI.DRV WSEQ.EXE. Results in /tmp/w86/b-TAG (SC55W1/2.LOG = before / after the restart, IDLE.TXT,
# WSEQ.LOG, SC55W.INI, shots). env: KEYS=0 (no dialog), SONG.
T=$1; SRC=$2; SECS=${3:-900}
H=$(cd "$(dirname "$0")" && pwd); V=/home/claude/vms; W=/tmp/w86; D=$W/acpi-$T
export B86=${B86:-/home/claude/86box/src/build/src/86Box} ROMS=${ROMS:-/tmp/dl/roms/roms-6.0}
SCR=${SCROMS:-/tmp/awerom}; SONG=${SONG:-$H/../../upstream/Nuked-SC55/test/integration/avmidi/0A.mid}
SF=/usr/share/sounds/sf2/TimGM6mb.sf2
mkdir -p $W && rm -rf $D && cp -r --sparse=always $V/w98-86box/acpi $D && cp --sparse=always $V/shared/test.img $W/d-$T.img
sed -i "s#^hdd_02_fn = .*#hdd_02_fn = $W/d-$T.img#" $D/86box.cfg
I="$D/win98.img@@32256"; J="$W/d-$T.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 ::/SC55/ROMS 2>/dev/null
for f in SC55W2.EXE SC55W.VXD; do mcopy -o -i $I $SRC/$f ::/SC55/WIN98/$f; done
mcopy -o -i $I $SRC/SC55MIDI.DRV ::/WINDOWS/SYSTEM/SC55MIDI.DRV
mtype -i $I ::/WINDOWS/SYSTEM.INI | python3 -c "import sys; s=sys.stdin.read(); sys.stdout.write(s if 'sc55midi' in s.lower() else s.replace('[drivers]\r\n','[drivers]\r\nmidi1=sc55midi.drv\r\n',1))" > $W/system.ini
grep -qi sc55midi $W/system.ini && mcopy -o -i $I $W/system.ini ::/WINDOWS/SYSTEM.INI
mcopy -o -i $I $SCR/* ::/SC55/ROMS/
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:300 /OPL:0 %s\r\n' "${OPTS:-/AWE:620 /AWERAM:8192 /AWEMPU:GM}" > $W/sbopts.txt; mcopy -o -i $I $W/sbopts.txt ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nRecordWav=C:\\REC.WAV\r\nRecordSecs=%s\r\nJoystick=0\r\nJoyBios=0\r\nPadKeys=0\r\n[AWE32 MIDI]\r\nSynth=1\r\nBank=GM\r\nReverb=4\r\nChorus=2\r\nBank1=D:\\T\\TIMGM.SF2\r\nBank2=D:\\T\\STANDIN.SBK\r\nPreset0_0=0,0,D:\\T\\TIMGM.SF2\r\n' "${RECSECS:-60}" > $W/sc55w.ini
mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
G=$W/g-$T; rm -rf $G; mkdir -p $G/T/R
python3 $H/../awe/sf2sbk.py $SF $G/T/STANDIN.SBK gm 12 > /dev/null; cp $G/T/STANDIN.SBK $G/T/STANDIN2.SBK
python3 $H/../awe/sf2sbk.py $SF $G/T/BIG.SBK gm > /dev/null; cp $SF $G/T/TIMGM.SF2
python3 $H/banksmid.py $G/T/BANKS.MID $G/T/STANDIN.SBK 2; python3 $H/songbank.py "$SONG" $G/T/SONGU.MID 1 > /dev/null
cp $SRC/WSEQ.EXE $G/T/; cp $H/../cpu/IDLEMTR.EXE $G/T/; nasm -f bin -o $G/T/AWELOAD.COM $H/../awe/aweload.asm
cp -r /tmp/g/duke $G/T/DUKE
sed -e 's/BlasterAddress = .*/BlasterAddress = 0x240/' -e 's/BlasterInterrupt = .*/BlasterInterrupt = 7/' \
    -e 's/BlasterDma8 = .*/BlasterDma8 = 3/' -e 's/BlasterDma16 = .*/BlasterDma16 = 7/' -e 's/MidiPort = .*/MidiPort = 0x300/' \
    -e 's/ScreenMode = .*/ScreenMode = 2/' -e 's/FXDevice = .*/FXDevice = 0/' $H/../awe/DUKEGM.CFG > $G/T/DUKE/DUKE3D.CFG
printf '\r\n[Comm Setup]\r\nPlayerName = "DUKE"\r\nRTSName = "DUKE.RTS"\r\nRTSPath = ".\\"\r\nUserPath = ".\\"\r\n' >> $G/T/DUKE/DUKE3D.CFG
{ echo '@ECHO OFF'; echo 'START C:\SC55\WIN98\SC55W2.EXE'; echo "CHOICE /T:Y,${WAIT:-90} /C:Y >NUL"
  echo 'D:'; echo 'CD \T'; echo 'DIR /B C:\SC55\ROMS > D:\T\R\ROMS.TXT'
  echo 'IDLEMTR 15 D:\T\R\IDLE.TXT idle'
  echo 'START /W D:\T\WSEQ.EXE AWE32 D:\T\BANKS.MID 100 30'
  echo 'CHOICE /T:Y,5 /C:Y >NUL'
  echo 'START D:\T\WSEQ.EXE AWE32 D:\T\SONGU.MID 100 30'
  echo 'CHOICE /T:Y,3 /C:Y >NUL'
  echo 'IDLEMTR 15 D:\T\R\IDLE.TXT song-bank1'
  echo 'CHOICE /T:Y,15 /C:Y >NUL'
  echo 'START D:\T\WSEQ.EXE AWE32 D:\T\BANKS.MID 100 25'
  echo 'CHOICE /T:Y,6 /C:Y >NUL'
  echo 'AWELOAD'
  echo 'CHOICE /T:Y,30 /C:Y >NUL'
  echo 'START /W D:\T\WSEQ.EXE AWE32 D:\T\BANKS.MID 100 30'
  echo 'START C:\SC55\WIN98\SC55W2.EXE /SETTINGS'
  echo 'ECHO A > D:\T\R\STEP1.TXT'
  for i in 1 2 3; do echo 'CHOICE /T:Y,80 /C:Y >NUL'; done      # CHOICE waits 99 s at most
  echo 'COPY C:\SC55\WIN98\SC55W.LOG D:\T\R\LOG1.TXT'
  echo 'START /W C:\SC55\WIN98\SC55W2.EXE /EXIT'; echo 'CHOICE /T:Y,15 /C:Y >NUL'
  echo 'START C:\SC55\WIN98\SC55W2.EXE'; echo "CHOICE /T:Y,${WAIT:-90} /C:Y >NUL"
  echo 'START /W D:\T\WSEQ.EXE AWE32 D:\T\BANKS.MID 100 30'
  echo 'CHOICE /T:Y,12 /C:Y >NUL'
  echo 'ECHO DONE > D:\T\R\DONE1.TXT'
  echo 'CD \T\DUKE'; echo 'DUKE3D.EXE'; } | sed 's/$/\r/' > $G/T/T6.BAT
mcopy -s -o -i $J $G/T ::/
mcopy -o -i $I $G/T/T6.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T6.BAT"
R=$W/b-$T; rm -rf $R; mkdir -p $R
k() { DISPLAY=:86 xdotool key --delay 120 "$@"; sleep 1; }    # the 86Box window has the focus (only window)
shot() { w=$(DISPLAY=:86 xdotool search --name "86Box" | tail -1); [ -n "$w" ] && DISPLAY=:86 import -window "$w" "$1" 2>/dev/null; }   # SDL title "86Box v7.0 - NN% - ..."
typ() { python3 -c "
import sys
m={':':'colon','\\\\':'backslash','.':'period',' ':'space','_':'underscore'}
print(' '.join(m.get(c, ('shift+'+c.lower()) if c.isupper() else c) for c in sys.argv[1]))" "$1"; }
sh $V/bin/86run.sh $D; T0=$(date +%s); n=0; keys=${KEYS:-1}
while [ $n -lt "$SECS" ]; do sleep 20; n=$((n + 20))
  if [ "$keys" = 1 ] && mcopy -n -i $J ::/T/R/STEP1.TXT /tmp/b98s-$T 2>/dev/null; then keys=2; sleep 25
    shot $R/settings.png
    k ctrl+Tab; k ctrl+Tab; k ctrl+Tab; shot $R/awepage.png
    k alt+a; sleep 2; k $(typ 'D:\T\BIG.SBK'); k Return; sleep 8          # slot = first free (3)
    shot $R/ramfull.png
    k alt+s; k $(typ 'D:\T\STANDIN2.SBK'); k Return; sleep 8
    shot $R/added.png; k Escape; echo "dialog keys sent at $n s"; fi
  mcopy -n -i $J ::/T/R/DONE1.TXT /tmp/b98d-$T 2>/dev/null && [ -z "$duke" ] && { duke=$n; echo "Duke (GM -> AWE32 MIDI Synth) from $n s"; }
  [ -n "$duke" ] && [ $((n - duke)) -ge ${DUKESECS:-180} ] && break
done
shot $R/duke.png; sh $V/bin/86run.sh $D stop
mcopy -n -i $J ::/T/R/LOG1.TXT $R/SC55W1.LOG; mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG $R/SC55W2.LOG; mcopy -n -i $I ::/WSEQ.LOG $R/ 2>/dev/null
mcopy -n -i $I ::/SC55/WIN98/SC55W.INI $R/SC55W1.INI; mcopy -n -i $J ::/T/R/IDLE.TXT ::/T/R/ROMS.TXT $R/ 2>/dev/null
rm -f /tmp/b98?-$T
echo "== IDLE"; tr -d '\r' < $R/IDLE.TXT 2>/dev/null
for b in 1 2; do echo "== SC55W run $b"; tr -d '\r' < $R/SC55W$b.LOG | grep -E 'AWE32|bank manager|failed' | tail -14; done
echo "== INI"; tr -d '\r' < $R/SC55W1.INI | grep -E '^(Bank|Preset)'
rm -rf $D $W/d-$T.img
