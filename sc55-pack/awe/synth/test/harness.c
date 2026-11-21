/* harness.c - feed an EMU8000 register script, write a stereo WAV.
 *
 *   harness [-r rate] [-o out.wav] [-q] script.awe
 *   harness [-o out.wav] [-q] [-rom ROM] [-ram KB] [-P probe.txt] [-solo CH] [-t SECS] -s AWEREC.BIN
 *
 * -tone: real-card output EQ on (emu8k_set_tone; off by default here).
 * -s replays a port stream recorded by an AWEREC SBEMU build (sbemu/vawe.c: every word access with
 * its time) at 44100 Hz; -P writes "frame ch pitch vol cut addr" every 64 frames for each sounding voice
 * (the CPF/CVCF/CCCA current values); -solo CH plays only that voice (every other IFATN write gets
 * attenuation 0xFF, filter byte kept); -t stops after SECS seconds of the stream.
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
void     be_probe(int ch, uint32_t *pitch, uint32_t *vol, uint32_t *cut, uint32_t *addr);

#ifndef REF86
#include "emu8k.h"
static int out_rate = 44100, g_tone;
int be_init(int ram_kb, const char *rom)
{
    emu8k_set_tone(g_tone);   /* the output EQ is off unless -tone: the engine is compared with 86Box's */
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
void be_probe(int ch, uint32_t *pitch, uint32_t *vol, uint32_t *cut, uint32_t *addr)
{
    uint32_t cvcf = emu8k_read(EMU8K_CVCF, ch);
    *pitch = emu8k_read(EMU8K_CPF, ch) >> 16; *vol = cvcf >> 16; *cut = cvcf & 0xFFFF;
    *addr = emu8k_read(EMU8K_CCCA, ch) & 0xFFFFFF;
}
#else
static int out_rate = 44100, g_tone;
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


/* ---- -s: replay an AWEREC stream ---- */
static FILE *probe_f;
static void render_probed(long frames)
{
    while (frames > 0) {
        long k = 64 - (wav_frames & 63);
        if (k > frames) k = frames;
        render(k);
        frames -= k;
        if (probe_f && !(wav_frames & 63)) {
            int c;
            for (c = 0; c < 32; c++) {
                uint32_t pi, vo, cu, ad;
                be_probe(c, &pi, &vo, &cu, &ad);
                if (vo)
                    fprintf(probe_f, "%ld %d %u %u %u %u\n", wav_frames, c, pi, vo, cu, ad);
            }
        }
    }
}
static int replay(const char *path, int ram_kb, const char *rom, int solo, double secs)
{
    FILE *f = fopen(path, "rb");
    unsigned char h[16], r[8];
    uint32_t hz, n, i, t0 = 0, ptr = 0;
    if (!f || fread(h, 1, 16, f) != 16 || memcmp(h, "AWEREC1", 8)) { fprintf(stderr, "%s: not an AWEREC1 file\n", path); return 1; }
    memcpy(&hz, h + 8, 4); memcpy(&n, h + 12, 4);
    if (be_init(ram_kb, rom)) return 1;
    for (i = 0; i < n && fread(r, 1, 8, f) == 8; i++) {
        uint32_t t; uint16_t off, val; long fr;
        memcpy(&t, r, 4); memcpy(&off, r + 4, 2); memcpy(&val, r + 6, 2);
        if (!i) t0 = t;
        fr = (long) ((double) (uint32_t) (t - t0) * out_rate / hz);   /* output frames (-r) */
        if (secs > 0 && fr > secs * out_rate) break;
        if (fr > wav_frames) render_probed(fr - wav_frames);
        if (off & 0x8000) {
            off &= 0x7FFF;
            if (off == 0x802) ptr = val;
            if (solo >= 0 && off == 0x800 && ((ptr >> 5) & 7) == 1 && (int) (ptr & 31) != solo) val |= 0xFF;
            be_io_w(off, val);
        } else
            (void) be_io_r(off);
    }
    render_probed(out_rate);   /* 1 s tail */
    fclose(f);
    return 0;
}
int main(int argc, char **argv)
{
    const char *out = "out.wav", *script = NULL, *rom = NULL;
    char line[512];
    int ram_kb = 512, inited = 0, i, lineno = 0, solo = -1;
    const char *stream = NULL;
    double secs = 0;
    FILE *f;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            out = argv[++i];
        else if (!strcmp(argv[i], "-r") && i + 1 < argc)
            out_rate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-q"))
            quiet = 1;
        else if (!strcmp(argv[i], "-s") && i + 1 < argc)
            stream = argv[++i];
        else if (!strcmp(argv[i], "-rom") && i + 1 < argc)
            rom = argv[++i];
        else if (!strcmp(argv[i], "-ram") && i + 1 < argc)
            ram_kb = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-P") && i + 1 < argc)
            probe_f = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "-tone"))
            g_tone = 1;
        else if (!strcmp(argv[i], "-solo") && i + 1 < argc)
            solo = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc)
            secs = atof(argv[++i]);
        else
            script = argv[i];
    }
    if (stream) {
        if (replay(stream, ram_kb, rom, solo, secs) || write_wav(out))
            return 1;
        if (probe_f) fclose(probe_f);
        if (!quiet) printf("%s: %ld frames\n", out, wav_frames);
        return 0;
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
