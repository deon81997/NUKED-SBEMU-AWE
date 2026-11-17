#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# run.sh STAGE VM - Win98 QuickInstall (VM store: store-w98qi-qemu, /home/claude/vms/w98qemu/{noacpi,acpi}.img) tests
# of the Windows 98 part, w98.8. Work dir /tmp/w98t (D: = test.img built from /tmp/w98t/d: PACK = the pack with ROMs,
# T = test programs + T*.BAT, R = results). QEMU runs with -no-reboot (a restart = QEMU exits; LESSONS: warm reboots hang).
#   fresh VM       copy the store image to /tmp/w98t/VM.img
#   d              rebuild test.img from /tmp/w98t/d (VM must be off)
#   install VM     boot, Start > Run SC55W /INSTALL, Enter (install), Enter (restart = QEMU exits)
#   ini VM K=V..   add lines to C:\SC55\WIN98\SC55W.INI [SC55W] (VM off)
#   t VM BAT [MAX] boot, Start > Run D:\T\BAT, wait for D:\R\DONE.TXT (or MAX s), copy D:\R to /tmp/w98t/r-VM-BAT
#   uninstall VM   boot, Start > Run C:\SC55\WIN98\SC55W.EXE /UNINSTALL, Enter, Enter, shut down; check files
# env: SHOTS (default /tmp/w98t/shots), BOOT (seconds to the desktop, default 150), QEXTRA (more QEMU args, e.g. -serial)
W=/tmp/w98t; V=/home/claude/vms; Q="python3 $V/bin/q.py"; export QSOCK=$W/q.sock SHOTS=${SHOTS:-$W/shots}
STAGE=$1; VM=$2; IMG=$W/$VM.img; BOOT=${BOOT:-150}
boot() { pkill -f "qemu-system-i386.*$W/" 2>/dev/null; sleep 1
  TESTIMG=$W/test.img SND=${SND:-ac97 sb16} setsid $V/bin/qrun.sh "$IMG" -no-reboot $QEXTRA > $W/qemu.log 2>&1 < /dev/null &
  sleep "$BOOT"; }
running() { pgrep -f "qemu-system-i386.*$IMG" > /dev/null; }
run_cmd() { $Q key esc; $Q key ctrl-esc; sleep 2; $Q key r; sleep 3; $Q type "$1"; $Q key ret; }
waitoff() { n=0; while running && [ $n -lt "$1" ]; do sleep 5; n=$((n + 5)); done; running && { echo "still running after $1 s: killed"; pkill -f "qemu-system-i386.*$IMG"; sleep 2; }; }
case $STAGE in
fresh) cp --sparse=always $V/w98qemu/$VM.img "$IMG" ;;
d) cp --sparse=always $V/shared/test.img $W/test.img && mdeltree -i $W/test.img@@32256 ::/PACK ::/T ::/R 2>/dev/null
   mcopy -s -Q -i $W/test.img@@32256 $W/d/* ::/ && mdir -i $W/test.img@@32256 ::/ ;;
install) boot; run_cmd 'D:\PACK\FILES\WIN98\SC55W.EXE /INSTALL'; sleep 15; $Q shot inst-$VM-1 >/dev/null
   $Q key ret; sleep 60; $Q shot inst-$VM-2 > /dev/null; $Q key ret; waitoff 240
   mdir -i "$IMG"@@32256 ::/SC55/WIN98 | grep -c -E 'SC55W|VXD|DRV'; mdir -i "$IMG"@@32256 ::/WINDOWS/SYSTEM/SC55MIDI.DRV | grep -c DRV ;;
ini) shift 2; t=$W/sc55w.ini; mtype -i "$IMG"@@32256 ::/SC55/WIN98/SC55W.INI > $t
   python3 -I -c 'import sys
p=sys.argv[1]; L=open(p,newline="").read().replace("\r","").split("\n"); L=[x for x in L if x]
if "[SC55W]" not in L: L.append("[SC55W]")
for kv in sys.argv[2:]:
    k=kv.split("=")[0]; L=[x for x in L if not x.startswith(k+"=")]; L.insert(L.index("[SC55W]")+1, kv)
open(p,"w",newline="").write("\r\n".join(L)+"\r\n")' $t "$@"
   mcopy -o -i "$IMG"@@32256 $t ::/SC55/WIN98/SC55W.INI && tr -d '\r' < $t | sed -n '/SC55W/,$p' ;;
t) B=$3; MAX=${4:-900}; boot; run_cmd "D:\\T\\$B"; n=0
   while [ $n -lt "$MAX" ]; do sleep 20; n=$((n + 20)); mtype -i $W/test.img@@32256 ::/R/DONE.TXT 2>/dev/null | grep -q done && break; done
   $Q shot t-$VM-$B > /dev/null; waitoff 120; R=$W/r-$VM-${B%.*}; rm -rf $R; mkdir -p $R; mcopy -s -n -i $W/test.img@@32256 ::/R/* $R/ 2>/dev/null; ls $R ;;
soak) B=$3; SECS=${4:-1800}; boot; run_cmd "D:\\T\\$B"; n=0
   while [ $n -lt "$SECS" ]; do sleep 60; n=$((n + 60)); [ $((n % 300)) = 60 ] && $Q shot soak-$VM-$n > /dev/null 2>&1; done
   $Q shot soak-$VM-end > /dev/null; pkill -f "qemu-system-i386.*$IMG"; sleep 3; R=$W/r-$VM-soak; rm -rf $R; mkdir -p $R
   mcopy -n -i "$IMG"@@32256 ::/SC55/WIN98/SC55W.LOG $R/ && tr -d '\r' < $R/SC55W.LOG | grep -E 'activity' | sed -n '1p;$p' ;;
uninstall) boot; run_cmd 'C:\SC55\WIN98\SC55W.EXE /UNINSTALL'; sleep 10; $Q shot un-$VM-1 > /dev/null; $Q key ret; sleep 20
   $Q shot un-$VM-2 > /dev/null; $Q key ret; sleep 5; run_cmd 'RUNDLL32.EXE USER.EXE,ExitWindows'; waitoff 240
   echo "SYSTEM.INI drivers with SC55MIDI: $(mtype -i "$IMG"@@32256 ::/WINDOWS/SYSTEM.INI | grep -ci sc55midi)"
   echo "AUTOEXEC SC55W lines: $(mtype -i "$IMG"@@32256 ::/AUTOEXEC.BAT | grep -ci sc55w)"
   echo "SC55MIDI.DRV left: $(mdir -b -i "$IMG"@@32256 ::/WINDOWS/SYSTEM/ 2>/dev/null | grep -ci sc55midi)" ;;
*) sed -n 2,12p "$0"; exit 1 ;;
esac
