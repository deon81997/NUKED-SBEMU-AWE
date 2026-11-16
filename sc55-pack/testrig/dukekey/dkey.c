/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* DKEY SECONDS - Duke Nukem 3D's keyboard handler (Build engine keyhandler: EOI first, port 61h toggle, E0/E1 ->
 * "extended" + 128, a byte equal to the one before is ignored) as a protected-mode INT 9 handler (DJGPP, HDPMI),
 * while MIDI chords keep SBEMU's synth (and its sound interrupt) busy. Writes "KEYGO" to COM2 when it listens and
 * "KEYEND" when done; the host types keys in between. At the end every key still down is a stuck key:
 * DKEY.TXT = bytes read + result, last line PASS or FAIL. Exit 0 PASS, 1 FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <time.h>
#include <crt0.h>
#include <sys/farptr.h>
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;

static volatile unsigned char keystatus[256], readch, oldreadch, extended;
static volatile unsigned char blog[16384]; static volatile int bn;
static volatile int downs[256], ups[256];
static _go32_dpmi_seginfo oldh, newh;

static void keyhandler(void)
{
    unsigned char t;
    outportb(0x20, 0x20);
    oldreadch = readch; readch = inportb(0x60);
    t = inportb(0x61); outportb(0x61, t | 0x80); outportb(0x61, t & 0x7F);
    if (bn < (int)sizeof blog) blog[bn++] = readch;
    if ((readch | 1) == 0xE1) { extended = 128; return; }
    if (oldreadch != readch)
    {
        int k = (readch & 127) + extended;
        if (!(readch & 128)) { if (!keystatus[k]) downs[k]++; keystatus[k] = 1; }
        else { if (keystatus[k]) ups[k]++; keystatus[k] = 0; }
    }
    extended = 0;
}

static unsigned long long rdtsc(void) { unsigned long long v; __asm__ __volatile__("rdtsc" : "=A"(v)); return v; }
static void com2(const char* s) { while (*s) { int i; for (i = 0; i < 100000 && !(inportb(0x2FD) & 0x20); i++) {} outportb(0x2F8, *s++); } }
static int midi_ok = 1, midi_n;
static void mpu_put(int v) { int i; if (!midi_ok) return; for (i = 0; i < 2000 && (inportb(0x331) & 0x40); i++) {} if (i == 2000) { midi_ok = 0; return; } outportb(0x330, v); midi_n++; }

int main(int argc, char** argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 30, i, stuck = 0, ch = 0, lasttick = 0;
    unsigned long long t0, tn, cps;
    unsigned long bt0 = _farpeekl(_dos_ds, 0x46C);
    FILE* f;
    outportb(0x2FB, 0x80); outportb(0x2F8, 1); outportb(0x2F9, 0); outportb(0x2FB, 3); outportb(0x2FA, 0);   /* COM2 115200 8N1 */
    outportb(0x331, 0x3F); for (i = 0; i < 100000; i++) if (!(inportb(0x331) & 0x80)) { inportb(0x330); break; }   /* MPU UART mode */
    newh.pm_offset = (unsigned long)keyhandler; newh.pm_selector = _go32_my_cs();
    _go32_dpmi_get_protected_mode_interrupt_vector(9, &oldh);
    _go32_dpmi_allocate_iret_wrapper(&newh);
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &newh);
    { uclock_t a = uclock(); unsigned long long r = rdtsc(); while (uclock() - a < UCLOCKS_PER_SEC / 2) {} cps = (rdtsc() - r) * 2; }
    com2("KEYGO\r\n");
    t0 = rdtsc();
    for (;;)
    {   /* a chord every 60 ms on 8 channels: the synth stays busy */
        tn = rdtsc();
        if ((tn - t0) / cps >= (unsigned long long)secs) break;
        if ((int)((tn - t0) * 1000 / cps / 60) != lasttick)
        {
            int c;
            lasttick = (int)((tn - t0) * 1000 / cps / 60);
            for (c = 0; c < 8; c++) { mpu_put(0x90 | c); mpu_put(36 + ((ch + c * 7) % 48)); mpu_put(0); }
            ch++;
            for (c = 0; c < 8; c++) { mpu_put(0x90 | c); mpu_put(36 + ((ch + c * 7) % 48)); mpu_put(100); }
        }
    }
    com2("KEYEND\r\n");
    { unsigned long long a = rdtsc(); while (rdtsc() - a < cps * 3) {} }   /* host's last releases arrive */
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &oldh);
    _go32_dpmi_free_iret_wrapper(&newh);
    for (i = 0; i < 16; i++) { mpu_put(0xB0 | i); mpu_put(123); mpu_put(0); }
    f = fopen("C:\\DKEY.TXT", "w");
    fprintf(f, "DKEY %d s: %d bytes read, BIOS ticks %lu, MIDI bytes %d\n", secs, bn, _farpeekl(_dos_ds, 0x46C) - bt0, midi_n);
    for (i = 0; i < bn; i++) fprintf(f, "%02X%s", blog[i], (i % 32) == 31 ? "\n" : " ");
    fprintf(f, "\n");
    for (i = 0; i < 256; i++) if (downs[i] || ups[i]) fprintf(f, "key %s%02X: %d down %d up%s\n", i >= 128 ? "E0 " : "", i & 127, downs[i], ups[i], keystatus[i] ? " STUCK" : "");
    for (i = 0; i < 256; i++) if (keystatus[i]) stuck++;
    fprintf(f, "%s: %d stuck keys\n", stuck ? "FAIL" : "PASS", stuck);
    fclose(f);
    printf("%s: %d stuck keys, %d bytes\n", stuck ? "FAIL" : "PASS", stuck, bn);
    return stuck ? 1 : 0;
}
