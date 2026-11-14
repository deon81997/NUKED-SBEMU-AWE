# EMU8000 registers (as 86Box emulates them)

Source: 86Box `src/sound/snd_emu8k.c` and `src/include/86box/snd_emu8k.h`
(fetched 2026-11-14). This is the map `emu8k.c` implements. Register code in
this library: `EMU8K_REG(port, r)`, where `r` is the 3-bit register number
written to the Pointer, and `ch` is the 5-bit channel (voice) number.

## Ports (EMU base = SB base + 0x400: 0x620, 0x640, 0x660, 0x680)

| Port | Address | 86Box name | Use |
| --- | --- | --- | --- |
| Data0 | base+0x000 (lo) / +0x002 (hi) | 0x620/0x622 | 32-bit voice registers, r 0-7 |
| Data1 | base+0x400 | 0xA20 | r 0/1: low word of a dword; r 2-7: word registers |
| Data2 | base+0x402 | 0xA22 | r 0/1: high word of the Data1 dword; r 2-7: word registers |
| Data3 | base+0x800 | 0xE20 | word voice registers |
| Pointer | base+0x802 | 0xE22 | write: bits 0-4 channel, 5-7 register. Read: same, high byte 0x80+counter (Impulse Tracker / OpenCP probe that it changes) |

`emu8k_io_write/read(offset, word)` implement these ports. `emu8k_write/read`
take whole values: Data0 and the Data1/Data2 dword pairs as one 32-bit value
on port 0 or 1; port 2 with r 0/1 only for the word registers SMRD and WC.

## Data0 (dword, per voice)

| r | Name | Bits | Meaning |
| --- | --- | --- | --- |
| 0 | CPF | 31-16 current pitch, 15-0 fractional address | 0x4000 = 1.0 (44.1 kHz). Envelope engine overwrites the pitch each sample; fraction is read-only |
| 1 | PTRX | 31-16 pitch target, 15-8 reverb send, 7-0 aux (pan) send | Target overwritten by the engine (from IP + modulation) |
| 2 | CVCF | 31-16 current volume, 15-0 current filter cutoff | Volume slides to the target by 0x400 per sample |
| 3 | VTFT | 31-16 volume target, 15-0 filter target | Written by the engine when on; by software when DCYSUSV bit 7 = 1 |
| 4 | Z2 | | Filter delay memory; stored, no effect |
| 5 | Z1 | | Filter delay memory; stored, no effect |
| 6 | PSST | 31-24 pan (0 = all right in 86Box: vol_l = pan, vol_r = 255 - pan), 23-0 loop start | Word address |
| 7 | CSL | 31-24 chorus send, 23-0 loop end | When the address reaches loop end it jumps back by (end - start). Start = end = 0: no loop |

## Data1 / Data2

| Port | r | ch | Name | Width | Meaning |
| --- | --- | --- | --- | --- | --- |
| 1+2 | 0 | any | CCCA | dword | 31-28 filter Q (0-15), 26 DMA active (voice muted), 25 DMA write, 24 right channel; 23-0 current address. Q also lowers DC gain (86Box filter_atten) |
| 1+2 | 1 | 9 | HWCF4 | dword | Chorus delay offset (1/256 sample) |
| 1+2 | 1 | 10 | HWCF5 | dword | Chorus LFO rate (86Box: milli-Hz) |
| 1+2 | 1 | 13, 14 | HWCF6, HWCF7 | dword | Unknown (chorus/LFO reset?) |
| 1+2 | 1 | 20, 21 | SMALR, SMARR | dword | Sample memory read address, left/right DMA stream (top byte not addressable) |
| 1+2 | 1 | 22, 23 | SMALW, SMARW | dword | Sample memory write address, left/right |
| 1 | 1 | 26 | SMLD | word | Write: store at SMALW, SMALW++. Read: returns the previously fetched word (one-read pipeline), fetches SMALR, SMALR++ |
| 2 | 1 | 26 | SMRD | word | Same for the right stream (SMARW / SMARR) |
| 2 | 1 | 27 | WC | word | 44.1 kHz sample counter (games busy-wait on it) |
| 1 | 1 | 29 | HWCF1 | word | Config 1; reads back (HWCF1 & 0xFE) \| (HWCF3 & 1). Power-on 0x59 |
| 1 | 1 | 30 | HWCF2 | word | Config 2; read is a bit shuffle of HWCF1/2/3 (86Box: "VLSI error"). Power-on 0x20 |
| 1 | 1 | 31 | HWCF3 | word | Bit 2 = audio enable (0x04). Muted at power-on; set to 4 on the first envelope start (Doom / OpenCP never init it) |
| 1 | 2 | 0-31 | INIT1 | word | Effects init (reverb/chorus parameters by channel); INIT1[0] = 0x03FF means init steps 1/2 |
| 2 | 2 | 0-31 | INIT2 | word | Effects init |
| 1 | 3 | 0-31 | INIT3 | word | Effects init (chorus feedback ch9, delay ch12, reverb input ch1) |
| 2 | 3 | 0-31 | INIT4 | word | Effects init (chorus depth ch3, reverb return ch31) |
| 1 | 4 | voice | ENVVOL | word | Volume env delay: bit 15 = none, else (0x8000 - v) * 32 samples |
| 1 | 5 | voice | DCYSUSV | word | 15 release, 14-8 sustain (0x7F = 0 dB, 0.75 dB steps), 7 = 1 engine off, 6-0 decay/release rate. 1->0 on bit 7 starts the envelopes and resets the LFOs |
| 1 | 6 | voice | ENVVAL | word | Mod env delay, as ENVVOL |
| 1 | 7 | voice | DCYSUS | word | Mod env: 15 release, 14-8 sustain (0x7F = full), 6-0 decay/release rate |
| 2 | 4 | voice | ATKHLDV | word | 15 = 1 no trigger, 14-8 hold ((0x7F-h) * 4096 samples), 6-0 attack (0 = never, 1-31: 11878/a ms, 32-127: 360 * 2^-((a-32)/16) ms). Bit 15 = 0 with the engine on: retrigger volume env + LFOs |
| 2 | 5 | voice | LFO1VAL | word | LFO1 delay, as ENVVOL |
| 2 | 6 | voice | ATKHLD | word | Mod env attack/hold, as ATKHLDV (retriggers the mod env) |
| 2 | 7 | voice | LFO2VAL | word | LFO2 delay, as ENVVOL |

## Data3 (word, per voice)

| r | Name | Bits | Meaning |
| --- | --- | --- | --- |
| 0 | IP | 15-0 | Initial pitch: 0xE000 = no shift, 0x1000 per octave; 0 = stopped |
| 1 | IFATN | 15-8 initial cutoff, 7-0 attenuation | Cutoff 0..255 = 125 Hz * 1.0164^n (~8 kHz open at 0xFF); attenuation 0.375 dB steps (0xFF = silent). An all-zero write to a silent, stopped voice is ignored (tracker "note off") |
| 2 | PEFE | 15-8 pitch, 7-0 filter (signed) | Mod env -> pitch (+-1 octave), -> cutoff (full range) |
| 3 | FMMOD | 15-8 vibrato, 7-0 filter (signed) | LFO1 -> pitch (+-1 octave), -> cutoff (+-3 octaves) |
| 4 | TREMFRQ | 15-8 tremolo (signed), 7-0 LFO1 rate | Tremolo +-12 dB; rate 0.01 + 0.042 Hz * n |
| 5 | FM2FRQ2 | 15-8 vibrato (signed), 7-0 LFO2 rate | LFO2 -> pitch |
| 6 | - | | Reads 0xFFFF |
| 7 | ID | | Write: stored. Read: 0x1C, \| 0xFF02 when bit 1 of the written value is set (lets programs see the card is in use) |

## Memory map (word addresses, 24 bit)

| Range | Contents |
| --- | --- |
| 0x000000-0x07FFFF | 1 MB GM ROM (AWE-DUMP images start 2 bytes late: detected by "1MGM" at word 3 and realigned) |
| 0x080000-0x1FFFFF | Unmapped (reads 0) |
| 0x200000-... | On-board RAM (AWE32: 512 KB = 0x40000 words; up to 28 MB). Writes past the end wrap (OpenCP sizes RAM this way) |
| 0xFFFFE0-0xFFFFFF | FM (OPL) stream area; writes ignored |

Interpolation reads one word after the address (the "interpolator offset"),
which is why drivers program loop points as `point - 1`.

## Envelope and modulation scaling (86Box)

- Attenuation range 0..2^21 = 0..96 dB; volume target = 65535 * 2^(-att/2^21 * 16).
- Volume env: delay -> attack (linear amplitude) -> hold -> decay to sustain;
  release ramps to silence at the DCYSUSV rate. Decay/release step table: 86Box
  `env_decay_to_dbs_or_oct` (from the Linux AWE driver's `decay_time_tbl`).
- Mod env: same shape, value 0..2^21 = 0..1 "full height"; attack is linear in
  Hz and mapped through log2. 86Box goes hold -> jump to sustain; this library
  decays from the peak after hold, as the docs describe.
- LFOs: triangle starting at 0 going up, period 65536 table steps.
- Mod amounts: signed byte scaled to +-0x4000; pitch += env*h>>14 (1 octave),
  LFO vibrato tri*h>>17, cutoff += env*h>>5 and LFO tri*h>>9, tremolo tri*h>>11.
