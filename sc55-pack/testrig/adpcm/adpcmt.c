/* ADPCMT.EXE - Creative ADPCM DMA test for SBEMU (beta.133, upstream 11d5924 "fix ADPCM crash").
 * Plays 4-, 2.6- and 2-bit ADPCM (DSP 75h/74h, 77h/76h, 17h/16h) in tiny single-cycle blocks (1..8 bytes, the
 * case that crashed in cv_bits_n_to_m) and longer ones, waits for the SB IRQ after each. Writes PASS/FAIL lines.
 * DJGPP: i586-pc-msdosdjgpp-gcc -O2 adpcmt.c -o ADPCMT.EXE. Needs BLASTER (A, I, D). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <time.h>
#include <dos.h>

static int base = 0x220, irq = 7, dma = 1;
static volatile int got;
static _go32_dpmi_seginfo oldv, newv;
static void isr(void) { inportb(base + 0xE); got++; if (irq > 7) outportb(0xA0, 0x20); outportb(0x20, 0x20); }
static void wdsp(int v) { int t = 100000; while ((inportb(base + 0xC) & 0x80) && --t); outportb(base + 0xC, v); }
static int vec(void) { return irq < 8 ? irq + 8 : irq - 8 + 0x70; }

static int play(int cmd, unsigned phys, int len, int ticks)
{
    static const int page[4] = { 0x87, 0x83, 0x81, 0x82 };
    got = 0;
    outportb(0x0A, 4 | dma); outportb(0x0C, 0); outportb(0x0B, 0x48 | dma);
    outportb(dma * 2, phys & 0xFF); outportb(dma * 2, (phys >> 8) & 0xFF); outportb(page[dma], phys >> 16);
    outportb(dma * 2 + 1, (len - 1) & 0xFF); outportb(dma * 2 + 1, (len - 1) >> 8);
    outportb(0x0A, dma);
    wdsp(cmd); wdsp((len - 1) & 0xFF); wdsp((len - 1) >> 8);
    clock_t t0 = clock();
    while (!got && clock() - t0 < ticks) ;
    return got;
}

int main(void)
{
    const char* b = getenv("BLASTER"); const char* p;
    if (b) { if ((p = strchr(b, 'A'))) base = strtol(p + 1, 0, 16); if ((p = strchr(b, 'I'))) irq = atoi(p + 1); if ((p = strchr(b, 'D'))) dma = atoi(p + 1); }
    printf("ADPCMT base %X irq %d dma %d\n", base, irq, dma);
    outportb(base + 6, 1); delay(1); outportb(base + 6, 0); delay(1);
    int t = 1000; while (!(inportb(base + 0xE) & 0x80) && --t); if (inportb(base + 0xA) != 0xAA) { printf("FAIL no DSP\n"); return 1; }
    int sel, seg = __dpmi_allocate_dos_memory(8192 / 16, &sel); if (seg < 0) { printf("FAIL dos mem\n"); return 1; }
    unsigned phys = seg * 16; phys = (phys + 4095) & ~4095u;           /* no 64K page crossing */
    for (int i = 0; i < 4096; i++) _farpokeb(_dos_ds, phys + i, (unsigned char)(0x80 + (i * 37) % 256));
    _go32_dpmi_get_protected_mode_interrupt_vector(vec(), &oldv);
    newv.pm_offset = (unsigned long)isr; newv.pm_selector = _go32_my_cs();
    _go32_dpmi_allocate_iret_wrapper(&newv); _go32_dpmi_set_protected_mode_interrupt_vector(vec(), &newv);
    if (irq < 8) outportb(0x21, inportb(0x21) & ~(1 << irq)); else outportb(0xA1, inportb(0xA1) & ~(1 << (irq - 8)));
    wdsp(0x40); wdsp(0xA6); wdsp(0xD1);                                /* 11 kHz, speaker on */
    static const int cmds[3][2] = { { 0x75, 0x74 }, { 0x77, 0x76 }, { 0x17, 0x16 } };
    static const char* nm[3] = { "4-bit", "2.6-bit", "2-bit" };
    int fails = 0, n = 0;
    n++; if (!play(0x14, phys, 2048, 60)) { printf("no IRQ: 8-bit PCM len 2048\n"); fails++; }
    n++; if (!play(0x14, phys, 4, 30)) { printf("no IRQ: 8-bit PCM len 4\n"); fails++; }
    for (int c = 0; c < 3; c++) { n++; if (!play(cmds[c][0], phys, 2048, 60)) { printf("no IRQ: %s len 2048 (first)\n", nm[c]); fails++; } }
    for (int c = 0; c < 3; c++)
        for (int len = 1; len <= 8; len++)
            for (int ref = 0; ref < 2; ref++) {
                n++;
                if (!play(cmds[c][ref ? 0 : 1], phys, len, 30)) { printf("no IRQ: %s len %d %s\n", nm[c], len, ref ? "ref" : "noref"); fails++; }
            }
    for (int c = 0; c < 3; c++) { n++; if (!play(cmds[c][0], phys, 2048, 60)) { printf("no IRQ: %s len 2048 (after the short ones)\n", nm[c]); fails++; } }
    wdsp(0xD3);
    _go32_dpmi_set_protected_mode_interrupt_vector(vec(), &oldv);
    printf("%d of %d blocks ended with an IRQ\n", n - fails, n);
    printf(fails ? "FAIL\n" : "PASS\n");
    return fails ? 1 : 0;
}
