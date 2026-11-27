#!/bin/bash
# floptest.sh IMGDIR : MS-DOS 7.1 on 86Box P2B-LS, A:\INSTALL C from the floppy images, then compare C: with the zip
set -e
I=$(cd $1; pwd); TR=/home/claude/nuked-sbemu-awe/sc55-pack/testrig; V=/tmp/flop; rm -rf $V; mkdir -p $V/c
cp --sparse=always /home/claude/vms/dos71/msdos71.img $V/hd.img
printf 'DEVICE=C:\\DOS\\HIMEM.SYS\r\nDOS=HIGH\r\nFILES=40\r\n' > $V/c/CONFIG.SYS
printf '@ECHO OFF\r\nPATH C:\\DOS\r\nCALL A:\\INSTALL C\r\nECHO done > C:\\DONE.TXT\r\nECHO TESTDONE\r\n' > $V/c/AUTOEXEC.BAT
mcopy -o -s -i $V/hd.img@@32256 $V/c/* ::/
sed -e 's#^hdd_01_fn = .*#hdd_01_fn = hd.img#' -e 's/^hdd_01_parameters = .*/hdd_01_parameters = 63, 16, 2080, 0, ide/' $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
printf '\n[Floppy and CD-ROM drives]\nfdd_01_type = 35_2hd\nfdd_01_fn = %s/DISK1.IMG\nfdd_02_type = none\n' $I >> $V/86box.cfg
bash $TR/86box/run.sh $V > /dev/null
python3 /tmp/fl/tool/floptest.py $V $I > $V/drv.log 2>&1 || true
sleep 3; pkill -f "86Box.*$V" || true; cat $V/drv.log | grep -v "^$" | tail -25
