#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# run.sh ZIP [VM] [CASES] - Windows 98 serial debug link (w98.9: SC55W /DBG, SC55W.VXD writes COM1 from ring 0) in the
# QEMU Win98 QuickInstall VM (store-w98qi-qemu, default acpi): COM1 -> TCP -> pack/FILES/SC55/DBGLOG.PY.
#   main   SC55W /INSTALL, then TD1.BAT in a DOS box: link on (forwarded to the running SC55W), COMHOLD cannot open
#          COM1 (claimed), TRAPB/DOSTEST/MPUINT = DOS box sound, MIDITEST + Media Player = Windows MIDI device open/close,
#          level 3 + /DBG:DUMP capture, /DBG:0, then COMHOLD holds COM1 and /DBG:COM1 must be refused (message + log)
#   hang   TD2.BAT: /DBG:HANG - the "!" line must be the last one, nothing for 60 s after it, the VM stays frozen
#   fault  TD3.BAT: /DBG:FAULT - a ring-0 invalid opcode (UD2) in the VxD: the link must go on (10+ S lines after it)
# Logs: /tmp/w98dbg/CASE.log (DBGLOG files), results /tmp/w98t/r-VM-TDn, screenshots /tmp/w98t/shots.
ZIP=$1; VM=${2:-acpi}; CASES=${3:-main hang fault}
H=$(cd "$(dirname "$0")" && pwd); P=$H/../..; R98=$H/../w98par/run.sh; W=/tmp/w98t; L=/tmp/w98dbg; PORT=${PORT:-4560}
mkdir -p $W $L && rm -rf $W/d && mkdir -p $W/d/T $W/d/R $W/u
rm -rf $W/u/* && unzip -q -o "$ZIP" -d $W/u && mv $W/u/NUKEDSB $W/d/PACK
cp $H/COMHOLD.EXE $H/TRAPB.COM $H/TEST.MID /tmp/tb/DOSTEST.EXE /tmp/tb/MPUINT.EXE /tmp/tb/MIDITEST.EXE $W/d/T/
S='C:\SC55\WIN98\SC55W.EXE'
w() { echo "CHOICE /T:Y,$1 /C:Y >NUL"; }
{ echo '@ECHO OFF'; echo "START $S /DBG:COM1:1"; w 20
  echo 'START /W D:\T\COMHOLD.EXE COM1 0 D:\R\C1.TXT'
  echo 'D:\T\TRAPB.COM >> D:\R\TB.TXT'; echo 'D:\T\TRAPB.COM 331 >> D:\R\TB.TXT'
  echo 'D:\T\DOSTEST.EXE'; echo 'D:\T\MPUINT.EXE'
  echo 'START /W D:\T\MIDITEST.EXE'; echo 'START /W C:\WINDOWS\MPLAYER.EXE /PLAY /CLOSE D:\T\TEST.MID'; w 5
  echo "START /W $S /DBG:COM1:3"; w 3; echo 'D:\T\TRAPB.COM >> D:\R\TB.TXT'
  echo "START /W $S /DBG:DUMP"; w 40
  echo "START /W $S /DBG:0"; w 3
  echo 'START D:\T\COMHOLD.EXE COM1 25 D:\R\C2.TXT'; w 5; echo "START /W $S /DBG:COM1"; w 30
  echo "START /W $S /DBG:COM1:1"; w 5
  echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TD1.BAT
{ echo '@ECHO OFF'; echo "START $S /DBG:COM1:1"; w 20; echo "START $S /DBG:HANG"; w 90; echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TD2.BAT
{ echo '@ECHO OFF'; echo "START $S /DBG:COM1:1"; w 20; echo "START $S /DBG:FAULT"; w 90; echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TD3.BAT
export QEXTRA="-chardev socket,id=c1,host=127.0.0.1,port=$PORT,server=on,wait=off -serial chardev:c1"
logger() {   # CASE: DBGLOG.PY on the VM's COM1 until logstop
  ( for i in $(seq 120); do [ -f $L/stop ] && break; python3 -I $P/pack/FILES/SC55/DBGLOG.PY socket://127.0.0.1:$PORT -o $L/$1.log -q \
      --timeout 1500 2>>$L/$1.err && break; sleep 2; done ) & }
logstop() { touch $L/stop; pkill -f "DBGLOG.PY socket://127.0.0.1:$PORT"; sleep 5; rm -f $L/stop; }
F=0
chk() { c=$(grep -cE "$3" $L/$1.log 2>/dev/null); [ "$c" -ge "${4:-1}" ] && echo "  ok   $2 ($c)" || { echo "  FAIL $2 ($c)"; F=1; }; }
for c in $CASES; do
  rm -f $L/$c.log $L/$c.err
  sh $R98 fresh $VM && sh $R98 d > /dev/null
  echo "== $c: install"; sh $R98 install $VM | tr '\n' ' '; echo
  case $c in
  main)  I=$W/$VM.img@@32256; mtype -i $I ::/SC55/WIN98/SC55W.INI > $W/i.ini; printf '[Debug]\r\nLink=COM1:1\r\n' >> $W/i.ini
         mcopy -o -i $I $W/i.ini ::/SC55/WIN98/SC55W.INI     # on from the start (SC55W.INI), the batch's /DBG: is then a level change
         logger main; BOOT=${BOOT:-200} sh $R98 t $VM TD1.BAT 900 > /dev/null; logstop
         echo "main: $(grep -vc '^#' $L/main.log) lines"; r=$W/r-$VM-TD1
         for f in SC55/WIN98/SC55W.LOG DOSTEST.LOG MPUINT.LOG MIDITEST.LOG; do mcopy -o -n -i $I ::/$f $r/ 2>/dev/null; done
         chk main "link line (as DOS, port claimed)" "W98  [0-9.]+ VXD I Serial debug link: COM1 \(3F8\), 115200 baud, level 1, port claimed from Windows"
         chk main "VxD load line" "VXD I VxD loaded: NUKED SB-EMU 1.0.0-beta.130-w98.9-usb"
         chk main "attach + traps" "VXD I port traps: SB ok, OPL ok, MPU ok"
         chk main "DOS box start/stop" "VXD I DOS box (start|stop) vm=" 2
         chk main "DOS box programs" "VXD I vm=[0-9A-F]{8} runs .*(TRAPB|DOSTEST|MPUINT)" 3
         chk main "SB use + DMA" "VXD I (vm=[0-9A-F]{8} uses the Sound Blaster|DMA play)" 2
         chk main "MPU lines" "VXD I MPU-401 (reset|UART mode)" 1
         chk main "Windows MIDI device open/close" "VXD I Windows MIDI device (opened|closed)" 2
         chk main "VxD stats once a second" "VXD S vm=[0-9A-F]{8} sb=[0-9]+ mpu=" 20
         chk main "helper stats" "SC55W S load=.* midi=.* late=.* early=.* resync=" 10
         chk main "helper log lines (synth start)" "SC55W [IE] (synth:|start-up complete)" 1
         chk main "level 3 capture" "VXD I capture: [0-9]+ port accesses"
         chk main "capture records" "W98  T [0-9]+ [io] [0-9A-F]{3} [0-9A-F]{2}$" 1000
         chk main "capture end" "VXD I capture end"
         chk main "link off line" "VXD I serial debug link off"
         echo "  COMHOLD while the link is on: $(tr -d '\r' < $r/C1.TXT 2>/dev/null)  / while off: $(tr -d '\r' < $r/C2.TXT 2>/dev/null)"
         grep -q "open failed" $r/C1.TXT && grep -q "open ok" $r/C2.TXT && echo "  ok   COM1 claimed while on, free while off" || { echo "  FAIL COM1 claim"; F=1; }
         tr -d '\r' < $r/SC55W.LOG | grep -q "serial debug link COM1:1 refused: COM1 (3F8) is in use in Windows" && echo "  ok   refused while a program holds COM1" || { echo "  FAIL refusal"; F=1; }
         echo "  TRAPB ticks: $(tr -d '\r' < $r/TB.TXT | tr '\n' ' ')"
         echo "  DOSTEST: $(tr -d '\r' < $r/DOSTEST.LOG 2>/dev/null | grep -ciE 'ok|pass') ok lines; MIDITEST: $(tr -d '\r' < $r/MIDITEST.LOG 2>/dev/null | tail -1)" ;;
  hang)  logger hang; BOOT=${BOOT:-200} sh $R98 t $VM TD2.BAT 160 > /dev/null; end=$(date +%s); logstop
         last=$(grep -v '^#' $L/hang.log | tail -1); echo "hang: last line: ${last#*  }"
         echo "$last" | grep -q "VXD ! hang test" && echo "  ok   the ! line is the last one" || { echo "  FAIL last line"; F=1; }
         gap=$(( end - $(date -d "${last%%.*}" +%s) ))
         [ $gap -ge 60 ] && echo "  ok   nothing in the $gap s after it (VM killed then)" || { echo "  FAIL gap $gap s"; F=1; }
         [ -f $W/r-$VM-TD2/DONE.TXT ] && { echo "  FAIL Windows went on"; F=1; } || echo "  ok   Windows stayed frozen" ;;
  fault) logger fault; BOOT=${BOOT:-200} sh $R98 t $VM TD3.BAT 160 > /dev/null; logstop
         chk fault "fault test announced" "VXD I fault test"
         n=$(sed -n '/VXD I fault test/,$p' $L/fault.log | grep -c "VXD S vm=")
         [ $n -ge 10 ] && echo "  ok   link goes on after the driver fault ($n S lines)" || { echo "  FAIL after the fault ($n)"; F=1; } ;;
  esac
  logstop
done
[ $F = 0 ] && echo PASS || echo FAIL
exit $F
