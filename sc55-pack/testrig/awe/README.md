# AWE32 test rig (86Box AWE64 vs SBEMU /AWE)

Used for awe.2 (2026-11-14). Two 86Box VMs from `../86box/p2bls-uhci.cfg` (P2B-LS, PII-400): **A** = `sndcard = sbawe64`
(`[Sound Blaster AWE64 #1] onboard_ram = 1024`; the Award PnP BIOS sets it to 220/5/1/5, EMU 620; the CMOS asks for F1,
drv.py presses it), **S** = ES1371 + SBEMU. 86Box records every stream with `audio start DIR` (automation socket):
86Box's EMU8000 is the `wavetable` stream (x SB16 mixer MIDI x master, 2 dB steps), SBEMU's output is `main`.
AWE ROM for tests: 86Box roms repo `sound/creative/awe32.raw` (copied to C:\ROMS, found by content).

Disks (`../mkdisk.py build DIR IMG 100`): FreeDOS KERNEL.SYS/COMMAND.COM (../rufus/res/freedos), HDPMI32I, and either
HIMEMX (`DEVICE=C:\HIMEMX.EXE`; HimemX built from github.com/Baron-von-Riedesel/HimemX with JWasm) = the user's usual
HIMEM-only setup, or JEMMEX NOEMS + `JLOAD QPIEMU.DLL` (real-mode traps). SBEMU needs XMS (no XMS: "Failed allocating
XMS", also beta.119). AUTOEXEC: `HDPMI32I -r`, `SBEMU /A220 /I5 /D1 /H5 /T6 /AWE:620 /AWERAM:1024 /JOY:1`, then the game.

Games (../../../testgames): Duke 3D 1.3D (`unzip DN3DSW13.SHR` - a PKZIP SFX), `DUKE3D.CFG` here (MusicDevice 6 = AWE32,
BlasterEmu 0x620). ROTT 1.3 (`unzip ROTTSW13.SHR`), `SOUND.ROT` here (written by its SNDSETUP on the AWE64 VM:
MusicMode 5 = AWE32, FXMode 2 = SB, SBEmu $620; a hand-written one with decimal numbers was ignored).

- `drv.py VMDIR SECS` boot (presses F1), record audio for SECS of emulated time, screenshot + text every step;
  `QUIT=1`: Duke's quit (Esc, Up, Enter, Y). Duke then shows shareware pages: Space x6 (or a.py) before SBDIAG runs.
- `k.py VMDIR shot.png 0x1c 0x150 w1000 ...` tap keys (60 ms), wait, screenshot (drove ROTT's SNDSETUP).
- `lev.py WAV...` dBFS per second; `cmp.py A.wav a0 a1 B.wav b0 b1` align B to A, level, envelope correlation, bands;
  `spec.py`, `hfband.py WAV lo hi` (band energy per second), `img.py` (mirror/shift image finder).
- `awedet.asm` (nasm -f bin -o AWEDET.COM): real-mode EMU8000 probe - ID, HWCF1/2 with word I/O, ID with byte I/O,
  SMALW/SMLD write + SMALR/SMLD read-back, 3 reads of the right stream (SMARR 0 = ROM). 86Box AWE64:
  `ID ok HWCF1 ok HWCF2 ok BYTE FAIL RAM ok 1234 5678 0000 0032 2E38`; SBEMU (QEMU, JEMMEX+QPIEMU): the same except
  BYTE ok (byte I/O is more lenient; drivers use words).

Results (awe.2): see root HANDOFF.md.
