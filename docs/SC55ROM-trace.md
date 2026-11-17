# SC-55 (mk1) control ROM, traced with Nuked-SC55 as the oracle (companion to SC55ROM.md)

What the oracle chat measured; its reference converter is sc55-pack/testrig/sc55sf/sc55sf_oracle.c (tools/sc55sf.c is the tool). Decoded with
Nuked-SC55 as the oracle (sc55-pack/testrig/sc55sf: `oracle` traces the firmware's control-ROM reads and its writes
to the PCM chip; `oprobe.py` / `collect.py` / `exp_*.py` / `fit_*.py` turn them into the fields below). Layout
facts first found by Kitrinx and NewRisingSun (SC55_Soundfont, no licence - only facts used, no code); every field
here was re-checked against the oracle. ROM set used: mk1, control ROM "Ver1.21" (R15209363), wave ROMs A/B/C.

## Files and the wave ROM

- Control ROM: 256 KB. Tables found by structure: "Piano 1     " at T, "Piano 2     " at T+0xD8 (mk1 v1.21: T = 0x10000).
- Wave ROMs: 3 x 1 MB, scrambled (address bits 2,0,3,4,1,9,13,10,18,17,6,15,11,16,8,5,12,7,14,19; data bits
  2,0,4,5,7,6,3,1 - Nuked-SC55 rom_io.cpp `unscramble`). SC55SF tells A/B/C apart by SHA-256 (no Roland data inside).
- Sample address (24 bit): bank = (addr >> 21) & 7 (0 = A, 1 = B, 2 = C), offset = addr & 0xFFFFF (chip config 3Dh bit 5 set).
- Sample format: 8-bit DPCM with a 4-bit exponent per 16 samples. Exponent byte of offset a: wave[bank | a >> 5],
  low nibble when (a & 0x10) == 0, else high. Value: `acc += (int8)wave[bank | a] << nibble`, clipped to 20 bits.
  Loops are seamless in that domain (end value = value before the loop start; drift 0 on all checked samples).

## Tables (offsets from T; a second set of the same three tables starts at T+0x10000)

| Table | Address | Count | Record |
| --- | --- | --- | --- |
| Instruments | T | 224 (+224 at T+0x10000) | 0xD8 bytes |
| Parts (key splits) | T+0xBD00 | 144 (+144) | 0x3C bytes |
| Samples | T+0xDEC0 | 532 (+532) | 0x10 bytes |
| Variation map (bank x program -> instrument) | T+0x20000 | 128 x 128 words (BE, 0xFFFF = none) | |
| Drum kits | T+0x28080 | 14 x 0x48C | |

Index rule: instrument i < 224 is in the first set, i >= 224 in the second (same for parts 144, samples 532).

### Instrument (0xD8)
- +0 name (12), +12 header (20): hdr[0] = instrument level (squared amplitude: 40*log10(L/127) dB).
  hdr[3]/hdr[4] are not the reverb/chorus sends (sends = CC91/CC93 as is, measured on the chip's send register).
- +32 partial 0, +124 partial 1 (92 bytes each): +0 0, +1 ?, +2 part index (BE, 0xFFFF = unused), +4 pp[88]:

| pp | Meaning (measured) |
| --- | --- |
| 5 | pan, 0x40 centre (0x01 = left) |
| 6 | coarse tune, semitones - 0x40 |
| 7 | fine tune, ~0.87 cent per step - 0x40 |
| 9 | pitch key follow: (pp9 - 0x40) x 10 %, centred on key 60 (0x4A = 100 %, 0x45 = 50 %, 0x42 = 20 %) |
| 10 | vibrato depth, ~1 cent per step (bit 7 = ? flag); rate ~5.5 Hz, delay ~0.2 s |
| 33 | TVF cutoff (index; coefficient table G_CUT in sc55sf_oracle.c, ~1.3 octaves per 16 steps) |
| 34 | TVF resonance: SVF damping = clamp(pp34, 8, 58) / 64 (the firmware opens it at high cutoffs) |
| 35 | TVF type: 0 = low-pass, 1 / 3 = high-pass (chip control bit 1) |
| 40 | TVF envelope depth: index = pp33 + (L - 64) x pp40/64 x 0.75 |
| 41-45 | TVF envelope levels L0 (start), L1..L4 (L4 = sustain) |
| 46-50 | TVF envelope times T1..T5 (same time table as the TVA) |
| 65 | partial level (squared amplitude) |
| 69 | TVA start level L0 (0 in all GM tones) |
| 70-73 | TVA levels L1..L4 (L4 = sustain); amplitude = (L/127)^2 |
| 74-78 | TVA times T1..T4, T5 = release. Bit 7 set = linear stage of D(t) ms; clear = exponential approach, time constant tau(t) |

Stage times (ms, every 8 steps of t, measured with exp_times.py): D = 0, 60, 142, 258, 413, 626, 912, 1308, 1841,
2553, 3522, 4860, 6642, 9210, 12498 (t = 0x78), tau = 0, 7.2, 17.3, 26, 36, 58, 84, 124, 170, 240, 335, 465, 639,
890, 1192, 1613, 2174. Stage time does not depend on the level change (time, not rate). Velocity: TVA peak x
(vel/127)^2 for most tones (-12 dB at 64; some -11/-14: velocity curve bytes not decoded).

Level of a partial (peak, dB) ~ 40log(hdr0/127) + 40log(pp65/127) + 40log(sample vol/127) + 40log(max L/127)
(rms error 1.4 dB over the GM set). Pitch (cents from the sample's own rate, 32 kHz voice rate):
100*(60 - root) + kf*(key - 60) + 100*(pp6-64) + 0.87*(pp7-64) + (sample pitch - 1024)/10 - 1.3 (rms 5 cents).

### Part (0x3C)
+0 name (12), +12 breaks[16] (highest key of split y), +28 sample[16] (BE word; split y = first y with key <= breaks[y]).

### Sample (0x10)
+0 level (squared amplitude), +1 address (3 bytes, see above), +4 ? (BE), +6 length (BE: end = addr + length,
inclusive), +8 loop length (BE: loop start = end - loop length), +10 mode (0 = forward loop, 1 = ping-pong,
2 = one shot), +11 root key, +12 pitch (BE, (v - 1024)/10 cents), +14 fine level (BE, 1024 = 0; not used yet).
Checked: the firmware writes start / loop / end exactly so (PCM regs ram1[4] / [2] / [0]), bank to ram2[7] bits 8-11.

### Drum kit (0x48C)
+0 instrument[128] (BE words), +256 level[128], +384 key[128] (the instrument plays at this key), +512 assign
group (exclusive class), +640 pan, +768 reverb, +896 chorus, +1024 flags (bit 0 = note-off received), +0x480 name.
Kit program -> table (oracle, reads at program change on channel 10): 0 -> 0 STANDARD, 8 ROOM, 16 POWER,
24 ELECTRONIC, 25 TR-808, 32 JAZZ, 40 BRUSH, 48 ORCHESTRA, 56 SFX, 127 -> 13 CM-64/32L (9-12 AC.* kits unused
by program change); other numbers fall back to the group (1 -> 0, 26 -> 24), 57 -> none.

### Variation map
Words for bank 0..127 x program; banks with own tones on mk1 v1.21: 1-9, 16, 126, 127 (127 = MT-32 set).

## PCM chip registers (Nuked-SC55 pcm.cpp, what the trace shows)
ram1[0] end, [2] loop, [4] address; ram2[0] pitch step (0x4000 = 1 sample per 32 kHz tick), [1] pan L/R gains,
[2] reverb/chorus send, [3] level ramp (target<<8 | speed; volmul1 = CC7/CC11 level), [4] TVA ramp, [5] TVF ramp,
[6] resonance (bits 8-14) + LPF/HPF (bit 1), [7] pitch slot, ping-pong (6), reverse (7), bank (8-11),
[9]/[10]/[11] current level / TVA / cutoff (Q14 SVF coefficient; the SVF runs at 64 kHz, voices at 32 kHz).

## Not decoded yet (chat 2)
Velocity curves, TVA/TVF time key-follow and velocity-follow, TVF key follow (pp38, small), LFO rate/2nd LFO
(pp11 = tremolo?), pp36/pp57/pp63, sample +4 and fine level, alternate/random pan, hdr[1..19]; mk2 control ROM
(512 KB, different layout).

## Checks against the /VSF engine (TinySoundFont, sbemu/tsf.h)
- testrig/sc55sf/tsfplay renders through tsf.h as vmpu.c drives it; ocompare.py uses it by default (RENDER=fluid for
  fluidsynth). fluidsynth and TSF differ: TSF's amp decay/release = time for 80 dB (exp(-9.226)), not 100; TSF
  ignores SF2 modulators (CC91/93 sends) and, per region, uses the modEnv only for pitch/cutoff.
- SF2 ibag/pbag generator indexes are 16-bit: more than 65535 igen records makes tsf.h read past its arrays (crash in
  tsf_load_presets). Keep common generators in each instrument's global zone and leave defaults out.
- 2026-11-16, 128 GM programs, notes 48/60/72, dry, TSF: tools/sc55sf.c (aa1e14a) env 4.8 dB, bands 2.6 dB, level 2.6 dB;
  sc55sf_oracle.c env 4.5 / bands 3.1 / level 2.7. Drum kits (10 kits x 10 notes): tools 10.9 / 3.5 / 4.3, oracle 6.1 / 3.4 / 1.5.
  avmidi 02 / 07 (60 s, DTW chroma vs Nuked with its default reverb): tools 0.769 / 0.627, oracle 0.764 / 0.613.
