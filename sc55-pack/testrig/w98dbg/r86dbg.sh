#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU
# r86dbg.sh WIN98DIR [SECS] - w98.9 serial debug link on 86Box P3B-F Pentium II 450 (store-w98qi-86box acpi, SB16 with
# its Windows driver: ours at /A:240 /I:7 /D:3 /H:7 /P:300 /OPL:0 as testrig/w98par/r86.sh). COM1 = 86Box "pipe" char
# device on a socat pty pair (/tmp/vcom1 <-> /tmp/hcom1): 86Box write()s each byte at once, so a killed VM loses
# nothing (its "file" device buffers). DBGLOG.PY reads /tmp/hcom1. StartUp batch: SC55W2 (SC55W.INI [Debug] Link=COM1:1),
# Media Player plays TEST.MID, Duke 3D (attract mode, SB 240 + MPU 300) in a new DOS box, after SECS SC55W2 /DBG:HANG
# while Duke plays: the "!" line must be the last one. Needs store-86rig, store-personal (ROMs), /tmp/g/duke (DUKE3D.CFG
# made here), testrig/w98dbg/TEST.MID. Never with QEMU running (2-core host). Log /tmp/w86d/dbg86.log.
# AWE=1 (AWE line, awe.10): OPTS /AWE:620, ROMS = only AWE32.RAW (AWEROM=, from store-86box-roms sound/creative/awe32.raw;
# no SC-55 boot in 86Box), Duke MusicDevice 6 = AWE32 music: EMU8000 traps line, DOS box takes the EMU8000, awe= in the S
# lines while Duke plays, "S awe" voices line, and the /DBG:HANG test while Duke plays AWE music.
SRC=$1; SECS=${2:-240}; WAIT=${WAIT:-360}; V=/home/claude/vms; W=/tmp/w86d; D=$W/acpi; H=$(cd "$(dirname "$0")" && pwd); P=$H/../..
ROMS=${ROMS:-/home/claude/tmp/p52/NUKEDSB/FILES/ROMS}; L=$W/dbg86.log; AWE=${AWE:-0}
if [ $AWE = 1 ]; then ROMS=$W/aweroms; rm -rf $ROMS; mkdir -p $ROMS; cp ${AWEROM:-/tmp/dl/roms/roms-6.0/sound/creative/awe32.raw} $ROMS/AWE32.RAW || exit 1; fi
mkdir -p $W && rm -rf $D && cp -r --sparse=always $V/w98-86box/acpi $D && cp --sparse=always $V/shared/test.img $W/d.img
sed -i "s#^hdd_02_fn = .*#hdd_02_fn = $W/d.img#" $D/86box.cfg
printf '\n[Ports (COM & LPT)]\nserial1_enabled = 1\nserial1_device = pipe\n\n[Named Pipe (COM) #1]\npath = /tmp/vcom1\nmode = 0\n' >> $D/86box.cfg
I="$D/win98.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 ::/SC55/ROMS 2>/dev/null
for f in SC55W2.EXE SC55W.VXD; do mcopy -o -i $I $SRC/$f ::/SC55/WIN98/$f; done
# the "NUKED SB-EMU" MIDI device by hand (as SC55W /INSTALL): DRV + SYSTEM.INI [drivers] midi1=, registry by REGEDIT /S
mcopy -o -i $I $SRC/SC55MIDI.DRV ::/WINDOWS/SYSTEM/SC55MIDI.DRV
mtype -i $I ::/WINDOWS/SYSTEM.INI | sed 's/^midi=mmsystem.dll\r$/&\nmidi1=sc55midi.drv\r/' > $W/system.ini; mcopy -o -i $I $W/system.ini ::/WINDOWS/SYSTEM.INI
M='HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\MediaResources\midi\sc55midi.drv'
mcopy -o -i $I $ROMS/* ::/SC55/ROMS/
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:300 /OPL:0%s\r\n' "$([ $AWE = 1 ] && echo ' /AWE:620')" > $W/sbopts.txt; mcopy -o -i $I $W/sbopts.txt ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nJoystick=0\r\nJoyBios=0\r\n[Debug]\r\nLink=COM1:1\r\n' > $W/sc55w.ini
mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
rm -rf $W/d && mkdir -p $W/d/T $W/d/R && cp -r /tmp/g/duke $W/d/T/DUKE && cp $H/TEST.MID $W/d/T/
printf 'REGEDIT4\r\n\r\n[%s]\r\n"Active"="1"\r\n"Description"="NUKED SB-EMU"\r\n"Driver"="sc55midi.drv"\r\n"FriendlyName"="NUKED SB-EMU"\r\n\r\n[HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Multimedia\\MIDIMap]\r\n"CurrentInstrument"="NUKED SB-EMU"\r\n"UseScheme"=dword:00000000\r\n\r\n' "$M" > $W/d/T/M.REG
cat > $W/d/T/DUKE/DUKE3D.CFG <<'EOF'
[Setup]
SetupVersion = "1.3D"

[Screen Setup]
ScreenMode = 2
ScreenWidth = 320
ScreenHeight = 200

[Sound Setup]
FXDevice = 0
MusicDevice = 4
FXVolume = 220
MusicVolume = 200
NumBits = 16
MixRate = 22000
NumChannels = 2
NumVoices = 8
SoundToggle = 1
MusicToggle = 1
VoiceToggle = 1
AmbienceToggle = 1
ReverseStereo = 0
BlasterAddress = 0x240
BlasterInterrupt = 7
BlasterDma8 = 3
BlasterDma16 = 7
BlasterType = 6
BlasterEmu = 0x620
MidiPort = 0x300

[Comm Setup]
UserPath = ""
RTSName = "DUKE.RTS"
EOF
[ $AWE = 1 ] && sed -i 's/^MusicDevice = .*/MusicDevice = 6/' $W/d/T/DUKE/DUKE3D.CFG   # 6 = AWE32
sed -i 's/$/\r/' $W/d/T/DUKE/DUKE3D.CFG
S='C:\SC55\WIN98\SC55W2.EXE'
{ echo '@ECHO OFF'; echo 'D:'; echo 'CD \T\DUKE'; echo 'DUKE3D.EXE'; } | sed 's/$/\r/' > $W/d/T/DK.BAT
w() { t=$1; while [ $t -gt 0 ]; do c=$t; [ $c -gt 90 ] && c=90; echo "CHOICE /T:Y,$c /C:Y >NUL"; t=$((t - c)); done; }  # CHOICE: 99 s max
{ echo '@ECHO OFF'; echo 'REGEDIT /S D:\T\M.REG'; echo "START $S"; w $WAIT
  echo 'START /W C:\PROGRA~1\WINDOW~1\MPLAYER2.EXE /play /close D:\T\TEST.MID'; echo 'CHOICE /T:Y,10 /C:Y >NUL'
  echo 'ECHO mp> D:\R\MP.TXT'; echo 'START D:\T\DK.BAT'; w $SECS
  echo 'ECHO hang> D:\R\H.TXT'; echo "START $S /DBG:HANG"; echo 'CHOICE /T:Y,60 /C:Y >NUL'; echo 'ECHO done> D:\R\DONE.TXT'; } \
  | sed 's/$/\r/' > $W/d/T/T.BAT
mcopy -s -o -i $W/d.img@@32256 $W/d/T $W/d/R ::/
mcopy -o -i $I $W/d/T/T.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T.BAT"
pkill -f "socat.*vcom1"; rm -f /tmp/vcom1 /tmp/hcom1
setsid socat pty,raw,echo=0,link=/tmp/vcom1 pty,raw,echo=0,link=/tmp/hcom1 < /dev/null > /dev/null 2>&1 & sleep 2
rm -f $L; setsid python3 -I $P/pack/FILES/SC55/DBGLOG.PY /tmp/hcom1 -o $L -q < /dev/null > $W/dbglog.err 2>&1 &
sh $V/bin/86run.sh $D; t0=$(date +%s); n=0; hang=0
while [ $n -lt $((WAIT + SECS + 1800)) ]; do sleep 20; n=$((n + 20))
  grep -q "VXD ! hang test" $L 2>/dev/null && { hang=$(date +%s); sleep 90; break; }; done
sh $V/bin/86run.sh $D shot $W/shot-dbg86.png > /dev/null 2>&1; sh $V/bin/86run.sh $D stop
pkill -f "DBGLOG.PY /tmp/hcom1"; pkill -f "socat.*vcom1"; sleep 2
R=$W/r-dbg86; rm -rf $R; mkdir -p $R; mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG $R/ 2>/dev/null; mcopy -n -s -i $W/d.img@@32256 ::/R $R/ 2>/dev/null
F=0
chk() { c=$(grep -cE "$2" $L); [ "$c" -ge "${3:-1}" ] && echo "  ok   $1 ($c)" || { echo "  FAIL $1 ($c)"; F=1; }; }
echo "86Box PII-450 ($n s): $(grep -vc '^#' $L) lines"
chk "link line" "VXD I Serial debug link: COM1 \(3F8\), 115200 baud, level 1, port claimed from Windows"
chk "VxD loaded" "VXD I VxD loaded: NUKED SB-EMU"
chk "port traps" "VXD I port traps: SB ok, OPL .{1,2}, MPU ok"
echo "  info Windows MIDI device lines: $(grep -cE "VXD I Windows MIDI device (opened|closed)" $L) (0 = Media Player played to the SB16 driver: the hand-made registration is not enough; QEMU main covers it after SC55W /INSTALL)"
chk "DOS box start" "VXD I DOS box start vm="
chk "Duke runs" "VXD I vm=[0-9A-F]{8} runs .*DUKE3D"
chk "Duke uses the SB" "VXD I (vm=[0-9A-F]{8} uses the Sound Blaster|DMA play)" 1
if [ $AWE = 1 ]; then
  chk "EMU8000 traps line" "VXD I AWE32: EMU8000 ports 620-623, A20-A23, E20-E23 trapped"
  chk "DOS box takes the EMU8000" "VXD I AWE32: DOS box vm=[0-9A-F]{8} takes the EMU8000"
  n=$(sed -n '/runs .*DUKE3D/,$p' $L | grep -cE "VXD S vm=.* awe=[1-9]")
  [ $n -ge 5 ] && echo "  ok   Duke plays AWE music ($n S lines with awe>0)" || { echo "  FAIL Duke AWE use ($n)"; F=1; }
  chk "S awe voices line" "VXD S awe vm=[0-9A-F]{8} ports=[1-9][0-9]* voices=[0-9]+/32" 5
  h=$(grep -B3 "VXD ! hang test" $L | grep -cE "VXD S (vm=.* awe=[1-9]|awe vm=)")
  [ $h -ge 1 ] && echo "  ok   hang test while Duke plays AWE music" || { echo "  FAIL no AWE activity right before the hang line"; F=1; }
else chk "Duke MPU" "VXD I MPU-401 (reset|UART mode)" 1; fi
chk "VxD stats" "VXD S vm=[0-9A-F]{8} sb=[0-9]+ mpu=" 20
chk "helper stats" "SC55W S load=" 10
last=$(grep -v '^#' $L | tail -1)
echo "$last" | grep -q "VXD ! hang test" && echo "  ok   the ! line is the last one: ${last#*  }" || { echo "  FAIL last line: $last"; F=1; }
[ $hang -gt 0 ] && echo "  ok   nothing in the 90 s after it" || { echo "  FAIL no hang line"; F=1; }
[ -f $R/R/DONE.TXT ] && { echo "  FAIL Windows went on"; F=1; } || echo "  ok   Windows stayed frozen"
[ $F = 0 ] && echo PASS || echo FAIL
