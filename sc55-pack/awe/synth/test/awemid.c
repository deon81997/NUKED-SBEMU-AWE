/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
/* awemid.c - plays a .MID through awemidi.c + emu8k.c into a WAV (host test of the AWE32 MIDI synth).
 * awemid [-r ROM] [-b BANK.SBK|SF2]... [-m gm|gs|mt32] [-v REV] [-c CHO] [-k RAM_KB] [-o OUT.WAV] [-x] FILE.MID
 *   no -b: the built-in map. -x: dry (no reverb/chorus). Prints the synth's stats at the end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8k.h"
#include "awemidi.h"

typedef struct { uint32_t tick; uint8_t m[256]; int n; int tempo; } Ev;
static Ev* ev; static int nev, cev;
static void add(uint32_t t, const uint8_t* m, int n, int tempo)
{
    if (nev == cev) { cev = cev ? cev * 2 : 4096; ev = (Ev*)realloc(ev, (size_t)cev * sizeof(Ev)); }
    ev[nev].tick = t; ev[nev].n = n > 256 ? 256 : n; memcpy(ev[nev].m, m, (size_t)ev[nev].n); ev[nev].tempo = tempo; nev++;
}
static uint32_t vlq(const uint8_t** p, const uint8_t* e) { uint32_t v = 0; while (*p < e) { uint8_t b = *(*p)++; v = v << 7 | (b & 127); if (!(b & 128)) break; } return v; }
static int cmpev(const void* a, const void* b) { const Ev *x = a, *y = b; return x->tick < y->tick ? -1 : x->tick > y->tick; }
static int div_;
static int load_mid(const char* path)
{
    FILE* f = fopen(path, "rb"); long sz; uint8_t* d; const uint8_t *p, *e; int ntr, i;
    if (!f) return -1;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    d = malloc((size_t)sz); if (fread(d, 1, (size_t)sz, f) != (size_t)sz) return -1; fclose(f);
    if (memcmp(d, "MThd", 4)) return -1;
    ntr = d[10] << 8 | d[11]; div_ = d[12] << 8 | d[13];
    p = d + 8 + (d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7]);
    for (i = 0; i < ntr && p + 8 <= d + sz; i++) {
        uint32_t len = (uint32_t)(p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7]), t = 0; uint8_t rs = 0;
        const uint8_t* q = p + 8; e = q + len; if (e > d + sz) e = d + sz;
        if (memcmp(p, "MTrk", 4)) { p = e; continue; }
        while (q < e) {
            uint8_t b, m[260]; t += vlq(&q, e); if (q >= e) break;
            b = *q;
            if (b == 0xFF) { uint8_t ty = q[1]; uint32_t l; q += 2; l = vlq(&q, e);
                if (ty == 0x51 && l == 3) add(t, m, 0, q[0] << 16 | q[1] << 8 | q[2]); q += l; continue; }
            if (b == 0xF0 || b == 0xF7) { uint32_t l; q++; l = vlq(&q, e); m[0] = 0xF0; if (l > 255) l = 255;
                memcpy(m + (b == 0xF0), q, l); add(t, m, (int)l + (b == 0xF0), 0); q += l; rs = 0; continue; }
            if (b & 0x80) { rs = b; q++; }
            if (!rs) { q++; continue; }
            m[0] = rs; m[1] = *q++; i = i;
            if ((rs & 0xE0) != 0xC0) { m[2] = *q++; add(t, m, 3, 0); } else add(t, m, 2, 0);
        }
        p = e;
    }
    qsort(ev, (size_t)nev, sizeof(Ev), cmpev);   /* stable enough: tracks merged by tick */
    return 0;
}
static void wav_hdr(FILE* f, uint32_t frames)
{
    uint32_t b = frames * 4, v;
    fwrite("RIFF", 1, 4, f); v = 36 + b; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); { uint16_t h[2] = {1, 2}; fwrite(h, 2, 2, f); }
    v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f); { uint16_t h[2] = {4, 16}; fwrite(h, 2, 2, f); }
    fwrite("data", 1, 4, f); fwrite(&b, 4, 1, f);
}
int main(int argc, char** argv)
{
    const char *rom = NULL, *out = "out.wav", *mid = NULL, *bp[4]; int nb = 0, mode = AWEM_GM, rev = 4, cho = 2, ram = 512, dry = 0, i;
    AwemBank* banks[4]; FILE* f; double us = 0, us_per_tick; int tempo = 500000; uint32_t lastt = 0, frames = 0; double fpos = 0;
    int16_t buf[1024 * 2];
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-r")) rom = argv[++i];
        else if (!strcmp(argv[i], "-b")) bp[nb++] = argv[++i];
        else if (!strcmp(argv[i], "-m")) { i++; mode = !strcmp(argv[i], "gs") ? AWEM_GS : !strcmp(argv[i], "mt32") ? AWEM_MT32 : AWEM_GM; }
        else if (!strcmp(argv[i], "-v")) rev = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-c")) cho = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-k")) ram = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o")) out = argv[++i];
        else if (!strcmp(argv[i], "-x")) dry = 1;
        else mid = argv[i];
    }
    if (!mid || load_mid(mid)) { fprintf(stderr, "usage: awemid [-r ROM] [-b BANK]... [-m gm|gs|mt32] [-o OUT.WAV] FILE.MID\n"); return 2; }
    if (emu8k_init(ram)) return 3;
    if (rom && emu8k_load_rom(rom)) { fprintf(stderr, "ROM?\n"); return 3; }
    emu8k_set_fx(!dry);
    for (i = 0; i < nb; i++) {
        char err[128] = ""; int kind = -1; char nm[64];
        int pr = awem_probe(bp[i], &kind, nm, sizeof nm);
        banks[i] = awem_bank_load(bp[i], (uint32_t)ram * 512u, err, sizeof err);
        if (!banks[i]) { fprintf(stderr, "%s: %s\n", bp[i], err); return 4; }
        printf("bank %s: probe %d kind %d name '%s', %d presets, RAM %u words, skipped %d\n", bp[i], pr, kind, nm,
               awem_bank_presets(banks[i]), awem_bank_ram_used(banks[i]), awem_bank_skipped(banks[i]));
    }
    if (!nb) { banks[0] = awem_bank_builtin(); nb = 1; }
    awem_start();
    awem_set_banks(banks, nb, mode);
    awem_set_fx(rev, cho);
    awem_reset();
    f = fopen(out, "wb"); wav_hdr(f, 0);
    us_per_tick = (double)tempo / div_;
    for (i = 0; i <= nev; i++) {
        double t_us; uint32_t target;
        if (i < nev) { us += (ev[i].tick - lastt) * us_per_tick; lastt = ev[i].tick; t_us = us; }
        else t_us = us + 3e6;   /* 3 s tail */
        target = (uint32_t)(t_us * 44.1 / 1000.0);
        while (frames < target) {
            int n = (int)(target - frames); if (n > 1024) n = 1024;
            emu8k_render(buf, n, 44100); awem_advance(n); fwrite(buf, 4, (size_t)n, f); frames += (uint32_t)n;
        }
        if (i == nev) break;
        if (ev[i].tempo) { tempo = ev[i].tempo; us_per_tick = (double)tempo / div_; continue; }
        { int k; for (k = 0; k < ev[i].n; k++) awem_byte(ev[i].m[k]); if (ev[i].m[0] == 0xF0 && ev[i].m[ev[i].n - 1] != 0xF7) awem_byte(0xF7); }
        (void)fpos;
    }
    fseek(f, 0, SEEK_SET); wav_hdr(f, frames); fclose(f);
    { AwemStats s; awem_stats(&s);
      printf("frames %u (%.1f s) bytes %u notes %u voices_max %u stolen %u missing %u active_end %u\n", frames, frames / 44100.0,
             s.bytes, s.notes, s.voices_max, s.stolen, s.missing, s.active); }
    return 0;
}
