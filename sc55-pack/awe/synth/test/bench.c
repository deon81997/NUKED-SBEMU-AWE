/* bench.c - EMU8000 engine speed: time to render audio with N active voices.
 *   bench [seconds]        (host or DOS: EMU8KBEN.EXE under HDPMI/CWSDPMI)
 * Prints ms per second of 44.1 kHz audio, CPU % of real time and ns per
 * voice-sample for: idle, 32 voices plain, 32 voices filter+LFO+env,
 * 32 released voices (envelope engine still on, silent). */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "emu8k.h"
#ifdef __DJGPP__
#include <time.h>
static double now(void) { return uclock() / (double) UCLOCKS_PER_SEC; }
#else
#include <time.h>
static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
#endif

static int16_t buf[2 * 4410];

static void setup(int voices, int heavy)
{
    uint32_t i, a = EMU8K_RAM_START;
    int ch;
    emu8k_init(512);
    emu8k_write(EMU8K_CFG1, 31, 4); /* HWCF3: audio on */
    for (i = 0; i < 40000; i++)
        emu8k_ram_write(a + i, (uint16_t) (int16_t) ((int) (i % 400) * 80 - 16000));
    for (ch = 0; ch < voices; ch++) {
        emu8k_write(EMU8K_DCYSUSV, ch, 0x80);
        emu8k_write(EMU8K_PTRX, ch, 0x40000000);
        emu8k_write(EMU8K_CPF, ch, 0x40000000);
        emu8k_write(EMU8K_ENVVOL, ch, 0x8000);
        emu8k_write(EMU8K_ENVVAL, ch, 0x8000);
        emu8k_write(EMU8K_DCYSUS, ch, 0x7F7F);
        emu8k_write(EMU8K_ATKHLDV, ch, 0x7F7F);
        emu8k_write(EMU8K_ATKHLD, ch, 0x7F20);
        emu8k_write(EMU8K_LFO1VAL, ch, 0x8000);
        emu8k_write(EMU8K_LFO2VAL, ch, 0x8000);
        emu8k_write(EMU8K_IP, ch, 0xC000 + ch * 0x155);
        emu8k_write(EMU8K_IFATN, ch, heavy ? 0x9030 : 0xFF30);
        emu8k_write(EMU8K_PEFE, ch, heavy ? 0x1040 : 0);
        emu8k_write(EMU8K_FMMOD, ch, heavy ? 0x0810 : 0);
        emu8k_write(EMU8K_TREMFRQ, ch, heavy ? 0x1020 : 0);
        emu8k_write(EMU8K_FM2FRQ2, ch, heavy ? 0x0818 : 0);
        emu8k_write(EMU8K_PSST, ch, ((uint32_t) (ch * 8) << 24) | (a + 20000));
        emu8k_write(EMU8K_CSL, ch, a + 36000);
        emu8k_write(EMU8K_CCCA, ch, ((uint32_t) (heavy ? ch % 16 : 0) << 28) | a);
        emu8k_write(EMU8K_DCYSUSV, ch, 0x7F7F);
    }
}

static void run(const char *name, int voices, int heavy, double secs)
{
    double t0, t;
    long frames = 0, total = (long) (secs * 44100);
    setup(voices, heavy > 0);
    if (heavy < 0) { /* release all, let them finish, then time the idle-but-on voices */
        int ch, i;
        for (ch = 0; ch < voices; ch++)
            emu8k_write(EMU8K_DCYSUSV, ch, 0x807F);
        for (i = 0; i < 20; i++)
            emu8k_render(buf, 4410, 44100);
    }
    t0 = now();
    while (frames < total) {
        emu8k_render(buf, 4410, 44100);
        frames += 4410;
    }
    t = now() - t0;
    printf("%-24s %8.1f ms/s  %6.2f %% CPU  %7.1f ns/voice-sample\n", name, t * 1000.0 / secs,
           t * 100.0 / secs, voices ? t * 1e9 / ((double) frames * voices) : 0.0);
}

int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 10.0;
    printf("EMU8000 engine bench, %.0f s of 44.1 kHz audio per case\n", secs);
    run("idle (0 voices)", 0, 0, secs);
    run("32 voices plain", 32, 0, secs);
    run("32 voices filt+LFO+env", 32, 1, secs);
    run("32 voices released", 32, -1, secs);
    emu8k_close();
    return 0;
}
