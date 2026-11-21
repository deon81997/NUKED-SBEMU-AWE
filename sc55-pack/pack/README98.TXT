=============================================================================
  NUKED SB-EMU FOR WINDOWS 98
  Sound Blaster / OPL3 / MPU-401 for DOS boxes + "NUKED SB-EMU" MIDI device
=============================================================================

The DOS pack plays the SC-55 when you boot to DOS. This part does the same
inside Windows 98:

  * DOS programs in a DOS box (a window or full screen) get a Sound Blaster
    (1.5 / 2.0 / Pro / Pro 2 / 16, as you choose), OPL3 FM music at 388h and
    an MPU-401 at 330h. Their MIDI music is played by the SC-55 (or the
    MT-32 or a SoundFont), the FM music by the DOSBox or Nuked OPL3 core,
    and the digital sound effects through your sound card.
  * AWE32 edition: DOS programs can also get a Sound Blaster AWE32 - the
    EMU8000 wavetable at 620h (or 640/660/680h), 512 KB to 28 MB sample
    RAM, the same engine as DOS SBEMU /AWE (with its reverb and chorus),
    mixed into the DOS box's sound. Settings > AWE32, or SC55W /AWE:620 [/AWERAM:kb /AWEMPU:0|1|GM|GS|MT32];
    the Sound Blaster becomes type 16 and BLASTER gets E620. The MPU-401
    keeps its SC-55 (/AWEMPU:1). Needs the 1 MB AWE ROM (86Box's
    awe32.raw) in C:\SC55\ROMS, found by content. beta.131-awe.2: its
    reverb and chorus start set up like a real card after AWEUTIL /S
    (Hall 2, Chorus 3). beta.131-awe.4: the chip's read-backs match DOS
    SBEMU (free voices marked free, VTFT = the live volume target), so
    Duke 3D / Descent with their own AWE32 drivers use all the voices.
  * AWE32 edition: Windows programs also get a second MIDI device, "AWE32
    MIDI Synth", played by the EMU8000 like Creative's AWE32 Windows
    driver: Creative's own banks SYNTHGM.SBK / SYNTHGS.SBK / SYNTHMT.SBK
    (from your AWE32/AWE64 drivers: found by content anywhere under
    C:\SC55 and copied to ROMS; see "Where do I get the SBK files?" in
    section 3), GM / GS / MT-32 like AWEUTIL /EM:GM|GS|MT32, Creative's 8
    reverb and 8 chorus types like AWE Control (Settings > AWE32). With no
    SBK: NUKED's own GM map over the AWE ROM. Its volume follows Windows
    (midiOutSetVolume). A DOS box that uses the AWE32 gets the chip, as on
    a real card; the MIDI device plays again when that DOS box is closed
    (or 10 seconds after it left the EMU8000 alone). Since awe.7 your own
    .SF2/.SBK banks 1-127 (bank select), single presets and a sample RAM
    gauge (Settings > AWE32), and /AWEMPU:GM|GS|MT32 plays DOS boxes'
    MPU-401 on it.
  * Windows programs (Media Player, MIDI sequencers, Windows games) get a
    MIDI device called "NUKED SB-EMU" played by the same synth. Its volume
    follows Windows (games' music sliders, midiOutSetVolume).
  * DOS programs also get a joystick: a game port (201h, or any address
    200h-20Fh) and the BIOS joystick call (INT 15h AH=84h), fed by the
    joystick Windows knows (an Xbox 360 pad with the driver that comes
    with the pack, or any USB pad). Stick types as in DOS: standard, CH
    Flightstick Pro, Thrustmaster FCS, Gravis GamePad Pro (GrIP).
  * Windows games get the "NUKED SB-EMU Joystick" (Control Panel > Game
    Controllers), the pad as NUKED SB-EMU passes it on, and PCs whose
    sound card has no game port get a "NUKED SB-EMU Game Port" in Device
    Manager (see "Joystick" in section 3).
  * A serial debug link, as SC55 /DBG in DOS (since beta.130-w98.9;
    since w98.10 / awe.10 levels 1-2 cost nothing, EMU8000
    ports included, so it can stay on):
    SC55W /DBG:COM1 (or Settings > Debug) sends what the driver and the
    program do as short text lines to a second PC (DBGLOG.PY, cable in
    C:\SC55\DBGLOG.TXT), also when Windows hangs. Off = no cost.
    AWE32 edition (beta.130-awe.9/.10): also the EMU8000 ports (awe= in the S
    line, captured at level 3), which DOS box takes the chip from the
    AWE32 MIDI Synth, its keys held and sample RAM, and the bank loads.

It is made for the Dell Latitude D810 with the SigmaTel AC'97 WDM driver,
but works with any sound card that has a Windows 98 driver. It runs on
the same CPUs as the DOS pack, Pentium II and newer: SC55W.EXE starts
SC55WM.EXE on CPUs with SSE2 (Pentium 4, Pentium M, Celeron D or M,
Athlon 64 and newer; the faster build) and SC55W2.EXE on a Pentium II /
III or Athlon XP (since beta.90-w98.6; before, those CPUs could not run
it at all).

MIDPLAY in a DOS box shows the SC-55's (or MT-32's) real display, as in
DOS: this program answers its front panel query (MPU command B0h).

It does NOT replace "Restart in MS-DOS mode" for games that need it (games
that want all of the memory, or that do not run in a DOS box): use the DOS
pack there, as before.

SBEMU itself does not load in a DOS box while Windows runs (it says so and
stops: Windows owns the sound card there). After Start > Shut Down >
"Restart in MS-DOS mode" Windows is gone and C:\SC55\SC55.BAT works as in
plain DOS. To start it there by itself, add this line to
C:\WINDOWS\DOSSTART.BAT (Windows runs that file only in MS-DOS mode):
     CALL C:\SC55\SC55.BAT


-----------------------------------------------------------------------------
 1. INSTALLING
-----------------------------------------------------------------------------

  1. Install your sound card's Windows 98 driver first (for the D810: the
     SigmaTel driver). Windows must be able to play a .WAV file.
  2. Copy the pack folder to the hard disk (or use it from CD) and run
     SETUP98.BAT, or FILES\WIN98\SC55W.EXE /INSTALL.
  3. The setup window:
       Pack folder      C:\SC55 (where the DOS pack's SETUP.BAT put it).
                        The Windows files go to C:\SC55\WIN98. ROMs that
                        are not in C:\SC55\ROMS yet are copied there.
       Start with Windows
                        leave on: the DOS sound and the MIDI device only
                        work while SC55W.EXE runs.
       Make "NUKED SB-EMU" the default MIDI device
                        Windows programs that use the MIDI Mapper (most do)
                        then play through the SC-55.
       Turn off Windows' own Sound Blaster emulation (SBEMUL)
                        Windows 98 SE has its own (basic) Sound Blaster
                        emulation for WDM sound drivers. It holds port 220
                        and must be off for this one. Greyed out if your
                        Windows does not have it.
       Set BLASTER= in AUTOEXEC.BAT
                        only if AUTOEXEC.BAT has no BLASTER line of its own:
                        an existing line is never changed, because SBEMU
                        and your games' setups go by it. Ours is kept up to
                        date when you change the settings. (Version 1 of
                        Setup replaced an existing line, which could make
                        games freeze; running Setup again puts it back.)
  4. Restart Windows (Setup asks). A keyboard icon appears next to the
     clock. It takes a few seconds after Windows starts before the SC-55
     is ready.

Nothing in CONFIG.SYS is changed. Setup notes everything it changes in
C:\SC55\WIN98\SC55W.INI and the uninstaller puts it back.


-----------------------------------------------------------------------------
 2. USING IT
-----------------------------------------------------------------------------

  Tray icon (next to the clock), right-click:
     Status...        what is running, any problems, the CPU load, and
                      counters (MIDI bytes, FM writes, Sound Blaster
                      interrupts). Double-click the icon for the same.
     Settings...      see section 3.
     All notes off    stops stuck notes.
     MIDI to          where the MIDI music goes, switched while a game
                      plays (since beta.130-w98.8; DOS: Ctrl+Alt+F11 /
                      SBEMU /MOUT): "Emulated synth" (the SC-55, MT-32 or
                      SoundFont) or any Windows MIDI output, e.g. a USB
                      MIDI cable (M-Audio Uno) to a real SC-55, or a sound
                      card's MIDI port. The old one is silenced, the new
                      one gets the song's state first (reset, instruments,
                      controllers, volume last, with the ~50 ms a real
                      SC-55 needs after a reset), so the music goes on
                      without stuck notes or a loud blast. The choice is
                      kept (SC55W.INI MidiOut=). The same from a command
                      line, also while SC55W runs:
                        C:\SC55\WIN98\SC55W.EXE /MOUT:SYNTH | USB | USB2 |
                                                  NEXT | 2 | "device name"
                      (USB = the first Windows MIDI output whose name says
                      USB, or the first one; NEXT = the next in the menu.)
                      SBOPTS.TXT /MUSB:1 (DOS: MIDI to USB) makes USB the
                      start choice when MidiOut= is not set.
     Exit             stops the program (DOS boxes lose their sound, the
                      MIDI device stops working).

  DOS games: start them in a DOS box as usual (double-click, or from an
  MS-DOS Prompt). In the game's setup choose
     Sound effects:   Sound Blaster 16 (or Pro / 2.0, whatever you set),
                      port 220, IRQ 5 or 7, DMA 1, 16-bit DMA 5 - as shown
                      in Status and in the BLASTER line.
     Music:           General MIDI / Roland Sound Canvas / MPU-401, port
                      330; or "Roland MT-32" when you chose the MT-32; or
                      AdLib / Sound Blaster FM.
  You do NOT load SBEMU, HDPMI or SC55.BAT inside Windows: this program
  takes their place there.

  Windows programs: choose the MIDI device "NUKED SB-EMU" (or leave the
  MIDI Mapper, if Setup made it the default). Control Panel > Multimedia >
  MIDI shows and changes the default.

  Joystick in DOS games: set it up in Windows first (Control Panel > Game
  Controllers: add it, calibrate it, check it on the Test page), then pick
  "Joystick" / "Gravis" / "CH" in the game's setup as usual.
     Stick 1     = Windows joystick 1, X and Y
     Stick 2     = Windows joystick 2, or joystick 1's second stick
                   (U or Z across, R down) when there is no joystick 2
     Buttons 1-4 = joystick 1's buttons 1-4 (joystick 2's buttons 1-2
                   also press buttons 3-4)
  Status shows the joystick Windows found and how often DOS read it.
  Windows' own programs keep using the joystick through Windows as before;
  they can also use the NUKED SB-EMU Joystick (Settings > Joystick).

  Xbox 360 controller: Windows 98 has no driver for it, so the pack has an
  unofficial one in WIN98\X360 (copied there by Setup; FILES\WIN98\X360 in
  the pack). Plug the pad in and point the Add New Hardware Wizard to that
  folder - X360.TXT has the steps. Then the left stick is DOS stick 1, the
  right stick DOS stick 2, and A B X Y buttons 1-4. When a 360 pad is
  plugged in without a driver, NUKED SB-EMU says so once and offers to show
  the steps (SC55W.INI X360Prompt=0 stops that).

  Game pad buttons as keys: when a DOS box has the keyboard, the pad's
  buttons and sticks can press keys, with a layout for the game running in
  it (found by its EXE name and size, or by the files in its folder).
     Layouts for 3500+ games: download keyb2joypad.cpp as DOWNLOADS.TXT
       says (the Keyb2Joypad project, as built into DOSBox Pure; not
       shipped with the pack) into C:\SC55\PADMAP (Setup copies it from
       the pack's FILES\PADMAP). The next start turns it into
       C:\SC55\PADMAP.TXT.
     Without a layout for the game ("Default"): sticks and A B X Y are the
       DOS joystick as above, D-pad = arrow keys, Start = Enter, Back = Esc.
     Your own layouts and changes: C:\SC55\PADUSER.TXT, same format as
       PADMAP.TXT. A section with a game's title (or [Default]) changes only
       the lines it has, e.g.
          [Default]
          Y=F1
          [Commander Keen 4 - Secret of the Oracle]
          RB=F2+F3
       Buttons: A B X Y LB RB LT RT LS RS Back Start DUp DDown DLeft DRight
       Sticks:  LStickX LStickY RStickX RStickY = key-for-left/up key-for-
       right/down. Keys as in PADMAP.TXT (LCtrl, Space, Up, F1, A ... Z,
       None); JoyButton1-4, JoyUp/Down/Left/Right, Joy2Up... = DOS joystick.
     Status shows the layout in use and the last button pressed with its
     Windows button number, to check the [Pad] settings below.
     Held keys are let go when Windows takes the keyboard.

  Command line (Start > Run):
     C:\SC55\WIN98\SC55W.EXE              start (or show the status)
     C:\SC55\WIN98\SC55W.EXE /SETTINGS    open the settings
     C:\SC55\WIN98\SC55W.EXE /EXIT        stop it


-----------------------------------------------------------------------------
 3. SETTINGS
-----------------------------------------------------------------------------

Five tabs: "Sound and MIDI", "Joystick", "Pad keys", "AWE32" and "Debug"
(Ctrl+Tab goes to the next one). Save / Apply / Cancel are for all five.

"AWE32" (AWE32 edition): AWE32 on/off (/AWE), EMU8000 port 620/640/660/680,
sample RAM 512 KB (AWE32) to 28 MB (/AWERAM), "MPU-401 keeps its synth"
(/AWEMPU), "Reverb and chorus" (/AWEFX, since awe.4: the EMU8000's effects as
the game sets them; off = dry); below: which ROM was found and the counters.
"AWE32 MIDI Synth" (since awe.5, SC55W.INI [AWE32 MIDI]): the second Windows
MIDI device on/off, Bank GM / GS / MT-32 (as AWEUTIL /EM), Reverb (Room 1-3,
Hall 1-2, Plate, Delay, Panning Delay; default Hall 2) and Chorus (Chorus 1-4,
Feedback, Flanger, Short Delay, Short Delay FB; default Chorus 3) as in
Creative's AWE Control, "Search for SBK files", and which bank plays. The
sample RAM setting is shared with the DOS boxes' AWE32 (one EMU8000).
Bank manager (since awe.7): the list shows Bank 0 (the GM / GS / MT-32 bank
above) and your own banks 1-127: "Add / replace bank..." takes any .SF2 or
.SBK file, "Replace one preset..." puts one preset of a file in place of a
bank/program (or a drum kit), "Remove" takes it out - at once, kept in
SC55W.INI [AWE32 MIDI] (Bank1=..., Preset0_0=...) and loaded again at start.
MIDI programs pick bank N with bank select (CC0 = N, or CC32 = N with CC0 0);
an empty bank plays bank 0. The gauge shows the sample RAM the banks use; a
bank that doesn't fit is refused with "needs ... KB, ... KB free" (more RAM:
Sample RAM above). "MPU-401 plays on" (/AWEMPU, as DOS SBEMU): the synth of
the first tab (1), the AWE32 MIDI Synth with its GM / GS / MT-32 bank
(GM / GS / MT32: General MIDI games get AWE32 music), or nothing (0).

Where do I get the SBK files? They come with Creative's AWE32 / AWE64 /
Sound Blaster 32 drivers; NUKED SB-EMU can't include them. Get a driver
set from one of these pages, unpack it, and copy the folder (or just
SYNTHGM.SBK, SYNTHGS.SBK, SYNTHMT.SBK) anywhere under C:\SC55 - the files
are found by what's inside, names and folders don't matter - then Settings
> AWE32 > "Search for SBK files" (or restart SC55W):
  Phil's Computer Lab, Creative drivers:
     https://www.philscomputerlab.com/creative-labs-drivers.html
  VOGONS Driver Library:    https://www.vogonsdrivers.com/
  Sound Blaster 32 PnP CD:  https://archive.org/details/CreativeSoundBlaster32

"Sound and MIDI" has the same options as SETTINGS.BAT / SBSET, with the
SBEMU switch they set:

  Sound Blaster (DOS)
     Port 220/240 (/A), IRQ 5/7/9/10 (/I), 8-bit DMA 0/1/3 (/D), 16-bit
     DMA 5/6/7 (/H, SB16 only), Type 1-6 (/T: SB 1.5, SB 2.0, SB Pro,
     SB Pro 2, SB16), FM music off / DOSBox (fast) / Nuked (accurate)
     (/OPL, /FMCORE).
  MIDI
     MPU-401 330 / 300 / off (/P, /VMPU), synth: Roland SC-55, SoundFont
     (/VSF), MT-32 auto / MT-32 / CM-32L (/VMT32), SC-55 ROM set (/VSCSET,
     "auto" or one of the sets found in C:\SC55\ROMS).
     MIDI to: the emulated synth or a Windows MIDI output (as the tray
     menu, see section 2).
  SC-55 / MT-32
     Gain (/VSCGAIN, 100 = unity), note limit (/VSCVOICES), reverb and
     chorus (/VSCFX), automatic speed control (/VSCAUTO: pauses the SC-55's
     reverb/chorus while the CPU cannot keep up), low-pass (/VSCLPF, on by
     default as in DOS beta.109: at 22050 Hz output the SC-55's 32 kHz sound
     is filtered first, so high notes and cymbals do not whistle; no effect
     at 32000 Hz and up; its CPU cost is too small to measure).
  SoundFont
     reverb, room, damping, chorus, chorus speed and depth, voices, follow
     the song's reverb/chorus sends (/VSFREV ... /VSFSEND).
  Volume
     master (/VOL), sound effects (/VSBVOL), FM (/VOPLVOL), MIDI (/VMIDIVOL,
     0-400). The DOS program's own Sound Blaster mixer settings apply too.
  Output
     sample rate (/K: 22050 / 32000 / 44100 / 48000), render-ahead
     (/VSCAHEAD: how far behind the game the music is made; in Windows at
     least 20 ms), fix time constants (/FIXTC).
  Start with Windows.

The line at the bottom shows exactly what is saved to SBOPTS.TXT. Options
that only matter in DOS (/VTIMER, /O, /PM, /RM, /VCDA, /MCOM ...) are kept
unchanged in the file.

  Save     writes SBOPTS.TXT (and SBOPTS.BAT) and applies the settings.
  Apply    applies them without saving (Cancel then goes back).
  Defaults the pack's defaults.

Changes take effect at once - for DOS boxes too (a running game may need to
be restarted to see a new port or IRQ). Volume and gain changes are
instant; changing the synth, ROM set or sample rate restarts the synth (a
few seconds of silence).

"Joystick":
  Game port for DOS programs
     Game port in DOS boxes   on/off (off = a real game port card answers
                              DOS directly)
     Address                  200-20F (201 is what nearly every game uses;
                              a PC with a real game port on 201 can put
                              ours elsewhere). DOS boxes use it at once.
     Stick type               Standard (2 sticks, 4 buttons), CH
                              Flightstick Pro (D-pad = the hat), Thrustmaster
                              FCS (D-pad = the hat on axis 4), Gravis GamePad
                              Pro digital (GrIP: all buttons of 2 pads) - as
                              SBEMU's /JOYMODE in DOS. Pick the same in the
                              game's setup.
     BIOS joystick call       INT 15h AH=84h for the games that use it.
     Also for Windows programs that read the port
                              old Windows programs that read the game port
                              themselves (not through Windows) get ours too.
                              Leave it off if the PC has a real game port.
  NUKED joystick for Windows games
     Feed the NUKED SB-EMU Joystick
                              Windows joystick "NUKED SB-EMU Joystick" shows
                              the pad: left stick X/Y, triggers Z, right
                              stick R (down) / U (across), D-pad = the hat,
                              buttons 1-12 (A B X Y LB RB Back Start LS RS
                              LT RT) - or, with "It shows: the game port as
                              DOS sees it", the 4 axes and 4 buttons of the
                              game port above (with its stick type).
     Dead zone                % of the stick's travel around the middle
                              that counts as centred (0-50, default 10).
     Set up...                adds the "NUKED SB-EMU Game Port" to Device
                              Manager (Sound, video and game controllers,
                              at the Address above) and the "NUKED SB-EMU
                              Joystick" to Game Controllers, then asks for a
                              restart. No Windows CD needed. Changing the
                              Address later moves the Device Manager entry
                              too (shown after a restart).
     Remove                   takes both out again (Uninstall does too).
     Game Controllers...      Control Panel > Game Controllers (calibrate,
                              test, the Xbox 360 driver's own settings).
  Test (live)                 the pad as Windows gives it (sticks, buttons),
                              what DOS boxes read from the game port, and
                              what the NUKED joystick shows Windows games.
  The NUKED joystick only moves while SC55W runs; Windows games that want
  joystick 1 use whatever Game Controllers lists first (Advanced tab:
  "Preferred device").

"Pad keys":
  Pad buttons press keys in DOS games    on/off (as PadKeys= below)
  Layout now / Last pad button           what is in use right now
  My layouts (PADUSER.TXT)...            opens it in Notepad
  PADMAP folder...                       where keyb2joypad.cpp goes
  Which Windows joystick button is which pad button: the [Pad] settings
  below. Click a box, press the button on the pad and its number is filled
  in (then the next box gets the cursor). "Xbox 360 defaults" puts the
  360 driver's numbers back.

Windows-only options, in C:\SC55\WIN98\SC55W.INI under [SC55W]:
     BufferMs=80        sound output buffer (20-500). Raise it if the
                        sound breaks up; lower it for less delay.
     BlockMs=10         size of each buffer block (5-50).
     BufferAuto=1       when a DOS game plays sound and the output keeps
                        running dry (full-screen games leave Windows
                        programs only short turns), the buffer grows to
                        160, then 300 ms; back to BufferMs after a
                        minute without DOS sound. 0 = off.
     TraceFile=0        1 = write the driver's DMA/IRQ events to
                        SC55W.TRC (troubleshooting).
     OutputSleep=1      stop the sound output while nothing makes a sound
                        (saves CPU; it starts again within about 20 ms).
                        0 = keep it running, if your sound card's driver
                        clicks or misses the first note after a pause.
     RecordWav=C:\X.WAV also record everything played to a WAV file
                        (troubleshooting).
     Joystick=1         game port for DOS programs (0 = off, e.g. for a
                        real game port card that DOS should read directly).
     JoyBios=1          BIOS joystick call INT 15h AH=84h (0 = off).
     X360Prompt=1       0 = do not offer the Xbox 360 driver when a 360 pad
                        without a driver is plugged in.
     PadKeys=1          game pad buttons -> keys in DOS boxes (0 = off).
   Under [Pad]: which Windows joystick button is which pad button, for pads
   that number them differently from the Xbox 360 driver (its order is the
   default: A=1 B=2 X=3 Y=4 LB=5 RB=6 Back=7 Start=8 LS=9 RS=10; D-pad =
   the POV hat; Triggers=Z (LT one way, RT the other); RStickX=U RStickY=R).
     CpuMax=60          most of the CPU (percent, 20-95) the synth may use.
                        When it cannot keep up the music lags or breaks up
                        instead of Windows slowing down.
     StartDelay=15      when it starts with Windows: seconds to wait before
                        starting the synth, so the other startup programs
                        get the CPU first (0 = start at once).
   Under [Joystick] (the Joystick tab writes them):
     Port=201           game port address, 200-20F
     Type=0             stick type: 0 standard, 1 CH, 2 FCS, 3 GrIP
     WindowsPort=0      1 = the game port for Windows programs too
     NukedJoystick=1    feed the NUKED SB-EMU Joystick
     NukedMode=0        0 = the pad, 1 = the game port as DOS sees it
     DeadZone=10        percent

If Windows froze while NUKED SB-EMU was starting, the next start with Windows
is skipped and a message tells you at which step it stopped (in
SC55W.INI [State] Step=). Start C:\SC55\WIN98\SC55W.EXE by hand, and if
Windows freezes again, send C:\SC55\WIN98\SC55W.LOG: for its first minute
it writes the load and priority every 5 seconds.


-----------------------------------------------------------------------------
 4. CPU LOAD (D810)
-----------------------------------------------------------------------------

The SC-55 emulation is heavy: about as heavy as under DOS. Status shows the
load. If music slows down or breaks up:
  * use 22050 or 32000 Hz (Settings > Output),
  * leave "Automatic speed control" on, or turn reverb and chorus off,
  * set a note limit of 16,
  * or use the original SC-55 ROM set instead of the mkII.
The DOS pack's Automatic setup (SETTINGS /TUNE) finds good values for the
D810; they are saved in SBOPTS.TXT, which Windows uses too.
When nothing has played for a few seconds the synths pause, and a second
later the sound output stops too (Status: "paused while silent"), so the
program uses almost no CPU while idle: on a Pentium II 450 about 0.25 % of
the CPU (it was 1.6 % before beta.81-w98.4). Windows' timer runs at its
normal rate unless a DOS program plays sound or reads the joystick.
The AWE32 MIDI Synth (AWE32 edition) is much lighter than the SC-55: a busy
song (30 voices, reverb + chorus) takes about a fifth of a Pentium II 450,
nothing when it is silent.


-----------------------------------------------------------------------------
 5. TROUBLESHOOTING
-----------------------------------------------------------------------------

  Windows freezes or shows a blue screen while a DOS game or MIDI plays
     Watch it live over a null-modem cable (beta.130-w98.9): on the second
     PC  python DBGLOG.PY COM3 , on this one  SC55W /DBG:COM1  (or Settings
     > Debug). SC55W.VXD writes the serial port itself, so the last lines
     arrive even when Windows stops; levels 1-3 as in DOS, SC55W /DBG:DUMP
     sends the level-3 port capture. COM1 is Windows' no more while the
     link is on; a port Windows uses (serial mouse, modem) is refused.

  "Port 220 is taken by another driver"
     Another Sound Blaster emulation has the port: SBEMUL (run Setup again
     with that box ticked), or a sound card driver with its own DOS
     support - turn that off in its settings, or choose port 240 here.
  "Ports 388-38B (AdLib/FM) belong to another driver: <name>"
     Since beta.131-awe.3 the status line, SC55W.LOG and the debug link
     name the Windows device that has the ports (Device Manager's name),
     or say it is a driver from SYSTEM.INI. Windows gives a port to one
     driver only, and taking it away would leave that driver without it
     until Windows restarts, so FM in DOS boxes stays with that driver
     (often a sound card's own DOS support: turn it off there). FM off in
     Settings hides the line.
  "IRQ n / DMA n could not be taken over"
     Choose another IRQ (5, 7, 9 or 10) or DMA channel; the game's setup
     must then use the same values.
  "Cannot open the sound output"
     The Windows sound driver is missing or busy. Check that Windows can
     play a sound (Control Panel > Sounds).
  No MIDI in a Windows program
     SC55W.EXE must be running (tray icon). Check Control Panel >
     Multimedia > MIDI, or pick "NUKED SB-EMU" in the program.
  Stuck notes:   tray icon > All notes off.
  Sound breaks up: raise BufferMs (section 3), lower the sample rate.
  A DOS game's sound comes in short bursts with gaps (Blood, Sound
                 Blaster 16 at 44 kHz, before beta.89-w98.5): Windows gave
                 our sound program too little time while the game ran full
                 screen. Fixed with BufferAuto (above); on older versions
                 set BufferMs=300.
  C:\SC55\WIN98\SC55W.LOG shows what happened at the last start, and
                 every 10 s while something plays: "activity: MIDI n bytes,
                 FM n writes, digital n frames ..." (BUGREP.BAT in DOS
                 copies it, with VERSIONS.TXT, into BUGREP.TXT).
  Which version is installed: C:\SC55\WIN98\SC55W.EXE /VER shows (and
                 C:\SC55\WIN98\VERSIONS.TXT lists) the version of every
                 Windows 98 file: SC55W*.EXE, SC55W.VXD, SC55MIDI.DRV (also
                 the copy in WINDOWS\SYSTEM), NUKEDJOY.VXD (since
                 beta.130-w98.8; older files say "no version tag").
  A game's music has wrong or extra notes with an MPU-401 in
                 "intelligent mode" (games that ask the MPU-401 for its
                 version, or send "want to send data" commands): fixed in
                 beta.130-w98.8 (as DOS beta.122): every command gets its
                 answer, command parameter bytes are no longer played as
                 notes. Status / SC55W.LOG count sequencer bytes the
                 MPU-401 ignores (its own sequencer is not emulated).
  Windows freezes or a DOS game stops for seconds when its music
                 changes (Descent, other HMI SOS games: the song ends, a
                 level loads, you quit): fixed in beta.131-awe.3. The
                 MPU-401's answer to a reset now arrives a moment later, as
                 on a card, so HMI's driver no longer reads the port a
                 million times; a driver that waits for data nobody sent
                 gets "active sensing" after ~10 ms. A debug link line
                 "MPU-401: ... reads the status 2048 times in a row" shows
                 it happening.
  Blue screen "exception 0E ... VxD SC55W(01) + 0000178A" while a DOS
                 game plays sound (Descent at its main menu), or a loud
                 burst of noise: fixed in beta.131-awe.8. A game whose
                 DOS extender locks its sound buffer itself (DOS4GW /
                 HMI SOS) hands Windows a physical address; SC55W read it
                 as a normal one. It now locks or maps the buffer first
                 and plays silence if it cannot - never a wrong address.
                 Debug link level 2: "DMA n: physical address ..." lines
                 and bad= on the F lines show it.
  Keys typed by themselves in a DOS game with a pad (stick drift, a
                 trigger resting half way): fixed in beta.130-w98.8 (as DOS
                 beta.125): a stick types its key past half the travel
                 (SBOPTS.TXT /JOYDZ, 10 % = half) and lets go below 3/4 of
                 that, only after it was seen at rest once and past the
                 line in two reads in a row; buttons count after two equal
                 reads; triggers on past 63 %, off below 38 %.


-----------------------------------------------------------------------------
 6. UNINSTALLING
-----------------------------------------------------------------------------

Control Panel > Add/Remove Programs > "NUKED SB-EMU for Windows 98", or
C:\SC55\WIN98\SC55W.EXE /UNINSTALL. It removes the MIDI device, the start
entry and the BLASTER line, turns SBEMUL back on if Setup turned it off,
restores the previous default MIDI device and takes out the NUKED SB-EMU
Game Port and Joystick if they were set up. The DOS pack, ROMs and your
settings stay. Restart Windows afterwards.

If you also remove the DOS pack (UNINSTAL.BAT), remove this Windows part
first: UNINSTAL.BAT deletes C:\SC55 including C:\SC55\WIN98.


-----------------------------------------------------------------------------
 7. FILES AND CREDITS
-----------------------------------------------------------------------------

  FILES\WIN98\SC55W.EXE     the program (synths, settings, setup)
  FILES\WIN98\SC55W.VXD     the virtual Sound Blaster / OPL3 / MPU-401
                            (a dynamic VxD, loaded by SC55W.EXE)
  FILES\WIN98\SC55MIDI.DRV  the Windows MIDI driver ("NUKED SB-EMU")
  FILES\WIN98\JOY\NUKEDJOY.VXD
                            the NUKED SB-EMU Joystick (a VJoyD joystick
                            minidriver: Windows loads it for that joystick)
  FILES\WIN98\JOY\NUKEDJOY.INF
                            the NUKED SB-EMU Game Port, for Add New Hardware
                            by hand (Settings > Joystick > Set up... does
                            the same without it)
  FILES\WIN98\SOURCE.ZIP    source code of all of them
  FILES\WIN98\COPYING.TXT   GNU General Public License, version 2

  Built on: Nuked-SC55 (nukeykt, jcmoyer fork; GPL v2+), Munt mt32emu
  (LGPL v2.1+), TinySoundFont (MIT), DOSBox DBOPL (GPL v2+), Nuked OPL3
  (LGPL v2.1+), the SBEMU sources (GPL). SC55W itself is GPL v2 or later.
  Roland ROMs are not included in the source; use your own dumps.
