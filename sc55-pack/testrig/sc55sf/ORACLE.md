# Oracle rig (testrig/sc55sf): decode the SC-55 control ROM by tracing Nuked-SC55

- `chmod +x mkoracle.sh; ./mkoracle.sh` builds `oracle` (our Nuked-SC55 fork + hooks on control-ROM reads and PCM-chip
  writes; patched copies in /tmp/sc55sf_core, pack sources untouched). `oracle ROMDIR SCRIPT`: script lines `m` MIDI
  bytes, `w` ms, `log 1|2|3` (PCM writes / + ROM reads / per-voice state every 64 frames + N voice starts),
  `poke ADDR BYTES` (override control-ROM bytes: one-field experiments), `wav FILE`.
- `gcc -O2 -o tsfplay tsfplay.c -lm`: MIDI file -> WAV through sbemu/tsf.h, the /VSF engine.
- Python (run with PYTHONPATH=.; SC55ROMS=ROM folder, default store-personal in /tmp/dl/spx): `oprobe.py` (run + parse),
  `rommap.py` (table layout), `collect.py OUT.json [bank|drum]` (chip state per voice, 5 keys x 2 velocities, ~3 min
  for 128 programs), `exp_tva.py attack|rate|level`, `exp_times.py` (stage time tables), `exp_tvf.py lev|base|table|
  scan|scanq|base33`, `fit_pitch.py`, `fit_level.py`, `fit_pan.py` (regressions over collect.py data).
- `ocompare.py SF2 [bank|drum] [progs]`: per-program notes, oracle vs tsfplay (RENDER=fluid: fluidsynth), env / band /
  level error, worst first. `midi2script.py SONG.mid REF.wav [s]` = oracle script for a song (then ../awe/chromadtw.py).
  `sf2dump.py SF2 bank prog` prints a preset's zones. `sc55sf_oracle.c` = this chat's converter (reference for
  porting curves into tools/sc55sf.c): `gcc -O2 -o so sc55sf_oracle.c ../../upstream/Nuked-SC55/src/backend/sha/sha224-256.c -lm`.
- Results and decoded fields: docs/SC55ROM-trace.md.
