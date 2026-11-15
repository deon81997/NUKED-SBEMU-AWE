#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
# sc55log.sh NAME SC55LOG.EXE : SC-55 emulation speed (TEST.BAT = SC55LOG ROMS) on 86Box P2B-LS Pentium II 450, FreeDOS,
# ROMs from store-personal. Prints the "x real time" lines of SC55LOG.TXT, keeps it as /tmp/sc55log-NAME/SC55LOG.TXT.
set -e
H=$(cd $(dirname $0); pwd); TR=$(dirname $H)
V=/tmp/sc55log-$1; rm -rf $V; mkdir -p $V/c/SC55/ROMS
cp $TR/rufus/res/freedos/KERNEL.SYS $TR/rufus/res/freedos/COMMAND.COM $V/c/
cp /home/claude/rel/NUKEDSB/FILES/JEMM/JEMMEX.EXE /home/claude/rel/NUKEDSB/FILES/HDPMI/HDPMI32I.EXE $V/c/SC55/; cp $2 $V/c/SC55/SC55LOG.EXE
for f in /home/claude/tmp/p52/NUKEDSB/FILES/ROMS/*; do case $(basename $f) in ROMHASH.CAC) ;; *) cp $f $V/c/SC55/ROMS/ ;; esac; done
printf 'DEVICE=C:\\SC55\\JEMMEX.EXE NOEMS\r\nSHELL=C:\\COMMAND.COM C:\\ /E:1024 /P\r\n' > $V/c/CONFIG.SYS
printf '@ECHO OFF\r\nC:\\SC55\\HDPMI32I -r\r\nCD \\SC55\r\nSC55LOG ROMS > NUL\r\nECHO DONE > C:\\DONE.TXT\r\n' > $V/c/AUTOEXEC.BAT
python3 $TR/mkdisk.py build $V/c $V/hd.img 100 > /dev/null
sed -e 's/^cpu_speed = .*/cpu_speed = 450000000/' -e 's/^cpu_multi = .*/cpu_multi = 4.5/' $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
bash $TR/86box/run.sh $V > /dev/null
for i in $(seq 40); do sleep 2; python3 $TR/86box/a.py $V/a.sock @screen 2>/dev/null | grep -q 'Press F1' && { python3 $TR/86box/a.py $V/a.sock 'key down 0x3b' 'key up 0x3b' > /dev/null; break; }; done
for i in $(seq 200); do sleep 3; python3 $TR/mkdisk.py get $V/hd.img DONE.TXT /dev/null >/dev/null 2>&1 && break; done
pkill -f "86Box.*$V" || true; sleep 1
python3 $TR/mkdisk.py get $V/hd.img 'SC55\SC55LOG.TXT' $V/SC55LOG.TXT > /dev/null && tr -d '\r' < $V/SC55LOG.TXT | grep -aiE "real time|ROM set|version|NUKED" | head -8
