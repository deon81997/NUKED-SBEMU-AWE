## beta.134-awe.1 Munt check (2026-11-27, Munt session; no new release): the AWE sync chat already shipped Munt 2.8.6 in beta.134-awe.1
- This chat's wip/awe-munt286-2026-11-27 (MUXED beta.134 Munt files) = beta.134-awe.1's Munt files byte for byte (rebase
  dropped it as "already upstream"), so no beta.134-awe.2: only test-rig fixes + one HWTEST line went to main.
- Tests old = beta.132-awe.1 (Munt 2.8.5) vs new (2.8.6): testrig/mt32host avmidi 05 MT-32 + CM-32L + float renderer
  byte-identical; MT32TEST differs from 10.7 s (2.8.6 update order; old vs old with events +1 frame differs 97%, octave
  bands within 0.35 dB). QEMU notetest /VMT32:MT32 MT32TEST 0 dropped both. Win98 enginetest mt32 old = new (byte-identical
  WAV; the AWE line's native build was broken: engine.cpp needs awe_* -> test/w32shim/awestub.cpp = AWE off), routetest
  PASS; SC55WM/SC55W2 imports = the awe.8 zip's. 86Box P3B-F PII-450 mt86.sh x2: MT-32 49% -> 44%, CPU 45% -> 42%;
  /AWE:620 + MT-32: AWE32 1% both, MT-32 48-49% -> 44%.
- mt86.sh: AWEROM= env (copies AWE32.RAW) + AWE line in the summary; kill -9 (Qt 86Box ignores SIGTERM: every run left
  one 86Box running, 5 at once by the 5th run).
- Branch wip/awe-munt286-2026-11-27 (mine, = c0c3545/358670e, superseded) can be deleted.
- Next: HWTEST beta.134 item (MT-32 with AWE32 off, AWE32 alone).

## beta.134-awe.1 (2026-11-27, AWE sync session): NUKED-SBEMU beta.133 + beta.134 on the AWE line (RELEASED, tag v1.0.0-beta.134-awe.1-usb)
- Sync: v1.0.0-beta.132-usb..133 (upstream SBEMU 82c2d66..9df0421) then 133..134 (Munt 2.8.6; JEMM 5.87 skipped upstream =
  stays 5.86), git apply -3 minus release/README.md/HANDOFF.md (+ NUKED's prebuilt win98 VXD/obj/map: rebuilt here).
  Hand fixes: main.c AWE options + /PCS both kept; qemm.c AWE split-port trap gets the _8087 guard; no FPU -> /AWE off too
  (emu8k tables are built with doubles); README98 = ours + "Munt mt32emu 2.8.6"; makefile/win98 VERSION/nsbver.h ->
  beta.134-awe.1. emu8k.c fx_reg/fx_hwcf integer-only (24/24 harness renders bit-identical; run_tests PASS): the trap
  wrappers already fnsave since beta.84, so it is no-FPU hygiene, not a fix. SB16 2 dB table feeds VAWE_MixInto via
  midivol (no other AWE mix path reads the mixer). beta.131/132 sc_sbliv.c/sc_emujack.h/sbdiag.c were already on main.
- Level: Duke AWE music ~11 dB quieter on purpose (MusicVolume 200 -> 34h C8h = -12 dB on a real CT1745; old read -2 dB);
  86Box AWE64 wavetable raw -39 dBFS - 12 = ours -51..-53. News + HWTEST say so.
- Tests: DOS clean build (63 warnings, none new); Win98 buildall with munt libmt32emu_2_8_6 (both targets), chkimp
  imports = awe.1; dbgtest/routetest/mputest PASS; duke.sh himem + jemm (QPIEMU) music plays, 0% lost ticks; 86Box
  PII-450 Duke: SBEMU 11% / AWE 10% / 3.08 ms old = new; smoke86 idle 1632719/1610185 -> 1634513/1611943; Win98 cpu86
  OPTS=/AWE:620 (24E x2, 301 x2, 620 x2; ticks): awe.1 157 157 156 156 188 188 / rerun 159 159 160 160 190 190,
  new 153 153 153 153 184 184. Not run: win98 enginetest mt32 (upstream showed 2.8.5 = 2.8.6 byte-identical).
- Zip = mkrel.py from the beta.132-awe.1 zip (28 members: DOS EXEs, README/HWTEST, README98 x2, all WIN98 binaries,
  SOURCE.ZIP); FILES/JEMM unchanged (5.86).
- Next: 1) HWTEST top item (D810 + P5PE-VM: Duke AWE level + volume steps, /PCS:0, AWECPU.TXT, Win98 /VER + MT-32);
  2) AWE wiki Changelog/Home row (priavte wiki/src, post job); 3) delete wip/awe-b133-2026-11-27 (= main) in the browser.
- Rig notes: vms/bin/86run.sh starts Xvfb :86 only if NO Xvfb runs - with duke.sh's :9 up the Qt 86Box dies (xcb):
  start `Xvfb :86` + openbox by hand; Qt 86Box also needs apt libxcb-cursor0 libxkbcommon-x11-0 libxcb-icccm4 ...
- Test downloads: DJGPP v3.4, OW snapshot, JWasm, munt (d885a95 + libmt32emu_2_8_6), 86Box 0935207 + xpad (SDL build
  /home/claude/86box/86box), apt mingw-w64 gcc-multilib cmake ninja SDL2/openal/slirp/serialport dev, pefile;
  store-86rig/dos71/w98qi-86box/86box-roms/personal.

## beta.132-awe.1 (2026-11-21, ZS S/PDIF session): NUKED-SBEMU beta.132 (Audigy 2 ZS S/PDIF out) on the AWE line
- Sync: NUKED-SBEMU 1685d70 (sc_sbliv.c, new sc_emujack.h, sbdiag.c, testrig/spdif jacktest.c/jacktrace.txt/smoke86.sh)
  applied cleanly (DOS only); README.TXT patch applied; HWTEST top item = beta.132 ZS item + AWE32-on line. makefile VERSION
  -> 1.0.0-beta.132-awe.1-usb-sc55 (was still awe.7: awe.8 was VxD-only).
- Tests: jacktest PASS (non-ZS 48/48 identical); make clean OK; QEMU testrig/spdif/run.sh PASS; 86Box P3B-F PII-450 ES1371
  smoke86.sh awe.8 vs new: IDLEMTR 1632720/1610187 vs 1632719/1610185 (noise). Win98 files in the zip = awe.8 (compared).
- Release: rezip.py from the awe.8 zip (19 members differ, none under WIN98). Wiki: AWE Sound-Cards gets the ZS line from
  priavte wiki/out/awe (not posted, no browser).
- Next: P5PE-VM HWTEST top item with AWE32 on.

## beta.131-awe.8 (2026-11-21, VxD 0E session): BLUE SCREEN 0E in SC55W.VXD at Descent's menu (user ASUS) - VxD reads DMA only via a proven address
- User (P5PE-VM, awe.6 = awe.4's VxD): Descent in a Win98 DOS box at the main menu with music: "exception 0E at
  0028:E994AB1A in VxD SC55W(01) + 0000178A, called from + 2B0" (photo testrig/w98dbg/user-asus-descent-0e.jpg) and
  loud noise bursts. +178A = convert() reading the SB DMA buffer (dmach[].addr) from Timer_CB (+2B0).
  User logs 11/20 23:33 / 23:45 (level 2, P5PE-VM): "F ... dma1=16D60/0" then 17D60 after 3.9 s (the F line printed
  only the low 20 bits, so address kind unknown; now %08X + bad=). No crash in those two logs.
- Fix (win98/vxd/dma98.c, #included by sbcore.c; host test win98/test/dmatest.sh): VDMAD_Get_Virt_State ESI = high
  linear only while VDMAD translation is on; a VDS-locked buffer (DOS4GW/HMI) gives a PHYSICAL address, and Timer_CB
  runs in any VM, so reading it = another VM's memory (noise) or not present (0E). Also V86 pages may be not present at
  timer time. sc_dma_changed (VM context) proves [addr, addr+count) when the channel is armed unmasked: inside the VM's
  CB_High_Linear 0..10FFFFh window -> _LinPageLock (63h) / unlock (64h) on change, box gone (sc_vm_gone) and detach;
  below 16 MB -> one _MapPhysToLinear (6Ch) alias of 0..16 MB, made once, never freed; else unproven. xf_pull /
  dsp_dma_id read only dma_at() (arithmetic, async-safe); unproven bytes = silence(k) (IRQs/position keep going,
  c_dmabad, "bad=" on F lines, I line "DMA n: physical address ..." / "... not readable, silence"). Service ordinals
  from the Win3.1/95 VMM table order (match 53/55/82/84/86/94/96/97 already used); _LinPageLock proven live in QEMU.
  Also on this release (from the WIP): sc_vm_gone stops the transfer when the owning DOS box ends (VM_Not_Executeable).
- Tests: dmatest/dbgtest/mputest PASS. QEMU acpi 512 MB hmi98.sh desc MENU=1 PLAY=600 DBGL=2 (new rig switches):
  AWE32 620 (MIDIDEV=0xa008): 10 min at the menu, quit, box closed, 1921 F lines bad=0, address E541BA60 (= high
  linear: QEMU keeps translation on, so the physical path is NOT exercised here), AWE voices mean 7.0 max 25;
  MPU 330 -> AWE32 MIDI Synth: same, 25191 notes, 1706 F bad=0. vmquit98.sh old vs new: both Windows fine after 3
  boxes closed mid-DMA (QEMU does not reproduce the box-closing crash either). 86Box r98.sh Duke 3D AWE32 (DUKEONLY,
  300 s, /DBG:COM1:1): old voices mean 8.2 max 17, new 8.5 max 18; REC.WAV -25.9 vs -25.8 dBFS. Not done: 86Box
  Descent menu run, cpu86.sh speed run (change = 3 compares per timer chunk + a lock per program).
- Release: awe.7 zip (AWE tune session, released mid-chat) with SC55W.VXD (VERSION awe.8; tested binary = awe.7-stamped,
  1 byte differs; other Win98 files = awe.7 zip's),
  README98/HWTEST, SOURCE.ZIP + dma98.c/dmatest. Open: the user's 15-min menu run + serial log on the P5PE-VM
  (HWTEST top item) to see which address kind the real PC uses.
- Rig lessons: hmi98.sh logger retry 600; kill orphans with `pkill -f w98par/run.sh` (a killed hmi98 leaves run.sh
  t/install sending keys into the next VM); setsid -f, not setsid &.
- Test downloads: OW snapshot, JWasm, gcc-multilib, qemu mtools socat xdotool arj zstd xvfb openbox python3-serial
  librtmidi6 imagemagick nasm; store-w98qi-qemu, store-86box-roms, store-86rig, store-w98qi-86box; testgames Descent + Duke.

## beta.131-awe.7 (2026-11-21, AWE tune session): AWE32 output tone matched to real-card recordings (emu8k.c)
- References: priavte store-kitr-awe (KITR Part 20 AWE32 + Part 15 AWE64 Gold, AAC: ignore > 12.8 kHz). Real-driver
  captures (86Box P2B-LS, AWEREC SBEMU, testrig/awe/rec.sh): Duke 1.3D title GRABBAG + attract E1L1 (90 s), Descent level 1
  (`DESCLEVEL=1 rec.sh descent EXE 110`: pilot "A", New Game; recdrv presses one Esc too many -> "Abort game?" box, music
  goes on; drop the last Esc). Replayed on the host: harness -s REC [-r RATE] [-tone]. Clips found by chroma
  (testrig/awe/kitrcmp.py aligns + 1/3-oct LTAS, width, rests, decay, attack; stwidth.py coherence/ILD; awetab.py one
  table row): Duke p20 1179.5-1208 s of p20.wav (= video 1517.5-1546; p20.wav starts at video 338 s) <-> ours 0-40 s,
  p15 689-717 s; Descent p20.wav 1088.5-1116.5 <-> ours 40-136, p15 1952-1980. Doom 2: no shareware here - not done.
- Proven (4 of 4: 2 cards x 2 drivers, + our MIDI path GRABBAG): ours 2-3 dB low at 2.5-6.3 kHz -> output peaking EQ
  +2.75 dB 3.6 kHz Q 1.2 (RBJ, Q12 32-bit, b1 = a1 trick, rounds toward 0: silence -> 0), off < 20 kHz output.
  /K 22050 took every 2nd 44.1 kHz sample: Descent's 11-22 kHz is only 4 dB under 5.5-11 kHz -> folded grit; now a
  39-tap half-band (Kaiser 7, flat to 9 kHz, -60 dB > 13.5 kHz). emu8k_set_tone(0) = old output (no switch exposed).
  Mean dev 2.5-5 kHz (Duke p20/p15, Descent p20/p15): 22050 -1.4/-2.1/-3.2/-2.7 -> +0.1/-0.6/-1.6/-1.0 dB;
  44100 -1.6/-2.2/-2.7/-2.1 -> 0.0/-0.5/-0.8/-0.2; Descent alias (8-11k vs 1-4k, 22050 vs 44100) +2.6 -> -1.1 dB.
- Not proven / not changed: Duke wider on both cards (L/R corr 0.73-0.74 vs ours 0.92-0.94, mid-band ILD spread 7.5 vs
  1.2 dB) but Descent's width matches (0.65 vs 0.62) -> likely Duke version (KITR probably Atomic) not the engine; reverb
  wet level (Duke p20 wants +3 dB, p15 none, Descent indifferent); 8-10 kHz (Duke +1.6..+5.6, Descent ~0); envelopes /
  attack equal (10-25 ms both). Models chapter: different material per card; where the same (CT3600~CT3630 chroma 0.87,
  CT3930~CT3990 0.94) the 1/3-oct bands agree within 0.8 dB -> no card-model EQ.
- Speed 86Box P2B-LS PII-450 duke.sh 60 s, alternated old/new x2-3: SBDIAG 10% -> 11%, AWE32 9% -> 10%, longest block
  3.10-3.13 -> 3.24 ms; callgrind (gcc -m32 -march=pentium2) +6.9% of engine instructions (~0.65% CPU). Music cmp 0.1 dB,
  env corr 0.996.
- Tests: run_tests.sh PASS (harness keeps the EQ off: -tone turns it on), awemidi_tests.sh PASS (first int64 EQ failed
  the "master volume 0" check: truncation limit cycle - fixed), ASan -tone at 11025/22050/32000/44100/48000 clean.
  harness -s now honours -r (it wrote 44100 frames at any -r). kitrcmp resamples with resample_poly (np.interp drooped
  22050 renders 3.5 dB at 8 kHz - wrong conclusions for an hour).
- Release: mkrel.py from the awe.6 zip: DOS make clean; SC55WM/W2/W.EXE rebuilt (build98.sh minus drv/vxd/joy, imports
  = awe.6's); VxD/DRV/JOY = awe.6's (copied into win98/out). Not run in the Win98 VM.
- Next: Win98 VM check of the AWE32 MIDI Synth (rate it renders at: engine.cpp awe_render); Duke Atomic capture to settle
  the width question; Doom 2 MAP01/09 if the user has DOOM2.WAD.

## beta.131-awe.6 (2026-11-20): AWE32 MIDI synth pan was ~17 dB left at centre (vs a real AWE32 recording)
- Reference: KITR (Karl's Retro Channel) Part 20 AWE32 audio, priavte branch store-kitr-awe (README = chapters + clip
  times; Duke 3D AWE32 1517-1546 = GRABBAG.MID from its start, clip 0.5 s = song 1.3 s). Host: awemid -r AWE32.RAW -b the
  user's real SYNTHGM.SBK (store-personal user-sc55-2026-11-20.7z) -m gm -v 4 -c 2 GRABBAG; cmp.py env corr 0.90.
- Bug: awemidi.c calc_pan_ used ALSA emux pan_volumes[] (constant-power, EMU10K1) as the EMU8000 PSST pan byte, which is a
  linear position (emu8k.c: L = pan, R = 255 - pan): centre = 180/255, ~17 dB left. Now linear (apan = 127 - 2*(pan-64)).
  Pan 64 +0.2 dB, 32 +9.9, 96 -9.3; GRABBAG L-R 0.4 dB (card 1.1, was 11.6). Only our MIDI path (Win98 AWE32 MIDI Synth,
  DOS /AWEMPU); games' own drivers write PSST themselves.
- Left vs the card (same song, ours through SYNTHGM.SBK, not Duke's own driver): bands within 2 dB (ours -1.5/-2 dB at
  1.6-6.4 kHz, +1.9 at 6.4-12.8k; above 12.8k = AAC cut), ours narrower (L/R corr 0.85 vs 0.73). Next: tuning chat
  (prompt in chat) - real-driver captures (rec.sh duke/descent, hmi98) vs the KITR clips before any filter change.
- Tests: awemidtest + ASan PASS. Release: DOS make clean (version is in every object) + Win98 exes as awe.5; VxD/DRV/JOY = awe.4's.

## beta.131-awe.5 (2026-11-20, user found it on the P5PE-VM): GM bank = Special Effects SYNTHGM.SBK; last SBK sample dropped
- User (P5PE-VM, awe.4): MPU-401 plays on "AWE32 MIDI Synth, GM bank" (/AWEMPU:GM) = bugged, GS fine. Screenshots: Bank 0
  "Special Effects (SYNTHGM.SBK)"; their C:\SC55 (7z, not in any repo): ROMS\SYNTHGM.SBK = SAMPLE.SBK (8 presets,
  "Special Effects", md5 3c95923c), the real GM bank (129 presets, ROM only) = C:\SC55\SYNTHGM.SBK. guess_kind gave -1,
  scan/vawe fell back to the file name -> GM. GS worked only because SYNTHGS.SBK came first. User log
  testrig/awe98/user-d810-heretic-awe4.log (= P5PE-VM? user says ASUS for the screenshots): 4507 of 6826 notes "without a sound".
- Fix: awemidi.c guess_kind = -2 for < 64 presets (after the INAM MT/GS name checks); vawe.c + awe.cpp scan skip -2;
  awe.cpp scan moves a -2 file already in ROMS to .OLD and copies the real one; load_banks refuses a -2 file with a note
  (GM mode then = NUKED's GM map). 2nd bug: bank load clamped RAM to the smpl data, so the last RAM sample (end + 48 guard)
  was always "left out" (SFX 1, SYNTHMT 2 zones): RAM = data + 48 zero words. awemidtest: 12-preset bank probes -2.
- Tests: awemidtest + ASan PASS (fluidsynth not installable here: chroma part not run; render path unchanged); user banks
  on the host (awemid, 02.mid, 28 MB): SFX as GM 9093/9093 missing, real GM 0; skipped 1/2 -> 0. Not run in a Win98 VM
  (awe.cpp scan/move = Win32 only): HWTEST top item. No runtime change (load path only): no 86Box speed run.
- Release: mkrel.py from the awe.4 zip (DOS rebuilt DJGPP v3.4 = awe.5; SC55WM/W2/W.EXE rebuilt mingw win32 + munt d885a95;
  SC55W.VXD/SC55MIDI.DRV/NUKEDJOY.VXD = awe.4's, no OW here, /VER shows awe.4 for those). README news/feature, HWTEST.
  Wiki not posted. Heretic "underwater" (wip/awe-heretic-2026-11-20) still OPEN - check whether it was this bank too
  (that log played Heretic + MIDPLAY GM via the effects bank).
- Test downloads: DJGPP v3.4, apt mingw-w64-i686-win32, munt d885a95, py7zr, pefile, 86Box roms-6_0 (awe32.raw).

## beta.131-awe.4 (2026-11-20): Win98 DOS-box AWE32 reads = DOS awe.2 (Duke 3D / Descent all voices in a DOS box)
- User (D810, awe.3, banks set): Descent with AWE32 music (HMI awemidi, 0xa008 at 620) "bugged"/thin; log S awe voices=0-2.
- Fix (sbcore.c mirror): DCYSUSV 0-29 start 807Fh / 30-31 0080h (AWEUTIL /S); VTFT read = awe_vtft(): attack start
  (vol 0 | written cut) after a note-on the helper has not rendered, else the helper's live vtft[] (SC55W_AWE vtft[32],
  interface 12; awe.cpp awe_snapshot) unless written since. dbgtest/mputest/routetest PASS. DOS files = awe.3 (= awe.2).
- 86Box r98.sh Duke 3D AWE32 music (DUKEONLY, 300 s, SERIALLOG + /DBG:COM1:1, SCROMS = AWE32.RAW only): awe.3 Win98
  files voices max 2 (mean 1.7, 223 S lines all <= 2) = the user's log; awe.4 max 17, mean 8.3 (18 of 169 <= 2).
- QEMU hmi98.sh desc MIDIDEV=0xa008 MIDIPORT=0x620 RUNS=1 PLAY=180: awe.3 mean 6.3 max 20 (27% of S lines <= 2),
  awe.4 mean 6.7 max 21 (17%) - this rig does NOT reproduce the user's 0-2 for Descent (QEMU timing), so Descent on the
  D810 is the open check (HWTEST top item). Not tested: banks set + the AWE32 MIDI Synth reclaiming the chip after
  10 s without DOS-box access (awe.cpp awe_midi_poll -> awem_reclaim/awem_start writes the shared engine; with awe.4
  VTFT reads come from that engine) - HWTEST step 4 asks for it; if Descent is still thin after a Media Player MIDI,
  reset the engine's voices to the AWEUTIL /S state on a DOS-box takeover.
- Release: awe.3 zip + Win98 rebuilt (mkrel.py --keepdos), README news/feature, README98, HWTEST, wiki src
  (Changelog row, Home, AWE32 page) + priavte wiki/post/awe-b131-awe.4.json (post after awe-b131-awe.3.json).
- Test downloads: OW snapshot, JWasm, munt d885a95, apt mingw-w64-i686-win32 qemu arj zstd mtools socat xdotool openbox
  xvfb; store-w98qi-qemu, store-86box-roms, store-86rig, store-w98qi-86box, store-personal; testgames desc14sw.exe +
  3dduke13.zip.
- Next: hardware results (HWTEST top item); post the wiki jobs; MUXED port of mpu98.c still open.
## beta.131-awe.3 (2026-11-20, Descent Win98 freeze session): MPU-401 real-card timing in DOS boxes, port owner names
- User log (D810, awe.10, level 3): Descent DOS box, MPU -> AWE32 MIDI Synth; ~75 s in: mpu=130666, "MPU-401 reset (FFh)",
  then ~190k reads/s with ack=0 for 4 s and serial output stops mid-line ("VXD I v") = hard hang; earlier ~6 s then exit.
- Cause (HMIMDRV.386 "mpu401.com" disassembled, 0x21d reset): mask PIC (or 0FDh = all but IRQ 1), wait up to 3FFFFh
  reads for DSR=0, out FFh, then loop up to 0FFFFFh reads WHILE DSR=0, read. A card's ACK comes after some us, so the
  first read says "no data" and HMI is done; our FEh was there at once -> 262143 + 1048575 trapped reads (= the log: 130666
  + 131k, then 5.6 s at 187k/s; QEMU HMIMPU baseline p=262143 w=1048575 exactly). Song change / quit = this reset.
- Fix (win98/vxd/mpu98.c, #included by sbcore.c; host test win98/test/mputest.sh, 6194 checks, old code fails it):
  a command's reply shows on DSR from the 2nd status read (mpu_hide); busy (DRR) at most 64 status reads per byte;
  2048 status reads in a row with nothing queued (UART / not intelligent) = one active-sensing FEh (ends HMI's pre-wait);
  FFh = no back-pressure until the next data byte; dbg I line per long spin + "S mpu spins=" line. HMIMPU now i=1 a=FE
  p=2047 w=0 r=FE. NOT done: VMM Release_Time_Slice on long spins (tried: the DOS box then got 1 slice/s = 4096 reads/s,
  HMIMPU 15 s per 64 bytes; VMM treats it as idle).
- The hard hang itself is not reproduced (QEMU: 3x1.38M reads in ~4 s, Windows fine). Facts: trap path per read = a few
  adds (no time call unless the ring is 3/4 full), dbg lines only at reset/UART/spin; the cut line was the first FIFO
  chunk of an I line ("vm=..."), async time-outs then stopped = ring-0 interrupts off / machine locked while the VM sat
  in HMI's masked-PIC loop. With the fix that loop is 1 read, so the window is gone; if the D810 still hangs, the
  log's last lines now show where (spins=, reset, ack=).
- OPL 388 owner: helper port_owner() reads HKEY_DYN_DATA\Config Manager\Enum\*\Allocation (IOType_Range 12 records)
  + HKLM\Enum\<HardWareKey> DeviceDesc; attach_problems (status window, start-up box) names the owner for SB/OPL/MPU
  port conflicts; "SC55W E OPL ports 388-38B belong to: <name>" on the debug link + SC55W.LOG. Claiming is not done:
  VMM has one handler per port and Remove_IO_Handler cannot give it back (until reboot) - documented in README98.
  86Box acpi (SB16 with its driver, ours at 240/300, OPL on): SC55W.LOG "OPL ports 388-38B belong to: 16-bit Sound
  [ROOT\*PNPB003\0000]" (= the SB16 devnode; /tmp/opl86.sh = cpu86.sh without /OPL:0, PORTS=24E).
- Tests: mputest/dbgtest/routetest PASS; QEMU acpi testrig/w98dbg/hmi98.sh (new): hmi (HMIMPU 3+2 + S spin) old
  p=262143 w=1048575 / new p=2047 w=0; desc (Descent 1.4 shareware from testgames, 4GWBIND in the VM, desckeys.sh;
  keys finished by hand in the VM, script fixed after): 3 starts + 3 quits, ~13 min of level/menu music, 789 S lines with
  midi, AWE32 MIDI Synth 190680 bytes / 28310 notes, normal play mpu= 240-1000/s (2 per byte), max 65638/s = HMI init's
  own 0FFFFh read loop, 6 MPU resets each done in ~50 ms, no hang.
- Final zip (awe.2 zip + Win98 rebuilt with awe.2's emu8k.c): QEMU MIDPLAY in a DOS box (hmi98.sh midplay; Start > Run
  missed once, run by hand): FFh, FFh, 3Fh handshake + TEST.MID played (2 reads per byte). 86Box r98.sh (copy with
  MUSDEV=4: Duke 3D General MIDI at 300 -> /AWEMPU:GM AWE32 MIDI Synth, DUKEONLY, SERIALLOG + /DBG:COM1:1): 386 s of
  music, 12678 notes, max mpu 2178/s (= 2 per byte), 1 reset, 0 spins. (86Box resyncs in SC55W E lines = emulator.)
- CPU 86Box P3B-F PII-450 cpu86.sh (8M trapped INs, 55 ms ticks, 24E x3 / 301 x3 = MPU status at /P:300), awe.1 vs
  this VxD alternated: old 158/162, new 155/154, old 158/161, new 155/155 - no cost (MPU path 4% fewer ticks).
- MUXED port (TODO, other chat): the standard Win98 build has the same front end (sbcore.c MPU section = mpu98.c here,
  copy it + test/mputest.*; nothing AWE-specific in it). DOS SBEMU main.c MAIN_MPU_331 non-USB path answers at once too
  (mpu_state 1 -> status 0 until read): HMI loops 1M reads in DOS as well (~1-2 s, PIC masked) - same hide/sense there.
- Test downloads: OW snapshot, JWasm, munt d885a95, apt nasm arj p7zip-full librtmidi6, store-w98qi-qemu,
  store-86box-roms (awe32.raw), store-86rig, store-w98qi-86box; Descent = testgames/desc14sw.exe (zip -> ARJ volumes).
## AWE native extras (2026-11-20, duplicate "native mode vs real EMU8000" chat; branch wip/awe-native-extras-2026-11-20, not released)
- Same task as beta.131-awe.2 (released 14:16 by the native-driver session); this chat found the same cause independently
  and stopped at the clash. Only test tools here, no engine change (emu8k.c untouched = awe.2).
- Why only DCYSUSV counts for Apogee (Duke 3D 1.3D, ROTT 1.3): Creative's AWE32 library inside DUKE3D.EXE (file 0xDC574)
  has a 32-bit register read that ends `or eax,esi` then `pop eax` (its saved EFLAGS): every VTFT/CCCA/PSST/CSL read
  returns EFLAGS. The allocator (0xDC5E8) scores voice = (table != FFFFh ? 200h : 0) + (DCYSUSV bit 15 ? 0 : 1000h)
  + VTFT(=EFLAGS>>16), takes the LAST lowest in order 0,3,..,30,1,..,31,2,..,29; its table is BSS 0 (never FFFFh), so
  only the release bit decides. A python model of it (allocator vs recorded reads) matched 748 of 748 Duke picks.
  On a real card AWEUTIL /S leaves 807Fh = all idle voices "released"; 0080h/0000h = 26 and 29 only.
- ROTT checked with awe.2: 2 voices (26, 29) on awe.1 -> 13 voices (2..29) on awe.2 (SBEMU stream, 70 s attract mode).
- Engine vs 86Box on the same stream (harness -s + -P probe, pvdiff.py; Duke/ROTT/Heretic): pitch 0.00 cents, volume
  p99 0.3-0.6 dB, cutoff/address equal. Open: Heretic (DMX, filtered voices IFATN hi DC/BA/E2, Q 0) is darker than 86Box
  above 5 kHz (-1..-9 dB 5-10 kHz, up to -15 dB > 10 kHz per voice, -solo): our bilinear biquad has a zero at
  Nyquist, 86Box a Chamberlin SVF without one. Which one a real EMU8000 is needs a real-card recording (not changed).
- Tools: AWEREC=1 SBEMU build (make AWEREC=1; vawe.c logs every EMU8000 word access + TSC into 16 MB; SBDIAG FLUSH
  writes C:\AWEREC.BIN, or testrig/awe/recmem.py copies it out of the running 86Box guest - ROTT never returns to DOS,
  its credits hang); testrig/awe/rec.sh duke|rott|heretic EXE SECS (NATIVE=1: 86Box AWE64 + AWEREC86 logger, patch
  in priavte testrig/awenative/86box-emu8krec.diff); harness -s REC [-P probe] [-solo CH] [-t SECS] (ours and
  harness_ref); pvdiff.py, recstat.py. HERETIC.CFG = snd_musicdevice 9 (AWE32).
- Test downloads: DJGPP v3.4, 86Box 0935207 (+ xpad, + emu8krec), store-dos71, store-86box-roms, Linux sound/ + tiwai/awesfx (refs).

## beta.131-awe.2 (2026-11-20, AWE native-driver session): Duke 3D / Descent AWE32 music = all voices (emu8k.c)
- User report: Duke 3D and Descent set to AWE32 music (their own drivers, Creative's AWE32 SDK inside Apogee Sound System
  / HMI SOS) thin + wrong instruments; General MIDI on awemidi + SYNTHGM.SBK right.
- Method (testrig/awe/cap.sh NAME sbemu|native duke|descent|heretic SECS): 86Box 0935207 capture build (io.c + snd_emu8k.c
  patch, EMU8K_TRACE=file: native card word accesses; port 5E0h = records from an SBEMU VAWE_TRACE=1 build, every trapped
  byte). emustream.py dump|diff|awe (diff aligns writes, lists reads that differ / path splits; awe = harness script),
  steals.py (note set-ups on a voice whose note had no note-off). Patch for 86Box: io.c emu8k_trace() + 5E0h handler,
  snd_emu8k.c emu8k_inw/outw wrappers = testrig/awe/86box-emu8ktrace.diff (git apply after 86box-xpad.diff).
- Findings: (1) the SDK drivers never init the chip (no INIT1-4, no SMALR/ROM reads, no DRAM sizing - they rely on
  AWEUTIL /S); (2) they pick voices by reading DCYSUSV (bit 7 set = not for notes) + VTFT. awe.1 started every voice at
  DCYSUSV 0080h -> Duke found 2 voices (26, 29) and set up 78% of its notes on a sounding voice (86Box native: the
  same, its power-on 0000 is no better for Apogee); Descent's HMI: fine until a read of VTFT right after a note-on
  returned the stale initial-attenuation target (33D2h vs 86Box 005Ch). Write streams SBEMU vs 86Box native were
  otherwise identical (Duke 45 s: 10 PTRX.hi read-modify-writes differ), and replaying the same stream into emu8k.c vs
  86Box's snd_emu8k.c gives -38 dB residual (same notes, same timbre); DJGPP build of emu8k.c = host bit for bit.
- Fix (emu8k.c only): power-on = after AWEUTIL /S: DCYSUSV 807Fh voices 0-29 (end of Creative's init sequence),
  0080h 30/31 (DRAM refresh), reverb Hall 2 + chorus Chorus 3 programmed (aweutil_fx; without it the effects engine
  had no program: no reverb, chorus = dry copy); VTFT read = vtft_now() (envelope copies, no state change; io Data0
  half-writes merge with the stored VTFT). IFATN-zero ignore also for 807Fh idle voices.
- Results: Duke 2 -> 15 voices, steals 78% -> 0%; Descent 24 -> 30 voices, 0 steals (VTFT fix alone: 17 voices, 7%
  steals - the two belong together); Heretic (DMX, own init + allocation over 32 voices) stream unchanged. Duke replay
  awe.1 vs new: +8 dB, notes held (spectrogram: chords / pads instead of chopped notes).
- Tests: awe/synth/test/run_tests.sh PASS (ref86box/glue.c gives 86Box the same AWEUTIL start; new t14_sdk: idle
  807F/807F, refresh 0080, VTFT at note-on 1414h vol 0, after 100 ms loud); awemidi_tests.sh PASS, all 12 WAVs
  bit-identical to awe.1 (GM mode unchanged); notetest /AWE:620 /AWEMPU:GM 02.mid: 0 dropped (old and new).
  86Box P2B-LS PII-450 duke.sh (HIMEM, 60 s): awe.1 SBEMU 5% CPU, AWE32 4%, longest block 0.98 ms (2 runs); new 10% /
  AWE32 9% / longest 3.1 ms - the cost of 15 voices instead of 2 (per-voice cost unchanged, callgrind: render_voice
  ~27%, reverb ~16% of the engine). Win98: SC55W2/SC55WM rebuilt (apt mingw win32; munt d885a95, nopl3/sbemu-src ->
  win98/third_party symlinks), r98.sh awe.1 vs new: AWEDET all ok, AWETIME / IDLEMTR equal (INIT 56.6 / 56.5 ms, idle 1518k both), AWELOAD 16
  voices, Duke -40 dBFS both, load 6.6% both (the DOS box still gets awe.1 read-backs from the VxD).
- NOT in Win98 DOS boxes yet (sbcore.c is the Descent-freeze chat's): the VxD mirror answers the reads. Owner of
  sbcore.c: line ~1409 `aw_reg[1][5][ch] = 0x80;` -> `ch < 30 ? 0x807F : 0x80;` (that alone gives Duke its voices);
  VTFT reads: the mirror returns the written VTFT - needs the helper's live VTFT in the snapshot (awe_snapshot +
  a vtft[32] in SC55W_AWE, IF version bump) for HMI. SC55W.VXD / SC55MIDI.DRV / NUKEDJOY.VXD in the zip = awe.10's
  (no OW/JWasm here; /VER shows awe.10 for those three).
- Release: mkrel.py from the awe.1 zip (DOS EXEs + README.TXT + HWTEST + Win98 EXEs). Wiki: priavte wiki/src
  (Changelog rows awe.1 + awe.2, AWE page CPU line) + out/awe regenerated; post in the browser (pending).
- Test downloads: DJGPP v3.4, 86Box master@0935207 (+ xpad diff + capture patch), store-dos71, store-86box-roms,
  store-personal (ROMS check: AWEROM.BIN = AWE32.RAW = 86Box awe32.raw), store-w98qi-86box, munt d885a95, apt arj nasm.
- Next: sbcore.c owner applies the two read fixes (then Win98 DOS boxes = DOS); HWTEST awe.2 item on D810/P5PE-VM.

## beta.131-awe.1 (2026-11-18, AWE b131 sync session): MUXED beta.131 (Audigy analog + S/PDIF at once) on the AWE line
- Sync: `git diff --binary v1.0.0-beta.130-w98.10-usb v1.0.0-beta.131-usb -- . ':!release' ':!README.md' ':!HANDOFF.md' | git apply -3 --index`;
  conflicts only makefile VERSION (-> 1.0.0-beta.131-awe.1-usb-sc55) and README98.TXT (kept AWE's five-tab text). sc_sbliv.c
  and sbdiag.c (SPDIF "analog outputs" line) merged cleanly, AWE lines intact; docs/90s already equal. No new F_SYNTH /
  VMPU_ENABLED uses, no new HDPMI trap slots. Win98 part unchanged (FILES/WIN98 = awe.10, md5 checked).
- Tests: 86Box P2B-LS at PII-450 testrig/awe/duke.sh (Duke 1.3D AWE32 music, HIMEM), awe.10 vs new, 2 runs each:
  SBDIAG 5% CPU both, AWE32 4% render, longest 0.98 ms, IRQ avg 0.15 ms; music cmp.py RMS 0.0 dB, env corr 0.993. No Audigy
  in emulators (MUXED: bit table checked, jack bit unchanged in 18 cases).
- Release: rezip.py from the awe.10 zip (28 swapped: DOS EXEs, README.TXT, HWTEST.TXT). README: awe.1 news on top, MUXED
  beta.131 at the top of Older news. HWTEST: beta.131 item + an AWE32-on line. Wiki not posted (README-only task).
- Test downloads: DJGPP v3.4 (/tmp/dl/djgpp), 86Box 0935207 + xpad diff (automation build), store-dos71, store-86box-roms.
- Next: HWTEST beta.131 item on the P5PE-VM (Audigy 2: speakers + receiver, AWE32 on).

## beta.130-awe.10 (2026-11-17, AWE w98.10 sync session): MUXED beta.130-w98.10 (Win98 debug link free when on) on the AWE line
- Sync: `git diff --binary v1.0.0-beta.130-w98.9-usb v1.0.0-beta.130-w98.10-usb -- . ':!release' | git apply -3 --index`;
  conflicts dbg98.c, dbgtest.c, run.sh, VERSION/nsbver.h, binaries, docs; sbcore.c merged cleanly.
- Hand fixes: (1) port counts out of sc_dbg_capture: sc_in/sc_out (MUXED) + c_awe++ once in awe_in8, awe_out8 and sc_iow
  (after its "ours" check; split words are counted as bytes) - link on or off, no call. sc_dbg_capture = level-3 capture
  only, sc_dbg_capw = level-3 capture only (2 T records, no count), awe_port() count gone; sc_dbg_cap = (level == 3) on
  both dbg_open paths; IO_Stub io_w keeps its sc_dbg_cap test (asm comments updated). c_awe added to sbcore.c's forward
  declaration. (2) dbgtest.c: level 1 = sc_dbg_cap 0, level 3 = 1; capture/capw at level 1 add no counts; awe=5 from the
  trap counter -> S awe=5 + S awe line; word = 2 T records; interface 11: PASS (20 checks). routetest PASS.
  (3) w98dbg/run.sh: MPLAYER2, version-agnostic VxD line (w98|awe), "S lines count trapped ports"; mpenter DROPPED
  (MPLAYER2 has no first-start dialog; its stray Enter cancelled a later CHOICE /T timeout = 15 min stall); awe case: Duke
  only with DUKE=1 (aborts QEMU). r86dbg.sh: AWE=1 variant (/AWE:620, ROMS = only AWE32.RAW from store-86box-roms, Duke
  MusicDevice 6, checks EMU8000 traps / takes / awe= S lines / S awe voices / AWE activity right before the hang line).
  (4) all Win98 binaries rebuilt as awe.10 (NUKEDJOY.VXD said awe.5 before; now awe.10 like the rest - /VER).
  AWE numbering kept (PM API 5/6, WM_REMOTE 8/9, IDD_PAGE_DBG 107). MUXED's README98.TXT had one LF-only line (fixed here).
- Tests: QEMU acpi run.sh main PASS (after one manual key for the mpenter stall), hang PASS, fault PASS (253 S lines),
  awe PASS (traps, ready, banks, open/close, takes x2, awe= 117 S lines, 16 voices, 4096 T records of EMU8000 ports).
  86Box P3B-F PII-450 cpu86.sh OPTS=/AWE:620 PORTS="24E 24E 301 301 620 620" (55 ms ticks, 8M INs each):
  awe.9 off 156/157/184, on 177/175/211; awe.10 off 150/157/184, 150/157/185, 157/163/188, 157/163/185;
  on 159/163/187, 157/164/188, 157/160/185, 163/160/188. Without /AWE: off 155/161, on 155/158 (= MUXED). Run-to-run
  spread with the same build is +-4 % (off 24E 150..157); on is within it (mean +3 % on 24E, +1 % on 620) - the trap path
  has no link-on code left; if a future run must split this further, alternate off/on 4+ times (one rig at a time:
  two cpu86.sh runs share /tmp/w86d). r86dbg.sh AWE=1 (86Box): PASS - Duke AWE32 music in a DOS box with the link on,
  223 S lines with awe= (median 16k EMU8000 accesses/s, voices 1-2/32), takes line, hang test while Duke plays: "!" last,
  Windows frozen. (SC55W E "clock/PCM resyncs" lines while Duke runs - 86Box timing; not compared with awe.9.)
- Release: awe.9 zip + Win98 files + README98/README.TXT/HWTEST via testrig/awe98/mkrel.py --keepdos (DOS EXEs = awe.8).
  Wiki: priavte wiki/post/awe-awe.10.json (Changelog row, Home awe.10 line, Windows-98 /DBG row) - post AFTER awe.json,
  mux-w98.10.json, awe-awe.9.json.
- Test downloads: OW snapshot (Current-build), JWasm (Baron-von-Riedesel), munt d885a95, apt mingw-w64-i686-win32
  gcc-multilib qemu-system-x86 socat mtools xdotool openbox librtmidi6 zstd, pip pyserial pefile; Duke from testgames.
- Next: HWTEST top item (link left on, CPU % with Duke AWE on/off on the D810/P5PE-VM); post wiki jobs.

## beta.130-awe.9 (2026-11-17, AWE debug-link session): MUXED beta.130-w98.9 (Windows 98 serial debug link) on the AWE line
- Sync: `git diff --binary v1.0.0-beta.130-w98.8-usb v1.0.0-beta.130-w98.9-usb -- . ':!release' | git apply -3 --index`;
  conflicts in resource.h, sc55w.rc, w98main.cpp (8), sc55midi.c, sbcore.c, sc55w.asm (5), VERSION/nsbver.h, docs; binaries rebuilt.
- Hand fixes: (1) sc55w.asm IO_Stub: the byte path already captured every trapped port (EMU8000 bytes too); the AWE word
  path io_w now tests sc_dbg_cap after sc_iow and calls sc_dbg_capw (dbg98.c: one awe= count, level 3 = its two bytes
  port/port+1); sc_dbg_capture counts awe_port() bytes as awe= (620-623/A20-A23/E20-E23 + base). (2) Settings: Debug is
  tab 5 (g_page[5], [4] = Debug), IDD_PAGE_DBG 107 (105 = AWE, 106 = AWEBANK), IDC_D_* 1360-1365 (AWE 1400+). WM_REMOTE:
  AWE keeps 6 (SBOPTS reload); /DBG moved to 8 (port) / 9 (DBGCMD); SC55W /AWE.. /DBG.. run again sends both.
  (3) IOCTL 0x18-0x1A + DBGCMD_*: no clash (AWE IOCTLs end at 0x17), unchanged. (4) PM API: AWE uses AX=3/4 for the
  second device's bytes/volume, so open/close = AX=5/6, DX = device (0 NUKED SB-EMU, 1 AWE32 MIDI Synth), CX = clients
  of that device (sc55midi.c dev_opens[2]); VxD line "Windows MIDI device <name> opened (n open)". (5) Shared state:
  mpu_out keeps c=1 (DOS box MIDI on the AWE line) + c_midi++; sc55midi volume[dev]; awe_detach sets aw = 0 BEFORE
  unmapping (the async time-out reads aw). No F_SYNTH-like misuse found.
- AWE events, level 1, all from the once-a-second S time-out (nothing runs while off): attach "AWE32: EMU8000 ports
  ... trapped, sample RAM N KB" / E lines for ST_ERR_AWEPORT/AWEBLOCK; "AWE32: DOS box vm=X takes the EMU8000" /
  "closed, the EMU8000 is free" (aw->vm changes; set by word I/O only); "S awe vm= ports= voices=N/32 ramw= ram=K" while
  used + one line after (voices = DCYSUSV bits 7 and 15 clear = keys held); E on EMU8000 ring overflow. Helper: link-on
  summary (AWE32 ROM/ready/error, AWE32 MIDI Synth banks + note + user banks not loaded), "AWE32 MIDI Synth: a DOS box
  took the EMU8000" / "has the EMU8000 again" (awe_midi_counts), bank manager "load failed: ..." = E, "left out" = E.
- Tests: dbgtest.sh PASS (new: awe= bytes+words, S awe line, level-3 word = 2 T records; interface 11), routetest PASS.
  testrig/w98dbg/run.sh (QEMU acpi): main PASS (mpenter = Enter for Media Player's first-start dialog), hang PASS, fault
  PASS (254 S lines after UD2), awe (new case: /AWE:620 + AWE32 MIDI Synth, TRAPB 620, AWELOAD, WSEQ on AWE32, DUMP):
  traps/ready/banks/open-close/takes (VxD + SC55W)/awe= 133 S lines/16 voices/4096 T records of 62x-E2x all ok.
  Duke 1.3D in a QEMU Win98 DOS box ABORTS QEMU (hw/char/serial.c:474 assert size == 1 && addr < 8, a word access at
  COM1+7) - also with the link off and never used (/tmp/nolink.sh run): QEMU + Duke, not the link. Duke AWE therefore on
  86Box: testrig/awe98/r98.sh (new SERIALLOG= file COM1, CMDARGS=/DBG:COM1:1, DUKEONLY=1): Windows + Duke AWE32 music
  ran 4 min with the link on, takes/took lines, 101 S lines with awe>0 (~13.5k EMU8000 accesses/s), voices 1-2/32.
  CPU 86Box P3B-F PII-450 cpu86.sh (new OPTS=/AWE:620, PORTS=24E 24E 301 301 620 620; 8M trapped INs, 55 ms ticks):
  awe.8 155/159/183, awe.9 off 156/156/184 (noise), on COM1:1 171/175/208 (+10-13 % on a loop of only trapped INs,
  ~160 ns per access while ON; Duke's 13.5k/s = 0.2 %).
- Release: awe.8 zip + Win98 files, README98, README.TXT, HWTEST, DBGLOG.PY/TXT, SOURCE.ZIP (+dbg98.c, dbgtest) via
  testrig/awe98/mkrel.py --keepdos (DOS EXEs unchanged = awe.8). Wiki: priavte wiki/post/awe-awe.9.json (post AFTER awe.json).
- Toolchain this session: apt g++-mingw-w64-i686-win32 gcc-multilib librtmidi-dev openbox; OW snapshot (curl the
  release URL; gh release download is 403), JWasm (make -f GccUnix.mak), munt d885a95, ln -s sbemu-src/nopl3 to the repo.
- Next: HWTEST top item (null-modem, Duke AWE in a DOS box); post wiki jobs; MUXED already has beta.130-w98.10 (debug link
  cost-free even when on) - sync it next the same way (diff w98.9..w98.10, check io_w/sc_dbg_capw against its trap-path change).

## beta.130-awe.8 (2026-11-17, AWE sub-menu session): SETTINGS > AWE32 options (DOS SBSET + MENU; SBEMU unchanged)
- tools/sbset.c: new fields F_AWEON/F_AWEPORT/F_AWERAM/F_AWEROM/F_AWEFX + F_AWEMPU on page M_AWE ("AWE32 options");
  opened by Enter on the MIDI synth line while it is AWE32 (activate/open_menu F_SYNTH), SBSET /PAGE:AWE, MENU >
  Settings > "AWE32 options" (sc55menu.c), 3D tab 7 "AWE32" (S3_TABS). awe_set(on) converts F_SYNTH <-> F_AWEMPU (off =
  the MPU's synth, SC-55 if it was off/AWE itself); any other AWE field change turns the AWE32 on (awe_changed).
  Saved only when != SBEMU default: /AWE:port always while on, /AWERAM (presets 512..28672, 0; other 0-28672 values
  read from the file become a "N KB" choice), /AWEROM (g_awerom; "" = auto), /AWEMPU, /AWEFX:0 (g_awefx0 is gone).
  Unknown options (opt_name/known_opt by name like opts.cpp) go to g_extra and are written at the end of the line
  (a later same-name one replaces); SBSET line buffers 1200, MENU g_opts 1200. ROM shown by content (aw_is_rom =
  vawe.c VAWE_IsRom copy, ROMS then AWE under the pack folder); red line (awe_problem: no ROM / /AWERAM+1300 KB > DPMI
  free memory, skipped while SBEMU is loaded) on the page, in the 3D info line and as main-screen warning.
  Win98 opts.cpp needed no change (splits /NAME at ':' - exact names; AWE32 tab cb_select adds unknown RAM sizes).
- Release: awe.7 zip with SBSET.EXE + SC55MENU.EXE (makefile VERSION awe.8) + README.TXT + HWTEST.TXT swapped
  (rezip.py); SBEMU.EXE and Win98 files as awe.7 (SBEMU still says awe.5 in its version line).
- Tests: testrig/sbset/opts_host.sh (sbset.c HOSTTEST + real win98/helper/opts.cpp, ASan/UBSan): every field x value
  round trip (SC-55 / AWE contexts), AWE defaults off the line, ports, 9 RAM presets + custom 3000, all 9 "MPU-401 also
  plays", /AWEMPU:MT-32 alias, /AWEROM:C:\path and folder, AWE off, kept options (/VSCLPF /VMPU /DBG /FOO, no doubles,
  /A220 no-colon form), disk save/load, red line (no ROM, AWE32.RAW name without signature = no ROM, ROM by content
  under another name, RAM vs SBSET_FREEKB), Win98 reads 4x9x9x2 AWE lines with the same values and SBSET reads Win98's
  line back: PASS 26541 checks. testrig/sbset/keys.py (QEMU MS-DOS 7.1, SETUP C, keypad keys - QEMU sends gray arrows
  twice): AWE32 via Left, Enter -> page (auto: AWE32.RAW), 640 / 4 MB / fx off / GM, F3 style, S; /LINE = SBOPTS.TXT
  = /AWE:640 /AWERAM:4096 /AWEMPU:GM /AWEFX:0 + kept /VSCLPF:1 /FOO:BAR; /PAGE:AWE reload shows the same; SC55 start
  line "address 640, 4096 KB RAM, no reverb/chorus ... plays on the AWE32 (GM)"; MENU "AWE32 + own GM": PASS.
  menustat 5/5. 86Box P2B-LS PII-400 mpu.sh (zip SBEMU, the saved AWE options, MIDPLAY busy song): SBDIAG "Synth:
  AWE32 ... needs 19% of real time" (awe.6: Duke GM 17%), music audible. Win98 (testrig/sbset/w98read.sh, 86Box acpi,
  the saved line): SC55W.LOG "AWE32: EMU8000 640, RAM 4096 KB, ROM: C:\SC55\ROMS\AWE32.RAW", MIDI Synth GM, /FOO:BAR kept,
  SBOPTS.TXT untouched. Its AWE32-tab picture failed (Xvfb :9 from mpu.sh was running, so 86run.sh started no :86; fixed).
- QEMU SBDIAG has no AWE32 line (AC97 "output 0 Hz" there) - use 86Box for SBDIAG AWE.
- Next: 1) HWTEST top item. 2) Post wiki pages AWE32-and-AWE64 + MENU-and-Settings (priavte wiki/out/awe, ac364cd; the
  built-in browser was offline in this session) 3) DOS /AWEBANK when it lands could get a field.
- Test downloads: DJGPP gcc 12.2 (build-djgpp v3.4), 86Box 0935207 + xpad diff, store-dos71, store-personal,
  store-86box-roms, store-w98qi-86box.

## beta.130-awe.7 (2026-11-16, AWE Win98 banks session): AWE32 bank manager in Windows 98 + /AWEMPU:GM|GS|MT32 for DOS boxes
- awe/synth/awemidi.c bank API (host-neutral, plain C; DOS can use it): awem_ram_set (/AWERAM words; 0 KB = none),
  awem_ram_used/free/total, awem_ubank_load/put/remove (user banks 1-127), awem_preset_load/put/remove (one preset per
  bank select slot 0-127 / 128 = drum kit + program; only its RAM samples are copied), awem_user_clear, awem_bank_preset,
  awem_bank_loaded; AwemStats last_addr + user_notes. RAM layout: zones of RAM samples are bank-relative (Zone.ram),
  relocated at note-on by owner->ram_base; layout order = GM/GS/MT-32 banks, user banks, presets; a bank that does not
  fit is refused ("<name> needs N KB of sample RAM, M KB free of T KB (/AWERAM)"), after a RAM size cut it is left out
  (loaded = 0). Bank select: user bank = CC0, or CC32 when CC0 = 0; program p = file bank 0/p, else any melodic p, else
  bank 0; drum channel + CC0 N = bank N's kit. Only awem_start() uploads before the chip is started (S.started: was 3
  uploads of 6.5 MB at attach; one VM run crashed the batch DOS box there - not seen again after). DOS awe.6's
  awempu_host.sh PASS with this awemidi (its "bank RAM overlap guard" in vawe.c is no longer needed: banks get own RAM).
- Win98 helper (awe.cpp): SC55W.INI [AWE32 MIDI] BankN=file, PresetS_P=src_bank,src_prog,file (no sscanf; Win98
  GetPrivateProfileSection fails with a buffer > 32767 - cost one VM run); loaded at start (sync_users, files read
  outside the lock), the dialog writes the INI at once. Settings > AWE32 (page now 256 dlu, Settings 334 dlu high):
  bank list (Bank 0 = GM/GS/MT-32 as above, banks, single presets), Add / replace bank..., Replace one preset...,
  Remove, sample RAM gauge + text, refusal message on top of the status box. Dialog IDD_AWEBANK (file + Browse, whole
  file / one preset, bank, preset combo from the file, program, drum kit). "MPU-401 plays on" combo = /AWEMPU:1|GM|GS|
  MT32|0 (opts.cpp parses + writes the names, AWEMPU_GM..=2..4 as DOS); engine.cpp routes DOS-box MPU bytes (EV_MIDI
  c = 1) to awe_midi_dos_byte: that bank, a GM reset when a DOS box starts and when it stops (10 s idle; t1 showed
  Windows' bank-select state leaking into Duke without it). The synth runs if the device is on OR /AWEMPU:GM etc.
  SC55W.LOG "AWE32 MIDI Synth:" line: notes from user banks, sample RAM used, "DOS MPU-401: GM"; start line lists banks.
- Release: DOS = awe.6 zip as is (mkrel.py --keepdos), Win98 EXEs rebuilt (VERSION awe.7), VXD/DRV/JOY from awe.6.
- Tests: awe/synth/test/awemidi_tests.sh (section G + ASan/UBSan): SF2 + stand-in SBK as banks 1/2, CC0 1/2, CC32 2,
  bank 0, empty bank 5, drum kit, RAM full (5.6 MB SBK refused), replace, remove, single preset 178 KB (program 1 stays
  ROM), RAM cut to 512 KB and back, DOS box zeroing RAM -> banks back: PASS; chroma 0.86-0.96 as before.
  Win98 VM testrig/awe98/b98.sh (86Box P3B-F PII-450, 8 MB, /AWEMPU:GM; run it from a copy - never edit a running sh
  script): INI banks 1/2 + Preset0_0 loaded (6556 KB); BANKS.MID (banksmid.py) after SC55W restart: 96 notes, 80 from
  user banks (= expected); AWELOAD in the DOS box: DOS took it 1x, back 1x, user notes after; dialog by keys: BIG.SBK
  -> "Test bank needs 5646 KB of sample RAM, 1636 KB free of 8192 KB (/AWERAM)", STANDIN2.SBK -> bank 3, INI Bank3,
  back after restart (7304 KB); Duke 3D DUKEGM.CFG (MPU 300) -> "DOS MPU-401: GM", 3995 notes, 0 stolen, 0 missing,
  no user-bank notes leaked. CPU IDLEMTR idle 1532822, song on SF2 bank 1 1241556 (-19.0%; awe.5 ROM map -17.7%).
- Not tested here: the dialog's "one preset" path in the VM (unit-tested), real hardware (HWTEST top item).
- Next: 1) HWTEST results. 2) DOS /AWEBANK on this API (DOS chat's next step). 3) Post wiki (priavte wiki/out/awe
  AWE32-and-AWE64 regenerated, not posted: no browser in this chat).
- Test downloads: apt mingw-w64 win32, munt d885a95 (/home/claude/munt), 86Box 0935207 + xpad diff, store-86rig,
  store-w98qi-86box, store-personal, store-86box-roms, Duke 3D from testgames, pefile/mido (pip), Win98 DLLs from the VM.

## beta.130-awe.6 (2026-11-16, AWE DOS session): /AWEMPU:GM|GS|MT32 - the MPU-401 on the EMU8000 in DOS
- SBEMU /AWEMPU:GM|GS|MT32 (option value 2/3/4; parsed by name in main.c; 0/1 as before): MPU-401 bytes -> sbemu/vawe.c
  VAWE_MPU (UART front end: command ACKs, SBDIAG side channel) -> awe/synth/awemidi.c's player (awemidi.o now in
  AWE_OBJS). Banks: VAWE_MidiStart scans the AWE ROM's folder (else ROMS) flat by content (RIFF sfbk + awem_probe rc 1/2,
  kind from presets, else file name), loads mode bank + GM fallback (skipped when both use sample RAM: they would
  overlap), else awem_bank_builtin. No VMPU (SC-55/SF/MT-32) is loaded; AMIS SYNTH=AWE; MAIN_SynthName "AWE32 GM".
  It replaces Creative's AWEUTIL /EM (needs the card's NMI; README.TXT 6c says so).
- Ownership: the player writes nothing to the chip until the first note-on (first test with awem_start at load made
  Duke's own AWE music 3 dB louder and 10% vs 4% CPU). Any trapped EMU8000 access (VAWE_Count, checked in VAWE_MixInto)
  -> awem_release; awem_reclaim on a note-on after >= 3 s without access (frames counted in VAWE_MixInto). A game
  playing AWE music sends no MPU bytes, so its RAM samples are never overwritten while it runs.
- SBSET: "MPU-401 also plays" + "AWE32 itself, GM/GS/MT-32" (values awegm/awegs/awemt, line /AWEMPU:GM...), warning
  line; MENU status "AWE32 + own GM"; MIDPLAY's AWE-only message points at "AWE32 itself, GM".
- Tests: testrig/awe/awempu_host.sh AWE32.RAW (vawe.c on the host with stub qemm.h/vdiag.h + stand-in SBKs with
  neutral names, + ASan/UBSan): PASS (bank by content GM/GS/MT-32, MPU ACKs, note on/off, release on game access,
  held while < 3 s, reclaim, built-in map without SBK, /AWERAM 128 KB -> "left out", game-first -> player stays off).
  86Box P2B-LS (testrig/awe/duke.sh, HIMEM): Duke DUKEGM.CFG + /AWEMPU:GM: 212 MIDI B/s, AWE renders 17% (PII-400),
  music audible from s19 (vs /AWEMPU:0: silent, AWE 1%); Duke AWE32 music + /AWEMPU:GM vs awe.5: 0.0 dB, 6% vs 6% CPU.
  PII-450: Duke AWE music awe.5 5% / AWE 4% vs new 5% / 4% (0.0 dB); Duke GM on the AWE (built-in map, 30 voices)
  16% / AWE 15%. SC55LOG PII-450 old 0.47x = new 0.47x. QEMU notetest (/AWEMPU:GM) avmidi 02/07: 0 dropped.
- Win98 side NOT changed (owned by the Win98 banks chat): win98/helper/opts.cpp reads /AWEMPU with num() -> "GM" = 0
  = DOS boxes' MPU silent, and saving Win98 Settings would write /AWEMPU:0 over the user's /AWEMPU:GM in SBOPTS.TXT.
  Fix there: treat GM/GS/MT32 as 1 (keep the token as is).
- /AWEBANK NOT in this release: the Win98 banks chat (bank API in awemidi.c, user banks 1-127) had no tag out after ~10 min of polling. Next DOS session: rebase onto its tag, then add /AWEBANK:file[,file...] (VAWE_MidiStart loads them as banks 1-127 via its API, /AWERAM limit, a clear "sample RAM full" line) + test MIDPLAY bank select.
- Wiki: priavte wiki/out/awe regenerated (AWE32-and-AWE64, Switch-Reference) but NOT posted (no Chrome in this chat): post from the browser.
- Next: 1) Win98 banks chat tag out -> /AWEBANK (see above). 2) Win98 opts.cpp /AWEMPU:GM fix (that chat). 3) HWTEST results.
- Resume: "Attach deon81997/priavte (push) and follow PLAYBOOK.md. Repo: deon81997/NUKED-SBEMU-AWE. Task: /AWEBANK on top of the Win98 banks tag (HANDOFF top section)."
- Hardware (HWTEST top item): D810 + P5PE-VM with the user's SYNTHGM.SBK: Duke GM, MIDPLAY, Duke AWE32 music then a GM game.
- Test downloads: DJGPP gcc 12.2 (build-djgpp v3.4), 86Box 0935207 + xpad diff, store-dos71, store-86box-roms,
  store-personal, rufus, apt qemu-system-x86 fluidsynth timgm6mb-soundfont.

## beta.130-awe.5 (2026-11-16, AWE session): "AWE32 MIDI Synth" Windows MIDI device
- SC55MIDI.DRV now has 2 devices: 0 "NUKED SB-EMU" (as before), 1 "AWE32 MIDI Synth" (MOD_SYNTH, 32 voices, own volume).
  VxD PM API AX=3/4 (bytes / volume of dev 1, EV_MIDI c = 2; AX=0 -> 0101h; the driver refuses dev 1 on an older VxD),
  SC55W_SHARED IF 11: reserved9 -> awe_midi_volume, awe_reserved -> awe_midi_volume_set; SC55W_AWE reserved2[0] -> vm_gone
  (sc_vm_gone: aw->vm = 0, vm_gone++ when the DOS box that touched the EMU8000 is destroyed).
- awe/synth/awemidi.c (new, plain C, host-testable): SoundFont 1 (.SBK) + SF2 loader (awesfx's parser, SBK->SF2 units,
  ADIP register formulas), GM/GS/MT-32 MIDI player on emu8k.c via emu8k_write (Linux emux: voices, volume/pitch/pan,
  register sequence of a note, get_voice stealing; 30 voices), Creative's 8 reverb / 8 chorus parameter sets (Linux
  emu8000.c), GS SysEx (reset, master volume, reverb/chorus macro, drum part), RPN 0/1/2, release/reclaim for DOS boxes.
  GPL-2.0-or-later sources credited in the file header. awegm.h = NUKED's own GM map over the ROM: testrig/awe/awegm.py
  (sample bounds between 49-word zero gaps, HPS pitch, tail loop search) + awegm.tune from testrig/awe/awegmcal.sh
  (register check of note 60 per program against TimGM6mb via fluidsynth). Approximate: timbres picked by ROM order and
  features; chroma vs fluidsynth on songs only 0.2-0.6 (stand-in SBK 0.86-0.96) - it is the no-SBK fallback.
- Helper: awe.cpp "AWE32 MIDI Synth" part: SBK search by content under the pack (SF1 RIFF + ifil 1.x in the first 4 KB,
  awem_probe kind: GS = >= 8 variation-bank presets, MT-32 = MT-32 names on programs 7/8/16/24, else file name, else GM;
  copied to ROMS as SYNTHGM/GS/MT.SBK unless one is there), banks in lookup order (mode bank, GM, else built-in map), the
  EMU8000 block now exists when /AWE or the MIDI device is on (VxD gets awe_port/addr only with /AWE). Ownership in
  awe_midi_poll(): any DOS AWE read/write (counters) or ring write -> awem_release(); back when that DOS box closes
  (vm_gone) or 10 s without DOS access. Gain: Windows owner = master x device volume; DOS owner = SB16 mixer path as before.
  SC55W.INI [AWE32 MIDI] Synth/Bank/Reverb/Chorus (not SBOPTS.TXT: DOS SBEMU matches options by prefix, /AWExxx would hit
  /AWE). Settings > AWE32 page re-laid out (DOS AWE top, "AWE32 MIDI Synth" group: on, bank, reverb, chorus, Search for
  SBK files, status with the "where to get them" hint). SC55W.LOG: start line (SBK search) + "AWE32 MIDI Synth: ..." /10 s.
- DOS: no change (zip keeps the awe.4 DOS EXEs; makefile VERSION says awe.5 for the next DOS build). Release built with
  testrig/awe98/mkrel.py --keepdos (old zip's DOS files, Win98 files + README98 + HWTEST swapped, SOURCE.ZIP + awemidi).
- Tests: awe/synth/test/awemidi_tests.sh AWE32.RAW (stand-in SBKs from TimGM6mb by testrig/awe/sf2sbk.py: RAM samples,
  neutral INAM, gm/gs/mt32 kinds; awemidtest unit + ASan/UBSan PASS: all 128 programs + drum keys 27-87 of the built-in
  map sound, 40 held notes -> 30 voices / 10 stolen / silent after notes off, release -> silent at once + note-ons held
  back + reclaim plays, GS master volume / GM on, bend range 12 = x2.00, probe kinds GM/GS/MT-32 from content, 128 KB RAM
  -> zones left out, GS bank 8 variation + bank 5 fallback; avmidi 02/07/15/17 stand-in vs fluidsynth chroma 0.86-0.96).
  routetest PASS. Win98 VM (testrig/awe98/m98.sh, 86Box P3B-F PII-450 acpi, 8 MB RAM): Media Player via the Mapper
  (MIDIMap CurrentInstrument = device name) 911/911 notes; WSEQ.EXE (win98/test/wseq.c, midiStream sequencer) song at
  volume 100 then 25: REC.WAV -12.1 dB (expected -12.0); dense 30 s (11520 note-ons, 380/s): 9039 stolen (native 9045),
  0 without a sound; AWELOAD in the batch DOS box -> DOS took it, back after 10 s; AWELOAD in a second DOS box that closes
  -> back at once; stand-in banks hidden as C:\SC55\EXTRA\BANKS\BANK1.DAT / EXTRA\OTHER.BIN -> found, copied, GS mode.
  86Box REC.WAV is not in real time (time compressed): compare it with testrig/awe/chromadtw.py (DTW chroma: VM vs native
  render 0.88, vs fluidsynth 0.86, another song 0.40), not chroma.py. Media Player's first start asks "settings changed by
  another program": m98.sh answers it from a screenshot (after 100 s; the boot screen looks the same before).
- CPU (86Box PII-450 at ~98% speed, host idle): IDLEMTR idle 1620574, song via the device 1333127 (-17.7%), dense
  1323092 (-18.4%); SC55W load 22-24% while playing, ~1-4% idle. 30 sounding voices (emu8k cost scales with sounding
  voices). Host work during a VM run (python analyses) made 86Box slow: load 268%, late 3586 - rerun idle was clean.
- Hardware to test (HWTEST top item): user's own SYNTHGM.SBK, Control Panel MIDI device, Media Player, GS/MT-32, reverb/
  chorus types, Volume Control, Duke in a DOS box taking the chip, CPU with a sequencer.
- Test downloads: /tmp/dl/ref (Linux emux/emu8000 sources, awesfx git), apt timgm6mb-soundfont + fluidsynth, store-86rig,
  store-w98qi-86box, store-personal, store-86box-roms.

## beta.130-awe.4 (2026-11-16, AWE session): EMU8000 reverb + chorus
- awe/synth/emu8k.c fx_*: 86Box snd_emu8k.c's effect structure and register meanings (GPL-2.0-or-later, credited in the
  file header): reverb = input damper, 6 damped reflection combs (2/4/8/13/19/18-33 x 242 samples, INIT2 ch 14h room),
  2 tails (delay + 2 diffusers), panned link type (INIT1 ch 7 = 8474h); chorus = 2 interpolated taps around INIT3 ch 0Ch,
  sine LFO (HWCF5 milli-Hz, depth INIT4 ch 3), right delay HWCF4/256, feedback INIT3 ch 9. Params ignored while INIT1[0]
  = 03FFh (init passes), as 86Box. Integer: Q12 coefficients with int32 products (sends clamped to +-2^18; >>3 headroom
  cost 10 dB of tail accuracy - not used), block-wise delay lines, LFO linear per 64-sample block, taps 1/256 sample.
  Voices with the same (reverb, chorus) sends share a stereo sub-mix (8 groups) and the mono send is taken once
  ((L+R)*send/255), else per-voice sends (voice_run FX path). Reverb / chorus run only while their sends were seen in the
  last 4 s (tails). emu8k_set_fx(0) = dry (fx state: calloc ~370 KB, separate from E).
- Switches: DOS /AWEFX:0|1 (main.c OPT_AWEFX, live when SBEMU runs again; SBSET keeps /AWEFX:0 on its line), Win98
  Options awefx + Settings > AWE32 "Reverb and chorus" (awe_fx() from Engine::render; AWE options no longer restart the
  synth: live_blank). The DOS-box AWE needed no VxD change (INIT/HWCF writes already went through the ring).
- Tests: awe/synth/test: new t12_rev0..7 / t13_cho0..7 scripts (Creative's values, from Linux emu8000.c) vs 86Box's
  engine (harness_ref): reverb 40.5-43 dB SNR, chorus 37-54 dB, tail -30 dB times equal (fxcmp.py); run_tests.sh PASS.
  Duke 3D (music only, /tmp/dukemus.cfg = SoundToggle 0) vs 86Box's own AWE64 (testrig/awe/duke86.sh, wavetable.wav):
  rests -12.0 vs -12.0 dB re median (dry -15.5), silent frames 0.2 vs 0.3% (dry 2.3%), -20 dB fall 390 vs 410 ms (dry
  280), bands within ~1 dB to ~6 kHz (fxtail.py, cmp.py). Win98 DOS box (r98 d4): rests -11.3, silent 0.2%, fall 365 ms.
  Speed PII-450 (duke.sh MHZ=450, SBDIAG): AWE render 2% -> 4% (longest block 0.44 -> 0.99 ms), /AWEFX:0 2%; SBEMU 3% ->
  5% of the CPU. SC55LOG 0.47x, 22591 frames/s before and after. callgrind (16 voices, both effects): +17.5M instr/s
  (first version +32.8M). Win98 helper load 3.0-3.5% -> 5.5% while Duke plays (86Box). routetest PASS.
- testrig: awe/duke86.sh (86Box AWE64 reference), duke.sh MHZ=450, aweload.asm -DFX (AWEFX.COM: Hall 2 + Chorus 3, sends
  FF), awe/fxtail.py, awe/synth/test/fxcmp.py. midi/perf.sh with EXTRA=ROMS lost the ROMS dir on the FreeDOS disk (SBDIAG
  "Nothing measured" with /AWEROM:C:\SC55\AWE32.RAW) - used duke.sh MHZ=450 instead. A dead Xvfb :86 makes 86run.sh skip
  starting it when Xvfb :9 runs (pgrep -x): no Win98 boot, no SC55W.LOG - start Xvfb :86 + openbox by hand.
- Hardware to test (HWTEST top item): Duke AWE music with reverb (DOS + Win98 DOS box), /AWEFX:0 vs 1, SBDIAG AWE %.

## beta.130-awe.3 (2026-11-16, AWE session): AWE32 in Windows 98 DOS boxes
- Win98 part now DIVERGES from MUXED (interface IF 10, AWE line only): future MUXED w98.N syncs must 3-way merge win98/
  (sc55wif.h SC55W_SHARED grew 16 bytes: awe_port/addr/size; SC55W_AWE block; EV_MIDI c=1 = from a DOS box) and rebuild.
- SC55W.VXD (vxd/sbcore.c "AWE32" section, sc55w.asm word-I/O path io_w -> sc_iow): traps base+0..3/+400h/+800h (620/640/
  660/680), word I/O in one call. Reads answered from a mirror: raw register file (every write), RAM streams SMALR/SMARR/
  SMALW/SMARW/SMLD/SMRD done in the VxD on the shared ROM+RAM (never through the ring), WC = VxD clock x 44.1 kHz, the
  dynamic CPF/CVCF/CCCA from the helper's snapshot (taken after each render) unless written since (ring position).
  HWCF1-3 / ID formulas and the DCYSUSV -> HWCF3=4 rule copied from emu8k.c. Writes -> AWE ring (16384 x 8 bytes).
- SC55W.EXE: helper/awe.cpp (block = header + 1 MB ROM + RAM, VirtualAlloc, locked by the VxD; emu8k_init_mem() = new
  in emu8k.c, DOS unchanged; ROM by content: /AWEROM, ROMS, ..\AWE), engine.cpp merges the AWE ring with the event ring by
  time, renders emu8k into the mix at SB16 mixer MIDI x master x /VMIDIVOL (as DOS), awe idle after 2 s silence; a
  watchdog thread drains the ring early if the render thread falls behind (synth start-up). Options /AWE /AWERAM
  /AWEMPU /AWEROM (opts.cpp, SBOPTS.TXT shared with DOS, /AWE forces /T:6, BLASTER E620), command line SC55W /AWE:620
  (saved, a running SC55W reloads: WM_REMOTE 6), Settings tab "AWE32" (IDD_PAGE_AWE). /AWEMPU:0 drops DOS-box MIDI (c=1).
  SC55W.LOG "AWE32: n writes, n reads, RAM n words, ring lost n, early n" every 10 s; INI RecordSecs=N (test rigs).
- Mirror vs stall measured (testrig/awe98/r98.sh, 86Box P3B-F PII-450 Win98 acpi VM, AWETIME.COM in a DOS box):
  detection x100 1.8 vs 13.5 ms; 10000 reads 12.6 vs 406 ms (1.3 vs 41 us/read); AWEUTIL-style init 57 vs 382 ms;
  512 KB upload 0.32 vs 1.30 s; DRAM sizing 0.33 vs 4.6 ms; WC over 110 ms 4851 (right) vs 448 (wrong: engine clock);
  Duke: stall blocked the DOS box ~22% of the time (6.5k reads/s). Mirror kept, stall code removed (commit 8ecc5ef has
  an early stall version; the one measured = VMM semaphores: the DOS box waits in the I/O trap, the helper thread
  waits in a DIOC, answers with a second DIOC - not committed).
- Tests: AWEDET in the DOS box = DOS SBEMU (ID/HWCF1/HWCF2/BYTE/RAM ok, 1234 5678 0000 0032 2E38). Duke 3D AWE32 music
  (SoundToggle 0) in the DOS box vs DOS SBEMU awe.2 duke.sh: chroma corr 0.973 (same notes), bands within ~1 dB to ~5 kHz
  (REC.WAV is 22050 Hz). CPU (IDLEMTR loops/tick in a DOS box): awe.2 1623728, awe.3 AWE off 1623663, AWE on idle
  1622516 (-0.07%), 16 voices (AWELOAD) 1537792 (-5.3%). Release binaries + SC55W /AWE:620 command line: same numbers,
  SBOPTS.TXT got /AWE:620. routetest PASS. DOS SBEMU rebuilt (awe.3 name, emu8k.c change): duke.sh vs awe.2 -0.0 dB,
  same bands, 0.0% ticks lost, AWE 2%. 86Box Duke runs show resyncs ~5/s in SC55W.LOG (full-screen DOS box time slices,
  known since w98 Blood work; not AWE).
- Rig notes: r98.sh TAG WIN98DIR OPTS [SECS] (env SCROMS=folder with only AWE32.RAW: no SC-55 boot, which takes ~5 min
  in 86Box and lost AWE writes before the watchdog; DUKEONLY, SFX=0, RECSECS, CMDARGS). Duke in a DOS box needs
  [Comm Setup] RTSName etc. (empty name -> "Error opening : Permission denied"). Win98 CreateThread needs &tid (NULL
  fails: cost 3 runs). Release: testrig/awe98/mkrel.py OLD NEW (rezip.py + win98/out + README98 + SOURCE.ZIP).
- Test downloads: /tmp/dl/{djgpp,roms,86b (86Box 0935207 automation),duke,munt,jw,ow.tar.xz}; store-w98qi-86box,
  store-86rig, store-personal, store-86box-roms, store-dos71.
- Hardware to test (HWTEST.TXT top item): D810 + P5PE-VM, Win98: Settings > AWE32, Duke AWE32 music in a DOS box,
  SC55W.LOG AWE32 line, CPU %, GM game on 330 still on the SC-55.

## beta.130-awe.2 (2026-11-16, AWE session): Windows 98 part = MUXED beta.130-w98.8
- main = beta.130-awe.1 + MUXED v1.0.0-beta.130-usb..v1.0.0-beta.130-w98.8-usb (650e443) as a 3-way patch (git diff --binary),
  minus MUXED's README/HANDOFF/btn-awe32.gif. All files applied cleanly: the AWE line's win98/ was identical to MUXED beta.130,
  and there is no AWE code in win98/. No DOS source change; no new F_SYNTH/VMPU_ENABLED uses. makefile VERSION -> awe.2.
- Release zip: rezip.py from 130-awe.1, then FILES/WIN98/* and README98.TXT copied from MUXED's w98.8 zip (34 entries:
  MUXED's prebuilt SC55W*.EXE, VXDs, SC55MIDI.DRV, SOURCE.ZIP). Rebuild Win98 binaries here only if win98/ ever diverges.
- README: awe.2 entry on top, 121-awe.1 moved to Older news (before 119-awe.2), MUXED w98.8 entry at the top of Older news;
  rows testrig/midi + "keysoak, dukekey" taken from MUXED; MT-32 / CM-32L 86box/roms note after the ROM table.
- Test: duke.sh HIMEM 60 s (awe.2 SBEMU): 327 int/s, 0.10 ms, AWE 2%, 0.0% ticks lost; cmp.py vs awe.1 same bands.
  Win98 part not run here (MUXED's w98par/routetest results apply; same source and binaries).

## beta.130-awe.1 (2026-11-16, AWE session): MUXED beta.128-130 merged into the AWE line
- main = beta.127-awe.1 + MUXED v1.0.0-beta.127-usb..v1.0.0-beta.130-usb (2985070) as a 3-way patch (branch awe-beta130).
  Brings usbmidi/igap /DBG stats (128), clean Ctrl+Alt+F11 switch without loud blast + MIDPLAY ignores hotkeys (129),
  port-60h key trap /KEYTRAP + /DBG K lines (130). No new F_SYNTH / VMPU_ENABLED uses in the diff (grepped all of it).
- Conflicts: makefile VERSION, testrig/midi/mkswitch.py (kept AWE's SWS + MUXED's SWQ/SWL), switch.sh (MUXED cases + SBX
  hook on loud/midplay/hotkey), README (AWE layout kept: awe.1 entry on top, 119-awe.2 moved to Older news by date, MUXED
  128-130 at the top of Older news, 2 feature bullets), HANDOFF (ours). main.c, vkbc.c, vdiag.c, usbpad.c merged cleanly.
- AWE-only fix: the /KEYTRAP PM trap is a new HDPMI slot (AWE + pad: 15 of 16, was 14). It is installed after the AWE32
  traps; if no slot is left it no longer counts as a failed trap in the AWE "HDPMI port traps" line (no red warning),
  the line says the hotkeys read port 60h the old way (main.c keyNoSlot). With /USBMS the KBC 60h trap is reused (no slot).
- Tests: duke.sh HIMEM (LOG=1) old/new: 326/324 int/s 0.10 ms, AWE 2%, lost 1.7/1.8% (LOG=1 cost), "15 of 16";
  cmp.py main.wav 25-75 s B-A -0.1 dB, same bands. dukekey HOTKEY=1 SBARGS=/AWE:620: PASS 0 stuck, 1975 K lines.
  perf.sh PII-450 old/new: /AWE idle 0.04 ms 1% both; SC-55 + AWELOAD 1.96 ms 67%, SC-55 302/301%, AWE 7% both.
  SC55LOG 0.47x, 22591 frames/s both. Not rerun: midi gamecap/switch, sata, menustat, keysoak (no AWE-specific change).
- Release: NUKED-SB-EMU-1.0.0-beta.130-awe.1-usb.zip (rezip.py from 127-awe.1, no new files), tag v1.0.0-beta.130-awe.1-usb.
- Wiki: priavte wiki/src Switch-Reference /KEYTRAP row; posted to this repo's wiki (clone verified = out/awe). out/mux has the
  same row, NOT posted to the NUKED-SBEMU wiki (MUXED line's page; its next docs chat posts it).
- Rig setup: testrig/rufus = `git clone --depth 1 https://github.com/pbatard/rufus` (perf.sh/sc55log.sh need it; not in
  this repo, in .git/info/exclude). Test downloads: /tmp/dl/djgpp, /tmp/dl/roms (store-86box-roms), /tmp/dl/timgm6mb
  (apt timgm6mb-soundfont), 86Box 0935207 built in /home/claude/86box/86box.
- Hardware to retest (P5PE-VM / D810 with AWE32): Duke 3D AWE music + arrows (no stuck key), Ctrl+Alt+F11 with /AWE
  (AWE plays on, MPU moves, no blast), SC55 /AWE line shows "HDPMI port traps: N of 16 used" in grey.

## Docs + store (2026-11-15, AWE docs session): README synced to MUXED (NUKED-SBEMU) beta.130 layout, wiki, AWE ROM
- README.md rebuilt from NUKED-SBEMU's README (tool tables with icons, Test lab, drivers, thanks) + AWE parts: header art,
  5 AWE news entries on top (127-awe.1 .. 119-awe.2), MUXED news <= beta.127 under Older news (128-130 not in this line),
  AWE section with the ROM source, AWE rows in features / tools (testrig/awe) / drivers (awe/synth, vawe.c) / thanks /
  "Stuff you bring yourself". Downloads + issues link to this repo; links use NUKED-SBEMU (renamed from -MUXED).
- AWE ROM: 86Box roms repo sound/creative/awe32.raw (1048576 bytes, AWE-DUMP layout "M1MG" at byte 6, sha1 6ac3c131)
  is accepted as-is by VAWE_IsRom (1 MB or 1 MB+2, content check) - no rename. SBEMU prints it as ROMS\awe32.raw.
- Wiki (sources in priavte wiki/, posted from the browser): AWE ROM on AWE32 page / FAQ / Quick Start / Troubleshooting,
  EMU8000 credits, download button, "private line" -> public repo link. NUKED-SBEMU: README + wiki Home link this edition.
- priavte store-personal: FILES/ROMS/AWE32.RAW added. 86Box duke.sh (P2B-LS PII-400, HIMEM) with only that file in ROMS:
  "ROM: ROMS\awe32.raw: enabled", AWE music -41..-57 dBFS, 327 int/s 0.09 ms, 0.0% ticks lost, AWE 2%. No code, no release.
- Test downloads: /tmp/dl/awe/awe32.raw (raw.githubusercontent 86Box/roms), 86Box src @0935207 (github) built in /home/claude/86box.

## beta.127-awe.1 (2026-11-15, AWE session): MUXED beta.127 (serial debug link /DBG) merged into the AWE line
- main = beta.126-awe.1 + MUXED v1.0.0-beta.126-usb..v1.0.0-beta.127-usb (c0630cf) as a 3-way patch, branch awe-beta127.
  Conflicts only in makefile VERSION, midplay.c includes (sbemuchk.h + dbg.h), README news, HANDOFF. main.c, hdpmipt.c,
  qemm.c merged cleanly; DBGL_Trap sits in the shared trap dispatchers, so AWE32 port writes (MAIN_AWE_IODT) go into the
  level 3 capture too. No new port traps (the link uses the UART directly).
- Tests: testrig/dbg (QEMU, SBX="/AWE:620", new SBX hook) pm + rm + both hang runs PASS (one earlier pm run had "tool
  direct" 0 lines once, not repeated). duke.sh HIMEM 325 int/s 0.10 ms 0.0% lost, AWE 2%; cmp.py vs awe.2 within 0.1 dB;
  /AWE /JOY:1 /MUSB:1 /SPDIF:1 + pad: 14 of 16 traps. perf.sh /AWE idle 0.04 ms; SC-55 + AWELOAD 1.96 ms 67%, SC-55
  302%, AWE 7% (= beta.126-awe.1). mpu.sh /MOUT:USB silent -> /MOUT:SYNTH MT-32 audible. sata 5/5, menustat 5/5,
  gamecap miles PASS, switch.sh mout PASS, keysoak JEMM 2 min 0 keys (all with /AWE:620 where the rig takes options).
- Rig: earlier uncommitted beta.126-awe.1 rig edits (duke.sh LOG=1, perf.sh EXTRA) committed now; dbg/run.sh is bash.
- Release: NUKED-SB-EMU-1.0.0-beta.127-awe.1-usb.zip (file list = MUXED beta.127), tag v1.0.0-beta.127-awe.1-usb.
- Hardware to retest: /DBG:COM1 with AWE32 on the P5PE-VM (null-modem to a second PC, DBGLOG.PY): Duke AWE music
  while level 2 runs (no stutter), Ctrl+Alt+F10 level 3 capture shows 620h/A20h/E20h writes.

## beta.126-awe.1 (2026-11-14, AWE session, repo deon81997/spare-01): MUXED beta.122-126 merged into the AWE line
- main = awe.2 + NUKED-SBEMU-MUXED v1.0.0-beta.121-usb..v1.0.0-beta.126-usb (6f5ec98) as a 3-way patch (same recipe as
  awe.1; branch awe-beta126). Brings USB MIDI byte-exact + MPU front end (122), live MIDI output switch /MOUT + MENU
  "MIDI to" + Ctrl+Alt+F11 (123), /VWC (124), random-keys fix + SBDIAG KEYS/SOAK (125), Audigy S/PDIF (126).
- Conflicts: makefile VERSION (-> 1.0.0-beta.126-awe.1-usb-sc55); sbdiag.c synth line (g_synth now "USB MIDI device
  [+ AWE32]"); sbset.c warning() (USB live line also when AWE + "MPU-401 also plays" = USB MIDI) and a real bug in
  MUXED's new build_line /MUSB:0 (used F_SYNTH: AWE + USB MIDI wrote /MUSB:1 /MUSB:0) -> mpu_synth(); sc55menu.c
  status (MPU ? rule for /AWEMPU:0 kept + USB marker; AWE line "AWE32 + USB MIDI" follows the live output by AMIS 05h).
  main.c, sc_sbliv.c, vkbc/usbpad, vesawc, README.TXT merged cleanly. No new port traps in 122-126.
- /MOUT with the AWE32 (decision): the switch moves only the MPU-401 at 330h (synth <-> USB 1/2); the EMU8000 keeps its
  own ports (620h..) and plays on. AMIS 04h SYNTH= now = the live output (USB while USB has the music). /AWEMPU:0: no
  emulated synth; with SBEMU's USB on the MPU-401's MIDI goes to a USB MIDI device (MUXED rule "no synth -> USB"),
  start line says so. README.TXT section 6c documents it.
- Tests (all on the release zip / output EXEs, AWE on where it matters):
  testrig/awe: duke.sh HIMEM 327 int/s 0.10 ms 3%, JEMM 323 int/s 0.12 ms 4%, 0.0% ticks lost, AWE 2% (= awe.2 326 int/s
  0.10 ms 3%); cmp.py awe.2 vs new 25-75 s: 0.0 dB, bands within 0.1 dB, env corr 0.94. /AWE + /JOY:1 + /MUSB:1 +
  /SPDIF:1 + pad (LOG=1): "found: pad 1", "HDPMI port traps: 14 of 16 used" (= awe.2). mpu.sh (JEMM) AWELOAD + MT-32 +
  /MOUT:USB then /MOUT:SYNTH: AWE32 -25 dBFS throughout, SBDIAG "MT-32 + AWE32"; without AWELOAD: MIDPLAY silent on USB,
  MT-32 audible after /MOUT:SYNTH. testrig/midi (uhci, SBX=/AWE:620): gamecap miles gssysex mi2 intel rm duke PASS
  (duke: 16 events in this rig, same without AWE); switch.sh rerun mout hotkey PASS. keysoak: HIMEM 5 min + JEMM 2 min
  with /AWE:620 = 0 keys PASS. testrig/sata 5/5 PASS (warning on 7, 8). vesawc (86Box Voodoo3) pm rm emm as MUXED
  (WC set/undone, EMM386 refused). menustat 5/5 PASS (SYNTH=USB: no ROMs in the zip). MENU matrix quick set (fd j32,
  SBX=/AWE:620): uhci+ehcic before, xhci+ohci after: 35/35 OK each.
- Speed (midi/perf.sh, 86Box P2B-LS PII-450; awe.2 release EXE vs new): /JOY:1 /VMPU:1 no ROMs 344 int/s 0.02 ms 1%
  both; /AWE idle 0.04 ms 1% both; SC-55 + AWELOAD playing (JEMM+QPIEMU) 1.96 ms 67%, SC-55 302/303%, AWE 7% both.
  SC55LOG 0.47x real time, 22591 frames/s both (6 version bytes differ).
- Rig fixes (MUXED has the same gaps, not pushed there): sata/SATA.BAT failed since beta.123 (no ROMs -> USB MIDI has
  the music, MPUTEST exit 3; DIR keeps ERRORLEVEL) - accepts 3; midi/switch.sh hotkey needed SWS.TXT nothing made
  (mkswitch.py now does); midi/mkrmbin.py (RM.BIN for gamecap rm); keysoak SBOPTS, switch.sh SBX, perf.sh EXTRA,
  duke.sh LOG=1 (SBEMULOG costs ~1.8% Duke ticks: off by default).
- Release: NUKED-SB-EMU-1.0.0-beta.126-awe.1-usb.zip (rezip.py from awe.2; file list = MUXED beta.126), tag
  v1.0.0-beta.126-awe.1-usb.
- Hardware to retest (P5PE-VM + Audigy): AWE32 + SC-55 with S/PDIF (/SPDIF:1) - Duke AWE music on the digital out;
  AWE + USB MIDI (Uno -> SC-55): SETTINGS "MPU-401 also plays" USB MIDI, MENU "AWE32 + USB MIDI", Ctrl+Alt+F11 in Duke
  (GM on 330) moves only the GM music; KEYSOAK with /AWE.
- Next: 1) the next MUXED beta: diff from v1.0.0-beta.126-usb; 2) awe.2 open items (4-pole filter, reverb/chorus).
- Test downloads: DJGPP v3.4 gcc12.2 (/tmp/dl/djgpp); 86Box 0935207 + xpad diff (/tmp/dl/86box/src, symlinked as
  /home/claude/86box/86box; roms repo /tmp/dl/86box/roms); rufus (testrig/rufus); apt: zstd qemu mtools socat nasm
  timgm6mb-soundfont arj exfatprogs exfat-fuse ntfs-3g genisoimage fdisk libzstd-dev; store-dos71, store-personal.

## beta.121-awe.2 (2026-11-14, AWE session, repo deon81997/spare-01): AWE32 + MPU-401 synth together (/AWEMPU)
- User report (P5PE-VM + Audigy, awe.1): with AWE picked, MIDPLAY silent and MENU "MPU ?" - /AWE zeroed OPT_VMPU_VOICES
  (AWE and SC-55 exclusive). Now /AWEMPU:1 (default) keeps the MPU-401's synth (SC-55, SoundFont, MT-32, USB MIDI)
  beside the EMU8000, like an AWE32 with a Sound Canvas on its MIDI port; /AWEMPU:0 = the awe.1 AWE-only setup.
  Branch awe-mpu, merged to main.
- main.c: OPT_AWEMPU; the exclusivity block zeroes VMPU only with /AWEMPU:0; resident re-run ignores /AWEMPU changes like
  the other /AWE options. Mix order unchanged and fine: SB/OPL, VMPU_GenSamples (SC-55 renders with IF on), then
  VAWE_MixInto (CLIS); the memset guard already checks vmpu_active. Volumes: both = SB16 mixer MIDI x master
  (VMPU_SetMixVolume / VAWE gain) x /VMIDIVOL, as on a card where wavetable + MIDI share the MIDI fader.
  SBDIAG A6-A8h: with VMPU on, VMPU_MPU answers them (as before awe.x); MAIN_MPU_330/331 only with VMPU off.
- vdiag: DIAG_RenderAwe(cycles, frames, freq) - own counters (layout 6 appends AWEN/AWEMS/AWEMAX/AWEFR), so the MPU
  synth's render time/frames are not overwritten; sbdiag.c prints "Synth: SC-55 + AWE32" and one "needs x%" line per
  synth, ISR "synth" share = both. Old SBDIAG + new SBEMU: 16 report bytes left unread (harmless, reset by A6).
- AMIS function 04h line: + "SYNTH=SC55|SF|MT32|USB|HW|NONE AWE=620" (what plays the MPU's MIDI). MIDPLAY: BLASTER E6x0
  and SYNTH=NONE (or the MPU does not answer with E6x0) -> "AWE32 mode without MPU-401: turn on SC-55 for 330h in
  SETTINGS" instead of playing into a silent MPU (the null MPU acks commands, so mpu_detect alone says "found").
- SETTINGS (sbset.c): new main row F_AWEMPU "MPU-401 also plays" (SC-55 / SoundFont / MT-32 / CM-32L / USB MIDI / off),
  enabled only with MIDI synth = AWE32; mpu_synth()/set_mpu_synth() route SC/SF sub-menus and SF checks to it. Window
  WY 2->1, WH 22->23 (MIDI group 5 rows; footer rows unchanged). Writes /AWE:620 [+ /AWEMPU:0] + the normal synth
  options; parse is order-independent (g_awempu0). New `SBSET dir /LINE` prints the line it would save (tests).
- MENU draw_status: "AWE32 + SC-55|SF|MT-32|CM-32L|USB MIDI", "AWE32 (no MPU)" with /AWEMPU:0, no "MPU ?" for that
  deliberate setup; sf/mt copied out of opt_val's 4-slot ring (more calls in one function overwrote them).
- HDPMI trap slots (hdpmipt.c counts them; SBEMU prints "HDPMI port traps: N of 16 used" with /AWE): VMPU adds none.
  P2B-LS rig, /AWE + MPU synth + /JOY:1 + pad: HIMEM only 14/16 (USB mouse goes through the BIOS without RM traps);
  JEMMEX+QPIEMU + /USBMS:2 (PS/2 traps 60h/64h) 16/16 - fits, zero headroom. Next new trap needs a merge (candidate:
  60h-64h as one trap with 61h = PC speaker inside, 62/63h passed through).
- Tests (86Box P2B-LS PII-400 unless noted; testrig/awe/mpu.sh, duke.sh with OPTS/CFG/ROMDIR, aweload.asm):
  /AWEMPU:0 MIDPLAY -> the new message; MENU (screenshots) "AWE32 + SC-55" / "AWE32 (no MPU)" / "AWE32 + MT-32";
  AWELOAD (16 looping ROM voices, real mode -> JEMM) + MIDPLAY on MT-32: both audible (-26 dBFS), SBDIAG "MT-32 +
  AWE32"; non-AWE MT-32 MIDPLAY unchanged (-50 dBFS, "Synth: MT-32"); Duke 3D AWE32 music awe.2 vs awe.1: level
  0.0 dB, bands within 0.1 dB, env corr 0.89 (game timing), same 3% CPU / AWE 2%; Duke with MusicDevice 4 (GM, 330)
  on MT-32 + SB FX with /AWE on: plays (MT-32 57%). SETTINGS /LINE round trip (QEMU, 8 lines incl. AWE + MT-32/CM-32L/
  SF/USB/off, either option order): identical after a second pass. testrig/menustat on the zip: 5/5 PASS (non-AWE
  MENU unchanged; AMIS line now ends "SYNTH=SC55 AWE=0").
- SC-55 audio cannot be checked in emulators: 86Box tops out at PII-450 / Celeron 533 (SC-55 needs 302-352%: SBDIAG
  dropouts, silence), QEMU AC97 (-audiodev wav, timer-period=2000) gave 39 IRQ/s and no SC-55 render even without AWE.
  MPU-path audio was tested with Munt (MT-32); the SC-55 path is the same VMPU code (SBDIAG shows it rendering).
- Speed (testrig/midi/perf.sh, 86Box P2B-LS PII-450, MIDPLAY busy song 40 s; awe.1 release EXE vs awe.2):
  /JOY:1 /VMPU:1 (no ROMs): 344 int/s, 0.02 ms, 1% both. AWE idle: awe.1 /AWE 0.04 ms 1%; awe.2 /AWE /VMPU:1 0.04 ms
  1%. SC-55 playing (ROMs, /VMPU:1): 1.74 ms avg, 3.21 max, 60% CPU, SC-55 302% of real time - identical. awe.2 only:
  /AWE + SC-55 1.77 ms 61% (AWE idle 1%); both playing (AWELOAD 16 voices + SC-55) 1.96 ms 67% (AWE 7%). SC55LOG
  (testrig/awe/sc55log.sh): 0.47x real time, 22591 frames/s, old = new (binaries differ only in the version string).
  New perf.sh hook POSTCMD (after SBDIAG START). MPUTEST as POSTCMD hung the run (no report): use MIDPLAY.
- Real hardware (P5PE-VM + Audigy): see the 4-step list in the chat / below.
  1. SETTINGS: MIDI synth AWE32, "MPU-401 also plays" SC-55, save, reboot, SC55: MENU top right "AWE32 + SC-55".
  2. MIDPLAY any .MID: plays on the SC-55. 3. Duke 3D (AWE32 music): unchanged. 4. A GM game on 330 (e.g. Duke with
  General MIDI, or Doom GM): SC-55 music + SB effects. Optional: /AWEMPU:0 -> MENU "AWE32 (no MPU)", MIDPLAY message.
- Build note: a VERSION bump does not rebuild main.o (no makefile dependency): touch main.c, or SBEMU's banner keeps
  the old version (seen in this session's first test runs).
- Test downloads: DJGPP v3.4 gcc12.2 (/home/claude/djgpp); 86Box 0935207 + xpad diff (/home/claude/86box); store-dos71,
  store-personal (ROMs incl. AWEROM.BIN, now in the store); zstd via apt.

## beta.121-awe.1 (2026-11-14, AWE session, repo deon81997/spare-01): MUXED beta.120 + beta.121 merged into the AWE line
- main = awe.2 + NUKED-SBEMU-MUXED df2766f..v1.0.0-beta.121-usb (2f7b3b3), applied as a 3-way patch (`git diff --binary
  df2766f TAG -- . ':!release' | git apply -3`; no shared history with MUXED). Branch awe-beta121, merged to main.
  Conflicts: makefile VERSION only (-> 1.0.0-beta.121-awe.1-usb-sc55), README.md news (both kept, 5 shown), HANDOFF.md
  (both kept). main.c (disk IRQ scan), README.TXT (SATA + WCSET), sc55menu.c (WCSET items), rezip.py merged cleanly.
  beta.120/121 add no port traps (sc_sbliv.c ISR bits, start-up disk scan, WCSET in tools/), so the 16-slot HDPMI budget
  is as in awe.2; rechecked: /AWE + /JOY:1 + USB pad on the P2B-LS loads ("found: pad 1") and plays Duke.
- Tests: testrig/sata 5/5 PASS (warning on the shared-IRQ cases 7, 8); testrig/wcset on 86Box Voodoo3 pm + rm PASS (WC
  MTRR set/cleared); testrig/menustat 5/5 PASS. AWE: new testrig/awe/duke.sh (86Box P2B-LS PII-400, MS-DOS 7.1, UHCI
  + emulated 360 pad, Duke 3D AWE music, SBDIAG): HIMEM only and JEMMEX+QPIEMU both play; awe.2 vs awe.1 SBEMU output
  (cmp.py 25-75 s): level 0.0 dB, bands within 0.1 dB, envelope corr 0.93 (game timing). SBDIAG in Duke: awe.2 324
  int/s avg 0.10 ms 3% CPU, awe.1 the same (JEMM 0.12 ms 4%), 0% ticks lost, AWE 2% of real time.
- Speed (testrig/midi/perf.sh, PII-450): /JOY:1 /VMPU:1 awe.2 = awe.1 = 344 int/s avg 0.02 ms 1%; /AWE idle 0.04 ms 1%
  both. SC55LOG/SC55WAV: code identical to awe.2 (cmp: only the 9 version-string bytes differ).
- Not testable here: beta.120's EMU10K ISR change (no Audigy in emulators; same as MUXED). Hardware to check: as awe.2.
- Release: NUKED-SB-EMU-1.0.0-beta.121-awe.1-usb.zip (rezip.py from awe.2, file list = MUXED beta.121), tag
  v1.0.0-beta.121-awe.1-usb by the release workflow. Rename to NUKED-SBEMU-AWE still pending (needs the browser).
- README (after the release): AWE32 edition decorations from docs/90s/awe90s.py (awe-edition.gif banner under the title,
  vox-part-awe.gif in "What's inside", btn-awe32.gif, vox-awe.gif in the new "AWE32 / AWE64 Wavetable!!" section).
- Next: 1) user hardware: AWE music on D810 / P5PE-VM (HIMEM only, JEMM), SATA Enhanced on the P5PE-VM; 2) the next
  MUXED beta: same recipe (fetch the tag into remote "muxed", diff from v1.0.0-beta.121-usb); 3) awe.2's open items
  (4-pole filter, reverb/chorus).
- Test downloads: DJGPP v3.4 gcc12.2 (/tmp/dl/djgpp); 86Box 0935207 + 86box-xpad.diff (/tmp/dl/86box, SDL, automation);
  rufus (FreeDOS kernel, /tmp/dl/rufus -> testrig/rufus).
- Resume: `git remote add muxed https://github.com/deon81997/nuked-sbemu-muxed`; build: export
  PATH=/tmp/dl/djgpp/djgpp/bin:$PATH; cd sc55-pack; make -j16.

## beta.119-awe.2 (2026-11-14, AWE session, repo deon81997/spare-01): AWE32/AWE64 EMU8000 in SBEMU, SETTINGS picks AWE or SC-55
- Repo: spare-01 is the AWE line (meant to be NUKED-SBEMU-AWE; that repo was not reachable from Claude's GitHub app, so the
  user said use spare-01). main = NUKED-SBEMU-MUXED beta.119 (df2766f) imported + branch synth-emu8000 merged (the EMU8000
  engine, sc55-pack/awe/synth, by the synth chat). "awe.1" (port trapping from the repo-setup chat) never landed anywhere:
  the trap layer was written here. Version names: `beta.NN-awe.M` = AWE update M on DOS beta.NN (like -w98.M); zip
  NUKED-SB-EMU-1.0.0-beta.119-awe.2-usb.zip, tag v1.0.0-beta.119-awe.2-usb. To move the AWE work into MUXED later: the
  diff is sbemu/vawe.[ch], awe/synth, main.c, qemm.[ch], hdpmipt.c, makefile, tools/sbset.c sc55menu.c sbdiag.c, README.TXT.
- sbemu/vawe.c: traps base+0..3, +400h..403h, +800h..803h (12 ports, 3 HDPMI traps + 1 QEMM table) -> emu8k_io_write/
  read. HDPMI (crazii fork, I2FHDPMI.ASM portloop) calls trap handlers once per BYTE, port then port+1, low byte first;
  vawe latches the even byte and commits the word on the odd byte (Pointer also on its even byte); a read of the even byte
  reads the whole word, the odd byte returns the latched high half. QEMM/QPIEMU passes AL only plus CL = size bits
  (Jemm QPIEMU: bit 2 OUT, bits 3-4 byte/word/dword): qemm.c now splits 16/32-bit IN/OUT into byte calls for IODT entries
  with QEMM_IODT_SPLIT (0x40000000; bits 16-23 of .port already hold QEMM's "was trapped" state - the uninstall test now
  masks 0x00FF0000). hdpmipt.c matches (port & 0xFFFF). Other ports: unchanged behaviour.
- main.c: /AWE:620|640|660|680 (hex, 0=off), /AWERAM:KB (512), /AWEROM:file|folder (default ROMS, then AWE; a 1 MB file
  with "1MGM" at word 2 or 3). /AWE forces /T6, turns VMPU off (SC-55/SF/MT-32: exclusive), BLASTER gets " E620".
  Mix: after SB+OPL, before CD-Audio: emu8k_render at aui.freq_card, gain = SB16 mixer MIDI x master (SBEMU's linear
  mapping, like OPL/VMPU) x /VMIDIVOL, rendered with interrupts off (CLIS/STIL) so a trapped write never lands inside a
  block; WC advances with the rendered frames. Resident re-run: /AWE* changes are ignored ("next time SBEMU loads").
- HDPMI has 16 trap slots (?IOTP_COUNT). /AWE (3) + /JOY:1 with USB (joystick 1 + keyboard 2) ran out: SBEMU did not load
  on the P2B-LS rig. With /AWE the DMA page traps 81-83h, 87h, 89-8Bh, 8Fh become one 81h-8Fh trap (MAIN_DMAPAGE_Install;
  84-86h, 88h, 8C-8Eh passed through untrapped). Without /AWE: the old four traps.
- SBDIAG with /AWE: the plain MPU handlers (VMPU off) now answer SBDIAG's A6-A8h side channel too; AWE render time is
  reported as synth 3 = "AWE32" (sbdiag.c prints it).
- SETTINGS: MIDI synth gets "AWE32 / AWE64 wavetable (EMU8000)" (6th choice, writes /AWE:620; reads /AWE:x back); the old
  `cur >= 2` MT-32 checks became `== 2 || == 3`. MENU status line: "AWE32 at 620". README.TXT: section 6c + option list.
- awe/synth compiled with its own flags (AWE_CFLAGS: -O2 -march=pentium2, no -flto/-ffast-math); only runs with /AWE.
- Tests (testrig/awe/README.md, 86Box 0935207 P2B-LS PII-400; A = its AWE64 (PnP, 1 MB), S = ES1371 + SBEMU, ROM = 86Box
  roms awe32.raw):
  - Duke 3D 1.3D (MusicDevice 6): same music: envelope correlation 0.93 (log, 10 ms), band spectra within 0.3 dB up to
    6.4 kHz, 1.2 dB less at 6.4-12.8 kHz. SBEMU's music/effects balance = 86Box's (~12 dB). Absolute level: /VOL:80 + /K:22050
    default -13 dB vs 86Box, /VOL:100 /K:44100 +5 dB (SBEMU's card volume, not the AWE path).
  - ROTT 1.3 (SNDSETUP: AWE32 music, SB16 FX): envelope correlation 0.955, bands within 0.6 dB to 6.4 kHz.
  - Above 12.8 kHz SBEMU has much more energy (-17 dB vs -44 dB of total, ~20 kHz, also at /K:48000 so it is not 86Box's
    resampling): the engine's 2-pole filter (86Box: 4-pole Moog) lets the ROM samples' aliasing through. Open: decide with
    a real AWE32 recording whether to switch to 4-pole, and reverb/chorus are stubs in the engine (86Box has both).
  - Modes: HIMEMX only + HDPMI32I (user's usual) /JOY:1 with USB: Duke plays; JEMMEX+QPIEMU: Duke plays. Real-mode word
    path: testrig/awe/awedet.asm under QEMU JEMMEX+QPIEMU = 86Box AWE64 word for word (ID, HWCF1/2, SMALW/SMLD write,
    SMALR/SMLD + SMARR/SMRD read-back incl. the buffered first read); byte read of ID passes here, fails on 86Box (fine).
    Without /AWE the ports stay untrapped (probe reads FFFF) and SBEMU defaults are unchanged (SB Pro).
  - Speed (testrig/midi/perf.sh, PII-450, /JOY:1 /VMPU:1): beta.119 344 int/s, 0.02 ms, 1% = this build without /AWE.
    /AWE idle (no game): 0.04 ms, 1%. Duke 3D AWE music (PII-400, HIMEMX, SBDIAG): 329 int/s, avg 0.09 ms, longest
    0.72 ms, 3% CPU (synth 62% of it), AWE needs 2% of real time, game got 867 of 878 ticks. SC-55 code unchanged, so
    SC55LOG not re-measured (only its /VER string differs).
  - SBEMU needs XMS: without HIMEM it stops with "Failed allocating XMS" (beta.119 too).
- Real hardware to check (D810, P5PE-VM): a game with AWE32 music (Duke 3D / ROTT) with AWE32.RAW in ROMS, both with
  HIMEM only and with JEMM; SETTINGS: pick AWE32, save, reboot, MENU shows "AWE32 at 620"; SBDIAG while it plays.
- Test downloads: DJGPP v3.4 gcc12.2; 86Box 0935207 + roms; HX (Baron) + crazii/HX (trap API reading); Jemm (QPIEMU
  reading); JWasm + HimemX (built HIMEMX.EXE for the rig); rufus res/freedos.
## beta.127 (2026-11-15, DOS session): serial debug link (SBEMU /DBG, DBGLOG.PY)
- User: /DBG:COM1|COM2[,baud][,level] (default off), level 1 events + errors + stats/s, 2 + per-frame port/DMA/IRQ,
  3 RAM capture of every trapped port sent on trigger/keypress; polled UART + ring, ISR/trap safe, flush on fatal;
  MENU/MIDPLAY/GAMETEST/SBDIAG on the same port (shared DBG.H); host DBGLOG.PY (pyserial) + cable README; stage 2
  (two-way, UDP/packet driver) design note only; no slowdown.
- Done: tools/dbg.h (parser - ':' = ',' because batch %1 splits at commas -, 16550 init FIFO/no IRQ, 8 KB ring,
  dbg_pump one FIFO load, dbg_sync time-limited; tool side: argv /DBG: or SBDBG, through SBEMU AMIS 0Ah when its
  link is on). sbemu/dbglink.c (lines "<s.ms> SB <kind> text"; E sync for IRQ/DMA events, repeats counted; stats from
  TSC; level 2 F lines; level 3 4096-record ring, Ctrl+Alt+F10 / SBDIAG DBG DUMP; DBGL_Fatal; hang test). Hooks:
  trace.c TRACE_Event (+ IRQ/ack counts, SdaLin), vdiag.c (MIDI, underruns, Ctrl+Alt+F10), hdpmipt.c + qemm.c traps,
  main.c (/DBG string option replaces upstream numeric /DBG, re-run, INT 28h pump, AMIS 0Ah, DBGL_Isr before
  DIAG_IsrEnd). SBDIAG DBG [DUMP|HANG|LEVEL n|LOG text]. pack/FILES/SC55/DBGLOG.PY + DBGLOG.TXT (cables, P5PE-VM,
  D810 D/Port), README.TXT 7 + 15, HWTEST beta.127, docs/DBGLINK.md (stage 2/2b design).
- Bugs found on the way: (1) put_line's `kind ? "%s %c %s" : "%s %s"` passed a char to %s -> SBEMU fault in AMIS
  (QEMU). (2) DPMI_LoadW at option time = LOAD_DS before DPMI_Init -> GPF ds=0 (86Box only): use _farpeekw(_dos_ds).
  (3) DPMI_LoadB/W of the PSP/MCB from inside the sound ISR froze 86Box PII-450+ES1371 (QEMU fine): dbglink reads them
  with _farpeek. trace.c prog() does the same from traps/ISR (TRACE_Irq) - untouched, but suspect if SBTRACE hangs.
  (4) HDPMI virtualises ring-3 CLI/HLT: the hang test spins instead of HLT.
- Tests: testrig/dbg/run.sh ZIP EXEs (QEMU dos71, COM1 -> TCP -> DBGLOG.PY, QAUDIO timer-period=2000; common/run.sh
  got env QAUDIO): pm (SC55.BAT) + rm (SC55JEM.BAT): link line, start events, stats, tool lines via SBEMU and direct,
  SBDIAG LOG, level 2 F lines, level 3 capture 4096 T lines + end, hang: "!" is the last line and nothing follows for
  40 s -> PASS. Speed (testrig/dbg/perf.sh, 86Box P3B-F PII-450 ES1371, TimGM6mb 40 s): base / off / level 1 all 61%
  CPU, ISR avg 1.77 ms; level 2 1.78 ms (+0.6% ISR time). SBT.COM (real-mode SB DMA) only in rm.
- Test downloads: DJGPP gcc 12.2 (andrewwutw build-djgpp v3.4), apt timgm6mb-soundfont, nasm, librtmidi6, pip pyserial.
- Next 3 steps: 1) P5PE-VM + D810 HWTEST beta.127 (dbg-*.log, capture, hang line); 2) stage 2 two-way commands
  (docs/DBGLINK.md); 3) MENU item "Serial debug link" in SETTINGS (port/level saved to SBOPTS.TXT).

## beta.126 (2026-11-15, DOS session): Audigy S/PDIF out (/SPDIF, jack switch, SBDIAG SPDIF/TONE)
- User: S/PDIF on every Audigy (1 / 2 / 2 ZS / 2 Value / 4), /SPDIF:0|1|ONLY (default 1 = analog + S/PDIF), shown in SBDIAG; S/PDIF
  shares the rear 3.5 mm Digital Out / Center-Sub jack (tip = coax via TRS-to-RCA): /SPDIF:1 must switch that jack to digital.
- Done: sc_sbliv.c SBLIVE_SpdifMode/Inv, snd_emu_spdif_jack (A_IOCFG GPOUT0 0x44 + HCFG GPOUT0 as ALSA; invert = CA0151 except SB0380;
  CA0108 Value/A4: P16V reg 7Ah), DSP routing per mode, S/PDIF 48 kHz + SPCS0-2 rate bits, live switch SBLIVE_SpdifSet. main.c /SPDIF
  (bad values refused), /SPDIFINV, AMIS 08h, re-run switch. SBDIAG SPDIF [0|1|ONLY], SBDIAG TONE [s]. README.TXT (section 7 + 15 "AUDIGY
  DIGITAL OUT": jack TIP = WHITE/left RCA -> COAXIAL in), HWTEST beta.126 P5PE-VM item, README news (5 shown; beta.121/120 -> Older).
- Merged onto beta.125 (keys): one sbemuchk.h sbemu_text(); AMIS 08h S/PDIF + 09h keys both in main.c; SBDIAG help/dispatch both.
- Tests: testrig/spdif/run.sh ZIP (QEMU, CARD=none path: start, re-run /SPDIF:ONLY, SBDIAG SPDIF 0, bad value refused) PASS;
  testrig/menustat 5/5 PASS. No Audigy in QEMU/86Box. Speed: init/option/AMIS code + EMU DSP microcode only; non-Audigy runtime
  path unchanged (not re-measured). Zip = beta.125 zip + rezip.py.
- Risk: CA0151 (Audigy 2/ZS): switching the jack clears A_IOCFG 0x44 incl. 0x40 (init's "unmute analog"), as ALSA does; HWTEST asks
  whether front speakers stay on with /SPDIF:1. If not: keep 0x40, toggle only 0x04 on CA0151.
- Next 3 steps: 1) P5PE-VM HWTEST beta.126 results (SPDIF1.TXT, plug, /SPDIFINV); 2) MENU Sound item for S/PDIF mode + SETTINGS save;
  3) 86Box speed check if an EMU10K2 model ever appears.

## beta.125 (2026-11-14, DOS session): no random key presses in games (pads, USB keyboard, mouse -> KBC)
- User: audit every place the pack injects keys / mouse bytes; generous dead zone + hysteresis, release-all on USB error, never
  inject while a real keyboard byte is pending, log every injected key's source (SBTRACE), 30-min idle soak = zero keys.
  (Audigy S/PDIF from the same prompt was split off: wip/spdif-audigy-2026-11-14, own HANDOFF section.)
- Causes found: padmap.c stick -> key at 13000 (40%) with no hysteresis and no "seen centred" check (drift / off-centre sliders /
  noise typed and repeated keys); triggers > 128 single threshold; no debounce; usbpad.c pipe_poll on a TD error kept the
  keyboard's keys held (VKBC repeat forever if no next report); vkbc kpush dropped single bytes when full (lost break = stuck
  key, lost E0 = wrong key); 50 ms "unstuck" re-read could take a reply a program asked for; mouse D3h byte seen as keyboard
  data (AUXB clear) would be a keystroke; our bytes could land between a real key's E0 and its code.
- Fix: padmap.c threshold 16000 at /JOYDZ 10 (+328/pct), off below 3/4, armed after |v| < thr/2, two polls past threshold;
  vjoy.c key_filter (buttons: 2 equal polls; triggers on > 160 off < 96) for key decisions only (game port raw); usbpad.c IN
  error -> KbFn(k, none) + prev cleared, pad buttons/axes 0, mouse buttons up, TRACE_Event, USBPAD_InErrors; vkbc.c kpushn
  (whole key or nothing, 16 places kept for breaks), source-tagged ring (KS_*: USB keyboard #id, USB repeat, pad layout, pad
  repeat, /JOYKEY, unread byte again) + totals, misrouted D3h byte read back (3 x -> no mouse via KBC), RealHold 30 ms after
  a real E0/E1 read through the trap, unstuck only 200 ms after a program's last KBC write, VKBC_ReleaseAll on SBEMU re-run.
  trace.c "Injected keys" part (TRACE_SetKeyLog / TRACE_Dirty). main.c AMIS 09h (ECX bit0 clear, bit1 byte log; EDX size).
  sbemuchk.h sbemu_text(); SBDIAG KEYS [CLEAR], SBDIAG SOAK [min] (KEYSOAK.TXT, also counts BIOS keys; works with old SBEMU).
  README.TXT, HWTEST.TXT (+ rezip.py now swaps HWTEST.TXT: zips since beta.121 had the beta.120 checklist), README news.
- Tests (testrig/keysoak/run.sh MIN [EXE_DIR], JEMM=1): QEMU dos71 HIMEM + HDPMI32I, OHCI usb-kbd + usb-mouse + fakepad idle.txt
  (noise, drift 43%, trigger chatter at 128, Start blip, 1-report spike) + PADUSER.TXT sticks/triggers on keys:
  beta.123 3 min = 394 keys at the BIOS (FAIL); this build 3 min = 0 (PASS), typed control key logged "USB keyboard #0 1E/9E";
  JEMM=1 2 min PASS; 30 min PASS (0 keys, 0 BIOS keys). menustat 5/5 PASS on the zip.
  Rig rule: never two run.sh rigs at once (shared /tmp/testrig-run-latest: the control key went to the other VM).
- Not done: 86Box PII-450 speed run skipped: per sound IRQ only key_filter (2 pads, ~40 instructions) + padmap compares; logging
  only when a key is typed; 86Box has no USB pass-through. Real hardware.
- Hardware ask: HWTEST beta.124 item (SBDIAG SOAK 30, a pad game 10 min + SBDIAG KEYS, unplug a keyboard with a key held).
- Next 3 steps: 1) user's KEYSOAK.TXT / KEYS1.TXT from P5PE-VM + D810; 2) if a source still types: its KS_ tag in KEYS1.TXT says
  which path; 3) S/PDIF branch rebase + release (separate chat).
## beta.124 (2026-11-14, DOS session): SBEMU /VWC:1 - write-combining for the VESA frame buffer while SBEMU is loaded
- Built on branch feature/vesa-wc (merged; a parallel session works on EMU10K hardware offload - none of its files
  touched). New files only: sc55-pack/vesawc.c/.h; main.c hooks: /VWC option (table + enum) and one VESAWC_Init call
  before the resident check (no cleanup hook needed: atexit undoes WC when SBEMU quits without going resident; a
  resident SBEMU never runs this code). SBDIAG: `SBDIAG WC` (exit 0 on, 1 off, 2 unavailable) + a report line.
- Reuses tools/wcset.h (DPMI 0301h real-mode stub: real mode under HDPMI raw, V86 under JEMM) + the 3dfx OVL's
  "MTRRs:" dump (deon81997/3dfx-dos-emulator- vcore vesa.c ran at ring 0 under DOS/4GW - SBEMU is ring 3, CPL 3
  confirmed, so its direct RDMSR path cannot be used; its MMX present does not apply: games draw, not SBEMU).
- LFB: VBE 4F03 current mode, else first LFB mode (4F01 PhysBasePtr) + VBE total memory; PCI class 03 BAR lookup is
  read-only then (BAR sized only in the PCI fallback when VBE gives nothing). Range < 1 MB refused (A0000-BFFFF and
  fixed MTRRs never touched). Re-run: SBEMU /VWC:1 sets, /VWC:0 clears SBEMU's MTRR (same range as WCSET ON).
- Ring 0 per memory manager (testrig/vesawc/run.sh ZIP pm rm emm): no EMM (SC55) real mode -> WC set; JEMMEX
  (SC55JEM) V86 -> WC set; EMM386 NOEMS -> refused before any MSR access (only SMSW runs), message, SBEMU loads;
  QEMM: not available here, same branch as EMM386 (V86 and not JEMM; QEMM named via INT 2Fh D200h / QEMM386$);
  Windows DOS box refused (wc_probe). 86Box P3B-F PII-450 + Voodoo3 + ES1371 (testrig/vesawc/86box): pm/rm/emm PASS
  (MTRR 1 = WC DE000000 16 MB, cleared by /VWC:0, set again by re-run, failed start /PADLOG -> restored); QEMU
  pm/rm/emm PASS (SeaBIOS UC MTRR 80000000-FFFFFFFF -> "unavailable", correct). menustat 5/5 on the zip.
- Speed (testrig/vesawc/perf.sh ZIP, 86Box PII-450, SoundFont TimGM6mb, MIDPLAY 40 s busy song): beta.123 / new off /
  new /VWC:1 = 344 int/s, avg 1.77 ms, longest 4.85/4.83/4.64 ms, 61% CPU, render 236/236/235%. SC55LOG.EXE md5
  47e60dc3.. identical. 86Box does not model WC speed: the gain can only be seen on real PCs.
- Zip = beta.123 zip + rezip.py (all EXEs beta.124, README.TXT, HWTEST.TXT; rezip.py now swaps HWTEST.TXT too).
- Hardware ask (HWTEST beta.124 item): D810 + P5PE-VM, SBDIAG WC line, VESA games timed with and without /VWC:1,
  VGA/Mode X games look unchanged, once with SC55JEM.
- Open: if the D810 BIOS covers the LFB with a UC MTRR, WC is "unavailable" -> PAT idea (as in WCSET's open item).
- Next 3 steps: 1) D810/P5PE-VM results; 2) if "unavailable" on real boards: PAT (PIII+) through the same stub;
  3) SETTINGS item for /VWC (now command line / SBOPTS only).
- Test downloads: DJGPP v3.4 (/tmp/dl/djgpp), TimGM6mb (apt-get download, /tmp/dl/timgm6mb).

## beta.123 (2026-11-14, DOS session): switch MIDI output (emulated synth <-> USB MIDI devices) without a restart
- User: "start SC55 with emulation, pick USB MIDI in MENU/SETTINGS, run SC55 again - shouldn't it just change?" (it did
  not: a re-run only copied options; /MUSB also skipped loading the synth). Asked: synth <-> USB and between USB
  devices; MENU item, command line and in-game hotkey.
- Done: main.c MAIN_MPU_Route (trap -> capture, pending hotkey switch, then VMPU_MPU or the /MUSB front end), MIDI
  state tracker TK_* (last GM/GS/XG/MT-32 reset + SysEx <= 2 KB, per channel CCs incl. bank, program, RPN 0 range,
  pitch bend, running status / half message), MAIN_SwitchOut (silence old: CC64/123/120 x16, replay to new), AMIS 07h
  (ECX 0 synth, 1.. slot, 0x80 USB, 0xFE next), /MOUT:SYNTH|USB|USB1|USB2|NEXT (start or re-run), re-run with
  /MUSB:0|1 switches (SBSET now writes /MUSB:0 too), synth always loaded with /MUSB (idle: no render while USB), USB MIDI
  flag always on with /JOY:1, MAIN_SynthOK (ROMs/SF found) else USB only. usbpad.c: MAX_PIPE 8 (M_HC 0x5000), 2 USB
  MIDI slots (kept per vid:pid across replug), packets tagged with the slot in the cable nibble (stripped on send),
  midi_skipdead, USBPAD_MidiSlot/Target. vdiag.c: Ctrl+Alt+F11 (BIOS shift flags too), DIAG_Toast 2 s note top left
  (text/13h) + 1-3 beeps (PIT ch2). AMIS 05h: OUT= SYN= |U1..|SYNTH=. MENU Sound "MIDI to: ..." items (SBEMU /MOUT:..),
  /STATUS "MENU MIDI output:", SBDIAG USB lists slots; MPUTEST/GAMETEST/SBDIAG use out>0. README.TXT, HWTEST, news.
- Tests: testrig/midi/README.md beta.123 (switch.sh rerun/mout/hotkey PASS, regression PASS, menustat 5/5).
- Not done: real hardware; 86Box speed (SC-55 path unchanged while synth has the music; with USB the synth is idle).
- Hardware ask: HWTEST beta.123 item (MENU MIDI to, Ctrl+Alt+F11 in Duke, SETTINGS + SC55 again) + beta.121/122 item.
- Next 3 steps: 1) user's Uno + SC-55 results; 2) MENU: keep the choice for next start from the MENU items (now only
  SETTINGS saves); 3) MIDI IN routing per slot (only the target's IN is meaningful today).

## beta.122 (2026-11-14, DOS session): USB MIDI (M-Audio Uno -> real SC-55) wrong notes, MENU "?" for USB MIDI, tests blind to /MUSB
- Causes (main.c MAIN_MPU_330/331 = the /MUSB path, VMPU off): only FFh/3Fh ACKed; intelligent-mode command parameter
  bytes went out as MIDI (notes); status port never "busy", so game bursts (GS reset + set-up SysEx + all channels)
  overflowed the 1024-packet queue -> lost program changes / note-offs / SysEx pieces; F0 inside an open SysEx lost up
  to 2 bytes; SBDIAG side channel (DIAG_*) absent with /MUSB (GAMETEST/SBDIAG blind); MENU knew /MUSB only from SBOPTS.
- Fix: main.c MU_* front end (ACK all, version/revision answers, D0-D7 / DF / E0-EF params, sequencer data ignored,
  DIAG first, AC/AD still answered), status bit 6 = USBPAD_MidiBusy (queue < 96 free; waits while the queue moves, 1 s
  without progress lets the game on; trap pump = light mout_poll, not on xHCI, no OHCI ED rewrite). usbpad.c: pacing
  3125 B/s, 128-byte burst, pieces >= 48 bytes; timeout checked after the transfer was looked at (late poll was a
  false NAK -> xHCI drop); product name (iProduct), port, state, last error -> AMIS 05h; /MIDICAP:1 capture pages
  "SBMIDCAP" (AMIS 06h, ECX=-1 clears). sbemuchk.h sbemu_musb()/musb_line(). MENU status panel + /STATUS + 3D title,
  Tests "USB MIDI test (MPUTEST /USB)", SETTINGS synth line, MPUTEST /USB (exit 3 = lost), GAMETEST /USB + screen +
  GAMETEST.TXT, SBDIAG USB / MIDICAP [file], BUGREP "USB MIDI" block, README.TXT, HWTEST.TXT.
- Tested (testrig/midi/README.md): gamecap.sh uhci miles gssysex mi2 intel rm duke heretic descent PASS; xhci gssysex
  miles duke PASS; ehcicc gssysex miles heretic PASS; ohci gssysex PASS; menumidi.sh xhci PASS; beta.120 SBEMU on the 4
  scripts FAIL. unomatrix quick PASS except sbemu-dos71-ohci QEMU-LIMIT: with rig.py now timer-period=2000 (real IRQ
  rate) beta.121 binaries also hit QEMU's usb_cancel_packet assertion there (2/2) - QEMU bug. menustat 5/5 on the zip.
- Not done: Monkey Island 2 / a real Miles game / a GS game (none free: scripts stand in); 86Box PII-450 speed run
  skipped: default SC-55 path untouched (VMPU handler, no MIDI pipe), /MUSB can't run in 86Box (no USB pass-through).
- Hardware ask (Uno + SC-55): HWTEST.TXT beta.121/122 item; send USBn.TXT (SBDIAG USB) + CAPn.BIN (SBDIAG MIDICAP) per
  game; `python3 testrig/midi/midicap.py CAP1.BIN` checks what SBEMU sent vs the game's writes.
- Next 3 steps: 1) user's Uno results; 2) if the real Uno still garbles: CAPn.BIN U pages vs game writes, try smaller
  MPACE_BURST; 3) intelligent-mode sequencer (SoftMPU-style) only if a game needs it (IGN= count in SBDIAG USB).
- Test downloads: DJGPP v3.4 (/tmp/dl/djgpp), rufus sparse (/tmp/dl/rufus), arj (apt), testgames/ unpacked to /tmp/g.

## beta.121 (2026-11-14, DOS session): WCSET write-combining shipped (Fast video)
- Done: WCSET.EXE in the zip (rezip.py NEW list adds new EXEs), MENU Settings "Fast video (WCSET)" = WCSET /ASK,
  Tests "Video speed (WCSET)" = WCSET /BENCH (view WCBENCH.TXT); README.TXT "FAST VIDEO" under 3D MENU; README news
  (5 on top, beta.116 moved to Older). Zip = beta.120 zip + rezip.py (all EXEs rebuilt as beta.121).
- Tested: 86Box P3B-F PII-450 + Voodoo3 3000 PCI (testrig/wcset/86box/86box.cfg; the CL-GD5446 cfg has no VBE LFB):
  pm (real mode) + rm (JEMM) PASS, WC write path works: MTRR 1 = WC DE000000 16 MB set, cleared, slot free after
  MENU/MIDPLAY exit; 86Box does not model WC speed (bench x1.00, fps equal). QEMU pm/rm PASS (SeaBIOS UC MTRR ->
  refused, correct). testrig/menustat 5/5 PASS. SBEMU code unchanged (WCSET only in tools/) -> no SBDIAG/SC55LOG cost.
- Hardware steps (D810 Radeon X300, P5PE-VM GeForce 6800), boot as usual (HIMEM + SC55, or JEMM):
  1. MENU > Tests > Video speed (WCSET): send WCBENCH.TXT (expect WC on >> off for stosd if the BIOS leaves a free MTRR).
  2. C:\SC55> WCSET : send the screen/LIST output (MTRRs, frame buffer range, "WC possible" or why not).
  3. SET M3PROF=1, MENU, F8 (3D) ~20 s, Esc; again with SET WCSET=OFF: send both M3PROF.TXT (fps).
  4. Any game in a VESA mode after WCSET ON (kept): glitches? Then WCSET OFF. SBDETAIL.TXT "[wc]" lines.
- Open: boards with default WB + UC MTRR over the PCI hole get no WC (PAT idea, not started).
- Next 3 steps: (1) hardware results above -> tune (e.g. offer PAT when MTRRs are taken); (2) D810 48 kHz task
  (PLAYBOOK prompt); (3) MIDPLAY vector/voxel modes could use WC numbers from step 1.

## beta.120 (2026-11-14, DOS session): P5PE-VM froze with ICH5 SATA "Enhanced" (user; "Compatible" fine)
- Cause (likely): Enhanced = SATA in native PCI mode, IRQ shared with the Audigy. EMU10K irq_routine returned
  non-zero for ANY IPR bit (MIDI bits nearly always set) -> SBEMU claimed the shared IRQ, EOI, no chain -> disk wait hung.
- Fix: sc_sbliv.c snd_emu_10kx_isr/snd_p16v_isr ack + return only IPR_INTERVALTIMER|SAMPLERATETRACKER|FXDSP (the
  INTE bits SBEMU sets). main.c: SBDETAIL "disk controller ... native PCI mode/legacy ... IRQ line" per IDE/SATA
  controller, WARNING + yellow screen warning when one shares the card IRQ. README.TXT troubleshooting, README news.
- Tested: testrig/sata/run.sh ZIP (QEMU, plain + ich9-ahci addr 6-9): warning exactly when AHCI shares IRQ 11 with
  AC97, MPUTEST + DIR OK; menustat matrix 5/5 PASS. Not testable here: the EMU10K ISR itself (no Audigy in QEMU/86Box),
  so the real check is the P5PE-VM. Runtime cost: none for other cards (startup PCI scan only), so no 86Box speed run.
- The beta.120 zip was built from b2ab800 (wip, WITHOUT the WCSET commit d25ca9c): WCSET defaults to AUTO and its
  MTRR write path is unverified, so it stays out of the pack until its own task finishes (see WCSET section).
  Yellow warning goes to the console (conio), not into redirected SC55 output.
- Hardware ask (P5PE-VM): SATA back to Enhanced, beta.120 SC55 -> freeze or not, SBDETAIL.TXT ("disk controller",
  "WARNING" lines). If it still freezes: P5TEST 1-4 logs.
- Next 3 steps: 1) P5PE-VM result above; 2) if still frozen, check the other P5PE-VM drivers that may claim foreign
  bits (USB host ISRs on the same IRQ); 3) continue the WCSET section's steps (86Box WC path, then pack it).

## WCSET write-combining (2026-11-14, DOS session; finished and released in beta.121, see above): faster DOS graphics via MTRR WC on the VESA LFB
- Done (main, unreleased): tools/wcset.h (shared: CPUID/MTRRcap/phys-width checks, real-mode stub via DPMI 0301h for
  RDMSR/WRMSR/CR0/WBINVD = real mode under HDPMI raw, V86 under JEMM (Jemm EMU.ASM emulates them); EMM386/QEMM V86 and
  Windows/NT/OS2 boxes refused; free variable MTRR only, any overlap with a set MTRR = left alone; SBDETAIL "[wc]" line,
  no duplicates). tools/wcset.c = WCSET.EXE LIST|ON|OFF|AUTO|/BENCH|/ASK (WCSET.CFG, env WCSET overrides; /BENCH =
  stosd/stosb/movsd MB/s WC off vs on at 640x480 LFB, appended to WCBENCH.TXT). Hooks: MENU main (wc_startup "MENU"),
  MIDPLAY main after clock_init, m3core.h after each LFB map (M3_WHO). AUTO = restore at exit (atexit); ON = kept.
  menu3d.h: env M3SECS=N leaves the 3D menu after N s (test rigs, with M3PROF). makefile: output/wcset.exe.
- Tested (testrig/wcset/run.sh ZIP pm rm, QEMU pentium3): PASS both; pm = "real mode", rm = "V86 (JEMM)"; SeaBIOS sets
  default WB + UC MTRR 80000000-FFFFFFFF, so WC is refused there (correct by the rule); bench/MENU 3D/MIDPLAY fps
  equal off vs auto (nothing changed). QEMU numbers: stosd ~1600-2100 MB/s, MENU 3D ~82 fps, MIDPLAY 3D ~19 fps.
- NOT done: 86Box PII-450 run (needs apt-get install librtmidi6 xvfb; Award P3B-F BIOS should allow the WC write path,
  never exercised yet), pack integration (WCSET.EXE into pack/FILES/SC55 + zip, MENU Settings item "Fast video
  (WCSET /ASK)", Tests item "Video speed (WCSET /BENCH)", README/CHANGELOG), SBDIAG/SC55LOG no-slowdown check (SBEMU
  untouched), release beta.120, D810/P5PE-VM test steps.
- Open risk: boards whose BIOS uses default WB + a UC MTRR over the PCI hole (like SeaBIOS) get no WC; the fix would be
  PAT (PIII+ only) - idea, not started. WC write path (op 3 in the stub) unverified on any emulator so far.
- Next 3 steps: (1) 86Box: EMU="-e 86box -c /home/claude/vms/dos71/86box/86box.cfg" TMO=900 sh testrig/wcset/run.sh
  ../release/NUKED-SB-EMU-1.0.0-beta.119-usb.zip pm rm - check "ON (MTRR n" lines and that LIST shows the slot free
  again after MENU/MIDPLAY exit; (2) pack + menu items + docs; (3) release beta.120 with hardware steps (WCSET /BENCH,
  MENU 3D fps via M3PROF) for D810 (Radeon X300) and P5PE-VM (GeForce 6800).
- Resume: export PATH=/tmp/dl/djgpp/djgpp/bin:$PATH (curl build-djgpp v3.4); cd sc55-pack; make -j16; restore.sh dos71.
- Test downloads: DJGPP v3.4 (/tmp/dl/djgpp), LordOfMice/Tools msr.zip (reference), Baron-von-Riedesel/Jemm + HX sources (/tmp/dl).

## beta.119 (2026-11-14, DOS session): beta.118 froze the ASUS P5PE-VM + Audigy (Win98 -> MS-DOS mode); beta.112 fine
- Suspect: beta.115 MAIN_CardWatchdog (main.c) calls aui.card_handler->irq_routine from SB port traps and INT 1Ch,
  outside the interrupt, for every card; proven only on ES1371 (86Box). Not reproducible here (no emulator has an
  EMU10K); root cause on the board unconfirmed until P5TEST results come back.
- irq_routine audit (outside the ISR, called with CLIS, MAIN_InINT guard): ES1371 safe (status read + toggle of the
  DAC int enable). ES1370/VIA82xx: ack-only, probably safe, unproven. EMU10K Live/Audigy (snd_emu_10kx_isr /
  snd_p16v_isr): acks ALL IPR/IPR2 bits incl. MIDI/UART and returns non-zero for any -> "pending" almost always, can
  eat the channel-loop IRQ the ISR then sees as foreign. ICH AC'97: advances LVI (DMA state). CMI: reads the MPU data
  port (eats MIDI IN) and rewrites HLDCLR. HDA: acks RIRBSTS under a running codec verb. -> unsafe.
- Fix: /CARDWD:0|1|2 (default 2 = auto: on only for "Ensoniq/SBPCI"); off = no trap work and NO INT 1Ch hook (the
  beta.115 RM->PM callback from IRQ0 was the other new start-up/IRQ-time change). SBDETAIL: "Sound card watchdog:
  ACTIVE/off (card X, /CARDWD:n ...)", trace event. New [step] lines: card start, CD-Audio link + INT 28h, flight
  recorder, watchdog INT 1Ch hook (finer start-up step log, each fsync'd).
- Other beta.113-118 SBEMU start-up/IRQ changes checked: uCDD -install + /VCDA:100 by default (beta.115: CDLINK,
  INT 28h hook, MAIN_CDService after every card IRQ) = suspect 2; SC55RUN.BAT honours SET SBNOCDA=1 to skip it.
  beta.114 USB-MIDI watchdog runs only with a USB MIDI device; beta.116 packver constructor (argv scan) and beta.118
  AMIS fn 04h run only when asked: not suspects. beta.113 = MENU only.
- Bisect kit in the zip: C:\SC55\P5TEST.BAT 1-4 (1 shipped/auto, 2 /CARDWD:1, 3 /CARDWD:1 + SBNOCDA, 4 /CARDWD:0 +
  SBNOCDA = beta.112 start-up); saves SBDETAIL+SBTRACE of the previous run to P5T<N>.LOG.
- Tested (QEMU store-dos71 HIMEM-only, AC97): menustat matrix none/pm/pmhand/rm/rmonly PASS (both SBEMU modes);
  default -> "off (ICH AC97, auto: not proven)", /CARDWD:1 -> ACTIVE + INT 1Ch, /CARDWD:0 off; P5TEST 2/3 logs OK.
  Not run: 86Box ES1371 (auto ACTIVE path; same code as beta.118 there) and the PII-450 speed check (change only
  removes work). testrig/common/rezip.py now also swaps/adds pack/FILES/SC55 files.
- OPEN: P5PE-VM results of P5TEST 1-4 (where it froze, P5T*.LOG). If 1 still freezes -> not the watchdog: check
  SBNOCDA (test 4), then MENU (beta.113).
- Test downloads: DJGPP v3.4 gcc12.2 (/tmp/dl/djgpp).

## beta.118 (2026-11-14, DOS session): MENU said "Not loaded" with SBEMU loaded (user, beta.116 personal pack, P5PE-VM)
- Cause class: MENU knew SBEMU only by an MPU-401 reset probe, which answers only when SBEMU's port trap reaches the
  MENU process (HDPMI trap for PM, QPIEMU for RM). Any setup where it doesn't (Protected mode off = /PM0, a different
  DPMI host, a port/trap problem on the real board) showed "Not loaded". Reproduced: SC55JEM /PM0 -> MPU no answer.
  NOT reproduced on the P5PE-VM's own path (MS-DOS 7.1 HIMEM only + SC55.BAT): QEMU AC97 answers at 330. Root cause on
  that board still open: get BUGREP.TXT "MENU status" line from it. beta.113-116 changes (uCDD -install, INT 1Ch
  watchdog, /VCDA, 3D menus) checked: none touch the MPU trap or the probe.
- Fix: SBEMU AMIS (INT 2Dh) function 04h = "VER= MODE=RM+PM MPU= TRAP= SB=" (main.c); tools/sbemuchk.h: AMIS
  signature + 04h, then the MPU probe; Loaded if either, "Unknown" if the AMIS scan failed or a Windows DOS box,
  else Not loaded. MENU text + 3D status, "MPU ?" when found by AMIS only, `SC55MENU [home] /STATUS` (exit 0/1/2),
  BUGREP.BAT "MENU status" block, README.TXT note. No runtime cost (AMIS handler runs only when called).
- Matrix: sc55-pack/testrig/menustat/run.sh ZIP [EXE..] (QEMU, store-dos71): none / pm / pmhand / rm / rmonly, all
  PASS on the beta.118 zip. Not run: Win98 DOS box (store-w98qi-qemu has no automation for a DOS box yet), FreeDOS.
- Real hardware: P5PE-VM + D810: SC55 then MENU -> status panel "Loaded"; run BUGREP, send the "MENU status" line.

## beta.117 (2026-11-14, DOS session): uCDD disc hotkey "stopped after a re-mount" = SHSUCDX 7 s media check; MENU Panels reason box
- Root cause of the beta.115 OPEN item: not uCDD and not the mount order. SHSUCDX asked the driver for a media change
  only after 128 ticks (~7 s) without an access to M:; the matrix swap step did DIR, Ctrl+Alt+PgDn, DIR within ~5 s,
  so the second DIR showed the old disc (the disc2 step had a longer gap and passed). uCDD had switched (cur 2 in a
  memory dump). Fix: third_party/shsucdx/shsucdx.nsm threshold 2 ticks (~0.1 s); README.TXT note. Real effect for
  users: a game's "insert disc 2" prompt answered right after the hotkey read the old disc.
- New fast rig sc55-pack/testrig/menumx/hotkey.py (QEMU, no MENU, ~40 s): GAMES.MDM -> hotkey -> RAP.CUE -> GAMES.MDM
  -> hotkey x2 -> GOT.CUE -> GAMES.MDM -> hotkey; DIR after each. Old SHSUCDX: every swap FAIL; new: PASS (HIMEMX and
  JEMMEX). QEMU needs `-global i8042.kbd-throttle=on` (else QEMU pops port 60h on read and a peeking INT 9 hook such
  as uCDD's sees every other scancode). menumx.py swap step is a real PASS/FAIL now (matrix case).
- MENU 3D Panels: a grey chosen row fits its label before the reason box (menu3d.h); tests/m3host m3menu.c 1b checks
  it at 640x480 (fails on the old code, M3SHOT=x.ppm dumps the frame).
- Not run: 86Box matrix (the store-86rig 86Box has no automation socket; rebuild per testrig/86box/README.md).
  SBEMU unchanged -> no speed measurement needed. Zip = beta.116 zip, rezip.py + SHSUCDX.COM swapped.
- Real hardware: on the D810, mount an .MDM in MENU, DIR M:, Ctrl+Alt+PgDn, DIR M: at once -> disc 2 shows.
- Test downloads: DJGPP v3.4 (/tmp/dl), HIMEMX from store-freedos (ibiblio blocked), rufus (testrig/rufus).

## beta.116 (2026-11-14, DOS session): /VER in every EXE (ported from wip/release-rc-2026-11-13)
- sc55-pack/tools/packver.c: a constructor in every EXE answers `NAME /VER` ("NAME.EXE: NUKED SB-EMU <VERSION>") and
  exits before main(); the makefile links output/packver.o into SBEMU and all 15 tools (packver.h/NSBVER stays for
  SBDIAG DOCTOR). BUGREP.BAT: "Program versions" block (each EXE /VER, CDGUI from C:\CDSET). README.TXT note.
- Tested (QEMU, store-qemu c.img, HDPMI32I -r): all 16 EXEs print 1.0.0-beta.116-usb-sc55. No runtime cost (argv
  scan once at start). Zip = beta.115 zip with the 16 EXEs + BUGREP.BAT + README.TXT swapped
  (sc55-pack/testrig/common/rezip.py OLD NEW, run from the repo root after make).
- Not ported (stale): the branch's HANDOFF/mkzip/menu-walk/UNINSTAL/CLEANUP changes. wip/release-rc-2026-11-13 and
  wip/menumatrix-2026-11-13 deleted.
- Real hardware: run BUGREP on the D810, check the "Program versions" block lists every EXE.
- Test downloads: DJGPP v3.4 gcc12.2 (/tmp/dl/djgpp).

## beta.115 (2026-11-14, DOS session): MENU matrix - CD music without HYBRID, sound-card IRQ watchdog, 24-run test matrix
- CD music: SC55RUN.BAT loads `UCDD.EXE -install` before SBEMU (no drive letter) and passes /VCDA:100 before @SBOPTS;
  SETTINGS "CD audio volume" default 100%, Off = /VCDA:0. Before, CD-Audio only worked with HYBRID.BAT; a disc mounted
  later from MENU was silent (SBEMU links to uCDD only at load). MENU/CDGUI/README.TXT texts updated.
- SBEMU watchdog (main.c MAIN_CardWatchdog, SB port traps + INT 1Ch hook): no card IRQ for 250 ms -> ack the card's
  pending interrupt. Found in 86Box (ES1371): a lost IRQ edge left STATUS FFFFFEC4 (DAC1 pending), PIC clean, and
  SBEMU silent until reboot, ~1 boot in 5 - God of Thunder then said "SoundBlaster ... BLASTER". Trace event "sound
  card interrupt was stuck". Speed (testrig/midi/perf.sh, PII-450): 344 int/s, 0.02 ms, 1% = beta.112; same with uCDD
  + /VCDA:100.
- MENU: the CD image / program picker shows its title + folder line again (header animation drew over it).
- Matrix (sc55-pack/testrig/menumx, 86Box P2B-LS PII-450 ES1371, FreeDOS + JEMMEX): FAT12/16/32 x ISO/CUE/MDM/long
  names x SC55JEM (real) / SC55 (protected): boot, mount, DIR, install (GOT INSTALL.BAT, Raptor Apogee INSTALL.EXE),
  play (sound level from 86Box), CD music, unmount, disc 2, disc swap. Table below. `python3 menumx.py all --jobs 2`.
- Findings that are not pack bugs: Apogee INSTALL.EXE renames RAPSW12.SHR -> .EXE in its own folder (fails from any
  read-only CD; the test disc ships the .EXE). uCDD applies a disc hotkey only while DOS is idle (fine in games).
- FIXED beta.117 (SHSUCDX 7 s media check): uCDD MDM hotkey stops switching after a single image and the set were mounted again (rig repro: mount
  GAMES.MDM, mount RAP.CUE, mount GAMES.MDM, Ctrl+Alt+PgDn + WAIT1 -> still disc 1). First swap after a mount works.
- Real hardware to check: CD music in a CUE game started from MENU (D810, P5PE-VM); SBTRACE.TXT "was stuck" lines.
- Matrix result (86Box, release candidate = beta.114 zip + these files; logs /home/claude/mx/all, not kept):
| case | real (SC55JEM) | protected (SC55) |
|---|---|---|
| fat12-cue | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat12-iso | PASS (n/a: cdmusic, swap) | PASS (n/a: game, cdmusic, swap) |
| fat12-lfn | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat12-mdm | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat16-cue | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat16-iso | PASS (n/a: cdmusic, swap) | PASS (n/a: game, cdmusic, swap) |
| fat16-lfn | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat16-mdm | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat32-cue | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat32-iso | PASS (n/a: cdmusic, swap) | PASS (n/a: game, cdmusic, swap) |
| fat32-lfn | PASS (n/a: swap) | PASS (n/a: game, swap) |
| fat32-mdm | PASS (n/a: swap) | PASS (n/a: game, swap) |
  swap n/a = the OPEN uCDD item above (disc 2 via Ctrl+Alt+PgDn passes in all 6 MDM runs); game n/a under SC55 =
  God of Thunder is real mode (no QPIEMU in protected-only mode, by design); cdmusic n/a = ISO has no audio tracks.
- Rig notes: 2 VMs on 2 cores drop/repeat keys -> verified typing (mxvm cmd, type_checked), sel() steers to the row,
  lost-Enter retries. QEMU (--vm qemu) fails menu navigation (SeaBIOS doubles grey arrows). ~11 min per pair.
- Probes (testrig/menumx/probes): SBIRQ (DSP 14 -> IRQ time), PIC (IMR/ISR), ESSTAT (ES1371 regs), DTRACE (INT 21h
  log; breaks MSC programs), OPENT/SEEKT (file API on M:), WAIT1, unlz.py (LZ91 unpacker), loop*.sh.
- Test downloads: HIMEMX (ibiblio FreeDOS 1.3 base/himemx.zip, mxdisk env HIMEMX), nasm (apt) for the probes.
- Resume (if anything is left): rebuild per docs/SESSION-2026-11-13-DOS.md, then `cd sc55-pack/testrig/menumx &&
  MX_ZIP=<zip> python3 menumx.py all --jobs 2 --resume --out /home/claude/mx/all`.

## beta.114 (2026-11-13, DOS session): M-Audio Uno froze the P5PE-VM - USB MIDI watchdog, [midi] step logs, real-Uno fake
- Report (user, beta.113): the Uno froze the ASUS P5PE-VM (first said D810) on every port, with any or no other USB
  hardware; boot = Win98 > MS-DOS 7.1, HIMEM only (no QEMM/JEMM), HDPMI32I + SC55.BAT. Which program / when: not given.
- Real Uno 0763:0150 rev 1.25 (usb-devices dump, xed.ch/help/midi.html): bcdUSB 1.00, ep0 64, Atr C0 0 mA, no serial,
  bulk IN 81h before bulk OUT 02h, 64 bytes; no Linux quirk except IN running-status CIN 4. Old MidiSport 0763:1010 etc.
  (EZ-USB firmware, vendor protocol) now refused by name. testrig/fakemidi.py default = that shape + UART pacing.
- QEMU does NOT reproduce (uhci, ohci, ehcicc, xhci; MS-DOS 7.1 HIMEM-only and FreeDOS JEMM). Audit found: USBTEST's
  note-off flush waited for ever (`while (USBPAD_MidiPending()) USBPAD_Poll();`) - a device that never takes OUT hung
  USBTEST; no OUT watchdog (a NAKing OUT stayed busy for ever, CLEAR_FEATURE retried without limit, all in SBEMU's
  sound interrupt); Shutdown flush read P[-1] when the device went away; trace not on disk until USBTEST's init ended.
  Descriptor walk, MidiByte, MPU trap, interrupts (all off), FIFO: already bounded.
- Fix (usbpad.c): OUT watchdog - not taken in 0.5 s: taken back (UHCI QH emptied, OHCI ED / EHCI QH restarted, toggle
  0), CLEAR_FEATURE; 3 in a row or 8 stalls in a row: both MIDI pipes stopped, "[midi] ... not used any more" - the PC
  keeps running, unplug/replug retries. [midi] lines: device/config summary, every descriptor, pipes, first OUT +
  done, timeouts, stalls, CLEAR_FEATURE, drop, shutdown. USBTEST: steps on disk per line (fsync) from the start,
  USBTEST.TXT without a file name too, M = raw descriptors, flush bounded 1 s. MIDPLAY /USB: USBMIDI.TXT the same way.
  SBEMU: start-up lines in SBDETAIL (committed), runtime ones via the flight recorder (SBTRACE.TXT).
- Tests: testrig/midi/unomatrix.sh (rig.py --dos71 = store-dos71 MS-DOS 7.1 HIMEM only): normal Uno USBTEST / SBEMU
  /MUSB + MIDPLAY with unplug at 15 s / MIDPLAY /USB, hostile OUT nak / stall, bLength 0, long wTotalLength, no
  endpoints, MidiSport 1010, beta.108 fake: 37 PASS + 2 QEMU-LIMIT (MIDPLAY /USB on QEMU OHCI, see below) on
  the release binaries; every hostile case reaches DONE with the reason logged.
- QEMU limits: its OHCI keeps one packet in flight and stops a multi-packet OUT when the answer is late (matrix: pacing
  off on ohci); it also loses appended OUT TDs like the NEC (lost-TD check must stay for OUT: removing it = 0 events);
  ED rewrites with an async usbredir packet in flight sometimes abort QEMU (usb_cancel_packet assertion) - QEMU bug.
  86Box: no USB pass-through device, the fake cannot run there.
- MENU matrix: run.sh MIDI=1 (fake on port 3, /MUSB:1, MPUTEST plays to it, MIDIEV >= 50; not uhci): 71 x32 (HIMEM only) +
  j32 on ohci and ehcic, before: all 4 PASS (disks A/B, 7 plays, 424 MIDI events each).
- Speed (86Box P2B-LS PII-450 ES1371, midi/perf.sh, /JOY:1 /VMPU:1): beta.113 = beta.114 344 int/s, avg 0.02 ms, 1%.
  SC55LOG.EXE byte-identical (md5 8cc1f00a..). 86Box rebuilt @ 0935207 + diffs (SDL, automation) in /tmp/dl/86box.
- Test downloads: DJGPP v3.4 gcc12.2, rufus (FreeDOS kernel), TimGM6mb (apt), 86Box 0935207, Linux sound/usb sources.
- Zip: beta.113 zip + all 17 EXE/COM rebuilt (VERSION beta.114), README.TXT, HWTEST.TXT. Personal pack: not built.
- Open (P5PE-VM, ask once): HWTEST.TXT beta.114 item (USBTEST first with SBEMU unloaded, MIDPLAY /USB, SC55 /MUSB game,
  then the NEC card): the last [midi]/[usb] line of USBTEST.TXT / USBMIDI.TXT / SBDETAIL.TXT names the step if it
  still freezes; USBTEST M dump = the real descriptors (compare with fakemidi.py).

## beta.113 (DOS session, 2026-11-13): MENU 3D everywhere + MENU bug hunt fixes - RELEASED from wip/menu3d-2026-11-13
Tests: host menurun.sh (menuasan + m3menu, ASan/UBSan) PASS; 30-min soaks (soak.py, ~3000 inputs, no CRASH.TXT):
QEMU hd, QEMU jem, 86Box PII-450 hd, 86Box jem PASS; Win98 SE (QI stock) DOS box via D:\LOOP.BAT, soak.py w 15 min.
CRASHKEY build: '!' in the text menu -> CRASH.TXT with registers/3D state/keys PASS (0xFFFFFFF0 write did not fault;
now null write + ud2). Bench 86Box PII-450 old (beta.110) vs new (soaked as beta.111 on beta.110; menu code unchanged in 112) fps: Carousel 19.3/19.4, Module 18.9/18.7, Panels
11.8/13.7; SBDIAG 3% CPU / 8% render both (SBEMU unchanged - SC55LOG not re-measured, SBEMU code not touched).
Rigs: testrig/menusoak (mkfolder.py jem = JEMMEX alone; BOOTLOG says LoadFailed for JEMMEX.EXE - false), runq.sh.
Win98 QI VM: boot without -usb; first boot hangs with a fresh D: (LESSONS) - add D: after the desktop is up.
Open (hardware, ask user once): 3D menu on the D810 (Radeon VESA modes/refresh list, F4 keep-mode), P5PE-VM (GeForce
6800), Win98 DOS box full screen. Cosmetic: Panels at 640x480 puts the grey reason box over the end of a long label.
Test downloads: DJGPP andrewwutw v3.4 gcc12.2 (/tmp/dl/djgpp), win98qi_v1.1.0_stock.iso, TimGM6mb.sf2, rufus sparse,
86Box 0935207 + roms, apt qemu-system-x86 mtools dosfstools zstd gcc-multilib.

## Personal pack beta.112 (2026-11-13): NUKED-SB-EMU-1.0.0-beta.112-usb-personal.zip
- Public beta.112 zip (sha256 1bfa7af2.. = release asset) + VM store store-personal files at the paths SETUP.BAT copies from:
  FILES\ROMS\* (41 ROMs + ROMHASH.CAC), FILES\XCDROM\MSCDEX.EXE, FILES\VIDECD\VIDE-CDD.SYS+VIDECD.TXT, FILES\GSCDROM\*,
  FILES\MOUSE\MOUSE.COM+MSMOUSE.TXT. Nothing else changed. Copy: priavte branch store-personal-1.0.0-beta.112-usb (not public).
- Test: fresh FreeDOS (mkdisk.py, KERNEL/COMMAND from store-qemu c.img), SETUP C, CONFIG JEMMEX NOEMS + VIDE-CDD /D:MSCD001,
  testrig/common/personal.bat via run.sh -r (+ -cdrom): CDROM.BAT -> D: lists the ISO; SC55.BAT: 41 ROMs hashed, set mk1-v2.00
  boots; SBDIAG: Synth SC-55, MIDI 19 B/s, 0 dropped (QEMU output 0 Hz = audiodev none quirk). No code change: no perf run.

## beta.112 (2026-11-13, DOS session): USB disks on xHCI, plug-after letters, UCDD from exFAT/NTFS letters (menumatrix)
- Built from branch wip/menumatrix-2026-11-13 (rebased on beta.111). Zip = beta.111 zip + 17 EXEs rebuilt (beta.112) +
  README.TXT HWTEST.TXT CDSET\UCDD.EXE CDSET\UCDDSRC.ZIP (mdm_helper.asm) (testrig/rc/mkzip.py with src = beta.111).
- Code: usbpad.c xHCI bulk (x_disk_eps: OUT/IN rings per disk at XO_MSD, one Configure Endpoint; x_bulk: Normal TRBs
  split at 64 KB, IN ISP, IOC last, events matched to the TRBs pushed; msd_clear = Stop EP + drop/add = new ring +
  DATA0; XEvBusy guard; light polls drain xHCI events only when that HC has pipes). usbdisk.c / usbredir.c: spare
  letters adopted per partition, 2 exFAT/NTFS spare letters (network, CDS +49h "SBUR"), USBDRV states 3/4.
  UCDD mdm_helper.asm: images on "SBUR" network letters allowed.
- Tested: testrig/menumatrix (QEMU; FreeDOS + MS-DOS 7.1 x JEMMEX / HIMEM only / 16-bit x UHCI OHCI EHCI EHCI+companion
  xHCI x sticks before/after SBEMU): logs/matrix-2026-11-13.txt 60/60 PASS (per ISO: mount from stick, install, bytes,
  MPUTEST play, unmount; stick swap). 86Box P2B-LS PII-450, /VSF TimGM6mb /JOY:1 /USBDISK:1, beta.110 vs beta.112:
  idle 345 IRQ/s avg 0.02 ms both; STRESS.MID ISR avg 1.77 ms / longest 4.73 vs 4.74, render 237 vs 238%, 0 lost ticks.
- OPEN (pre-existing, not from this work): QEMU xHCI with USB sticks attached, even /USBDISK:0: the SoundFont often
  never renders (SBDIAG "Synth: SC-55, output 0 Hz", MIDI arrives, AC97 runs). Comes and goes with code layout
  (a debug build with counters in main.c/vmpu.c always played): smells like a stray write. No sticks = plays.
  Repro: `SBX=/USBDISK:0 testrig/menumatrix/run.sh fd j32 xhci before` (WAV loud 0.0). Real xHCI PCs untested.
- Rig notes: xhci p2=8,p3=0 (QEMU usb-storage otherwise lands on a USB 3 port that SBEMU leaves off); audiodev
  timer-period=2000 (else ~8 IRQ/s); PASS = 7x PLAY OK, loud = info; FreeDOS x32 = HimemX (MS HIMEM hangs the FreeDOS
  kernel; build: Baron-von-Riedesel/HimemX + JWasm `make -f GccUnix.mak`, `jwasm -mz HimemX.asm`); HASISO.COM skips
  empty letters (MS-DOS 7.1 asks Abort/Retry/Fail there - normal DOS, documented in README.TXT).
- Resume rig: DJGPP + apt (qemu-system-x86 socat mtools exfatprogs exfat-fuse ntfs-3g gdisk fdisk dosfstools
  genisoimage nasm); FULL.866 = `isoinfo -J -i win98qi_v1.1.0_ALL.iso -x /osroots/1/FULL.866` (github.com/oerg866/
  win98-quickinstall v1.1.0), mercy.py -> /tmp/dl/w98qi/x (IO.SYS MSDOS.SYS COMMAND.COM WINDOWS\HIMEM.SYS);
  testrig/rufus/res/freedos + src from github.com/pbatard/rufus; TimGM6mb (apt-get download timgm6mb-soundfont)
  -> /tmp/dl/timgm6mb; release zip -> /tmp/rel; `make; sh tools/ucdd/mkcdset.sh; mkstick.sh /home/claude/vm/st`;
  `J=2 testrig/menumatrix/all.sh OUT`. 86Box perf: 86Box @0935207 + 86box-xpad.diff, VM = mkdisk build of
  KERNEL/COMMAND/JEMM/HDPMI + SC55\A|B\SBEMU.EXE + PERF.BAT (SBDIAG 3x DELAY idle, MIDPLAY STRESS.MID /S /Q:30),
  cfg p2bls-uhci.cfg with cpu_speed 450000000 / multi 4.5, F1 at the CMOS message.
- Real-PC checks: HWTEST.TXT beta.112 lines (plug-after partitions + exFAT/NTFS, CD image on such a stick, USB 3 port).

## beta.111 (2026-11-13, DOS session): release candidate - menu walk, SETUP/UNINSTAL/CLEANUP, Win98 test, USB review
- Branch wip/release-rc-2026-11-13 rebased on beta.110 (its own /VER packver.c + BUGREP block dropped: main's packver.h +
  SBDIAG DOCTOR already do it). Zip = beta.110 zip + all 17 EXEs rebuilt (VERSION beta.111) + SETUP UNINSTAL CLEANUP
  SC55RUN README.TXT (testrig/rc/mkzip.py; rig: testrig/rc/t86.py v un.py cl.py, menuwalk-2026-11-13.log).
- Tested: 86Box FreeDOS: every MENU entry with and without SBEMU (all launch + return; USBTEST and Automatic setup
  greyed while SBEMU is loaded, by design), CD image mount M:, SETUP; UNINSTAL K and D (D = disk identical to before
  SETUP), CLEANUP with planted leftovers and with none. Win98 (QEMU QuickInstall): DOS-box SETUP/CDINST/MENU (Start
  refuses: "Windows is running"), SETUP98 + restart, SC55W tray + MPUTEST answers at 330h in a DOS box, Restart in
  MS-DOS mode + SC55 (SBEMU runs, no false Windows message; QEMU's ISA SB16 = "no supported soundcard", expected).
  beta.111 zip smoke test in 86Box: SETUP, SC55, MENU header beta.111.
- Fixes: UNINSTAL D left C:\HDPMI\HDPMI16.TXT + C:\GSCDROM\README.TXT; K saves C:\SC55\*.SF2; CLEANUP "nothing to
  delete" + TYPE|FIND (no header noise); SETUP MSCDEX copy guarded; SC55RUN no CTMOUSE when %windir% is set (Win98 box).
- Known issues (not fixed): UNINSTAL / CLEANUP need MS-DOS's MOVE FC ATTRIB FIND CHOICE (a bare FreeDOS boot without
  them stops at once, "NOTHING was removed"); Sound > JEMM / QEMM entries without JEMM/QEMM loaded from CONFIG.SYS only
  show SBEMU's "QEMM or QPIEMU not installed" line; Win98 binaries (SC55W*) carry no pack version; one QEMU hang at
  "C:\>" on SETUP98's restart (a DOS box was open; did not repeat - watch on real PCs); README counter.gif still 000109.
- Rig lesson: never mcopy into a disk image while Windows runs on it (Windows reused the clusters: the BAT became
  IOS.LOG text, "Bad command or file name" x20). Copy with the VM off.
- USB code review (beta.108 + beta.110, usbpad.c; not changed here - owned by the USB sessions):
  1) hc_quiesce UHCI: "stop (HCHalted)" timeout now SKIPS the controller. UHCI spec: USBSTS default 0000h and HCHalted
     is only set after Run/Stop goes 1->0, so a UHCI the BIOS never started can read HCH=0 while stopped -> its ports
     dropped (beta.109 went on). QEMU sets HCH at reset, so usbmulti.py cannot show it. Linux only warns here.
     Suggest: fail only if USBCMD.RS still reads 1, else log and go on.
  2) A /USBSKIP'd or failed EHCI keeps whatever CONFIGFLAG the BIOS left (usually 1 = all ports on the EHCI), so its
     companions see no devices; only the "[usb]" line says why. Worth a sentence in README.TXT /USBSKIP.
  3) OHCI phase 1 only turns interrupts off; one left running by a BIOS (IR=0, HCFS operational) keeps running its
     schedule until phase 2 resets it (same as before beta.110).
  4) pci_d0: D3hot->D0 without No_Soft_Reset may clear the BARs; mem_bar() is read after it, so a lost BAR shows as
     an unusable controller rather than a hang - fine, maybe log it.
  5) USB MIDI packer (USBPAD_MidiByte): running status, SysEx CIN 4/5/6/7, system common CIN 2/3/5, real time CIN F
     all per USB-MIDI 1.0. Minor: SysEx cut by a status byte right after a full 3-byte piece sends no end packet.

## Fast test harness (sc55-pack/testrig/common, 2026-11-13)
- `restore.sh NAME` pulls store-NAME from deon81997/priavte and unpacks it (qemu -> /tmp/mq, 86rig -> /home/claude/86rig
  + 86box; win98 = both halves; skips if already restored, -f forces). Needs `apt-get install zstd qemu-system-x86 mtools
  xvfb librtmidi6 imagemagick`.
- `run.sh [-e qemu|86box] [-t SEC] [-r] [-p FILE] [-s OUT.png] IMAGE BATCH` copies IMAGE (original untouched), hooks
  BATCH into AUTOEXEC.BAT (-r: AUTOEXEC = only the batch), boots headless, waits for C:\RESULT.LOG with a PASS/FAIL
  line LAST, prints only that log, kills the VM. Exit 0 PASS / 1 FAIL / 124 timeout. `shot.sh OUT.png` = screenshot,
  only when asked (also `run.sh -s`).
- Tested: FreeDOS /tmp/mq/c.img and MS-DOS 7.1 /home/claude/86rig/dos.img, PASS and FAIL batches: QEMU ~4-5 s,
  86Box (Celeron 366, 86rig cfg; disk geometry set from the image size) ~13 s.

## beta.110 (2026-11-13, DOS session): freeze with two USB 2.0 hosts (P5PE-VM: ICH5 + NEC uPD720101 card)
- Report: hard freeze while SC55.BAT starts SBEMU's USB with the ICH5 (4 UHCI + EHCI 24DD) and a Belkin F5U219 (NEC
  EHCI 1033:00E0 + 2 OHCI 1033:0035, no option ROM) both on; Win98 fine; board alone worked; card alone untested.
- QEMU repro rig: testrig/usbmulti.py (ICH9 EHCI + 3 UHCI companions, ich9-usb-ehci2 + 2 pci-ohci companions -
  QEMU's usb-ehci takes no companions; USB keyboard + stick on one, mouse + stick on the other; pm = HIMEM.SYS from
  the 86Box rig's Win98 image, jem = JEMMEX + JLOAD QPIEMU from JemmB_v586). beta.109 does NOT freeze there (QEMU has
  no BIOS SMI code), both modes, with and without /USBDISK:1. So the fix is from the audit:
- Audit (usbpad.c): every register poll already had a bound (tsc or a counter) - no software endless loop. Found:
  (1) each controller was taken from the BIOS only when its own setup ran, in type order (EHCI before its UHCI
  companions): the BIOS's SMI code could still run the companions while CONFIGFLAG / port owner changed on the EHCI,
  and with the card in, a second EHCI did the same; (2) a timeout (BIOS handoff, HCHalted, HCRESET, OHCI HCR) did not
  drop the controller, it just went on with it; (3) no D3 -> D0; (4) EHCI USBSTS / OHCI status left pending (the NEC
  chip has no INTx-disable bit, so a shared IRQ line could be held); (5) the trace stopped going to SBDETAIL once its
  4 KB buffer was full; (6) a 17th controller was dropped silently. No USB IRQ is used (polled), controller list 16.
- Fix: phase 1 for ALL controllers in PCI order (as Linux's early quirks; on Intel the UHCIs come before their EHCI):
  power D0, UHCI LEGSUP 8F00 + stop + INTx off, OHCI ownership change + all interrupts off + status cleared, EHCI
  USBLEGSUP handoff (BIOS not letting go in 1 s: taken anyway, as Linux, logged) + stop + USBINTR 0 + USBSTS cleared.
  Phase 2 = the old setup order. Any wait that times out logs "[usb] bb:dd.f TYPE: <step> timed out, controller
  skipped" and that controller is not used. Every step is logged BEFORE it runs ("[usb] ..." lines in SBDETAIL.TXT,
  committed per line; trace buffer 8 KB, full buffer still feeds SBDETAIL).
- Switches: /USB:0 (SBEMU touches no USB controller; also turns /MUSB off), /USBSKIP:<list> ("vvvv:dddd" or "bb:dd.f",
  joined with + because a batch %1 splits at commas); skipped = not touched at all (no bus master). USBTEST / MIDPLAY
  /USB: SET USBSKIP=... . README.TXT, HWTEST.TXT (P5PE-VM item first).
- Tests (QEMU usbmulti.py): pm and jem, default and /USBDISK:1 (4 drive letters), /USBSKIP:8086:293c+00:06.0 (both
  "skipped", nothing else touched), /USB:0: all reach DONE, same devices as beta.109. USB start-up step time the
  same within QEMU noise (fsync per line, about 14 more lines).
- Speed (86Box P2B-LS PII-450 ES1371, midi/perf.sh, SBEMU /JOY:1 /VMPU:1, no ROMs): beta.109 344 int/s avg 0.02 ms
  ISR 1% = beta.110 identical. SC55LOG.EXE md5 8cc1f00a.. identical (no USB code in it). Runtime path unchanged
  (the change is start-up only). 86Box with automation rebuilt from testrig/86box/README.md into /home/claude/86box/86box.
- Not tested (needs the P5PE-VM): whether phase 1 is the fix; HWTEST.TXT has the steps (SBDETAIL.TXT, then
  /USBSKIP:1033:00E0, /USB:0). If it still freezes, the last "[usb]" line names the step.
- Zip: beta.109 + all built tools (version), README.TXT, HWTEST.TXT. Personal pack: not in this session.

## beta.108 (2026-11-13, DOS session): USB MIDI (class compliant, e.g. M-Audio Uno) - SBEMU /MUSB, MIDPLAY /USB, USBTEST
- usbpad.c: audio class / MIDIStreaming interface (alt 0) -> two pipes, PT_MOUT (bulk OUT) and PT_MIDI (bulk IN), in the
  periodic schedule like the pads (UHCI frame list QH, OHCI interrupt ED (D=OUT for the OUT pipe, oslow path shared),
  EHCI periodic QH (split for FS behind a TT), xHCI bulk endpoint contexts type 2/6). Flag USBPAD_F_MIDI; one device.
  USBPAD_MidiByte: MPU byte stream -> 4-byte USB-MIDI packets (running status, SysEx 3-byte pieces CIN 4/5/6/7, real time
  CIN F anywhere, system common CIN 2/3) into a 1024-packet FIFO, no USB access (safe in a port trap); dropped while no
  device. mout_poll never waits: a transfer of up to 256 bytes (64 events; UHCI: one TD per packet, depth first, 16 max)
  started by one poll, collected by the next. Stall: CLEAR_FEATURE on the next full poll. IN: packets -> USBPAD_SetMidiIn
  callback. OUT pipe obuf = its ibuf area (256 bytes up to the xHCI ring). ILEN(): IN transfers 64 bytes even with
  512-byte HS endpoints. Shutdown flushes the FIFO (1 s max). QEMU's OHCI (106B): MIDI IN pipe not used (one transfer in
  flight per controller there + shared buffer: OUT bytes came back as IN).
- SBEMU: /MUSB:1 (needs /JOY:1): MAIN_MPU_330 out -> USBPAD_MidiByte; MPU trap installed for it; turns /VMPU (launchers)
  and the SC-55 auto-load off. SETTINGS: MIDI synth "USB MIDI device" (writes /MUSB:1). MENU status: "USB MIDI device".
- MIDPLAY /USB: links usbpad.c (makefile MIDPLAY_USB_SRC, -DMIDPLAY_USB), drives the controllers itself (no SBEMU, PS/2
  keyboard: no USB keyboard there); no LCD queries then. USBTEST: lists it, test note (C4) when found, N again, MIDI IN shown.
- Tests (testrig/midi/README.md, testrig/fakemidi.py = usbredir USB-MIDI device, QEMU 8.2 from Ubuntu): UHCI, OHCI, EHCI +
  UHCI companions, EHCI HS (no companions), xHCI FS and HS: USBTEST note + MIDI IN; SBEMU /MUSB + MIDPLAY 35 s with
  device_del/device_add at 30 s: event streams identical on all six, music resumes after the re-plug; MIDPLAY /USB; 360
  pad + MIDI on one bus (UHCI, xHCI) both work. Parser host test: 190000 random bytes round-trip. MENU menurun.sh quick PASS.
- Speed (86Box P2B-LS PII-450 ES1371, testrig/midi/perf.sh, SBEMU /JOY:1 /VMPU:1 without ROMs, MIDPLAY 40 s): SBDIAG
  beta.107 344 int/s, avg 0.02 ms, ISR 1% = beta.108 identical. SC55LOG.EXE byte-identical to beta.107 (md5 2cd851ff..):
  speed unchanged. No ROMs in this session: SC-55 render load not measured. /MUSB itself is off by default (SBDIAG cannot
  measure without the VMPU side channel).
- Not tested (needs the real PCs): the Uno on the D810 (ICH6) and on the NEC card (oslow OUT path: QEMU asserts with
  -DOSLOWFORCE + usbredir async bulk); real MIDI IN. HWTEST.TXT has the items.
- Zip: beta.107 + SBEMU MIDPLAY USBTEST SBSET SC55MENU GAMETEST JOYTEST KMTEST MPUTEST SBDIAG TIMETEST USBDRV PADCONV
  SC55WAV SC55LOG CDGUI (version / tui.h / USB), README.TXT, HWTEST.TXT. Includes the MENU bug-hunt fixes below (tree
  builds, menurun.sh quick PASS; its menuwalk / 3D / Win98 checks are still open). Personal pack: not in this session.

## MENU bug hunt (2026-11-13, DOS session) - fixes released in beta.108 (see above); the open checks below stay open
Stopped by the user mid-task. Tree builds; tests/m3host/menurun.sh and run.sh pass.
- Done (tools/sc55menu.c, menu3d.h, tui.h):
  - New host test tests/m3host/menuasan.c + menurun.sh [quick|fit|ed|shots|seed N]: whole MENU on the fake DJGPP (fake disk
    /tmp/menufs/C = C:\, fake MPU-401 for "SBEMU loaded", scripted keys + INT 33h mouse, DUMP/SHOT keys), ASan+UBSan:
    every entry x text/Panels/Carousel/Module x SBEMU off/on (MENUGO.BAT checked), game editor + browser, first start,
    MENUVIEW, safe mode, no VESA, no mouse, every F4 mode + F7 fonts, 60 random key/mouse runs. dosstub.h: HS_HOOKS.
  - Bugs fixed: (1) file browser had no Home/End; (2) Tab drive from A:/B: computed a bad letter; (3) game editor showed
    only 14 of up to 24 games (scrolls now, "full (24 games)"); (4) tui_yesno/tui_message sized lines by length, so word
    wrap cut text ("..."), e.g. the first-start question - now tui_wrap_lines; (5) 3D: descriptions cut (Carousel/Module 3
    lines, Panels 4): now 4 / 5 lines, CD images + game texts shortened (menurun.sh "fit" checks every item); (6) 3D: grey
    items gave no reason (text menu does) - m3_reason (overlay / in the Panels row); (7) 3D had no F1 help / F5 MIDPLAY;
    (8) 3D start without a VESA mode: no message, MENU3D.CFG kept; (9) 3D MENUGO.BAT write error silent (g_goerr);
    (10) Module LCD cache stale after the game editor (g_menugen). Proof: sc55-pack/testrig/menuq/3d-reasons-4lines.png.
  - New QEMU rig: testrig/menuq.py (MS-DOS 7.1 or FreeDOS disk, pack installed by its own SETUP.BAT, SoundFont, OHCI USB
    kbd/mouse; text screen via pmemsave) + testrig/menuwalk.py (starts every entry, gets back). MS-DOS 7.1 boot + MENU +
    Start SC-55 (SoundFont, USB kbd/mouse on OHCI) + return: OK (testrig/menuq/msdos71-menu.png). Full walk was running.
- Session 2 (f3a7f28+): menuwalk fixed (menuq.text() decoded CP437 00-1F as control chars, so ► never matched; print
  format bug) and run on MS-DOS 7.1 + FreeDOS: every entry OK; grey ones show their reason; JEMM/QEMM/Auto setup OK
  from a fresh boot. Fixed: BUGREP.BAT without MEM/FIND ("Bad command"): finds them on PATH/C:\DOS/WINDOWS\COMMAND/
  FDOS, else notes it. USBTEST: after it a BIOS-driven USB kbd/mouse is dead until restart (user stuck in MENU's
  USBTEST.TXT view) -> on exit with a USB kbd/mouse it asks "Restart the PC now? Y/N" (Y: SMARTDRV commit, KBC reset;
  QEMU: back in MENU, keys work). usb3d soak 600 s with new MENU/MIDPLAY/SBEMU: 23 clean exits, no CRASH.
- NOT done: Win98 DOS box (QI VM installs fine from win98qi stock ISO; first boot from disk sat >10 min on the splash
  - retry without -usb, alone on the CPU; /home/claude/w98vm qi0.img = fresh install), 86Box PII-450 perf, release
  beta.108 (zip: SC55MENU, USBTEST, BUGREP.BAT + tui.h users), README news (BUGREP, USBTEST restart, MENU fixes).
- Closed (session 3): all of the above shipped in beta.110/111; beta.111 did the Win98 DOS-box MENU test and the
  full walk. Re-checked on beta.110's USB init: USBTEST restart prompt still works (QEMU, Y -> MENU, keys OK). No perf
  run needed (SBEMU runtime untouched). Win98 QI VM note: a mkdisk FAT16 D: disk at first boot hangs IO.SYS (black
  cursor) - attach D: only after hardware setup; the 2nd boot after setup also sat at "continuing to load Windows".
- Resume: DJGPP + build per docs/SESSION-2026-11-13-DOS.md; `sh sc55-pack/tests/m3host/menurun.sh quick`;
  `apt-get install qemu-system-x86`; unzip release zip to /home/claude/rel; MS-DOS files: see Test downloads;
  `python3 -I sc55-pack/testrig/menuq.py /home/claude/mq/ms build msdos`, then `from menuq import Q; Q(dir).start(usb='ohci')`.
- Open: none confirmed beyond the list above; Win98 DOS box untested (LFN: menu sets LFN=n, browser uses 8.3 names).

### Test downloads (kept outside the repo, /tmp/dl/<name>)
- rufus `src/ms-sys/inc` (mkdisk.py boot records): `git clone --depth 1 --filter=blob:none --sparse https://github.com/pbatard/rufus /tmp/dl/rufus` + `sparse-checkout set src/ms-sys/inc`, symlink testrig/rufus.
- Windows 98 QuickInstall v1.1.0 `win98qi_v1.1.0_stock.iso`: https://github.com/oerg866/win98-quickinstall/releases/download/v1.1.0/win98qi_v1.1.0_stock.iso
  (curl works; gh release download is blocked). MS-DOS 7.1: `isols.py ISO OSROOTS/1/FULL.866 FULL.866`, then
  `mercy.py FULL.866 ms71 IO.SYS MSDOS.SYS COMMAND.COM` (menuq.py writes its own MSDOS.SYS, BootGUI=0).
- TimGM6mb.sf2 (GPL, 6 MB test SoundFont): https://raw.githubusercontent.com/craffel/pretty-midi/main/pretty_midi/TimGM6mb.sf2
- FreeDOS KERNEL.SYS/COMMAND.COM: git clone --depth 1 https://github.com/pbatard/rufus (res/freedos) -> /home/claude/fd
- DJGPP gcc 12.2: andrewwutw/build-djgpp v3.4; 86Box @0935207 + 86Box/roms (built OK with testrig/86box/README.md cmake line).
- User uploads this session (not redistributable, not in repo): sc55.sf2, SC-55.sf2, CT2MGM/CT4MGM/CT8MGM, DOGOS2, Pipalun,
  Saron, Tabla .SF2 - for SoundFont tests in the next session.

## beta.107 (2026-11-13, DOS session): Win98 DOS box + MS-DOS 7.1 USB letters tested; 320x200 3D title fix
- Source of Win98 / MS-DOS 7.1: public QuickInstall reference ISO (github.com/oerg866/win98-quickinstall releases v1.1.0,
  win98qi_v1.1.0_ALL.iso, 1.3 GB; OSROOTS/1/FULL.866 -> testrig/padkeys/isols.py + mercy.py for IO.SYS/MSDOS.SYS/COMMAND.COM).
  Win98 VM: testrig/mkfat32.py empty 1024 MB disk, boot the ISO with Enter spam, it installs 98SE (98Lite Micro) and
  reboots; then boot from disk (QEMU pc, pentium3, 128 MB, std-vga, sb16). Rufus headers needed: br_fat16_0x3e.h,
  br_fat32_0x0.h, br_fat32_0x52.h, br_fat32_0x3f0.h (testrig/rufus is gitignored).
- Win98 DOS box (MIDPLAY /X from a FAT16 D: disk via Start > Run): opens full screen; channel table, SC-55 panel, 3D (mode 13h)
  solid/vector/voxel all draw; Alt+Enter -> window: 3D, text, panel shown in the window; S back to 3D from a window switches
  Windows to full screen (Windows' choice); Esc quits to the desktop. VESA modes in a DOS box not tried.
- Bug found + fixed: 3D view at 320x200 - "name - title" ran into "Song 1/1" (midplay3d.h mp3_bar: title cut to the room left).
- MS-DOS 7.1 (MKDISK_BOOT=msdos, BootGUI=0) + beta.104 SBEMU /USBDISK:1, 3 sticks at boot: FAT16 D: (read, COPY exact,
  write CFG.TXT landed on the image), spare letters E:/F: for the exFAT/NTFS sticks ("Not ready" if used - expected),
  redirector G: NTFS, H:/I: exFAT (MBR / GPT): DIR, TYPE nested, 40-entry dir, COPY byte-exact x3, COPY/MD refused.
  MS-DOS 6.22: not available (Win98 ISO has 7.1 only) - stays on HWTEST.TXT.
- Zip: beta.106 + every exe carrying the version (GAMETEST JOYTEST KMTEST MIDPLAY MPUTEST SBDIAG SBEMU SC55MENU TIMETEST
  USBDRV CDGUI), HWTEST.TXT header. QEMU usb3d smoke (2 min) fine. Personal pack built from the user's beta.98 personal zip.

## beta.106 (2026-11-13): flight recorder (SBTRACE.TXT), per-program SB/MIDI command counts, BUGREP doctor
- sbemu/trace.c/.h: two RAM banks (ISR fills one); 64-event ring (same event within 2 s counts up), 6 program slots
  (current PSP from the DOS SDA +10h, name from its MCB; matched by PSP *and* name - DOS reuses PSPs). Hooks: sbemu.c
  command start (TRACE_Dsp), 22E/22F reads with INT_STS set (TRACE_IrqAck), MAIN_InvokeIRQ (TRACE_Irq: 16 raised, 0 acked
  -> event), render DMA addr 0 (TRACE_DmaCheck), MPU wrapper MAIN_MPU_T0/T1 around whatever handler is set (VMPU or
  hardware; MAIN_MPU_Wrap at both setups), vdiag.c DIAG_Midi/Fifo/TimerReplayed, usbpad.c plug/unplug, cdlink io_open.
- Written by TRACE_Idle from INT 28h (now always hooked) when InDOS=1, no critical error and the current PSP is the
  shell (own parent / COMMAND / 4DOS / NDOS); or TRACE_Flush on MPU command A9h when InDOS=0 (SBDIAG FLUSH; BUGREP
  does it first, so the MENU workflow is covered). Appends to SBTRACE.TXT next to SBEMULOG (64 KB cap, then new);
  SBEMUTRACE=OFF / =path. Path is only set when SBEMULOG is set (SC55.BAT does).
- Bug found by the trace: unknown DSP commands left SBEMU_DSPCMD set, so every later byte was a parameter until a
  DSP reset. Now SBEMU_DSP_Known() + parameter-byte table (24h 2, 0Eh 2, 0Fh/38h 1, Bxh/Cxh record 3, others 0)
  skip them; 80h = 2 bytes then IRQ (SBEMU_DSPCMD_SILENCE); B2h/C2h map to B0h/C0h.
- BUGREP doctor: tools/doctor.h in SBDIAG (`SBDIAG DOCTOR folder body out`): <=5 plain-English lines from SBLOG
  (no IRQs, Error/Failed, VMPU warning, Windows), ROMS empty, BLASTER unset, SBTRACE (IRQ never acked, DMA not set up,
  NOT SUPPORTED DSP, intelligent MPU, MIDI dropped, late renders), GAMETEST FAIL; then versions of every EXE/COM/SYS
  in the folder ("NSBVER <ver>" marker: makefile PACKVER forces tools/packver.h into 11 tool links; SBEMU/MENU have
  the version string). BUGREP.BAT writes the body to BUGREP.$$$, doctor puts summary on top.
- Tests: TRTEST (a small DJGPP program: E1, D1, 40, 80, 24, 20x F2 without ack, 14 without DMA, MPU FF/3F/AC, notes,
  SysEx, pitch bend). QEMU (FreeDOS + JEMMEX + QPIEMU + HDPMI32I + SBEMU on AC97, usb-kbd hot-plug via device_add) and
  86Box P2B-LS ES1371: SBTRACE + BUGREP summary as expected. Host doctor test with sample files (/tmp/dr).
- Zip: SBEMU, SC55MENU, SBDIAG, MIDPLAY, GAMETEST, MPUTEST, TIMETEST, JOYTEST, KMTEST, USBTEST, USBDRV, PADCONV,
  SC55WAV, SC55LOG, CDSET/CDGUI, BUGREP.BAT, README.TXT. SBSET.EXE still not swapped (see beta.100).
- Real hardware to check: SBTRACE.TXT after a few games on the D810 and P5PE-VM; BUGREP from MENU.

## beta.105 (2026-11-13, DOS session): MIDPLAY Options > "MPU-401 port"
- tools/midplay.c: g_mpuport (0 = Auto: mpu_find = /P, BLASTER P=, search), Options row 13 cycles Auto / 300..360,
  only ports that answer (mpu_probe from mpuport.h, instant on an empty port) - silent ones are skipped, wraps to Auto.
  Switch is live: silence() on the old port, UART on the new. Saved as MPU=<hex> in MIDPLAY.CFG; /P: on the command
  line still wins for that run. 86Box: SBEMU at 330 -> Right = 330h (300-320 skipped), Right = Auto, Left = 330h.
- Zip = beta.104 zip + FILES/SC55/MIDPLAY.EXE + README.TXT.

## beta.104 (2026-11-13, DOS session): emulator-coverable "NOT tested" items closed; HWTEST.TXT for the real PCs
- exFAT / NTFS through the release build (beta.103 SBEMU, FreeDOS, QEMU EHCI, testrig/qemu-usbdrive.sh + new testrig/mkxn.sh
  images): stick 1 MBR NTFS + exFAT (F: G:), stick 2 GPT exFAT (H:); DIR, long-name aliases, nested TYPE, 40-entry dir,
  300 KB COPY from all three byte-exact, COPY to / DEL / MD refused and volumes unchanged. Spare FAT letters D:/E: listed
  as "no FAT partition". PASS. Note: NTFS times show UTC (Windows stores UTC; DOS has no zone) - left as is.
- 86Box (testrig/86box/run86.py VMDIR OUT): Pentium 133 (P/I-P55TP4XE) and Celeron (Mendocino) 300 (Acorp 6VIA90AP), ViRGE/DX,
  ES1371, 64 MB: SBEMU loads; MIDPLAY text, panel, 3D, 3 looks x 4 players, themes cycled, quits; MENU text, 3D, styles
  (Panels/Carousel/Module), back to text; no CRASH.TXT. 30/30 PASS each. "Few frames" hint shown in 3D on both (expected).
- SC-55 ROM sets + MT-32 in notetest: done by the other session (see its 063ee1f note: 75 runs, 0 dropped).
- NOT coverable here (need the user's files): MIDPLAY in a Win98 DOS box (no Win98 image), MS-DOS 6.22 / 7.1 with spare
  USB letters (no IO.SYS/MSDOS.SYS). Both are in HWTEST.TXT; with the user's QuickInstall ISO (testrig/padkeys/isols.py)
  a later session can do them in QEMU.
- MENU shows "SBEMU not loaded" when no SC-55 ROMs / SoundFont are present (mpu_find needs SBEMU's synth) - by design?
- HWTEST.TXT (pack/HWTEST.TXT -> NUKEDSB\HWTEST.TXT): one checklist per PC (D810, P5PE-VM) + both.
- Zip: beta.103 + SBEMU.EXE, SC55MENU.EXE (version) swapped, HWTEST.TXT added.


## beta.103 (2026-11-13): MIDPLAY "out of memory" for songs on 8-12 MB PCs fixed
- Cause: at start MIDPLAY prepared the 3D frame + depth buffers (640x480: 2.4 MB, more with DJGPP malloc rounding)
  even in the text views; at 8 MB only ~0.8 MB DPMI memory was left, the 140 KB stress song (35570 events) failed.
- Fix: mp3_prepare only when the 3D view is on at start; start_index: song "out of memory" -> to_text(),
  mp3_free_buffers(), load again; the 3D view then comes back through the low-memory fallback (smaller mode).
  ui_log records failed songs and "3D memory freed"; CRASH.TXT also shows DPMI free memory.
- Tested 8 MB (QEMU and 86Box P2B-LS/ViRGE): text view plays (1.7 MB left); 3D view plays, 3D at 320x200.
  ASan host test still clean. Zip: MIDPLAY, SBEMU, SC55MENU (version) swapped.

## beta.102 (2026-11-13, DOS session): USB keyboard/mouse bytes enter the 8042 at the END of the sound interrupt (vkbc.c, main.c)
- Was "Known issue": P5PE-VM, Logitech Unifying 046d:c52b on the NEC card's OHCI ("ED rewrite per transfer" mode): MENU 3D GPF
  on F2/F3, MIDPLAY 3D "Stack Fault", both at harmless instructions; PS/2 keyboard fine.
- NOT reproduced in QEMU: testrig/usb3d (pci-ohci, and NEC-like EHCI + 2 OHCI companions with SBEMU built -DOSLOWFORCE),
  30 min each cycling every look/theme/player and MENU style (on beta.99 sources + this fix), 71 normal program exits each,
  no crash (also none before the fix). 5 min again on beta.101 sources: fine.
- Audit: every async entry already keeps the program's FPU (sound IRQ PM/RM: MAIN_FPU_ENTER; PM traps hdpmipt.c; RM traps
  qemm.c); no x87 before those saves (objdump of non-LTO objects). Stack (gcc -fcallgraph-info=su): sound IRQ ~7.8 KB of 32 KB
  (go32 wrapper / RMCB stacks), trap handlers < 2.5 KB of 16 KB. Instrumented run (counters at 0040:00E0-00FF): no nested PM
  trap, IF never 1 inside PM/RM trap handlers. Note: our HDPMI32I keeps a trapped frame in ONE host buffer (?USE_HOST_MEM=1
  in I2FHDPMI.ASM), so a nested PM trap would return to the wrong CS:EIP/SS:ESP - exactly this crash pattern; not seen here.
- Change: VKBC_Tick (inside MAIN_Interrupt, interrupts ON) no longer puts the byte into the 8042; VKBC_Deliver() does, at the
  end of MAIN_InterruptPM/RM after EOI + CD service with interrupts off. IRQ 1/12 now come when the CPU is back in the
  program (like a PS/2 keyboard) instead of running the program's/BIOS's/mouse driver's handlers nested inside SBEMU.
  Keys/mouse checked over USB in QEMU (trace usb_ohci_td_pkt_short shows the reports).
- Release: beta.101 zip with SBEMU.EXE and SC55MENU.EXE (version) swapped. Personal pack not rebuilt (user's personal zip
  not in this session).
- Needs the real P5PE-VM: Unifying receiver on the NEC OHCI, MENU 3D F2/F3 and MIDPLAY 3D V/C/J (MIDPLAY's beta.100 crash
  catcher writes CRASH.TXT). If it still crashes: CRASH.TXT / the HDPMI exception dump (CS:EIP, SS:ESP, error code) and the
  SBEMU log; next step would be an HDPMI32I with ?USE_HOST_MEM=0.

## README screenshots + art (2026-11-13, PARTLY DONE - user paused at usage limit)
- Retaken from beta.109 in QEMU (rig: /home/claude/shots: c/ = pack installed like SETUP + ROMs + test MIDI/MUS/HMP/XMI/KAR
  + CD images; start.sh = QEMU pentium3, AC97, pci-ohci + testrig/fakepad.py live pad; shot.py = monitor sendkey/screendump.
  NOTE: SeaBIOS doubles grey arrow keys): joytest, main-menu (Sunset), menu-3d-panels/carousel/module, settings,
  settings-3d, automatic-setup, midplay-channels (key+2, A-B, sleep shown)/panel/playlist/files/options/3d/3d-looks.
- STILL OLD: cd-images-groovy/classic/midnight, cd-images-3d, cd-files, gametest + gametest-3d, sbdiag, timetest,
  usbdrv (ok, 11-12), win98-settings.png + w98 shots (need the Win98 VM), padkeys game shots (fine).
- All version-stamped tools in the beta.109 zip rebuilt (same source, now say beta.109; SBSET also gets the low-pass).
  SC55WAV/SC55LOG not swapped. sc55synth.o's PGO profile is stale (core profiles still used).
- TODO from the user: a section listing everything brand new (now "brand new" only links to What's NEW), and new
  90s art (make90s.py icons) for each new feature: USB MIDI row reuses icon-sc55; game music, A-B/key/sleep, low-pass,
  test bench have no art yet. Repo About description/topics: user pastes them (proxy refuses settings writes).

## beta.109 (2026-11-13): SC-55 anti-alias low-pass (/VSCLPF), offline renderer, ST "hum" explained
- sc55/sc55engine.hpp: 3 biquads (6th order Butterworth, fc = 0.42 x card rate) on the native frames before the linear
  resampler, only when the card rate < native; integer DF1, Q28 coefficients, 8 extra fraction bits (no x87 denormal
  slowdowns). set_lowpass(); SC55_SetLowpass; VMPU_SetSC55Lowpass; SBEMU option /VSCLPF:0|1 (OPT_VSC_LPF, default 1,
  also changeable on a resident SBEMU). NOT in SBSET (SETTINGS) yet.
- tests/engine_test.cpp: test_antialias_lowpass (both native rates x 11025..48000): 1.5x Nyquist -32..-84 dB, 2x Nyquist
  -52..-inf dB, in band -0.15..0 dB, chunk invariance; the exact-arithmetic tests run with set_lowpass(false).
  Cost: ~4% on top of the SC-55 (offline render 6.3 -> 6.6 s); QEMU "needs" 218 -> 224%.
- tests/sc55render.cpp (make -f tests/Makefile.host out/sc55render): MIDPLAY host byte list -> WAV through SBEMU's real
  SC-55 path, any ROM set / rate / filter, as fast as the host. Use THIS to judge sound, not QEMU recordings.
- CORRECTION to the recording section below: the ST "hum" in QEMU was NOT aliasing. (1) QEMU TCG renders every SC-55
  set too slowly (needs 106-265%, only CM-300 < 100%), so SBEMU's render-ahead runs dry and pads silence every 128-frame
  block; (2) the SC-55ST (st-v1.01) idles with a real 517 Hz tone + harmonics at -36 dBFS (also in upstream
  nuked-sc55-render, so it is the core or the ST ROM, not SBEMU). Chopped by (1) it became the out/128 Hz hum. The
  filter does not change (2) (in band). Open: report the ST idle tone upstream / compare with a real SC-55ST.
- Zip = beta.108 zip + SBEMU.EXE (1.0.0-beta.109) + README.TXT (/VSCLPF in the option list). To test on real PCs:
  SC-55 at 22050 with /VSCLPF:1 vs :0 (whistles on high notes / cymbals), CPU headroom on the D810 / Celeron.

## MIDPLAY sound recordings per ROM set x rate (2026-11-13, done)
- notetest.sh: WAV=file (QEMU HDA; AC97 records only silence), SND, WARM / TAIL (silent songs: in QEMU the SC-55 starts
  up and renders late), SCREEN=1 (SBEMU's synth / ROM-set line from VGA text memory via the QEMU monitor). Test songs
  testrig/midplay/METRO.MID (20 piano notes 1 s apart) and PAD.MID (string chord held 10 s), both on channel 1.
- wavcheck.py [--metro N]: sound, level, peak, clipping, dropouts (= sound cut off loud -> nothing within 10 ms, before
  the last loud part), clicks, beats (10 ms resolution, 0.5 s debounce, +-30 ms).
- Matrix (75 runs, 2 QEMUs at once): every run 0 bytes dropped and the ROM set confirmed on screen. Findings:
  1. SC-55ST (st-v1.01): a constant tone under everything. Upstream nuked-sc55-render (built in /tmp/nb, needs
     librtmidi-dev libsdl2-dev) does the same at its native 66207 Hz: the tone is ~17 kHz / 32 kHz there (core / ROM).
     SBEMU's LINEAR resampler (sc55/sc55engine.hpp) folds it to an AUDIBLE 170-600 Hz hum (pitch changes with /K).
     => SBEMU needs an anti-alias low-pass before decimating (also helps every set's highs). NOT done.
  2. MT-32 / CM-32L silent: test songs on channel 1 (MT-32 parts 2-9, rhythm 10). On channel 2 (METRO2/PAD2): 20/20
     beats 0.99-1.01 s, no dropouts. Not a bug.
  3. JV-880 silent: upstream renderer silent with METRO.MID too (JV-880 is not GM/GS). Not ours; untested further.
  4. SC-55 family: beats 20/20 everywhere, jitter up to +-50 ms (SoundFont +-40, MT-32 +-10: probably /VSCAHEAD 50 ms
     render blocks); 0-20 short gaps (10-20 ms silence) with 2 QEMUs, 1 when mk1-v1.20 /K11025 ran alone -> mostly QEMU
     load, not decided for real PCs.
- Listening MP3s (44100, +20 dB): sent to the user as recordings-beta101.zip (not in the repo).

## beta.101 (2026-11-13): MIDPLAY - MUS/HMP/XMI, transpose, A-B loop, sleep timer, Ctrl+F search
User asked for: karaoke lyrics (DROPPED by the user mid-task - not needed), mute/solo, tempo + pitch, A-B loop,
MUS/HMP/XMI/RMI, M3U, search, sleep timer. Already there before: mute/solo (M/O/U), tempo ([ ]), M3U load/save,
type-to-jump. Files: tools/midplay.c only (+ pack/README.TXT section 8, testrig/midplay/fmtcheck.py, hostsweep.sh).
- Formats (song_load sniffs the contents, not the name): MUS (load_mus: 140 Hz = div 70 @ 500000; MUS ch 15 -> 10,
  others in order of first use skipping 10, CC123 on first use like Doom's mus2mid), HMP (load_hmp: tracks at 308h /
  388h for "013195", count at 30h, division at 38h, quarter = 1 s, HMP varlen = low group first, last byte bit 7 set,
  also for meta/SysEx lengths - layout as ZMusic midisource_hmi.cpp), XMI (load_xmi: first EVNT via iff_find, delays =
  summed bytes < 80h, note-on + VLQ length -> generated note-off (min 1 tick), div 60 @ 500000 = 120 Hz, tempo meta
  ignored), RMI (riff_midi walks the chunks to "data"). CC 110-119 (AIL/HMI loop+lock marks) dropped for HMP/XMI.
  song_info uses song_load for MUS/HMP/XMI. Extensions .smf .mus .hmp .xmi added; .m3u8; M3U skips a UTF-8 BOM and
  trailing blanks. KAR titles: "@T" text, track names "Soft Karaoke"/"Words" no longer become the title (meta_title).
  NOT supported: HMI ("HMI-MIDISONG"), XMI files with several songs (first only), MIDI format 2.
- Transpose: g_pitch -12..+12 ({ } \, PITCH= in MIDPLAY.CFG), channel 10 untouched; g_tmap[ch][note] remembers the note
  sent so a key change while notes sound ends the right ones (silence() clears it).
- A-B loop (A key, ab_key/ab_check): mp_pump sends nothing at/after B; the main loop seeks to A. Cleared by a new song.
- Sleep timer (Z, sleep_key): off, end of song (next_index not taken), 15/30/45/60/90/120 min -> mp_pause (SPACE plays on;
  /S quits). Shown as "Zz:end" / "Zz14m" in the right-aligned mode flags on row 3 (with key+n, A-B, speed...).
- Search: Ctrl+F on Playlist / Files (F or Ctrl+F on the Player page opens the Playlist search): substring of file name
  or title, "k of n", Ctrl+F / F6 next, ENTER plays, ESC closes (srch_key runs first in handle_key, so SPACE types).
- Tests: testrig/midplay/hostsweep.sh [FUZZ] = ASan+UBSan host build: --queue, fmtcheck.py (one tune as MID/KAR/RMI/
  MUS/HMP v1+v2/XMI -> identical note-on/off lists and times; transpose incl. changes mid-song = no stuck notes; A-B
  counts; sleep), busy genmid song with options, FUZZ damaged copies of every format (0 problems in 2400). Host main
  takes --pitch n --pitchat ms n --ab a b loops --sleep s --info. Also run: tests/m3host/run.sh (4101 frames, 0
  failures) and notetest.sh "/VSF:C:\SC55\GM.SF2 /VSFVOICES:24" on all formats + busy song: 0 bytes dropped (note:
  plain /VSF without a path = SC-55 without ROMs = everything "dropped").
- ROM x rate matrix (user's personal zip ROMs, notetest, 10 s each of T.MID/T.MUS/T.HMP/T.XMI/BUSY.MID): every complete
  set found by hash - mk1 v1.00/1.10/1.20/1.21/2.00, mk2 v1.01, st v1.01, sc155 rev1, sc155mk2 v1.01, cm300 v1.30, jv880
  v1.0.0/1.0.1 (/VSCSET:name) + /VMT32:MT32 + /VMT32:CM32L + SoundFont, each at /K11025, 22050, 32000, 44100, 48000:
  75 runs, 0 bytes dropped (SoundFont runs show "busy" = MIDPLAY waited, nothing lost). Incomplete in the zip: cm300
  v1.10/1.20, scb55, rlp3237. SBEMU's ROM set confirmed on screen (QEMU screendump) for mk2. QEMU's wav audiodev
  recorded only silence, so the sound itself was not checked; SBDIAG's synth section reads 0 in QEMU.
- Not tested: the new keys interactively in QEMU/86Box (only through the host build), real game files (made-up test
  files only - try Doom .MUS, Descent .HMP, Ultima/Dune II .XMI), real hardware (D810, P5PE).
- Zip = beta.100 zip + FILES/SC55/MIDPLAY.EXE + README.TXT.

## beta.100 (2026-11-13): MIDPLAY 3D made stable (crash catcher, ASan sweep, low-memory fallback)
- Crash catcher (tools/midplay.c, crash_handler): SIGSEGV/SIGFPE/SIGILL/SIGABRT/SIGNOFP -> text mode 3, CRASH.TXT in
  MIDPLAY's folder (signal, CPU exception number, registers from __djgpp_exception_state_ptr, text/3D mode, look/theme/
  player, song, last 50 UI actions from ui_log: keys, 3D on/off/mode, songs), silence, _exit(3). EIP is from the stripped
  exe: rebuild the same source without -s and use addr2line. Test build: `-DCRASHKEY` makes '!' do a GPF.
- Host sanitizer test: `sh sc55-pack/tests/m3host/run.sh [quick|crash]` (needs gcc-13-multilib). Builds the whole
  midplay.c 32-bit against a fake DJGPP (tests/m3host/inc/dosstub.h: fake DOS memory, fake VBE BIOS with 44 modes
  320x200..1600x1200 incl. 321x201, 1366x768, 1599x1199 in 8/15/16/24/32 bit, fake LFB/bank window that aborts on any
  write past the screen), ASan+UBSan+float-cast-overflow. Covers every look x theme x player, F4 half, pause, mode
  switches, styles, long clock, long titles, OOM limits. ~4100 frames, 0 findings now.
- Found/fixed: m3_tri_raster float->int overflow near the camera (U/V/light/fog 16.16 and ys/xs) -> m3_fi() clamps
  (m3core.h, so MENU 3D, CDGUI, GAMETEST, SBSET too). Textures (MIDPLAY panel/keys/face, m3 floor) are static now.
- Out of memory: mp3_buffers(w,h) allocates BEFORE the mode is set (frees old first); mp3_try_smaller picks the
  biggest smaller mode (same depth first), message "Not enough memory..."; the OOM mode is not saved as last good.
- QEMU (FreeDOS, std-vga, HDPMI32I, MIDPLAY /X): 1600x1200x32 + 130 random V/C/J/F4/SPACE/seek/S keys, no crash;
  -m 12 -> fell back to 1280x800; CRASHKEY build wrote C:\CRASH.TXT (page fault, registers, actions). MENU 3D styles OK.
- Zip: MIDPLAY, SC55MENU, SBEMU (version), GAMETEST, CDSET/CDGUI.EXE, README.TXT swapped. SBSET.EXE NOT swapped (zip's
  copy is 130 KB smaller than a fresh build - built differently; it gets the m3_fi fix next time it is rebuilt).
- 86Box (built from source @0935207 + testrig diffs, P2B-LS PII-400, S3 ViRGE/DX 4 MB, VBE 1.2 BIOS, FreeDOS +
  HDPMI32I): auto 640x480, 1600x1200x8 (banked), 1280x1024x16, 1024x768x32, 320x200 with 25-60 random V/C/J/F4/SPACE
  keys each: no crash; 800x600x24 not offered -> auto. 8 MB RAM + 1600x1200 wanted -> 800x600 "(low memory)";
  CRASHKEY '!' -> CRASH.TXT (GPF 0Dh, banked mode, actions). Seen: at 8 MB (and QEMU 12 MB) the 140 KB genmid stress
  song does not load even in the text view - not looked into (not a 3D issue).
- Real hardware still to test: MIDPLAY 3D on the D810 and the P5PE-VM (big modes, CRASH.TXT if anything happens).

## SSF2T demo investigation (2026-11-10, unfinished, no release)
- **Cause 1, found and fixed (not shipped yet):** the pack's HDPMI32I (crazii fork, based on HDPMI 3.20) gives a GPF when Borland 32RTM runs `mov cr0,eax` in ring 3. HDPMI 3.21+ (Baron) emulates it. The exception text is invisible (direct video output), so HDPMI just waits for 'c'/'s' → it looks like a hang at MAKESWP.
  - Patch: `sc55-pack/third_party/hdpmi32fix/mov-cr0.diff` (against crazii/HX 43182b4).
  - Build: `build32.sh HXDIR OUTDIR` (jwasm + OpenWatcom wlink, no JWlink needed). Debug build: `VIO=0 build32.sh HX OUT -D?DOSOUTPUT=1` prints exceptions through DOS.
  - With the patch, MAKESWP runs OK when only HDPMI is loaded.
- **Cause 2, open:** with SBEMU resident (even plain `SBEMU.EXE`), MAKESWP prints "Swapfile required 0K" and then hangs on exit. The CPU ends up in real mode with TF set, running freed 32-bit code (32RTM teardown). Things tried without success: ignoring the cr0 write (same hang), `HDPMI=32` (crash). Next ideas:
  - 32RTM's INT 31h 0202/0203 exception handlers or IRQ-route cleanup in crazii's I2FHDPMI on client exit.
  - Compare with Baron v3.24 client-exit code.
- Official HDPMI32i 3.24 runs SSF2T fine but crashes our SBEMU (different trap API).
- Test roots: ~/mptest/rsf (SBEMU+game), rsfB (HDPMI only); tools ~/tools/lin.py (page-walk memory reader via 86Box automation).

## MIDPLAY with thousands of MIDI files (2026-11-10, sc55-pack/tools/midplay.c only)
Review for 2,000-10,000 files, deep folders, long names. No fixed-size song arrays were found (playlist, folder scan and browser already grew with realloc). Fixed:
- **Long names cut off:** browser entries were `char name[80]`, so a longer LFN name became a path that could not be opened (the file showed but did nothing). Names are now strdup'd at full length.
- **Slow folders:** scan_dir (/D, MIDPLAY's folder) and the file browser used readdir()+stat() per file (several DOS calls and a directory search each). They now use findfirst/findnext (name, attributes and size in one call).
- **O(n^2) "already in the playlist":** pl_find was a linear stricmp scan. It was called for every visible browser row on every frame, and once per file by ^A/^P (10k x 10k compares). It is now a case-insensitive hash, rebuilt lazily after remove/sort/move/clear (pl_dirty). Browser: getcwd() is called once per folder load (fl_cwd), not per row or file.
- **M3U lines over 299 chars** were split into two bad entries. The buffer is now 1024 and the rest of a longer line is skipped. Save buffer is 600.
- **Playlist number column:** `%3d` ran into the play mark at 10,000+ songs. It is now `%5d` at column 1.
- **Shuffle with many songs:** shuffle picked any song except the current one, so songs repeated long before every song had played. It now picks only songs not played yet (PE.played, combining two rand() calls for >32k), then refills; Repeat off stops when all songs have played.
- Not changed: F5 sort by title or length reads every unread song first (synchronously, music keeps playing via mp_pump). With 10k files that takes a while and shows no progress. The 3D view shows only "Song n/N" (no file list), so it was not affected.
- Built clean with DJGPP gcc 12.2 (`make output/midplay.exe`). **Not yet run in 86Box:** this workspace had no 86Box build or VM images. Next: make a FAT16 image with 10k generated .MID files in nested folders with long names, then check /D load time, browser scrolling, ^A, F5, and shuffle.
## CD image browser: any drive, last folder, home folder (2026-11-11, committed, not released yet)
- CDGUI (MENU > CD-ROM > CD images): opens any folder on any drive. D = next drive, Backspace = up,
  G = go home, F2 = make this folder home. CDGUI.CFG now keeps CUR= (last folder, any drive) and HOME=.
  The old DIR= line is still read. An image outside C:\CDIMAGES is mounted and stored in MOUNTED.FLG by its full path.
- MENU > Games > edit > CD image: the same kind of browser (Tab drive, G home, N none). GAMES.CFG cd field is now
  96 characters wide (a full path); MENUGO.BAT mounts full paths as they are.
- tools/crit24.h: crit_dir_ok() checks a drive or folder. CD drives are asked through INT 2Fh 150Bh/1505h first.
- **Bug found:** a DJGPP program doing access("M:\") on the EMPTY uCDD drive locks up HDPMI32I (host halt, IF=0).
  Also, a DJGPP program started straight after CDON.BAT in AUTOEXEC hung at startup in one test, while it worked
  when started through GUI.BAT. Not explained yet; it may be the cause of the "read error / init error" reports.
  Do not set the real-mode INT 24h vector under HDPMI32I: that locked it up too.
- Tested in 86Box (cdtest/rootN): D skips the empty M:; GAME.ISO mounted from C:\OTHER and its files listed on M:;
  F2 home and the last folder (CUR=M:) were saved.

## beta.92 (2026-11-11): empty CD drive freeze fixed in HDPMI32I; CD browser released
- **Cause:** DOS raises INT 24h ("General failure", e.g. reading the EMPTY uCDD drive M:). HDPMI's real-mode INT 24h
  callback calls segm2sel -> allocxsel, which does `push ds`/`pop ds`. At that point DS still held a REAL-MODE segment,
  so `pop ds` in ring 0 gave a GPF. The exception text is invisible (?VIODIROUT), so HDPMI just waits for a key.
  Found with a debug build (`VIO=0 build32.sh ... -D?DOSOUTPUT=1`) and the map file (fault at end of allocxsel,
  return address in segm2sel).
- **Fix:** load DS=_FLATSEL_ before the INT 24h translation (sc55-pack/third_party/hdpmi32fix/hdpmi32i-fixes.diff,
  together with the mov cr0 fix). Binary: third_party/hdpmi32fix/HDPMI32I.EXE (crazii/HX 43182b4 + both fixes,
  jwasm + OW wlink). It is also in the release zip.
- Tested in 86Box: a DJGPP program doing access("M:\") on the empty drive returns -1 (before: freeze), both without
  and with SBEMU /VMPU + SC-55. CD read test (CDCHK2) passes with CD music playing. MIDPLAY STRESS.MID runs in all 3 views.
- Note: our HDPMI is built from crazii HEAD, not the exact old v0.1-beta4fix2 binary (38646 bytes). Watch for regressions.
- Still open: SSF2T MAKESWP hangs on exit with SBEMU resident (see the SSF2T section).

## beta.99 (2026-11-13): MIDPLAY 3D looks, themes, players (midplay3d.h)
- Keys in the 3D view: V look (g_scene: 0 solid, 1 vector, 2 voxel), C theme (g_ctheme 0..10), J player (g_model 0..3);
  saved in MIDPLAY.CFG (SCENE3D, THEME3D, PLAYER3D). Themes table T[] in mp3_theme_apply: hue range, sky, grid, and the
  module colours (body, face, LCD, ink, accent); 7..10 = SC-55 / SC-88 / MT-32 / CM-32L styles (colours only, no logos).
- Players are box lists (mp3_model: kind 1 screen = LCD texture, 2 speaker/knob, 3 light strip, 4 platter); drawn solid
  (m3_box), as lines (mp3_wbox, Wu lines, afterglow = fb * 5/8 each frame, always full resolution) or as blocks.
- Shared: mp3_notes_scan (new notes from g_viz.nl jumps) -> note river (mp3_river), voxel drops, drum pulse
  (mp3_drum: camera nudge, vector grid); mp3_labels: the 3 loudest instruments (640+ wide only). Voxel: mp3_lod halves
  the block count when a frame takes > 90 ms.
- Tested in QEMU (FreeDOS, std-vga, SBEMU, a generated 90 s song with drums): every look x player x several themes, 640x480
  32-bit and 320x200 256 colours. Not tested on real hardware yet (speed of voxel on the Celeron).
- Ideas left: .KAR lyrics in 3D, a showcase mode (player/theme/look change per song), pitch bend in the river, camera
  choice, per-player LCD layouts. Open items: USB keyboard on the NEC OHCI crash (beta.102, real-PC test), 2-partition stick after
  Win98 MS-DOS mode on the real PC (beta.98 moves blocked letters), stale screenshots (GAMETEST, TIMETEST, 3D menus),
  rendertest branch to delete, a plain-HTTP mirror for Win98 downloads, modules (docs/MODULES.md not written yet).

## beta.98 (2026-11-12): USB letters refused after Win98 "Restart in MS-DOS mode"
- Symptom (user's P5PE-VM, NEC EHCI): D: E: "Invalid function", stub 0 calls, INT 21h 7302h CF ax 000Fh, 36h FFFF.
  F8 clean boot fine. Reproduced in QEMU (/home/claude/w98, stock.iso as CD): only when the stick was mounted in
  Windows first (device_add on the desktop), then Restart in MS-DOS mode: the letters Windows gave the stick are
  refused by the DOS kernel (WIN.COM's INT 21h hook is a plain jmp to DOS; cause inside IO.SYS, not found).
- Fix (usbdisk.c skip_blocked): after install, INT 21h 7302h (32h on DOS < 7) per unit with a temporary INT 24h
  "fail" stub; a letter whose call never reaches the stub (counter at 28h) moves to the next free letter (LoL+20h
  raised). QEMU: E: H: I: J:, both partitions read. CDS tail now written clean (cds_set), not copied from C:.
- USBDRV /L also prints the DPB chain, our DPBs + device header, INT 13/21/25/2F owners, DOS answers for drive 1.

## Win98 -> MS-DOS mode with SweetLow's USB 2.0 stack (QuickInstall 1.1.0), tested in QEMU (2026-11-11)
- The user's QuickInstall v1.1.0 stock ISO (= our Win98 test image) carries SweetLow's USB 2.0 + USB storage stack
  in OSROOTS/1/DRIVER.866 (USB2.INF, usbehci/usbport/usbhub20/USBSTOR.SYS). Windows installs it for a real EHCI.
  86Box has no EHCI, so our 86Box Win98 tests never loaded it; QEMU does.
- QEMU setup (/home/claude/w98vm, ehci.img = base98.img + EHCI drivers installed): ich9-usb-ehci1 + 2 pci-ohci
  companions (masterbus=ehci.0; the NEC card is OHCI+EHCI too) + usb-storage + usb-kbd (hot-plug it with
  device_add after boot: QEMU sends monitor keys to the newest keyboard). Registry check: PCI\VEN_8086&DEV_293A =
  "Standard PCI to USB Enhanced Host Controller" (SweetLow's stack active in Windows).
- Results after Start > Shut Down > Restart in MS-DOS mode:
  - A USB keyboard is DEAD at the DOS prompt (no BIOS legacy USB after Windows had the controllers) until our driver
    runs. So on PCs with only a USB keyboard, SC55 (SBEMU /USBKB) must start by itself in MS-DOS mode
    (DOSSTART.BAT), or the user cannot type.
  - USBTEST (same code as SBEMU /JOY): EHCI ports handed to companions; OHCI "[try1: win] re-reset" (it sees the
    state Windows left and resets again); the USB stick reads (CRC OK) and the hot-plugged keyboard works (keys seen).
    Same with ICH9 UHCI companions. So the Windows-left controller state is handled; not the cause of the
    "lost a TD" on the user's NEC card (that log was a cold DOS boot anyway: "DOS version 7.10, no Windows").

## beta.95 (2026-11-11): USB disk fixes since beta.94 built in
- SBEMU, USBTEST, SBSET, SC55LOG rebuilt from main (usbdisk.c: drives follow their stick when usbpad renumbers disks,
  a card swapped in a card reader is seen after 2 s idle). Everything else in the zip is beta.94's.
- fsread (exFAT/NTFS reader) is still a separate library, not built into SBEMU yet.

## Shareware test games: General MIDI set (2026-11-11)

- `testgames/` now holds ten untouched shareware archives. Added: `3dduke13.zip` (MD5 04e4ca70b8a2d59ed56c451c5c1d5d39, dosgames.com = archive.org 3dduke13SW), `htic_v12.zip` (MD5 420b23b3d8f2cbd164c121369eaa2b09, idgames mirror), `1rott13.zip`, `1rap12.zip` (user uploads), `desc14sw.exe` (dosgames.com original; a ZIP SFX holding ARJ .SOW parts).
- Licences read from inside each archive: Duke LICENSE.TXT (free WWW/FTP sites OK), ROTT/Raptor VENDOR.DOC (free copying encouraged, unmodified), Heretic LICENSE.DOC (electronic distribution, compressed only), Descent README.TXT section 10 (freely distributable unmodified, no fees; read from the user's DOSBox copy since the ARJ parts were not unpacked here).
- Doom 1.9 (`doom19s.zip`) has no licence file inside, so it stays a link only.
- Tip: Doom/Heretic `*.1` + `*.2` joined with `cat` are a plain ZIP; Apogee `.SHR` files are ZIP SFX (`unzip` works).
- README: new "★ Free Shareware Games!! ★" section after Download Zone with direct download links; Download Zone points to it.
- Not done yet: playing each game's MIDI music through the SC-55 in 86Box.
