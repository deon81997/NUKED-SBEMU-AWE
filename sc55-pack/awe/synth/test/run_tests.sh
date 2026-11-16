#!/bin/sh
# run_tests.sh: build, render every script with this engine (also under
# ASan/UBSan) and with 86Box's EMU8000, compare. Last line: PASS or FAIL.
# AWEROM=path/to/awe32.raw enables t08_rom (86Box's roms repo has one).
T=$(cd "$(dirname "$0")" && pwd); S="$T/.."; W=${W:-/tmp/emu8k-test}
CC=${CC:-cc}; fail=0
mkdir -p "$W/ours" "$W/asan" "$W/ref"
python3 "$T/mkscripts.py" "$W/scripts" >/dev/null || exit 1
$CC -std=c99 -O2 -Wall -Wextra -Werror -I"$S" "$T/harness.c" "$S/emu8k.c" "$S/emu8k_rom.c" -lm -o "$W/harness" || exit 1
$CC -std=c99 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I"$S" \
    "$T/harness.c" "$S/emu8k.c" "$S/emu8k_rom.c" -lm -o "$W/harness_asan" || exit 1
sh "$T/fetch_build.sh" >/dev/null || exit 1
for s in "$W"/scripts/*.awe; do
  b=$(basename "$s" .awe)
  if [ "$b" = t08_rom ] && [ ! -f "$AWEROM" ]; then echo "skip $b (no AWEROM)"; continue; fi
  "$W/harness" -q -o "$W/ours/$b.wav" "$s" > "$W/ours/$b.txt" || { echo "FAIL render $b"; fail=1; }
  "$W/harness_asan" -q -o "$W/asan/$b.wav" "$s" > "$W/asan/$b.txt" 2> "$W/asan/$b.err" || { echo "FAIL asan $b"; head -3 "$W/asan/$b.err"; fail=1; }
  cmp -s "$W/ours/$b.wav" "$W/asan/$b.wav" || { echo "FAIL asan output differs: $b"; fail=1; }
  "$T/harness_ref" -q -o "$W/ref/$b.wav" "$s" > "$W/ref/$b.txt" || { echo "FAIL ref $b"; fail=1; }
done
for r in 11025 22050 48000; do
  "$W/harness_asan" -q -r $r -o "$W/asan/rate$r.wav" "$W/scripts/t07_32voices.awe" 2>"$W/asan/rate.err" || { echo "FAIL asan rate $r"; fail=1; }
done
if [ -f "$AWEROM" ]; then
  # same ROM as SF2 (ROM layout) and as an AWE-DUMP image (2 bytes late) must render identically
  python3 -I - "$AWEROM" "$W" <<'PY'
import struct, sys
rom = open(sys.argv[1], "rb").read(); w = sys.argv[2]
if rom[6:10] == bytes.fromhex("4d314d47"):  # AWE-DUMP layout: make the aligned image
    rom = rom[2:] + b"\0\0"
smpl = b"smpl" + struct.pack("<I", len(rom)) + rom
sdta = b"LIST" + struct.pack("<I", 4 + len(smpl)) + b"sdta" + smpl
info = b"LIST" + struct.pack("<I", 4 + 12) + b"INFO" + b"ifil" + struct.pack("<IHH", 4, 2, 1)
body = b"sfbk" + info + sdta
open(w + "/rom.sf2", "wb").write(b"RIFF" + struct.pack("<I", len(body)) + body)
open(w + "/romdump.bin", "wb").write(b"\0\0" + rom[:-2])  # AWE-DUMP style
open(w + "/aligned.bin", "wb").write(rom)
PY
  for f in rom.sf2 romdump.bin aligned.bin; do
    AWEROM="$W/$f" "$W/harness" -q -o "$W/ours/t08_$f.wav" "$W/scripts/t08_rom.awe" &&
    cmp -s "$W/ours/t08_$f.wav" "$W/ours/t08_rom.wav" && echo "ROM as $f: identical" || { echo "FAIL ROM as $f"; fail=1; }
  done
fi
cmp -s "$W/ours/t11_reads.txt" "$W/ref/t11_reads.txt" || { echo "FAIL register read-back differs from 86Box"; diff "$W/ours/t11_reads.txt" "$W/ref/t11_reads.txt"; fail=1; }
p() { for b in "$@"; do [ -f "$W/ours/$b.wav" ] && printf '%s %s ' "$W/ours/$b.wav" "$W/ref/$b.wav"; done; }
echo "-- exact paths (SNR >= 40 dB, level within 0.5 dB)"
python3 "$T/compare.py" --min-snr 40 --max-lvl 0.5 $(p t01_sine t03_env t06_pan t08_rom t09_direct) || fail=1
echo "-- pitch modulation (modulation every EMU8K_CTRL samples: level within 1 dB)"
python3 "$T/compare.py" --min-snr 0 --max-lvl 1 $(p t02_pitch t10_modpitch) || fail=1
echo "-- filter: 2-pole here vs 86Box 4-pole Moog (level within 6 dB)"
python3 "$T/compare.py" --min-snr 0 --max-lvl 6 $(p t04_filter t05_lfo t07_32voices) || fail=1
echo "-- reverb (8 Creative types) and chorus (8 types): same structure as 86Box, integer (SNR >= 20 dB, level within 1 dB)"
python3 "$T/compare.py" --min-snr 20 --max-lvl 1 $(p t12_sends t12_rev0_room1 t12_rev1_room2 t12_rev2_room3 t12_rev3_hall1 \
  t12_rev4_hall2 t12_rev5_plate t12_rev6_delay t12_rev7_pandelay t13_cho0_chorus1 t13_cho1_chorus2 t13_cho2_chorus3 \
  t13_cho3_chorus4 t13_cho4_feedback t13_cho5_flanger t13_cho6_shortdelay t13_cho7_shortdelayfb) || fail=1
python3 "$T/fxcmp.py" "$W/ours" "$W/ref"
echo "-- t11_reads: read-back text compared above"
[ $fail = 0 ] && echo PASS || echo FAIL
