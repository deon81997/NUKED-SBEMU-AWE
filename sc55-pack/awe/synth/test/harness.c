/* harness.c - feed an EMU8000 register script, write a stereo WAV.
 *
 *   harness [-r rate] [-o out.wav] [-q] script.awe
 *
 * Built against emu8k.c (default) or, with -DREF86, against 86Box's
 * snd_emu8k.c (see fetch_build.sh) for reference renders. Script syntax:
 * README.md "Register scripts". Everything goes through 16-bit port
 * writes (emu8k_io_write), exactly as a trapped game would do it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <stdint.h>

int      be_init(int ram_kb, const char *rom);
void     be_io_w(int off, uint16_t v);
uint16_t be_io_r(int off);
void     be_render(int16_t *out, int frames);

#ifndef REF86
#include "emu8k.h"
static int out_rate = 44100;
int be_init(int ram_kb, const char *rom)
{
    if (emu8k_init(ram_kb))
        return -1;
    if (rom && emu8k_load_rom(rom)) {
        fprintf(stderr, "cannot load ROM %s\n", rom);
        return -1;
    }
    return 0;
}
void be_io_w(int off, uint16_t v) { emu8k_io_write(off, v); }
uint16_t be_io_r(int off) { return emu8k_io_read(off); }
void be_render(int16_t *out, int frames) { emu8k_render(out, frames, out_rate); }
#else
static int out_rate = 44100;
#endif

typedef struct { const char *name; int port, r, ch, dword; } regname_t;
static const regname_t names[] = {
    {"CPF", 0, 0, -1, 1}, {"PTRX", 0, 1, -1, 1}, {"CVCF", 0, 2, -1, 1}, {"VTFT", 0, 3, -1, 1},
    {"Z2", 0, 4, -1, 1}, {"Z1", 0, 5, -1, 1}, {"PSST", 0, 6, -1, 1}, {"CSL", 0, 7, -1, 1},
    {"CCCA", 1, 0, -1, 1}, {"HWCF4", 1, 1, 9, 1}, {"HWCF5", 1, 1, 10, 1}, {"HWCF6", 1, 1, 13, 1},
    {"HWCF7", 1, 1, 14, 1}, {"SMALR", 1, 1, 20, 1}, {"SMARR", 1, 1, 21, 1}, {"SMALW", 1, 1, 22, 1},
    {"SMARW", 1, 1, 23, 1}, {"SMLD", 1, 1, 26, 0}, {"HWCF1", 1, 1, 29, 0}, {"HWCF2", 1, 1, 30, 0},
    {"HWCF3", 1, 1, 31, 0}, {"SMRD", 2, 1, 26, 0}, {"WC", 2, 1, 27, 0},
    {"INIT1", 1, 2, -1, 0}, {"INIT3", 1, 3, -1, 0}, {"ENVVOL", 1, 4, -1, 0}, {"DCYSUSV", 1, 5, -1, 0},
    {"ENVVAL", 1, 6, -1, 0}, {"DCYSUS", 1, 7, -1, 0}, {"INIT2", 2, 2, -1, 0}, {"INIT4", 2, 3, -1, 0},
    {"ATKHLDV", 2, 4, -1, 0}, {"LFO1VAL", 2, 5, -1, 0}, {"ATKHLD", 2, 6, -1, 0}, {"LFO2VAL", 2, 7, -1, 0},
    {"IP", 3, 0, -1, 0}, {"IFATN", 3, 1, -1, 0}, {"PEFE", 3, 2, -1, 0}, {"FMMOD", 3, 3, -1, 0},
    {"TREMFRQ", 3, 4, -1, 0}, {"FM2FRQ2", 3, 5, -1, 0}, {"ID", 3, 7, -1, 0}, {NULL, 0, 0, 0, 0}};

static const int port_off[4] = {0x000, 0x400, 0x402, 0x800};
static int16_t *wav;
static long wav_frames, wav_cap;
static int quiet;

static void render(long frames)
{
    if (wav_frames + frames > wav_cap) {
        wav_cap = (wav_frames + frames) * 2;
        wav = realloc(wav, (size_t) wav_cap * 4);
        if (!wav) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
    }
    be_render(wav + 2 * wav_frames, (int) frames);
    wav_frames += frames;
}

static const regname_t *lookup(const char *n)
{
    int i;
    for (i = 0; names[i].name; i++)
        if (!strcasecmp(n, names[i].name))
            return &names[i];
    return NULL;
}

static void wreg(const regname_t *rn, int ch, uint32_t v)
{
    if (rn->ch >= 0)
        ch = rn->ch;
    be_io_w(0x802, (uint16_t) ((rn->r << 5) | ch));
    if (rn->port == 0) {
        be_io_w(0x000, (uint16_t) v);
        be_io_w(0x002, (uint16_t) (v >> 16));
    } else if (rn->dword) {
        be_io_w(0x400, (uint16_t) v);
        be_io_w(0x402, (uint16_t) (v >> 16));
    } else
        be_io_w(port_off[rn->port], (uint16_t) v);
}

static uint32_t rreg(const regname_t *rn, int ch)
{
    if (rn->ch >= 0)
        ch = rn->ch;
    be_io_w(0x802, (uint16_t) ((rn->r << 5) | ch));
    if (rn->port == 0)
        return be_io_r(0x000) | ((uint32_t) be_io_r(0x002) << 16);
    if (rn->dword)
        return be_io_r(0x400) | ((uint32_t) be_io_r(0x402) << 16);
    return be_io_r(port_off[rn->port]);
}

static void fill(uint32_t addr, uint32_t len, const char *kind, double period, double amp)
{
    const regname_t *smalw = lookup("SMALW"), *smld = lookup("SMLD");
    uint32_t i, seed = 12345;
    wreg(smalw, 0, addr);
    be_io_w(0x802, (uint16_t) ((1 << 5) | 26));
    for (i = 0; i < len; i++) {
        double ph = fmod(i / period, 1.0), s;
        if (!strcmp(kind, "sine"))
            s = sin(2 * 3.14159265358979 * ph);
        else if (!strcmp(kind, "saw"))
            s = 2 * ph - 1;
        else if (!strcmp(kind, "square"))
            s = ph < 0.5 ? 1 : -1;
        else {
            seed = seed * 1103515245u + 12345u;
            s = ((seed >> 8) & 0xFFFF) / 32768.0 - 1.0;
        }
        be_io_w(0x400, (uint16_t) (int16_t) lrint(s * amp));
    }
    (void) smld;
}

static void put16(FILE *f, int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void put32(FILE *f, long v) { put16(f, (int) (v & 0xFFFF)); put16(f, (int) ((v >> 16) & 0xFFFF)); }

static int write_wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    long i;
    if (!f)
        return -1;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + wav_frames * 4);
    fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, 2);
    put32(f, out_rate); put32(f, out_rate * 4L); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, wav_frames * 4);
    for (i = 0; i < wav_frames * 2; i++)
        put16(f, wav[i]);
    return fclose(f);
}

int main(int argc, char **argv)
{
    const char *out = "out.wav", *script = NULL, *rom = NULL;
    char line[512];
    int ram_kb = 512, inited = 0, i, lineno = 0;
    FILE *f;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            out = argv[++i];
        else if (!strcmp(argv[i], "-r") && i + 1 < argc)
            out_rate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-q"))
            quiet = 1;
        else
            script = argv[i];
    }
    if (!script || !(f = fopen(script, "r"))) {
        fprintf(stderr, "usage: harness [-r rate] [-o out.wav] [-q] script.awe\n");
        return 1;
    }
    while (fgets(line, sizeof(line), f)) {
        char cmd[32] = "", a[256] = "", b[64] = "", c[64] = "", d[64] = "", e[64] = "";
        int n;
        lineno++;
        if (line[0] == '#' || (n = sscanf(line, "%31s %255s %63s %63s %63s %63s", cmd, a, b, c, d, e)) < 1)
            continue;
        if (!strcmp(cmd, "ram")) { ram_kb = atoi(a); continue; }
        if (!strcmp(cmd, "rom")) {
            static char rp[256];
            const char *env = getenv("AWEROM");
            snprintf(rp, sizeof(rp), "%s", (!strcmp(a, "$AWEROM") && env) ? env : a);
            rom = rp;
            continue;
        }
        if (!inited) {
            if (be_init(ram_kb, rom))
                return 1;
            inited = 1;
        }
        if (!strcmp(cmd, "w") && n >= 4) {
            const regname_t *rn = lookup(a);
            if (!rn) { fprintf(stderr, "%s:%d: unknown register %s\n", script, lineno, a); return 1; }
            wreg(rn, (int) strtol(b, NULL, 0), (uint32_t) strtoul(c, NULL, 0));
        } else if (!strcmp(cmd, "rd") && n >= 3) {
            const regname_t *rn = lookup(a);
            if (!rn) { fprintf(stderr, "%s:%d: unknown register %s\n", script, lineno, a); return 1; }
            printf("%s %s = 0x%08X\n", a, b, (unsigned) rreg(rn, (int) strtol(b, NULL, 0)));
        } else if (!strcmp(cmd, "io") && n >= 3)
            be_io_w((int) strtol(a, NULL, 0), (uint16_t) strtoul(b, NULL, 0));
        else if (!strcmp(cmd, "ior") && n >= 2)
            printf("io %s = 0x%04X\n", a, be_io_r((int) strtol(a, NULL, 0)));
        else if (!strcmp(cmd, "wait") && n >= 2)
            render(atol(a) * out_rate / 44100);
        else if (!strcmp(cmd, "ms") && n >= 2)
            render(atol(a) * out_rate / 1000);
        else if (!strcmp(cmd, "fill") && n >= 6)
            fill((uint32_t) strtoul(a, NULL, 0), (uint32_t) strtoul(b, NULL, 0), c, atof(d), atof(e));
        else {
            fprintf(stderr, "%s:%d: bad line: %s", script, lineno, line);
            return 1;
        }
    }
    fclose(f);
    if (write_wav(out))
        return 1;
    if (!quiet)
        printf("%s: %ld frames\n", out, wav_frames);
    free(wav);
    return 0;
}
