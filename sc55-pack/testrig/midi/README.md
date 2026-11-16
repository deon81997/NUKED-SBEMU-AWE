# USB MIDI test rig

## beta.129: no loud events on a switch (loudcheck.py)
- `switch.sh loud` (MPUPLAY SWQ = quiet GS set-up with repeated part-level DT1s + SC-55 display SysEx, SWL = notes;
  `mkswitch.py /tmp/scr`) and `switch.sh midplay` (MIDPLAY /DBG:COM1 /Q:90 on genmid.py's song; rig.py env RIGCOM1FILE=file
  = QEMU COM1 to a file): 6 x Ctrl+Alt+F11, then `loudcheck.py CAPTURE`: rebuilds what the emulated synth (game bytes +
  capture port 4) and each USB slot got, models each output (deaf 150 bytes after a replayed reset, like a real SC-55)
  and checks every note-on after a switch against the game's own state (program, bank, CC7/10/11, bend range), the old
  output (nothing held, no open SysEx) and the reset gap (>= 150 F8). midplay also wants no "key 018B" in MIDPLAY's log.
  beta.127 EXEs: loud FAIL (reset gaps 0), midplay FAIL (4647 loud events, MIDPLAY got 018B); beta.129 both PASS.

## beta.123: MIDI output switch without a restart
- `switch.sh rerun|mout|hotkey` (S=/tmp/scr: `mkswitch.py /tmp/scr` + MPUPLAY.EXE + TimGM6mb as TIM.SF2): the emulated
  synth is the SoundFont; `rerun` = SBEMU @OPT0 (/MUSB:0) -> SBEMU @OPT1 (/MUSB:1) again -> back -> again (the user's
  SETTINGS + SC55 way), `mout` = 2 fake devices (rig.py FAKEMIDI2=1: 0763:0151 "Second MIDI Cable") and /MOUT:USB1,
  USB2, NEXT, USB1, `hotkey` = Ctrl+Alt+F11 x4 (rig.py --keys T:qemu-key+holdms,..., '!cmd' = monitor command; QEMU
  needs a 300 ms hold). midicap.py follows the capture's port-2 switch records and port-3 SBEMU-made bytes, splits
  U pages per slot (cable nibble), matches each fake log to its slot and checks every switch to USB replays every
  channel's program. Run them one at a time (fixed key times; 2 VMs on 2 CPUs boot too slowly).
- Results beta.123: switch.sh rerun, mout, hotkey PASS; gamecap duke rm intel (uhci), gssysex (xhci), miles (ehcicc),
  menumidi xhci, unomatrix quick PASS (sbemu-dos71-ohci QEMU-LIMIT, as beta.121/122); menustat 5/5 on the zip.

## beta.122: byte-exact game streams (wrong notes on the real Uno + SC-55)
- SBEMU `/MIDICAP:1` keeps what the game writes to the MPU-401 (G pages) and every USB MIDI packet sent (U pages) in
  4 KB pages tagged "SBMIDCAP"; `SBDIAG MIDICAP FILE` writes them (users), `rig.py --dump FILE` saves the guest RAM
  (rig: no need to quit the game). `midicap.py CAPTURE [fakemidi.log]` runs the game's writes through a reference
  MPU-401 model (UART / intelligent mode), cuts both sides into MIDI events (running status expanded, real-time bytes
  their own events, SysEx whole) and wants the fake's events identical (a tail still queued at the dump is allowed).
  `--script X.TXT` takes the writes from an MPUPLAY script instead (old SBEMU without a capture).
- `gamecap.sh CASE [HC]` (SECS, RIGPORT, OLD=dir with an old SBEMU.EXE): games duke heretic descent (unpacked into
  /tmp/g: Duke/ROTT/Raptor .SHR and Heretic's HTIC_V12.1+.2 are zips, Descent's .SOW = ARJ volumes DESCENT.ARJ/.A01
  (`apt-get install arj`), bound in the VM with 4GWBIND; configs: HERETIC.CFG snd_musicdevice 6 / snd_mport 816,
  DUKE3D.CFG MusicDevice = 4 / MidiPort = 0x330 / FXDevice = 13, DESCENT.CFG MidiDeviceID=0xa001 / MidiPort=0x330),
  scripts miles gssysex mi2 intel (`mkscript.py /tmp/scr`, MPUPLAY.EXE from mpuplay.c, + " W4000"), rm (RMPLAY.COM
  from rmplay.asm plays MILES as RM.BIN from real mode through JEMM + QPIEMU).
- `menumidi.sh [HC]` = the MENU matrix USB MIDI case: MENU /STATUS, GAMETEST /USB, MPUTEST /USB, SBDIAG USB, then
  Duke 3D's stream byte-exact.
- Results beta.122 (fake Uno, UART pacing): uhci miles gssysex mi2 intel rm duke heretic descent PASS; ehcicc
  gssysex miles heretic PASS; xhci miles duke PASS; ohci gssysex (FAKEMIDI_PACE=0) PASS; menumidi xhci PASS.
  beta.120 SBEMU on the scripts: all 4 FAIL (SysEx pieces / notes lost when the queue overflowed, intelligent-mode
  parameter bytes sent as notes, no answers to version requests).

## beta.108 / beta.114: real-Uno fake + matrix

beta.114: `../fakemidi.py` now looks like the real M-Audio USB Uno 0763:0150 rev 1.25 (usb-devices dump at
xed.ch/help/midi.html: bcdUSB 1.00, ep0 64 bytes, no serial, Atr C0 / 0 mA, bulk IN 81h listed before bulk OUT 02h,
64 bytes, interval 0) and paces OUT like its 31250-baud UART (late answers = NAKs); `FAKEMIDI_OLD=1` = the beta.108
fake. Hostile modes: `FAKEMIDI_OUT=nak|stall`, `FAKEMIDI_BAD=zero|long|nojack`, `FAKEMIDI_PID=1010` (pre-firmware
MidiSport). `rig.py ... --dos71` boots MS-DOS 7.1 + HIMEM only + HDPMI32I (the user's setup) from the VM store image.
`unomatrix.sh [quick]` (ONLY=regex for a subset) runs USBTEST / SBEMU /MUSB + MIDPLAY with unplug at 15 s / MIDPLAY /USB
on uhci, ohci, ehcicc, xhci in dos71 and jem, plus the hostile devices; results.txt, last line PASS/FAIL.
QEMU's OHCI cannot take a late OUT answer (it stops sending the rest of a multi-packet TD; a cancel then trips
usb_cancel_packet's assertion): on ohci the matrix runs the Uno shape with `FAKEMIDI_PACE=0`. 86Box has no USB
pass-through device, so the fake cannot run there.

`../fakemidi.py` is a pretend class-compliant USB MIDI interface (M-Audio Uno 0763:0150: audio-control interface +
MIDIStreaming interface, bulk OUT 02h / bulk IN 81h, 64 bytes; `FAKEMIDI_SPEED=high`: high speed, 512 bytes) on QEMU's
usbredir. It logs every OUT packet and the MIDI in it (`MIDI 90 3C 64`), and sends `FAKEMIDI_IN` events on its IN endpoint.

`rig.py HC "AUTOEXEC lines" SECONDS [--high] [--hotplug T] [--files ..] [--pad]` boots FreeDOS + JEMMEX + HDPMI32I with
the built `output/*.exe` in C:\SC55 on QEMU (Ubuntu's qemu-system-i386 8.2), the fake device on `HC` = uhci (PIIX3),
ohci (pci-ohci), ehcicc (ICH9 EHCI + 3 UHCI companions), ehci (no companions, with `--high`), xhci (qemu-xhci).
`--hotplug T`: device_del at T s, a new chardev + device_add 3 s later. `--pad`: fakepad.py's 360 pad on the same bus.
Results: `out-HC.txt` (C:\OUT.TXT), `fakemidi-HC[-hs].log`. Needs testrig/rufus (FreeDOS kernel, ms-sys boot code:
`git clone --depth 1 https://github.com/pbatard/rufus`) and the release zip unpacked in /home/claude/rel.

Checked for beta.108 (all six: uhci, ohci, ehcicc, xhci, ehci-hs, xhci-hs):
- `USBTEST C:\OUT.TXT /T:12 /F`: device listed, test note on + off arrive, MIDI IN events shown.
- `SBEMU /MUSB:1 /JOY:1|MIDPLAY /S /Q:35 C:\T.MID` (genmid.py song) with `--hotplug 30`: the MIDI streams the device got
  are identical event for event on all six; after the re-plug the music continues.
- `MIDPLAY /USB /S /Q:8 C:\T.MID` (no SBEMU).
- The byte -> USB-MIDI packet parser (running status, SysEx with real-time bytes inside, system common) on the host:
  190000 random bytes in, the packets decode back to the same stream.

QEMU limits: QEMU's OHCI keeps one transfer in flight per controller and one data buffer (a waiting bulk IN held every OUT
back, and OUT bytes came back as IN data), so on QEMU's OHCI (106B:003F) the MIDI IN pipe is left off. `-DOSLOWFORCE`
(NEC OHCI mode) + usbredir's asynchronous bulk OUT trips an assertion in QEMU (usb_cancel_packet); the NEC path for
MIDI OUT needs the real card.

`perf.sh NAME SBEMU.EXE "options"`: 86Box P2B-LS, Pentium II 450, ES1371: SBDIAG START, MIDPLAY 40 s, SBDIAG report.
