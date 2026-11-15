#!/bin/sh
# run.sh ZIP CASES... - SBEMU /VWC (vesawc.c) tests on store-dos71: pm (HIMEM only + SC55), rm (JEMMEX + SC55JEM),
# emm (HIMEM + EMM386 NOEMS + SC55). Run from sc55-pack after make. EMU="-e 86box -c testrig/wcset/86box/86box.cfg"
# runs the MTRR write path (Voodoo3 3000: VBE LFB; QEMU's SeaBIOS UC MTRR over 2-4 GB makes WC "unavailable").
# Prints each case's RESULT.LOG to OUT/CASE.log and the checks. Exit 0 = every case reached DONE.
set -u
H=$(cd "$(dirname "$0")" && pwd); W=${OUT:-/tmp/vwcrig}
ZIP=$1; shift
sh "$H/../menustat/mkimg.sh" "$ZIP" "$W" output/sbemu.exe output/sbdiag.exe || exit 2
for m in pm rm; do mcopy -o -i "$W/$m.img@@32256" "$H/VWT.BAT" ::/; done
cp --sparse=always "$W/pm.img" "$W/emm.img"
printf 'DEVICE=C:\\DOS\\HIMEM.SYS\r\nDEVICE=C:\\DOS\\EMM386.EXE NOEMS\r\nDOS=HIGH,UMB\r\nFILES=40\r\nBUFFERS=20\r\nLASTDRIVE=Z\r\n' > "$W/CONFIG.SYS"
mcopy -o -i "$W/emm.img@@32256" "$W/CONFIG.SYS" ::/
rc=0
for c in "$@"; do
  printf '@ECHO OFF\r\nCALL C:\\VWT.BAT %s\r\n' $c > "$W/T_$c.BAT"
  "$H/../common/run.sh" ${EMU:-} -t ${TMO:-400} "$W/$c.img" "$W/T_$c.BAT" > "$W/$c.log" 2>&1; e=$?
  echo "$c: exit $e"; [ $e = 0 ] || rc=1
done
exit $rc
