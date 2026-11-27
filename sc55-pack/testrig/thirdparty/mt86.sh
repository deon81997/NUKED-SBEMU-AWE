#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU
# mt86.sh LABEL SBEMU.EXE [OPTS] [maxsecs] - MT-32 (Munt) cost on 86Box P3B-F Pentium II 450 + ES1371, the user's
# MS-DOS 7.1 HIMEM-only boot (store-dos71): HDPMI32I, SBEMU /VMT32:MT32 OPTS (ROMs MT107/MTPCM in C:\SC55\ROMS),
# then twice: SBDIAG START, MIDPLAY /S of MID (default testrig/mt32host MT32TEST.MID; Q seconds), SBDIAG.
# Appends "== LABEL" + SBDIAG's CPU lines to /tmp/mt86.txt. B86 / ROMS86: store-86rig. MID / Q / MTROMS: env.
L=$1; X=$2; OPTS=${3:-}; MAX=${4:-900}; T=$(cd "$(dirname "$0")/.." && pwd); P=$(dirname "$T")
B86=${B86:-/home/claude/86box/src/build/src/86Box}; ROMS86=${ROMS86:-/home/claude/86box/roms}
MTROMS=${MTROMS:-/home/claude/tmp/p52/NUKEDSB/FILES/ROMS}; Q=${Q:-60}
R=/tmp/mt86/root-$L V=/tmp/mt86/vm-$L; rm -rf $R $V; mkdir -p $R/SC55/ROMS $R/HDPMI $V
cp "$X" $R/SC55/SBEMU.EXE; cp $P/output/sbdiag.exe $R/SC55/SBDIAG.EXE; cp $P/output/midplay.exe $R/SC55/MIDPLAY.EXE
cp ${HDPMI:-/home/claude/rel/NUKEDSB/FILES/HDPMI/HDPMI32I.EXE} $R/HDPMI/; cp $MTROMS/MT107.ROM $MTROMS/MTPCM.ROM $R/SC55/ROMS/
if [ -n "${MID:-}" ]; then cp "$MID" $R/T.MID; else python3 $T/mt32host/smf2ev.py --gen /tmp/mt86.ev $R/T.MID; fi
cp -r /home/claude/vms/dos71/86box/nvr $V/
sed -e 's/^sndcard = .*/sndcard = es1371/' -e 's|^hdd_01_fn = .*|hdd_01_fn = hd.img|' -e '/^hdd_02/d' /home/claude/vms/dos71/86box/86box.cfg > $V/86box.cfg
D='SBDIAG START > NUL\r\nMIDPLAY /S /Q:'$Q' C:\\T.MID > NUL\r\nSBDIAG > C:\\DIAG%s.TXT\r\n'
printf "@ECHO OFF\r\nPATH C:\\\\DOS\r\nC:\\\\HDPMI\\\\HDPMI32I.EXE -r -x\r\nCD \\\\SC55\r\nSBEMU.EXE /VMT32:MT32 $OPTS > C:\\\\SB.TXT\r\n$D$D""ECHO DONE> C:\\\\STAGE.TXT\r\n" 1 2 > $R/AUTOEXEC.BAT
cp --sparse=always ${DOS71:-/home/claude/vms/dos71/msdos71.img} $V/hd.img; mcopy -o -s -i "$V/hd.img@@32256" $R/* ::/ || exit 1
( cd $V; QT_QPA_PLATFORM=offscreen ALSOFT_DRIVERS=null SDL_AUDIODRIVER=dummy exec $B86 -R $ROMS86 -N -L $V/86box.log -P $V > $V/stdout.log 2>&1 ) & PID=$!
t0=$(date +%s)
while sleep 10; do
  mtype -i "$V/hd.img@@32256" ::/STAGE.TXT 2>/dev/null | grep -q DONE && break
  [ $(( $(date +%s) - t0 )) -gt $MAX ] && { echo "TIMEOUT"; break; }
done
kill $PID 2>/dev/null; pkill -f "86Box.*-P $V"; sleep 1
for f in SB.TXT DIAG1.TXT DIAG2.TXT; do mtype -i "$V/hd.img@@32256" ::/$f > $V/$f 2>/dev/null; done
{ echo "== $L $(( $(date +%s) - t0 ))s"; tr -d '\r' < $V/SB.TXT | grep -a -m2 "^SBEMU 1\|MPU: Roland"
  for i in 1 2; do tr -d '\r' < $V/DIAG$i.TXT | grep -a "Measured\|uses .*CPU\|needs .*real time" | head -3; done; } | tee -a /tmp/mt86.txt
