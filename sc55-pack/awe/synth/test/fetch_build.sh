#!/bin/sh
# Builds test/harness_ref: the same harness driving 86Box's snd_emu8k.c.
# 86Box source is GPL-2; it is fetched into $DL, never committed here.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DL=${DL:-/tmp/dl/86box}
if [ ! -f "$DL/src/sound/snd_emu8k.c" ]; then
  git clone -q --depth 1 --filter=blob:none --sparse https://github.com/86Box/86Box "$DL"
  git -C "$DL" sparse-checkout set src/sound src/include/86box
fi
cp "$DL/src/include/86box/snd_emu8k.h" "$HERE/ref86box/inc/86box/"
# 86Box bug: the mix pointer only advances on samples where the voice has
# volume, so a voice silent for k samples lands k samples early in that
# chunk. Patch a copy so the reference is sample-aligned.
sed -e 's|(\*buf++) += (dat \* emu_voice->vol_l) >> 8;|buf[(pos - emu8k->pos) * 2] += (dat * emu_voice->vol_l) >> 8;|' \
    -e 's|(\*buf++) += (dat \* emu_voice->vol_r) >> 8;|buf[(pos - emu8k->pos) * 2 + 1] += (dat * emu_voice->vol_r) >> 8;|' \
    "$DL/src/sound/snd_emu8k.c" > "$HERE/ref86box/snd_emu8k_fixed.c"
grep -q 'buf\[(pos - emu8k->pos) \* 2 + 1\]' "$HERE/ref86box/snd_emu8k_fixed.c" || { echo "86Box patch failed"; exit 1; }
${CC:-cc} -O2 -std=gnu11 -DREF86 -I"$HERE/ref86box/inc" -I"$HERE/.." -w \
  "$HERE/harness.c" "$HERE/ref86box/glue.c" "$HERE/ref86box/snd_emu8k_fixed.c" -lm -o "$HERE/harness_ref"
echo built "$HERE/harness_ref"
