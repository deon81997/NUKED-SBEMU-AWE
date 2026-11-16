#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# awemidi_tests.sh AWE32.RAW - the AWE32 MIDI Synth on the host: stand-in SBKs made from TimGM6mb.sf2 (no Creative
# files), awemidtest (+ ASan/UBSan), then songs rendered with the stand-in SBK vs fluidsynth with the same SF2 (chroma)
# and with the built-in map. Needs gcc, fluidsynth, timgm6mb-soundfont, python3 + numpy. Last line PASS or FAIL.
T=$(cd "$(dirname "$0")" && pwd); S=$T/..; R=$(cd "$S/../../testrig/awe" && pwd); W=${W:-/tmp/awemidi-test}
SF=/usr/share/sounds/sf2/TimGM6mb.sf2; MID=$S/../../upstream/Nuked-SC55/test/integration/avmidi; fail=0
mkdir -p "$W"
for k in gm gs mt32; do python3 "$R/sf2sbk.py" "$SF" "$W/b_$k.dat" $k > /dev/null || exit 1; done
cc -std=c99 -O2 -Wall -Wextra -Werror -I"$S" "$T/awemidtest.c" "$S/awemidi.c" "$S/emu8k.c" "$S/emu8k_rom.c" -lm -o "$W/awemidtest" || exit 1
cc -std=c99 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I"$S" "$T/awemidtest.c" "$S/awemidi.c" "$S/emu8k.c" \
   "$S/emu8k_rom.c" -lm -o "$W/awemidtest_asan" || exit 1
cc -std=c99 -O2 -I"$S" "$T/awemid.c" "$S/awemidi.c" "$S/emu8k.c" "$S/emu8k_rom.c" -lm -o "$W/awemid" || exit 1
"$W/awemidtest" "$1" "$W/b_gm.dat" "$W/b_gs.dat" "$W/b_mt32.dat" | grep -v '^  ' | tee "$W/unit.txt" | tail -n 1 | grep -q PASS || fail=1
"$W/awemidtest_asan" "$1" "$W/b_gm.dat" "$W/b_gs.dat" "$W/b_mt32.dat" > "$W/asan.txt" 2>&1 || { echo "FAIL asan"; tail -n 3 "$W/asan.txt"; fail=1; }
cat "$W/unit.txt"
for f in 02 07 15 17; do
  fluidsynth -ni -g 0.5 -r 44100 -F "$W/fs_$f.wav" "$SF" "$MID/$f.mid" > /dev/null 2>&1
  "$W/awemid" -r "$1" -k 8192 -b "$W/b_gm.dat" -o "$W/sbk_$f.wav" "$MID/$f.mid" > "$W/sbk_$f.txt"
  "$W/awemid" -r "$1" -o "$W/bi_$f.wav" "$MID/$f.mid" > "$W/bi_$f.txt"
  c=$(python3 "$R/chroma.py" "$W/fs_$f.wav" "$W/sbk_$f.wav" 90); b=$(python3 "$R/chroma.py" "$W/fs_$f.wav" "$W/bi_$f.wav" 90)
  echo "avmidi/$f.mid: stand-in SBK vs fluidsynth: $c | built-in map: $(echo "$b" | sed 's/.*chroma/chroma/')"
  echo "  $(tail -n 1 "$W/sbk_$f.txt")"
  echo "$c" | awk '{ if ($9 + 0 < 0.80) exit 1 }' || { echo "FAIL: chroma below 0.80"; fail=1; }
done
[ $fail = 0 ] && echo PASS || echo FAIL
