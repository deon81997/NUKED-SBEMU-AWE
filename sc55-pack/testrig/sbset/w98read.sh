#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sbset)
# w98read.sh ZIP "SBOPTS line" - does Windows 98 read what SETTINGS (SBSET) saved? 86Box store-w98qi-86box acpi
# (P3B-F PII-450, SB16 with its driver: ours at 240/7/3/7, MPU 300, OPL off). The line's AWE32 options go into
# C:\SC55\SBOPTS.TXT as SBSET wrote them (/A /I /D /H /T /P replaced by the rig's ports); SC55W2 starts, then
# SC55W2 /SETTINGS: the host opens the AWE32 tab and takes a picture. Out: /tmp/w98read/{SC55W.LOG,awepage.png,
# SBOPTS.TXT}; prints the AWE32 log line. Base: testrig/awe98/b98.sh (lessons there: CHOICE <= 99 s, xdotool "86Box").
ZIP=$1; LINE=$2; H=$(cd "$(dirname "$0")" && pwd); V=/home/claude/vms; W=/tmp/w98read; D=$W/vm
export B86=${B86:-/home/claude/86box/86box/build/src/86Box} ROMS=${ROMS:-/home/claude/86box/roms-6.0}
rm -rf $W; mkdir -p $W/z; unzip -q "$ZIP" 'NUKEDSB/FILES/WIN98/*' -d $W/z; S=$W/z/NUKEDSB/FILES/WIN98
cp -r --sparse=always $V/w98-86box/acpi $D
I="$D/win98.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 ::/SC55/ROMS 2>/dev/null
for f in SC55W2.EXE SC55W.VXD; do mcopy -o -i $I $S/$f ::/SC55/WIN98/$f; done
mcopy -o -i $I $S/SC55MIDI.DRV ::/WINDOWS/SYSTEM/SC55MIDI.DRV
mtype -i $I ::/WINDOWS/SYSTEM.INI | python3 -c "import sys; s=sys.stdin.read(); sys.stdout.write(s if 'sc55midi' in s.lower() else s.replace('[drivers]\r\n','[drivers]\r\nmidi1=sc55midi.drv\r\n',1))" > $W/system.ini
grep -qi sc55midi $W/system.ini && mcopy -o -i $I $W/system.ini ::/WINDOWS/SYSTEM.INI
mcopy -o -i $I ${AWEROM:-/home/claude/tmp/p52/NUKEDSB/FILES/ROMS/AWE32.RAW} ::/SC55/ROMS/AWE32.RAW
echo "$LINE" | python3 -c "
import sys, re
t = [x for x in sys.stdin.read().split() if not re.match(r'(?i)^/(A|I|D|H|T|P|OPL):', x)]
sys.stdout.write(' '.join(['/A:240', '/I:7', '/D:3', '/H:7', '/T:6', '/P:300', '/OPL:0'] + t) + '\r\n')" > $W/SBOPTS.TXT
mcopy -o -i $I $W/SBOPTS.TXT ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nJoystick=0\r\nJoyBios=0\r\nPadKeys=0\r\n' > $W/sc55w.ini; mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
{ echo '@ECHO OFF'; echo 'START C:\SC55\WIN98\SC55W2.EXE'; echo 'CHOICE /T:Y,90 /C:Y >NUL'
  echo 'START C:\SC55\WIN98\SC55W2.EXE /SETTINGS'; echo 'ECHO A > C:\STEP1.TXT'
  echo 'CHOICE /T:Y,90 /C:Y >NUL'; echo 'ECHO D > C:\DONE1.TXT'; } | sed 's/$/\r/' > $W/T.BAT
mcopy -o -i $I $W/T.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T.BAT"
k() { DISPLAY=:86 xdotool key --delay 120 "$@"; sleep 1; }
shot() { w=$(DISPLAY=:86 xdotool search --name "86Box" | tail -1); [ -n "$w" ] && DISPLAY=:86 import -window "$w" "$1" 2>/dev/null; }
pgrep -f 'Xvfb :86' >/dev/null || { Xvfb :86 -screen 0 1024x768x24 >/dev/null 2>&1 & sleep 2; }   # 86run.sh skips it when another Xvfb (:9) runs
sh $V/bin/86run.sh $D; n=0; done=
while [ $n -lt ${SECS:-900} ]; do sleep 15; n=$((n + 15))
  if [ -z "$done" ] && mcopy -n -i $I ::/STEP1.TXT $W/step 2>/dev/null; then
    sleep 25; shot $W/settings.png; k ctrl+Tab; k ctrl+Tab; k ctrl+Tab; sleep 2; shot $W/awepage.png; done=$n; k Escape; fi
  [ -n "$done" ] && [ $((n - done)) -ge 30 ] && break
done
sh $V/bin/86run.sh $D stop
mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG $W/SC55W.LOG 2>/dev/null; mcopy -n -i $I ::/SC55/SBOPTS.TXT $W/SBOPTS.AFTER 2>/dev/null
echo "SBOPTS: $(tr -d '\r' < $W/SBOPTS.TXT)"; tr -d '\r' < $W/SC55W.LOG 2>/dev/null | grep -E 'AWE32' | head -6
rm -rf $D
