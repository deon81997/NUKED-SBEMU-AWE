# SC-55 control ROM layout (for SC55SF)

Source: libEmuSC (LGPL-2.1+, github.com/skjelten/emusc, libemusc/src/control_rom.cc/.h, wave_rom.cc, pitch.cc, tva.cc, tvf.cc).
All multi-byte values big-endian. "Control ROM" = 256 KB ROM2 (mk1: M1_ROM2.BIN), "CPU ROM" = 32 KB ROM1 (M1_ROM1.BIN).

## Identify (by content)
- SC-55 mk1: ctrl ROM 0xF380 = "Ver" + "1.21" ... date at +24 (user's M1_ROM2: "Ver1.21 ... '91/08").
- SC-55mkII: ctrl 0x3D148 = "GS-28 VER=2.00  SC              " (version 4 chars at 0xFFF0).
- SCC-1: ctrl 0x3D155 = "VER"; SC-88: ctrl 0x7FC0 = "GS-64 VER=3.00  SC-88   " (not parsed).
- Wave ROMs: 1 MB (mk2 WAVE1 2 MB), plain 32-byte header "ROLAND GSS      INTERNAL VER..." / "Roland GSS".
  Same header on several files: order slots by decode sanity (looped samples: |s[end]-s[loopStart]| small).

## Banks (SC-55 and mk2): {0x10000, 0x1BD00, 0x1DEC0, 0x20000, 0x2BD00, 0x2DEC0, 0x30000, 0x38000}
- Instruments: 216 B each, x = b0..b4, jump b1 -> b3; skip if name[0]==0 (indices = kept order).
  0 name[12], 12 volume, 14 LFO1 wave, 15 rate, 16 delay, 17 fade, 18 partialsUsed (bit0/1), 19 pitchCurve.
  Partial p at +32+92*p: 1 rootKeyOffset, 2-3 partialIndex (0xFFFF unused), 4-7 LFO2 wave/rate/delay/fade, 8 TVF flags,
  9 pan (0x40 centre), 10 coarse (0x40 = 0 semis), 11 fine (0x40 = 0 cents), 12 randPitch, 13 pitchKeyFlw (0x4A=100%),
  14/15 TVP LFO1/2 depth, 16 pitch env depth, 18-22 P-env L0 L1 L2 L3 L5, 23-27 P-env T1-T5, 30-33 P-env time keyfollow,
  34/35 P-env vel sens / time vel sens, 36 TVF cutoff vel curve, 37 TVF base cutoff, 38 resonance, 39 TVF type
  (0 LPF, 1 HPF, else off), 40/41 cutoff keyfollow curve/amount, 42/43 TVF LFO depth, 44 TVF env depth,
  45-49 TVF env L1-L5, 50-54 TVF env T1-T5, 57-60 TVF time keyfollow, 61 cutoff vel sens, 62/63 TVF time vel sens,
  64 TVA level vel curve, 69 volume, 70 bias point, 71 bias level, 72/73 TVA LFO depth,
  74-77 TVA env L1-L4 (L0=L5=0), 78-82 TVA env T1-T5 (bit7 set = linear shape), 85-88 TVA time keyfollow,
  89/90 TVA time vel sens.
- Partials: 60 B, x = b1..b5, jump b2 -> b4, keep if name[0]: name[12], breaks[16], samples[16] (u16).
  Sample pick: breakPitch = basePitchC - rootKeyOffset + 0x40; first j with breaks[j] >= breakPitch or == 0x7F.
- Samples: 16 B, x = b2..b6, jump b3 -> b5, keep if len != 0: 0 volume, 1-3 address, 4-5 porta offset, 6-7 len,
  8-9 loopLen (loop start = len - loopLen), 10 loop mode (0 fwd, 1 ping-pong, 2 none), 11 root key,
  12-13 pitchInit, 14-15 pitchSust (value-1024 = 1/1000 semitone; sustain offset applies after first loop).
- Variations: b6 + bank*256: 128 x u16 instrument index per program (0xFFFF = none; SC-55 falls back bank-1 until found).
- Drum sets: b7 = 128-byte LUT program -> set index; sets from b7+128, 1164 B each up to 0x3C028:
  preset[128] u16, volume[128], key[128] (play key), assignGroup[128], pan[128], reverb[128], chorus[128],
  flags[128] (0x10 note on, 0x01 note off), name[12]; skip names "AC." or name[0] >= 0x80.

## Wave data (Nuked-SC55 rom_io.cpp unscramble = same as EmuSC)
- Address bit j of dest = src bit aa[j], aa = {2,0,3,4,1,9,13,10,18,17,6,15,11,16,8,5,12,7,14,19}; data bits dd = {2,0,4,5,7,6,3,1}.
- Sample address bank (addr & 0x700000)>>20: 0 -> 0x000000, 1 -> 0x100000, 2 -> mk1 0x100000 / mk2 0x200000, 4 -> 0x200000.
- DPCM: for i in 0..len: a = rom+i; d = (int8)rom[a]; nib byte = rom[((a&0xFFFFF)>>5)|(a&0xF00000)], nibble = (a&0x10)? hi : lo;
  acc += d << nib (float: acc += d*2^(nib-17)). Ping-pong: EmuSC appends the mirrored loop (span+1 values).
- Playback: native step 1.0 = 32 kHz; at key == rootKey sample plays (pitchInit-1024)/1000 semitones above native
  -> SF2: rate 32000, originalPitch = rootKey, pitchCorrection = (pitchInit-1024)/10 cents (to verify vs renders).

## CPU ROM LUTs (SC-55 1.21; mk2 1.01 offsets in control_rom.h)
PitchParamScale 0x14C6 (21 x u16; [10]=32767 -> 1 semitone/key), EnvTimeScale 0x67C6, EnvelopeTime 0x6F12 (128 x u16,
EmuSC prints dur*8/32000 s), TVFCutoffFreq 0x7612 (129 x u16, SVF f = 2*v/32768 -> fc = 32000/pi*asin(v/32768),
capped 0xE600), TVFResonance 0x7816, TVALevelIndex 0x6B0F (128 B: param -> attenuation steps, 127 -> 0, 64 -> 36),
TVALevel 0x6B8F (256 B linear amplitude, ~0.35 dB/step), TVABiasLevel 0x6C8F(?), TVAPanpot 0x6B8F-... see header.
Prog LUTs in ctrl ROM: VelocityCurves 0x3D1E8 (10 x 128), KeyMapperIndex 0x3DC72, KeyMapper 0x3DD82.
User's M1_ROM1.BIN gives sane tables at these offsets (checked 2026-11-16).
TVA level: idx = max(0xFF - LI[partVol] - LI[smpVol] - LI[instVol] - LI[velCurve(vel)], 1) (+/- bias);
env level n = TVALevel[idx - LI[Ln]].

## Calibration against Nuked-SC55 (tools/sc55sf.c, testrig/sc55sf/probe.py, 2026-11-16)
- Pitch: SF2 rootKey 60, coarse/fine = (coarse-0x40) + 60 - sample rootKey + (pitchInit-1024)/1000 + (fine-0x40)/100 semitones,
  scaleTuning = PitchParamScale[|kf|]*200/65536 (kf = pitchKeyFlw-0x40), 32 kHz samples: median error 2 cents. Confirmed.
- Sample pick per key (breaks vs basePitchC - rootKeyOffset + 0x40) gives the right multisample zones.
- Wave ROM slots: each 1 MB slot scored alone (loop-end continuity of the samples that live there) picks M1_WAVE1/2/3 right.
- TVA times: EnvelopeTime*8/32000 s is ~5.5x too short vs Nuked for T1-T4 and ~3.5x for T5 (fitted -t550 -r350; cause
  not traced - envelope update rate?). TVA phases: bit 7 of T set = linear, clear = exponential (2^-16 per phase = ~96 dB).
- TVA level: TVALevel[0xFF - LI[partial vol] - LI[sample vol] - LI[inst vol] (- LI[drum vol]) - LI[Ln]] is right to ~5 dB sd.
- TVF: cutoff coef = 2*TVFCutoffFreq[(base<<8 + TVF env level init)>>8] (tvf.cc); the env term (L1-L5 through TVFEnvScale
  0x79f2 x TVFEnvDepth 0x7512 x velocity sens, + cutoff key follow 0x74d2) is large: base alone (~400 Hz for pianos) is
  far too dark (band error 7.8 dB with base only, 3.1 dB with no filter). Mapped: initialFilterFc = lowest of key follow +
  L1-L4, modEnv attack T1 / decay T2-T4 / sustain L4 / release T5 -> modEnvToFilterFc = peak - lowest: band error 2.4 dB.
  Mind the casts: be16() is unsigned, (km - 0x4000) must be int.
