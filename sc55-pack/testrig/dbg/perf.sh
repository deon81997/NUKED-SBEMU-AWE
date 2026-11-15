#!/bin/sh
# perf.sh ZIP - no-slowdown check for SBEMU /DBG on 86Box P3B-F Pentium II 450 + ES1371 (../vesawc/86box, PERF.BAT):
# base = the zip's SBEMU.EXE, off = output/sbemu.exe (no /DBG), dbg1 = /DBG:COM1 (level 1), dbg2 = /DBG:COM1:2.
# Each: TimGM6mb SoundFont, SBDIAG START, MIDPLAY 40 s busy song, SBDIAG -> OUT/perf-CASE.log. CASES="base off" to pick.
set -u
H=$(cd "$(dirname "$0")" && pwd); V=$H/../vesawc; W=${OUT:-/tmp/dbgperf}; ZIP=$1
SF=${SF2:-/tmp/dl/timgm6mb/x/usr/share/sounds/sf2/TimGM6mb.sf2}
sh "$H/../menustat/mkimg.sh" "$ZIP" "$W" output/sbdiag.exe output/midplay.exe || exit 2
T=$(mktemp -d); unzip -q -o "$ZIP" 'NUKEDSB/FILES/SC55/SBEMU.EXE' -d "$T"
cp "$T/NUKEDSB/FILES/SC55/SBEMU.EXE" "$T/SBEMUOLD.EXE"; cp output/sbemu.exe "$T/SBEMUNEW.EXE"; cp "$SF" "$T/TIMGM6MB.SF2"
python3 "$H/../midplay/genmid.py" "$T/T.MID" 60 2 > /dev/null
mcopy -o -i "$W/pm.img@@32256" "$T/SBEMUOLD.EXE" "$T/SBEMUNEW.EXE" "$T/TIMGM6MB.SF2" "$T/T.MID" "$V/PERF.BAT" ::/
rm -rf "$T"
for c in ${CASES:-base off dbg1 dbg2}; do
  case $c in base) a="SBEMUOLD.EXE" ;; off) a="SBEMUNEW.EXE" ;; dbg1) a="SBEMUNEW.EXE /DBG:COM1" ;; dbg2) a="SBEMUNEW.EXE /DBG:COM1:2" ;; esac
  printf '@ECHO OFF\r\nCALL C:\\PERF.BAT %s\r\n' "$a" > "$W/P_$c.BAT"
  "$H/../common/run.sh" -e 86box -c "$V/86box/86box.cfg" -t ${TMO:-900} "$W/pm.img" "$W/P_$c.BAT" > "$W/perf-$c.log" 2>&1
  echo "$c: exit $?"; tr -d '\r' < "$W/perf-$c.log" | grep -a "interrupts (\|uses .*CPU\|needs .*real time\|Measured" | cut -c1-200
done
