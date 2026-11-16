/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* mpuint.c - MPU-401 intelligent mode the way some DOS games talk to it (w98.8 VxD front end, DOS beta.122).
 * Writes C:\MPUINT.LOG. Open Watcom 16-bit DOS: wcl -q -bt=dos -ms mpuint.c
 *   MPUINT       ACK/answers check, then a silent note-on (velocity 0) followed by intelligent-mode commands with
 *                parameter bytes: before w98.8 the parameters became MIDI (running status note-on = a loud note).
 *   MPUINT UART  the same bytes after 3Fh (UART mode): there they ARE MIDI, the note must sound (control case). */
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <i86.h>

#define MPU 0x330
static FILE *lg;

static unsigned long ticks(void) { return *(volatile unsigned long __far *)MK_FP(0x40, 0x6C); }
static int wait_out(void) { long n = 100000L; while (n-- > 0) if (!(inp(MPU + 1) & 0x40)) return 1; return 0; }
static int rd(void) { long n = 100000L; while (n-- > 0) if (!(inp(MPU + 1) & 0x80)) return inp(MPU); return -1; }
static void cmd(int c) { wait_out(); outp(MPU + 1, c); }
static void dat(int d) { wait_out(); outp(MPU, d); }

int main(int argc, char **argv)
{
    int uart = argc > 1 && !strcmp(argv[1], "UART"), a, b, i;
    unsigned long t;
    lg = fopen("C:\\MPUINT.LOG", "a");
    if (!lg) return 1;
    fprintf(lg, "== mpuint %s\n", uart ? "UART" : "intelligent");
    cmd(0xFF); a = rd(); fprintf(lg, "reset -> %02x (fe)\n", a & 0xFF);
    if (uart) { cmd(0x3F); a = rd(); fprintf(lg, "uart -> %02x (fe)\n", a & 0xFF); }
    else {
        cmd(0xAC); a = rd(); b = rd(); fprintf(lg, "AC version -> %02x %02x (fe 15)\n", a & 0xFF, b & 0xFF);
        cmd(0xAD); a = rd(); b = rd(); fprintf(lg, "AD revision -> %02x %02x (fe 01)\n", a & 0xFF, b & 0xFF);
        cmd(0x8E); a = rd(); fprintf(lg, "8E -> %02x (fe)\n", a & 0xFF);
    }
    /* program 0 on channel 1, then a silent note-on: running status = 90h */
    if (!uart) cmd(0xD0); dat(0xC0); dat(0x00);
    if (!uart) cmd(0xD0); dat(0x90); dat(0x48); dat(0x00);
    if (uart) { dat(0x48); dat(0x7F); }          /* UART: running status note-on, MUST sound */
    else for (i = 0; i < 4; i++) {               /* intelligent: E0h-EFh take one parameter byte - not MIDI */
        cmd(0xE0 + i * 2); a = rd(); dat(i & 1 ? 0x7F : 0x48);
    }
    fprintf(lg, "sent; holding 2 s\n");
    t = ticks(); while (ticks() - t < 36) {}
    if (!uart) cmd(0xD0); dat(0x80); dat(0x48); dat(0x00);
    cmd(0xFF); rd();
    fprintf(lg, "done\n");
    fclose(lg);
    return 0;
}
