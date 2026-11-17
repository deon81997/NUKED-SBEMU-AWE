#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# cpu86.sh TAG WIN98DIR [LINK] - w98.9 debug link cost on 86Box P3B-F Pentium II 450 (store-w98qi-86box acpi, SB16 with
# its Windows driver: ours at /A:240 /P:300 /OPL:0 as testrig/w98par/r86.sh). SC55W2.EXE + SC55W.VXD from WIN98DIR,
# SC55W.INI [Debug] Link=LINK (e.g. COM1:1; empty = off). A StartUp batch starts SC55W2, waits 60 s, then in a DOS box
# TRAPB 24E x3 (DSP status, 8M trapped INs each) and TRAPB 301 x3 (MPU status); ticks of 55 ms in /tmp/w86d/r-TAG/TB.TXT.
# Needs store-86rig (86Box), store-w98qi-86box; never with QEMU running (2-core host).
T=$1; SRC=$2; LINK=$3; V=/home/claude/vms; W=/tmp/w86d; D=$W/acpi; H=$(cd "$(dirname "$0")" && pwd)
mkdir -p $W && rm -rf $D && cp -r --sparse=always $V/w98-86box/acpi $D && cp --sparse=always $V/shared/test.img $W/d.img
sed -i "s#^hdd_02_fn = .*#hdd_02_fn = $W/d.img#" $D/86box.cfg
I="$D/win98.img@@32256"
mmd -i $I ::/SC55 ::/SC55/WIN98 2>/dev/null
for f in SC55W2.EXE SC55W.VXD; do mcopy -o -i $I $SRC/$f ::/SC55/WIN98/$f; done
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:300 /OPL:0\r\n' > $W/sbopts.txt; mcopy -o -i $I $W/sbopts.txt ::/SC55/SBOPTS.TXT
printf '[SC55W]\r\nStartDelay=0\r\nJoystick=0\r\nJoyBios=0\r\nPadKeys=0\r\n' > $W/sc55w.ini
[ -n "$LINK" ] && printf '[Debug]\r\nLink=%s\r\n' "$LINK" >> $W/sc55w.ini
mcopy -o -i $I $W/sc55w.ini ::/SC55/WIN98/SC55W.INI
mdeltree -i $W/d.img@@32256 ::/R ::/T 2>/dev/null; mmd -i $W/d.img@@32256 ::/R ::/T; mcopy -o -i $W/d.img@@32256 $H/TRAPB.COM ::/T/
{ echo '@ECHO OFF'; echo 'START C:\SC55\WIN98\SC55W2.EXE'; echo 'CHOICE /T:Y,60 /C:Y >NUL'
  for p in 24E 24E 24E 301 301 301; do echo "D:\\T\\TRAPB.COM $p >> D:\\R\\TB.TXT"; done
  echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/T.BAT
mcopy -o -i $I $W/T.BAT "::/WINDOWS/Start Menu/Programs/StartUp/T.BAT"
sh $V/bin/86run.sh $D; n=0
while [ $n -lt 1500 ]; do sleep 20; n=$((n + 20)); mtype -i $W/d.img@@32256 ::/R/DONE.TXT 2>/dev/null | grep -q done && break; done
sh $V/bin/86run.sh $D shot $W/shot-$T.png > /dev/null 2>&1; sh $V/bin/86run.sh $D stop
R=$W/r-$T; rm -rf $R; mkdir -p $R; mcopy -n -i $W/d.img@@32256 ::/R/TB.TXT $R/ 2>/dev/null; mcopy -n -i $I ::/SC55/WIN98/SC55W.LOG $R/ 2>/dev/null
echo "$T ($n s): $(tr -d '\r' < $R/TB.TXT | awk '{print $2}' | tr '\n' ' ')  link: $(tr -d '\r' < $R/SC55W.LOG | grep -m1 'serial debug link' | cut -c12-)"
