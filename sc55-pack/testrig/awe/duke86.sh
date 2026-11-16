#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig, reverb/chorus)
# duke86.sh NAME [SECS] : the reference for duke.sh - the same P2B-LS PII-400 VM (store-dos71, HIMEM) with 86Box's own
# AWE64 (sndcard = sbawe64, 1 MB RAM; the PnP BIOS puts it at 220/5/1/5, EMU 620, drv.py presses F1) and no SBEMU:
# Duke 3D 1.3D with AWE32 music (CFG= as duke.sh) for SECS. 86Box's EMU8000 (with its reverb/chorus) = aud/music.wav.
# Compare: python3 testrig/awe/cmp.py /tmp/duke86-A/aud/music.wav 20 90 /tmp/duke-B/aud/main.wav 20 90 (+ fxtail.py)
set -e
H=$(cd $(dirname $0); pwd); TR=$(dirname $H)
N=$1; SECS=${2:-60}; V=/tmp/duke86-$N; rm -rf $V; mkdir -p $V/c/DUKE
cp --sparse=always /home/claude/vms/dos71/msdos71.img $V/hd.img
cp /tmp/duke/D/* $V/c/DUKE/; cp ${CFG:-$H/DUKE3D.CFG} $V/c/DUKE/DUKE3D.CFG
printf 'DEVICE=C:\\DOS\\HIMEM.SYS\r\nDOS=HIGH\r\nFILES=40\r\n' > $V/c/CONFIG.SYS
printf "@ECHO OFF\r\nPATH C:\\\\DOS\r\nSET BLASTER=A220 I5 D1 H5 P330 E620 T6\r\nCD \\\\DUKE\r\nDUKE3D\r\nECHO DONE > C:\\\\DONE.TXT\r\n" > $V/c/AUTOEXEC.BAT
mcopy -o -s -i $V/hd.img@@32256 $V/c/* ::/
sed -e 's#^hdd_01_fn = .*#hdd_01_fn = hd.img#' -e 's/^hdd_01_parameters = .*/hdd_01_parameters = 63, 16, 2080, 0, ide/' \
    -e 's/^sndcard = es1371/sndcard = sbawe64/' $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
printf '\n[Sound Blaster AWE64 #1]\nonboard_ram = 1024\n' >> $V/86box.cfg
bash $TR/86box/run.sh $V > /dev/null
python3 $H/drv.py $V $SECS > $V/drv.log 2>&1 || true
pkill -f "86Box.*$V" || true; sleep 1
ls -la $V/aud/*.wav | awk '{print $5, $9}'
