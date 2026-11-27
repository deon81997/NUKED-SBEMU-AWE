#!/bin/bash
# smoke86.sh LABEL SBEMU.EXE SBDIAG.EXE [maxsecs] - 86Box P2B-LS Pentium II 450 + ES1371 (no Audigy): boot MS-DOS,
# HDPMI + SBEMU, SBDIAG SPDIF (must say CARD=none), IDLEMTR x2 = idle CPU left with SBEMU loaded. Appends to /tmp/smoke86.txt.
# B86 / ROMS: 86Box binary + ROM dir (store-86rig). Disk = store-dos71 msdos71.img (P3B-F PII-450 cfg, sndcard -> es1371).
L=$1; X=$2; D=$3; MAX=${4:-600}; T=$(cd "$(dirname "$0")/.." && pwd)
B86=${B86:-/home/claude/86box/src/build/src/86Box}; ROMS=${ROMS:-/home/claude/86box/roms}
R=/tmp/smoke86/root-$L V=/tmp/smoke86/vm-$L; rm -rf $R $V; mkdir -p $R/SC55 $R/HDPMI $V
cp "$X" $R/SC55/SBEMU.EXE; cp "$D" $R/SC55/SBDIAG.EXE; cp ${HDPMI:-/tmp/z131/NUKEDSB/FILES/HDPMI/HDPMI32I.EXE} $R/HDPMI/
cp $T/cpu/IDLEMTR.EXE $R/; cp -r /home/claude/vms/dos71/86box/nvr $V/
sed -e 's/^sndcard = .*/sndcard = es1371/' -e 's|^hdd_01_fn = .*|hdd_01_fn = hd.img|' -e '/^hdd_02/d' /home/claude/vms/dos71/86box/86box.cfg > $V/86box.cfg
printf '@ECHO OFF\r\nSET BLASTER=A220 I5 D1 T6 P330\r\nC:\\HDPMI\\HDPMI32I.EXE -r -x\r\nC:\\SC55\\SBEMU.EXE > C:\\SB.TXT\r\nC:\\SC55\\SBDIAG.EXE SPDIF > C:\\SPDIF.TXT\r\nC:\\IDLEMTR.EXE 8 C:\\M.LOG %s0\r\nC:\\IDLEMTR.EXE 8 C:\\M.LOG %s1\r\nECHO DONE> C:\\STAGE.TXT\r\n' $L $L > $R/AUTOEXEC.BAT
cp --sparse=always ${DOS71:-/home/claude/vms/dos71/msdos71.img} $V/hd.img; mcopy -o -s -i "$V/hd.img@@32256" $R/* ::/ || exit 1
( cd $V; QT_QPA_PLATFORM=offscreen ALSOFT_DRIVERS=null SDL_AUDIODRIVER=dummy exec $B86 -R $ROMS -N -L $V/86box.log -P $V > $V/stdout.log 2>&1 ) & P=$!
t0=$(date +%s)
while sleep 10; do
  mtype -i "$V/hd.img@@32256" ::/STAGE.TXT 2>/dev/null | grep -q DONE && break
  [ $(( $(date +%s) - t0 )) -gt $MAX ] && { echo "TIMEOUT"; break; }
done
kill $P 2>/dev/null; pkill -9 -f "86Box.*-P $V"; sleep 1
for f in SB.TXT SPDIF.TXT M.LOG; do mtype -i "$V/hd.img@@32256" ::/$f > /tmp/smk.$f 2>/dev/null; done
{ echo "== $L $(( $(date +%s) - t0 ))s"; tr -d '\r' < /tmp/smk.SB.TXT | grep -i -m3 "sbemu\|es1371\|found\|error"; tr -d '\r' < /tmp/smk.SPDIF.TXT; tr -d '\r' < /tmp/smk.M.LOG; } | tee -a /tmp/smoke86.txt
