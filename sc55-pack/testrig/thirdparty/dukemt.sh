#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU
# dukemt.sh LABEL PACKDIR [SECS] - an MT-32 game in QEMU: FreeDOS + JEMMEX + QPIEMU + HDPMI32I + SBEMU /VMT32:MT32,
# Duke Nukem 3D 1.3D shareware attract mode (music only: G/duke/DUKE3D.CFG MusicDevice 4, MidiPort 0x330, FXDevice 0).
# QEMU records the HDA output to /tmp/dukemt-LABEL.wav; prints the music level per 10 s (dBFS) = Munt played the game's
# MIDI. PACKDIR = JEMMEX.EXE JLOAD.EXE QPIEMU.DLL HDPMI32I.EXE SBEMU.EXE ROMS\ (MT-32). G = unpacked games (/tmp/g).
L=$1; PK=$2; SECS=${3:-150}; T=$(cd "$(dirname "$0")/.." && pwd); G=${G:-/tmp/g}
W=/tmp/dukemt-$L; rm -rf $W; mkdir -p $W/c/SC55; cp -r $PK/* $W/c/SC55/; cp -r $G/duke $W/c/DUKE
cp $T/rufus/res/freedos/KERNEL.SYS $T/rufus/res/freedos/COMMAND.COM $W/c/
printf 'DEVICE=C:\\SC55\\JEMMEX.EXE NOEMS\r\nSHELL=C:\\COMMAND.COM C:\\ /E:1024 /P\r\n' > $W/c/CONFIG.SYS
printf '@ECHO OFF\r\nC:\\SC55\\JLOAD C:\\SC55\\QPIEMU.DLL > C:\\JL.TXT\r\nC:\\SC55\\HDPMI32I -r\r\nCD \\SC55\r\nSBEMU /VMT32:MT32 > C:\\SB.TXT\r\nCD \\DUKE\r\nDUKE3D\r\n' > $W/c/AUTOEXEC.BAT
python3 $T/mkdisk.py build $W/c $W/c.img 120 > /dev/null
timeout $SECS qemu-system-i386 -m 64 -cpu pentium2 -display none -hda $W/c.img -audiodev wav,path=/tmp/dukemt-$L.wav,id=snd \
  -device intel-hda -device hda-output,audiodev=snd > $W/qemu.log 2>&1
python3 - /tmp/dukemt-$L.wav $L <<'EOF'
import sys, wave, numpy as np
w = wave.open(sys.argv[1]); r = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(float)
x = x.reshape(-1, w.getnchannels()).mean(1); s = 10 * r
lv = [20 * np.log10(np.sqrt((x[i:i+s]**2).mean()) / 32768 + 1e-9) for i in range(0, len(x) - s, s)]
print(sys.argv[2], 'music dBFS per 10 s:', ' '.join('%.0f' % v for v in lv), '| loud blocks (> -50 dBFS):', sum(v > -50 for v in lv))
EOF
mtype -i $W/c.img ::JL.TXT 2>/dev/null | tr -d '\r' | head -2
