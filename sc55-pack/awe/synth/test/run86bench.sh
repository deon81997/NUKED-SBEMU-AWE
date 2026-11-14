#!/bin/sh
# run86bench.sh EXE [EXE2 ...]: run DOS bench builds in the 86Box rig
# (store-86rig in deon81997/priavte, restored to /home/claude/86rig) and
# print their output. Needs mtools, xdotool, Xvfb, openbox.
set -e
R=/home/claude/86rig
echo "drive c: file=\"$R/dos.img\" offset=1048576" > ~/.mtoolsrc
mcopy -o c:/autoexec.bat /tmp/autoexec.orig
cp /tmp/autoexec.orig /tmp/autoexec.new
printf 'C:\\HDPMI\\HDPMI32I.EXE -r\r\n' >> /tmp/autoexec.new
i=0
for e in "$@"; do
  i=$((i+1)); mcopy -o "$e" c:/B$i.EXE
  printf 'ECHO == %s >> C:\\BEN.TXT\r\nC:\\B%d.EXE 5 >> C:\\BEN.TXT\r\n' "$(basename $e)" $i >> /tmp/autoexec.new
done
printf 'ECHO EMU8K-BENCH-END >> C:\\BEN.TXT\r\n' >> /tmp/autoexec.new
mcopy -o /tmp/autoexec.new c:/autoexec.bat
mdel c:/BEN.TXT 2>/dev/null || true
if mdir c:/BEN.TXT >/dev/null 2>&1; then echo "cannot delete old BEN.TXT"; exit 1; fi
python3 -c "import sys; sys.path.insert(0,'$R'); import vm86; vm86.start(wait=5)"
for t in $(seq 1 60); do
  sleep 10
  if mtype c:/BEN.TXT 2>/dev/null | grep -q EMU8K-BENCH-END; then break; fi
done
pkill -x 86Box || true; sleep 2
mcopy -o /tmp/autoexec.orig c:/autoexec.bat
mtype c:/BEN.TXT
grep -m1 -E "cpu_family|cpu_speed" $R/vm/86box.cfg; grep -m1 cpu_speed $R/vm/86box.cfg
