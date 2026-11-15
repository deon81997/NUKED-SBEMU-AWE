#!/bin/bash
# run.sh ZIP [EXE...] - SBEMU /DBG serial debug link in QEMU MS-DOS 7.1 (store-dos71), COM1 -> TCP -> pack/FILES/SC55/DBGLOG.PY.
# SBT.COM (real-mode SB DMA) only in rm: without JEMM no real-mode port is trapped.
# Both SBEMU modes: pm (SC55.BAT, HIMEM + HDPMI) and rm (SC55JEM.BAT, JEMMEX). Per mode:
#   run 1 (DBG.BAT MODE): link line, SBEMU start, stats, tool lines (SBDIAG direct + via SBEMU, MENU, GAMETEST),
#         level 2 frame lines, level 3 capture (T lines + capture end);
#   run 2 (DBG.BAT MODE HANG): SBEMU stops the PC with interrupts off - the "!" line must be the last line received.
# Logs: /tmp/dbgtest/MODE[-hang].log (DBGLOG.PY files). Needs pyserial, nasm.
#   sh testrig/dbg/run.sh ../release/NUKED-SB-EMU-*.zip output/sbemu.exe output/sbdiag.exe output/sc55menu.exe output/gametest.exe
set -u
H=$(cd "$(dirname "$0")" && pwd); W=/tmp/dbgtest; T=$H/../..
ZIP=$1; shift
mkdir -p "$W"; nasm -f bin -o "$W/SBT.COM" "$H/../sbt.asm" || exit 2
sh "$H/../menustat/mkimg.sh" "$ZIP" "$W" "$@" || exit 2
for m in pm rm; do mcopy -o -i "$W/$m.img@@32256" "$W/SBT.COM" "$H/DBG.BAT" ::/ || exit 2; done
export QAUDIO=${QAUDIO:-none,id=snd,timer-period=2000}
F=0
logrun() {  # MODE NAME BATCHARG TIMEOUT UNTIL
  local m=$1 n=$2 IDLE=${IDLE:-} arg=$3 tmo=$4 until=$5 port=$((4500 + RANDOM % 400)) img=pm
  [ "$m" = rm ] && img=rm
  printf '@ECHO OFF\r\nCALL C:\\DBG.BAT %s\r\n' "$arg" > "$W/T_$n.BAT"
  rm -f "$W/$n.log"
  "$H/../common/run.sh" -t "$tmo" "$W/$img.img" "$W/T_$n.BAT" -- -chardev socket,id=c1,host=127.0.0.1,port=$port,server=on,wait=off -serial chardev:c1 > "$W/$n.result" 2>&1 &
  local rp=$!
  for i in $(seq 100); do
    python3 -I "$T/pack/FILES/SC55/DBGLOG.PY" "socket://127.0.0.1:$port" -o "$W/$n.log" -q --until "$until" --timeout $((tmo + 30)) ${IDLE:+--idle $IDLE} 2> "$W/$n.dbglog" && break
    grep -q "cannot open" "$W/$n.dbglog" || break; sleep 0.2
  done
  wait $rp; echo $? > "$W/$n.rc"
}
chk() {  # NAME LABEL REGEX [min count]
  local c; c=$(grep -cE "$3" "$W/$1.log"); [ "$c" -ge "${4:-1}" ] && echo "  ok   $2 ($c)" || { echo "  FAIL $2 ($c)"; F=1; }
}
for m in pm rm; do
  logrun $m $m "$m" 240 "SBDIAG batch done"
  echo "$m: batch $(tail -1 "$W/$m.result" | tr -d '\r'), $(grep -c . "$W/$m.log") lines"
  chk $m "tool direct (own UART, link at level 0)" " SBDIAG run WC" 1
  chk $m "link line" "SB I Serial debug link: COM1 \(3F8\), 115200 baud, level 1"
  chk $m "SBEMU start event" "SB I (SBEMU .* started|sound card watchdog)"
  chk $m "stats once a second" "SB S prog=.* load=.* isr=" 3
  chk $m "tool lines through SBEMU" " (MENU|GAMETEST) started" 2
  chk $m "SBDIAG LOG line" "SBDIAG hello from the batch"
  chk $m "level 2 frame lines" "SB F mpu=" 3
  chk $m "level 3 capture" "SB I capture: last"
  chk $m "capture records" "^[0-9 :.-]+  T [0-9]+ [io] [0-9A-F]{3} [0-9A-F]{2}$" 20
  chk $m "capture end" "SB I capture end"
  IDLE=40 logrun $m $m-hang "$m HANG" 120 "no-such-line"   # reads on 40 s past the last line: nothing may follow it
  last=$(grep -v "^#" "$W/$m-hang.log" | tail -1)
  echo "$m-hang: last line: ${last:25}"
  echo "$last" | grep -q "SB ! forced hang test" && echo "  ok   last line before the freeze arrived" || { echo "  FAIL last line"; F=1; }
  [ "$(cat "$W/$m-hang.rc")" = 124 ] && echo "  ok   PC stayed frozen (batch never finished)" || { echo "  FAIL VM was not frozen"; F=1; }
done
[ $F = 0 ] && echo PASS || echo FAIL
exit $F
