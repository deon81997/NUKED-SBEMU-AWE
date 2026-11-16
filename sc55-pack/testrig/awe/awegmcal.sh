#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# awegmcal.sh AWE32.RAW - regenerate awe/synth/awegm.h and its register table awegm.tune: note 60 of all 128 programs
# with the map (no tune) and with TimGM6mb.sf2 (fluidsynth), pitch of both (HPS), transposition in semitones where they
# differ by more than 80 cents (programs 0-119). Needs fluidsynth, timgm6mb-soundfont, gcc, python3 + numpy.
set -e
H=$(cd "$(dirname "$0")" && pwd); P=$(dirname "$(dirname "$H")"); S=$P/awe/synth; W=$(mktemp -d)
: > "$H/awegm.tune"
python3 "$H/awegm.py" "$1" > "$S/awegm.h" 2>/dev/null
gcc -std=c99 -O2 -I"$S" "$S/test/awemid.c" "$S/awemidi.c" "$S/emu8k.c" "$S/emu8k_rom.c" -lm -o "$W/awemid"
python3 "$H/notes.py" mid "$W/t.mid"
"$W/awemid" -r "$1" -x -o "$W/b.wav" "$W/t.mid" >/dev/null
fluidsynth -ni -R 0 -C 0 -g 0.5 -r 44100 -F "$W/f.wav" /usr/share/sounds/sf2/TimGM6mb.sf2 "$W/t.mid" >/dev/null 2>&1
python3 "$H/notes.py" cmp "$W/f.wav" "$W/b.wav" | awk '$1 < 120 && ($2 > 80 || $2 < -80) {print $1, -100 * int(($2 + ($2 > 0 ? 50 : -50)) / 100)}' > "$H/awegm.tune"
python3 "$H/awegm.py" "$1" > "$S/awegm.h" 2>/dev/null
"$W/awemid" -r "$1" -x -o "$W/b2.wav" "$W/t.mid" >/dev/null 2>&1 || true
gcc -std=c99 -O2 -I"$S" "$S/test/awemid.c" "$S/awemidi.c" "$S/emu8k.c" "$S/emu8k_rom.c" -lm -o "$W/awemid" && "$W/awemid" -r "$1" -x -o "$W/b2.wav" "$W/t.mid" >/dev/null
echo "transposed: $(wc -l < "$H/awegm.tune") programs; still off by > 80 cents:"
python3 "$H/notes.py" cmp "$W/f.wav" "$W/b2.wav" | awk '$1 < 120 && ($2 > 80 || $2 < -80)' | tr '\n' ' '; echo
rm -rf "$W"
