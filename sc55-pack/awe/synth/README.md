# EMU8000 synth engine (AWE32 / AWE64)

Standalone C library that emulates the EMU8000 wavetable chip: 32 voices
playing from ROM/RAM sample memory with loops, pitch, volume and modulation
envelopes, two LFOs, a resonant low-pass filter and pan. Register map and
behaviour follow 86Box's EMU8000 emulation ([REGS.md](REGS.md)); the code is
written from scratch, integer-only in the sample loop, and builds with DJGPP.
Port trapping and the SB16 side live elsewhere (repo setup chat); this folder
only holds the synth.

## Interface (`emu8k.h`)

```c
int      emu8k_init(int ram_kb);                     /* 0..28672 KB sample RAM; 0 = ok   */
void     emu8k_write(int reg, int channel, uint32_t value);
uint32_t emu8k_read(int reg, int channel);
void     emu8k_ram_write(uint32_t addr, uint16_t word); /* word address, RAM at 0x200000 */
uint16_t emu8k_ram_read(uint32_t addr);
void     emu8k_render(int16_t *stereo, int frames, int rate); /* any rate; engine 44.1 kHz */
int      emu8k_load_rom(const char *path);           /* AWEROM.BIN or ROM-layout .SF2   */
```

- `reg` = `EMU8K_REG(port, r)`: port 0 Data0, 1 Data1, 2 Data2, 3 Data3, r = the
  3-bit register number; names such as `EMU8K_DCYSUSV`, `EMU8K_IP` are in the
  header. Dword registers take the whole 32-bit value (see REGS.md).
- Port-level glue for the trap layer (optional, 16-bit accesses, offset from
  the EMU base 0x620/0x640/0x660/0x680):
  `emu8k_io_write(offset, word)`, `emu8k_io_read(offset)` with offsets 0x000,
  0x002 (Data0 lo/hi), 0x400 (Data1), 0x402 (Data2), 0x800 (Data3), 0x802
  (Pointer). This handles the Pointer register and dword halves exactly as 86Box.
- `emu8k_clock(samples)` advances the WC sample counter without rendering, for
  games that busy-wait on WC while no audio is pulled (WC also advances with
  every rendered 44.1 kHz frame).
- One global instance; not thread-safe: serialize writes with rendering
  (e.g. render from the same interrupt context, or block it while trapping).
  `emu8k_render` never allocates.

Compile-time switches: `EMU8K_CTRL` (default 16) = samples per envelope/LFO
update (1 = per sample like 86Box); `EMU8K_LINEAR=1` = linear instead of
4-point interpolation (about 15-20% faster, duller highs).

Memory: about 250 KB static tables/state, 1 MB ROM, plus sample RAM.

## Status

| Part | State |
| --- | --- |
| Register file, Pointer, dword halves, read-back | Done, read-back identical to 86Box (t11) |
| Sample memory, SMALR/SMALW/SMLD/SMRD streams, RAM wrap, ROM map | Done |
| 32 voices, loops, 4-point interpolation, pitch (IP, CPF/PTRX) | Done, matches 86Box at 42-109 dB SNR |
| Volume envelope (delay/attack/hold/decay/sustain/release), slides | Done, 60 dB SNR vs 86Box (83 dB with EMU8K_CTRL=1) |
| Mod envelope -> pitch/cutoff, LFO1 (vibrato, tremolo, filter), LFO2 | Done |
| Resonant low-pass (Q 0-15, cutoff 125 Hz-8 kHz) | Done, 2-pole (see deviations) |
| Pan, HWCF3 mute, DMA-voice mute | Done |
| Reverb / chorus | Stub: sends and INIT/HWCF parameters stored, no effect output |
| ROM: raw dump / AWE-DUMP offset / ROM-layout SF2 | Done |
| ROM from an arbitrary GM SoundFont (remap to SYNTHGM ROM sample slots) | Not done (needs a SYNTHGM ROM sample map) |
| Capture of real-game register logs from 86Box | Not done here (no AWE games in the rig yet) |

Deviations from 86Box (deliberate):
- Filter: 2-pole resonant low-pass (as the EMU8000 docs describe) instead of
  86Box's 4-pole Moog approximation; same cutoff/Q mapping and DC gain table.
  Q0 sweeps match within 0.2 dB, Q15 within 1 dB; low static cutoffs are
  ~4.5 dB brighter, and at mid Q 86Box loses more passband (up to ~7 dB).
- Mod envelope after hold decays from the peak (86Box jumps to sustain).
- Delay/hold counters reload from their registers at every trigger.
- 86Box bug found while testing: in `emu8k_update` the mix pointer only
  advances on samples where a voice has volume, so a voice that is silent for
  k samples (every note start) lands k samples early in that 980-sample chunk
  and leaves zeros at its end. The reference build patches that one line.

## Speed

`EMU8KBEN.EXE` (DJGPP, `-O2 -march=pentium2`, HDPMI32I) in the 86Box rig
(store-86rig: Celeron Mendocino 366, P6 core, dynarec). 450 MHz = scaled by
366/450.

| Case (44.1 kHz) | CPU @ 366 MHz | @ 450 MHz | per voice @ 450 |
| --- | --- | --- | --- |
| idle | 0.5 % | 0.4 % | - |
| 32 voices, no filter/LFO | 19.0 % | 15.4 % | 0.48 % (~49 cycles per voice-sample) |
| 32 voices, filter + LFOs + mod env | 33.2 % | 27.0 % | 0.84 % (~86 cycles per voice-sample) |
| 32 released voices (engine on, silent) | 0.6 % | 0.5 % | ~0 |
| same, EMU8K_LINEAR=1 | 15.2 / 28.5 % | 12.4 / 23.1 % | |

Started at 48.5 % (heavy case, 366 MHz); see git log for the steps. Host
(Xeon 2.1 GHz): ~11-29 ns per voice-sample.

## How to test (host only)

```sh
make                       # test/harness, test/bench
AWEROM=/path/awe32.raw make test   # last line PASS/FAIL
```

`test/run_tests.sh` generates the scripts (`test/mkscripts.py`), builds the
engine normally and with ASan/UBSan, builds `test/harness_ref` from 86Box's
own `snd_emu8k.c` (`test/fetch_build.sh` fetches it to /tmp/dl/86box; GPL-2,
never committed), renders every script with all three, and compares the WAVs
(`test/compare.py`: SNR at best lag, worst 50 ms level difference) plus the
register read-back text. It also checks that the ROM loads identically as a
raw dump, an AWE-DUMP image and a ROM-layout SF2, and renders at 11/22/48 kHz
under the sanitizers. 86Box's roms repo has `sound/creative/awe32.raw`.

Speed: `make dos`, then `test/run86bench.sh EMU8KBEN.EXE [more.exe]` boots the
store-86rig VM, runs them and prints the results.

### Register scripts (`*.awe`)

One command per line, `#` comments. Every write goes through the 16-bit port
glue, exactly like a trapped game.

| Command | Meaning |
| --- | --- |
| `ram KB`, `rom PATH` | Before anything else; `rom $AWEROM` uses the env var |
| `w NAME CH VALUE` | Register by name (REGS.md), e.g. `w DCYSUSV 3 0x7F7F`; dwords as one value |
| `io OFFSET WORD` | Raw port write (offset as for `emu8k_io_write`), for captured logs |
| `rd NAME CH`, `ior OFFSET` | Print a register / port read |
| `wait FRAMES`, `ms N` | Render 44.1 kHz frames / milliseconds |
| `fill ADDR LEN sine\|saw\|square\|noise PERIOD AMP` | Write a test wave into sample RAM via SMALW/SMLD |

A capture from 86Box or from the trap layer only needs `io OFFSET WORD` lines
for every EMU port write and `wait N` for the 44.1 kHz frames between them.

## Files

| File | |
| --- | --- |
| emu8k.h, emu8k.c | Engine |
| emu8k_rom.c | ROM / SF2 loader |
| REGS.md | Register map and scaling from 86Box |
| test/harness.c | Script -> WAV (also the 86Box backend with -DREF86) |
| test/mkscripts.py, test/scripts/ | Hand-written test scripts |
| test/compare.py, test/run_tests.sh | Comparison vs 86Box, sanitizer runs |
| test/fetch_build.sh, test/ref86box/ | 86Box reference build glue |
| test/bench.c, test/run86bench.sh | Speed benchmark (host, DOS, 86Box rig) |
