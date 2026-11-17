/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
 * SC55SF - builds SC55.SF2 from the user's own SC-55 ROMs (found by content, file names do not matter).
 * Usage: SC55SF ROMDIR [OUT.SF2] [-s OUT.SBK [-k AWERAM_KB]] [-q] [-v]   (-q: no SC-55 ROMs = exit 3, silent)
 * Nothing Roland ships with this tool: the instruments, partials, samples and drum kits are read from the
 * control ROM, the lookup tables from the CPU ROM and the PCM from the wave ROMs, on the user's PC.
 * Layout: docs/SC55ROM.md (from libEmuSC by Hakon Skjelten, LGPL-2.1+, and Nuked-SC55's wave unscramble).
 * Builds on Linux (gcc) and DJGPP (tools/makefile). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define MAXF 64
static int verbose, quiet;
static uint8_t *ctrl, *cpu, *wave; static long wavesz; static int mk2;
static void die(const char *m) { printf("SC55SF: error: %s\n", m); exit(1); }
static unsigned be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static unsigned be24(const uint8_t *p) { return (p[0] << 16) | (p[1] << 8) | p[2]; }

static uint8_t *loadf(const char *path, long *sz) {
    FILE *f = fopen(path, "rb"); uint8_t *b; long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(n ? n : 1); if (!b) die("out of memory");
    if (fread(b, 1, n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f); *sz = n; return b;
}

/* ---- ROM identification by content ---- */
static int cpu_ok(const uint8_t *c, const unsigned *lut) {
    int i; unsigned p = 0;
    for (i = 0; i < 128; i++) { unsigned v = be16(c + lut[3] + 2 * i); if (v < p) return 0; p = v; }   /* EnvelopeTime rises */
    if (be16(c + lut[3]) != 0 || p < 10000) return 0;
    if (be16(c + lut[0] + 20) != 32767 || be16(c + lut[0]) != 0) return 0;                     /* PitchParamScale[10] */
    if (c[lut[25]] != 255 || c[lut[25] + 127] != 0) return 0;                                  /* TVALevelIndex */
    for (i = 1, p = 0; i < 256; i++) { if (c[lut[26] + i] < p) return 0; p = c[lut[26] + i]; } /* TVALevel rises */
    return 1;
}
/* CPU LUT offsets (libEmuSC control_rom.h): 0 PitchParamScale 1 EnvTimeKFSens 2 EnvTimeScale 3 EnvelopeTime ...
   12 TVFCutoffFreq 14 TVFResonance 25 TVALevelIndex 26 TVALevel */
static const unsigned LUT1[29] = { 0x14c6, 0x679a, 0x67c6, 0x6f12, 0x7012, 0x7112, 0x7212, 0x7312, 0x7412, 0x74d2,
    0x74fc, 0x7512, 0x7612, 0x7714, 0x7816, 0x78c6, 0x78dc, 0x78f2, 0x79f2, 0x7a32, 0x67ba, 0x6b06, 0x6d10, 0x69c6,
    0x6c8f, 0x6b0f, 0x6b8f, 0x7b7a, 0x7d7a };
static const unsigned LUT2[29] = { 0x1310, 0x650e, 0x653a, 0x6c86, 0x6486, 0x6e86, 0x6f86, 0x7086, 0x7186, 0x7246,
    0x7270, 0x7286, 0x7386, 0x7488, 0x758a, 0x763a, 0x7650, 0x7666, 0x7766, 0x77a6, 0x652e, 0x687a, 0x6a84, 0x673a,
    0x6a03, 0x6883, 0x6903, 0x78ee, 0x7aee };
static const unsigned *LUT;

/* ---- control ROM structures ---- */
typedef struct { uint8_t d[92]; } Part;  /* instrument partial, raw bytes */
typedef struct { char name[13]; uint8_t vol, used, pcurve; Part p[2]; } Inst;
typedef struct { uint8_t breaks[16]; uint16_t smp[16]; } Prt;
typedef struct { uint8_t vol, mode, root; uint32_t addr; uint16_t len, llen, pinit, psus; int sf; } Smp;
typedef struct { uint16_t preset[128]; uint8_t vol[128], key[128], grp[128], pan[128], rev[128], cho[128], flg[128];
                 char name[13]; } Drum;
static Inst inst[1024]; static int ninst;
static Prt prt[1024]; static int nprt;
static Smp smp[2048]; static int nsmp;
static Drum drum[32]; static int ndrum;
static uint16_t var[128][128]; static uint8_t drumlut[128];
static const unsigned BK[8] = { 0x10000, 0x1BD00, 0x1DEC0, 0x20000, 0x2BD00, 0x2DEC0, 0x30000, 0x38000 };

static void parse_ctrl(void) {
    unsigned x; int i, j;
    for (x = BK[0]; x < BK[4]; x += 216) {
        Inst *n;
        if (x == BK[1]) x = BK[3];
        if (!ctrl[x]) continue;
        if (ninst >= 1024) die("too many instruments");
        n = &inst[ninst++]; memcpy(n->name, ctrl + x, 12); n->name[12] = 0;
        n->vol = ctrl[x + 12]; n->used = ctrl[x + 18]; n->pcurve = ctrl[x + 19];
        memcpy(n->p[0].d, ctrl + x + 32, 92); memcpy(n->p[1].d, ctrl + x + 124, 92);
    }
    for (x = BK[1]; x < BK[5]; x += 60) {
        if (x == BK[2]) x = BK[4];
        if (!ctrl[x]) continue;
        if (nprt >= 1024) die("too many partials");
        memcpy(prt[nprt].breaks, ctrl + x + 12, 16);
        for (i = 0; i < 16; i++) prt[nprt].smp[i] = be16(ctrl + x + 28 + 2 * i);
        nprt++;
    }
    for (x = BK[2]; x < BK[6]; x += 16) {
        const uint8_t *s; Smp *m;
        if (x == BK[3]) x = BK[5];
        s = ctrl + x;
        if (!be16(s + 6)) continue;
        if (nsmp >= 2048) die("too many samples");
        m = &smp[nsmp++]; m->vol = s[0]; m->addr = be24(s + 1); m->len = be16(s + 6); m->llen = be16(s + 8);
        m->mode = s[10]; m->root = s[11]; m->pinit = be16(s + 12); m->psus = be16(s + 14); m->sf = -1;
    }
    for (i = 0; i < 128; i++) for (j = 0; j < 128; j++) var[i][j] = be16(ctrl + BK[6] + (i * 128 + j) * 2);
    memcpy(drumlut, ctrl + BK[7], 128);
    for (x = BK[7] + 128; x + 1164 <= 0x3C028; x += 1164) {
        const uint8_t *s = ctrl + x; Drum *d;
        if (!memcmp(s + 1152, "AC.", 3) || s[1152] >= 0x80) continue;
        if (ndrum >= 32) break;
        d = &drum[ndrum++];
        for (i = 0; i < 128; i++) d->preset[i] = be16(s + 2 * i);
        memcpy(d->vol, s + 256, 128); memcpy(d->key, s + 384, 128); memcpy(d->grp, s + 512, 128);
        memcpy(d->pan, s + 640, 128); memcpy(d->rev, s + 768, 128); memcpy(d->cho, s + 896, 128);
        memcpy(d->flg, s + 1024, 128); memcpy(d->name, s + 1152, 12); d->name[12] = 0;
    }
}

/* ---- wave ROM: unscramble + DPCM decode ---- */
static const int AA[20] = { 2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19 };
static const int DD[8] = { 2, 0, 4, 5, 7, 6, 3, 1 };
static void unscramble(const uint8_t *src, uint8_t *dst) {   /* one 1 MB block */
    static uint8_t dt[256]; static int init; uint32_t i; int b;
    if (!init) { for (i = 0; i < 256; i++) { int v = 0; for (b = 0; b < 8; b++) v |= ((i >> DD[b]) & 1) << b; dt[i] = v; } init = 1; }
    for (i = 0; i < 0x100000; i++) {
        uint32_t a = 0;
        if (i >= 0x20) { for (b = 0; b < 20; b++) a |= ((i >> AA[b]) & 1) << b; dst[a] = dt[src[i]]; }
        else dst[i] = src[i];
    }
}
static uint32_t romaddr(uint32_t a) {
    uint32_t bank;
    switch ((a & 0x700000) >> 20) {
    case 0: bank = 0; break;
    case 1: bank = 0x100000; break;
    case 2: bank = mk2 ? 0x200000 : 0x100000; break;
    case 4: bank = 0x200000; break;
    default: return 0xFFFFFFFF;
    }
    return (a & 0xFFFFF) | bank;
}
/* decode sample m into out (float), returns count; out may be NULL to count */
static int decode(const Smp *m, float *out) {
    uint32_t ra = romaddr(m->addr), i; float acc = 0; int n = m->len + 1, span = m->llen, ls = m->len - m->llen;
    if (ra == 0xFFFFFFFF || ra + n > (uint32_t)wavesz) return 0;
    if (m->mode == 1 && span > 0) n += span + 1;
    if (!out) return n;
    for (i = 0; i <= m->len; i++) {
        uint32_t a = ra + i; int8_t d = (int8_t)wave[a];
        uint8_t sb = wave[((a & 0xFFFFF) >> 5) | (a & 0xF00000)];
        int nib = (a & 0x10) ? (sb >> 4) : (sb & 15);
        acc += (float)d * ldexpf(1.0f, nib - 17); out[i] = acc;
    }
    if (m->mode == 1 && span > 0) {   /* ping-pong: turn + mirrored segment (as libEmuSC) */
        float sl = ls > 0 ? out[ls - 1] : 0, se = out[m->len - 1], sed = out[m->len]; int k, o = m->len + 1;
        out[o++] = sl + (sed - se);
        for (k = 1; k < span; k++) out[o++] = 2.0f * sl - out[m->len - k - 1];
        out[o++] = sl;
    }
    return n;
}
/* how well looped samples join up with this wave ROM order (lower = better) */
static double wave_score(int slot) {   /* only samples that live in 1 MB slot (-1 = all) */
    int i, n = 0; double s = 0; static float buf[70000];
    for (i = 0; i < nsmp; i++) {
        Smp *m = &smp[i]; int c; uint32_t ra = romaddr(m->addr);
        if (m->mode != 0 || m->llen < 8 || m->llen > m->len || m->len >= 65000 || ra == 0xFFFFFFFF) continue;
        if (slot >= 0 && (int)(ra >> 20) != slot) continue;
        if (slot < 0 && (i % 7)) continue;
        c = decode(m, buf); if (!c) return 1e9;
        s += fabs(buf[m->len] - buf[m->len - m->llen]) + fabs(buf[m->len] - buf[m->len - 1]); n++;
    }
    return n ? s / n : 1e9;
}

/* ---- SF2 writer ---- */
typedef struct { uint8_t *b; long n, cap; } Buf;
static void put(Buf *o, const void *p, long n) {
    if (o->n + n > o->cap) { o->cap = (o->n + n) + (o->n + n) / 2 + 4096; o->b = realloc(o->b, o->cap); if (!o->b) die("out of memory"); }
    memcpy(o->b + o->n, p, n); o->n += n;
}
static void put16(Buf *o, unsigned v) { uint8_t b[2] = { v & 255, (v >> 8) & 255 }; put(o, b, 2); }
static void put32(Buf *o, uint32_t v) { uint8_t b[4] = { v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24 }; put(o, b, 4); }
static void putname(Buf *o, const char *s) { char n[20]; memset(n, 0, 20); strncpy(n, s, 19); put(o, n, 20); }

static int envT_scale = 550, envR_scale = 350;   /* % calibration knobs (testrig/sc55sf) */
static double gain_db = 0; static int nofilt;
static Buf shdr, igen, ibag, inst_, pgen, pbag, phdr;
static int nshdr, nigen, nibag, ninst_, npgen, npbag, nphdr;
static int16_t *pcm; static long npcm, pcmcap;

static double sec2tc(double s) { if (s < 0.001) return -12000; return 1200.0 * log2(s); }
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

static int sf_sample(int si) {   /* returns SF2 sample index */
    Smp *m = &smp[si]; int n, i, ls, le; float *f; char nm[20];
    if (m->sf >= 0) return m->sf;
    n = decode(m, NULL); if (n <= 0) return -1;
    f = malloc(sizeof(float) * n); decode(m, f);
    if (npcm + n + 46 > pcmcap) { pcmcap = (npcm + n + 46) + (npcm + n + 46) / 4 + 65536; pcm = realloc(pcm, pcmcap * 2); if (!pcm) die("out of memory"); }
    for (i = 0; i < n; i++) pcm[npcm + i] = (int16_t)clampi((int)lrintf(f[i] * 32767.0f), -32768, 32767);
    free(f);
    memset(pcm + npcm + n, 0, 46 * 2);
    if (m->mode == 1 && m->llen > 0) { ls = m->len - m->llen; le = n; }
    else if (m->mode == 0) { ls = m->len - m->llen; le = m->len + 1; }
    else { ls = n - 1; le = n; }
    if (ls < 0) ls = 0;
    if (le <= ls) le = ls + 1;
    sprintf(nm, "S%04d", si); putname(&shdr, nm);
    put32(&shdr, npcm); put32(&shdr, npcm + n); put32(&shdr, npcm + ls); put32(&shdr, npcm + le);
    put32(&shdr, 32000); { uint8_t b[2] = { 60, 0 }; put(&shdr, b, 2); } put16(&shdr, 0); put16(&shdr, 1);
    npcm += n + 46; m->sf = nshdr++;
    return m->sf;
}
static void gen(Buf *g, int *cnt, int op, int v) { put16(g, op); put16(g, (uint16_t)(int16_t)v); (*cnt)++; }
static void genr(Buf *g, int *cnt, int op, int lo, int hi) { put16(g, op); { uint8_t b[2] = { lo, hi }; put(g, b, 2); } (*cnt)++; }

static double etime(int t) { return be16(cpu + LUT[3] + 2 * (t & 0x7f)) * 8.0 / 32000.0; }

/* TVA envelope: simulate the firmware's 5 phases (libEmuSC tva.cc: levels are TVALevel amplitudes, a phase moves
   linearly (time bit 7 set) or exponentially (2^-16 per phase, ~96 dB) to its end level), then fit SF2's
   attack (linear amplitude) / decay (linear dB, 96 dB time) / sustain / release by a grid search in dB. */
#define ESTEP 0.01
#define EN 800
static double envsim(double a, double b, double pos, int lin) {
    if (lin) return a + (b - a) * pos;
    return b > a ? a + (b - a) * (1 - pow(2, -16 * pos)) : b + (a - b) * pow(2, -16 * pos);
}
static double db(double v) { return v > 0.004 ? 20 * log10(v / 255.0) : -96; }
static double fit_lin_db(const double *e, int n, double from, double to, double *best_t) {   /* fall rate fit */
    double bt = 0.01, be = 1e18, t;
    for (t = 0.002; t < 200; t *= 1.04) {
        double err = 0, r = 96.0 / t; int i;
        for (i = 0; i < n; i++) { double m = from - r * i * ESTEP; if (m < to) m = to; err += fabs((m < -70 ? -70 : m) - (e[i] < -70 ? -70 : e[i])); }
        if (err < be) { be = err; bt = t; }
    }
    *best_t = bt; return be / (n ? n : 1);
}
typedef struct { uint8_t k[12]; double r[5]; } EnvC;
static EnvC *ecache; static int necache;
static void envfit2(const uint8_t *d, int idx, double *peak, double *att, double *dec, double *sus, double *rel, int nooff);
static void envfit(const uint8_t *d, int idx, double *peak, double *att, double *dec, double *sus, double *rel, int nooff) {
    uint8_t k[12]; int i;
    memcpy(k, d + 74, 9); k[9] = idx; k[10] = nooff; k[11] = envT_scale ^ (envR_scale << 3);
    for (i = 0; i < necache; i++) if (!memcmp(ecache[i].k, k, 12)) break;
    if (i == necache) {
        if (!(necache & 255)) { ecache = realloc(ecache, (necache + 256) * sizeof *ecache); if (!ecache) die("out of memory"); }
        memcpy(ecache[i].k, k, 12);
        envfit2(d, idx, &ecache[i].r[0], &ecache[i].r[1], &ecache[i].r[2], &ecache[i].r[3], &ecache[i].r[4], nooff);
        necache++;
    }
    *peak = ecache[i].r[0]; *att = ecache[i].r[1]; *dec = ecache[i].r[2]; *sus = ecache[i].r[3]; *rel = ecache[i].r[4];
}
static void envfit2(const uint8_t *d, int idx, double *peak, double *att, double *dec, double *sus, double *rel, int nooff) {
    double v[6], t[6], e[EN], mx = -200; int lin[6], i, k, n = 0, imax = 0, ph; double tt = 0;
    v[0] = 0;
    for (i = 0; i < 4; i++) v[i + 1] = cpu[LUT[26] + clampi(idx - cpu[LUT[25] + d[74 + i]], 0, 255)];
    for (i = 0; i < 5; i++) { t[i] = etime(d[78 + i]) * (i == 4 ? envR_scale : envT_scale) / 100; lin[i] = (d[78 + i] & 0x80) != 0; }
    /* held note: phases 1-4 for up to 8 s */
    for (ph = 0; ph < 4 && n < EN; ph++) {
        int steps = (int)(t[ph] / ESTEP);
        for (k = 0; k < steps && n < EN; k++) e[n++] = db(envsim(v[ph], v[ph + 1], (double)k / steps, lin[ph]));
    }
    while (n < EN) e[n++] = db(v[4]);
    for (i = 0; i < EN; i++) if (e[i] > mx + 0.01) { mx = e[i]; imax = i; }
    *peak = mx;
    for (i = 0; i < imax; i++) if (e[i] > mx - 1) break;    /* attack: to within 1 dB of the peak */
    *att = i * ESTEP; if (*att < 0.001) *att = 0.001;
    *sus = mx - db(v[4]); if (*sus < 0) *sus = 0; if (*sus > 96) *sus = 144;
    tt = 0;
    if (*sus > 0.5) {
        int m = EN - imax; if (m > 300) m = 300;   /* first 3 s after the peak */
        fit_lin_db(e + imax, m, mx, mx - (*sus > 96 ? 96 : *sus), dec);
    } else *dec = 0.001;
    {   /* release from the sustain level (or from where a no-note-off drum is after 1 s) */
        double a = nooff ? 0 : v[4], r[EN]; int steps = (int)(t[4] / ESTEP), m = 0;
        if (nooff || a < 0.004) { *rel = nooff ? (*dec > 1 ? *dec : 1) : (t[4] < 0.01 ? 0.01 : t[4]); return; }
        for (k = 0; k < steps && m < EN; k++) r[m++] = db(envsim(a, 0, (double)k / steps, lin[4]));
        while (m < 40) r[m++] = -96;
        fit_lin_db(r, m, db(a), -96, rel);
    }
    (void)tt;
}

/* TVF (libEmuSC tvf.cc): cutoff index = base<<8 + level init (env level x depth x velocity sens + key follow);
   TVFCutoffFreq is ~exponential (~93 cents per index), so the env maps to SF2 modEnv -> filterFc in cents. */
static double tvf_c;   /* mean cutoff of TVF stages 1-4 (cents), set by tvf() */
static double fc_cents(int acc) {
    unsigned v = be16(cpu + LUT[12] + 2 * clampi(acc >> 8, 0, 128)) * 2; double fc;
    if (v > 0xE600) v = 0xE600;
    fc = 32000.0 / M_PI * asin(v / 65536.0);
    if (fc < 20) fc = 20;
    return 1200 * log2(fc / 8.176);
}
static void tvf(const uint8_t *d, int key, Buf *g, int *cnt) {
    int vel = ctrl[0x3D1E8 + clampi(d[36], 0, 9) * 128 + 100], cofvs = d[61] - 0x40, vs = 0x7fff, depth, kf = 0, L[6], i;
    int kfrom = d[41] - 0x40, base = d[37];
    double c[6], cmin, cpk, sus, t1, t25, t5, rng;
    if (cofvs) vs -= ((127 - vel) * be16(cpu + LUT[10] + 2 * clampi(abs(cofvs), 0, 10))) & 0xffff;
    depth = be16(cpu + LUT[11] + 2 * clampi(d[44], 0, 127)) * vs * 2;
    if (kfrom) {
        int kmi = be16(ctrl + (mk2 ? 0x3DD7C : 0x3DC72) + 2 * clampi(48 + d[40], 0, 135));
        int km = (int)be16(ctrl + 0x30000 + kmi + key * 2);
        kf = ((km - 0x4000) * (int)be16(cpu + LUT[9] + 2 * clampi(abs(kfrom), 0, 20))) >> 8;
        if (kfrom < 0) kf = -kf;
    }
    L[0] = kf;
    for (i = 0; i < 5; i++) {
        int lv = d[45 + i], sc = cpu[LUT[18] + clampi(abs(lv - 0x40), 0, 63)];
        int tmp = (sc * (int)(((unsigned)depth & 0xffff0000u) >> 16)) * 2, r = ((tmp & 0xff00) >> 8) + ((tmp & 0xff0000) >> 8);
        L[i + 1] = lv >= 0x40 ? r + kf : kf - r;
    }
    for (i = 0; i < 6; i++) c[i] = fc_cents(clampi((base << 8) + L[i], 0, 0x7fff));
    tvf_c = (c[1] + c[2] + c[3] + c[4]) / 4;
    if (!g) return;
    if (getenv("SC55SF_DBG")) fprintf(stderr, "tvf base %d dep %d vs %d kf %d L %d %d %d %d %d c %.0f %.0f %.0f %.0f %.0f\n", base, d[44], vs, kf, L[1], L[2], L[3], L[4], L[5], c[0], c[1], c[2], c[3], c[4]);
    cmin = c[0]; for (i = 1; i < 5; i++) if (c[i] < cmin) cmin = c[i];
    cpk = c[1] > c[2] ? c[1] : c[2];
    rng = cpk - cmin;
    if (cmin > 13400) return;
    gen(g, cnt, 8, clampi((int)lrint(cmin), 1500, 13500));
    if (rng < 20) return;
    t1 = etime(d[50]) * envT_scale / 100; t25 = (etime(d[51]) + etime(d[52]) + etime(d[53])) * envT_scale / 100;
    t5 = etime(d[54]) * envR_scale / 100;
    if (c[2] > c[1] + 20) { t1 += etime(d[51]) * envT_scale / 100; t25 -= etime(d[51]) * envT_scale / 100; }
    sus = (c[4] - cmin) / rng; if (sus < 0) sus = 0; if (sus > 1) sus = 1;
    gen(g, cnt, 11, clampi((int)lrint(rng), 0, 12000));
    gen(g, cnt, 26, (int)lrint(sec2tc(t1 < 0.001 ? 0.001 : t1)));
    if (sus < 0.99) { gen(g, cnt, 28, (int)lrint(sec2tc(t25 / (1 - sus) < 0.001 ? 0.001 : t25 / (1 - sus)))); gen(g, cnt, 29, (int)lrint((1 - sus) * 1000)); }
    gen(g, cnt, 30, (int)lrint(sec2tc(t5 < 0.001 ? 0.001 : t5)));
}

/* TVA from the firmware trace (testrig/sc55sf/ORACLE.md, docs/SC55ROM-trace.md): stage times from the traced
   linear (bit 7) / exponential (time constant) tables, level bytes = squared amplitude. Used for drums (-o1, default)
   or every zone (-o2). */
static const double D_LIN[17] = {0, 60, 142, 258, 413, 626, 912, 1308, 1841, 2553, 3522, 4860, 6642, 9210, 12498, 17000, 23000};
static const double D_TAU[17] = {0, 7.2, 17.3, 26.0, 36.1, 57.7, 83.7, 124.1, 170.2, 239.5, 334.7, 464.5, 639.1, 890.1, 1191.7, 1612.9, 2174};
static int tva_mode = 2, hpf_on = 1; static double trace_off = 0, atten_cb = 25;
static int max4(const uint8_t *p) { int m = p[0], i; for (i = 1; i < 4; i++) if (p[i] > m) m = p[i]; return m; }
static double tab8(const double *t, int x) {
    int i = x >> 3; double f = (x & 7) / 8.0;
    if (x <= 0) return 0; if (i >= 16) return t[16];
    if (t[i] <= 0) return t[i + 1] * f;
    return exp(log(t[i]) * (1 - f) + log(t[i + 1]) * f);
}
static double stage_ms(int tb) { return (tb & 0x80) ? tab8(D_LIN, tb & 0x7f) : 4.6 * tab8(D_TAU, tb & 0x7f); }
static double amp_lvl(int L) { return (L / 127.0) * (L / 127.0); }
static double db_lvl(int L) { return L <= 0 ? -100 : 40 * log10(L / 127.0); }
static double ms_s(double ms) { return ms < 1 ? 0.001 : ms / 1000.0; }
static double env_at(const double *lv, const int *tb, double t) {
    double a = lv[0]; int s;
    for (s = 0; s < 4; s++) {
        double d = stage_ms(tb[s]), b = lv[s + 1];
        if (t <= d) {
            if (tb[s] & 0x80) return d > 0 ? a + (b - a) * t / d : b;
            { double tau = tab8(D_TAU, tb[s] & 0x7f); return tau > 0 ? b + (a - b) * exp(-t / tau) : b; }
        }
        t -= d; a = b;
    }
    return a;
}
static double todb(double a) { return a <= 1e-5 ? -100 : 20 * log10(a); }
/* pp = partial bytes + 4; out: attack/hold/decay/release in s (hold/decay < 0 = none), sustain dB down */
static void tva_trace(const uint8_t *pp, int nooff, double *att, double *hold, double *dec, double *sus, double *rel) {
    double lv[5], pk = 1e-6, t, th, t0, tend, t30, r; int tb[4], i;
    lv[0] = amp_lvl(pp[69]); for (i = 0; i < 4; i++) { lv[i + 1] = amp_lvl(pp[70 + i]); tb[i] = pp[74 + i]; }
    for (i = 1; i < 5; i++) if (lv[i] > pk) pk = lv[i];
    for (i = 0; i < 5; i++) lv[i] /= pk;
    *att = ms_s((tb[0] & 0x80) ? stage_ms(tb[0]) : 2.3 * tab8(D_TAU, tb[0] & 0x7f));
    t0 = stage_ms(tb[0]); tend = t0 + stage_ms(tb[1]) + stage_ms(tb[2]) + stage_ms(tb[3]);
    *sus = -todb(lv[4]); if (*sus > 100) *sus = 100;
    for (t = t0; t < tend && todb(env_at(lv, tb, t)) > -1.0; t += 2) ; th = t - t0;
    if (*sus < 1.0) { *hold = -1; *dec = -1; }
    else {
        double target = *sus > 60 ? -30 : -*sus / 2, tx = -1;
        for (t = t0 + th; t < tend + 1; t += 2) if (todb(env_at(lv, tb, t)) <= target) { tx = t; break; }
        if (tx < 0) tx = tend;
        *hold = ms_s(th); *dec = ms_s((tx - t0 - th) * 100.0 / -target);
    }
    { int b = pp[78]; double d = stage_ms(b);
      t30 = (b & 0x80) ? d * (1 - pow(10, -1.5)) : tab8(D_TAU, b & 0x7f) * log(pow(10, 1.5));
      r = t30 * 100.0 / 30.0;
      if (nooff) { double nat = 0; for (i = 0; i < 4; i++) nat += stage_ms(tb[i]); if (nat > r) r = nat; }
      *rel = ms_s(r < 5 ? 5 : r); }
}

/* High-pass TVF (type != 0, base > 0): SF2 has no high-pass, so take the level the filter removes off the zone
   (2-pole SVF on the decoded sample at 32 kHz, cutoff from the base index; cached per sample + cutoff). */
static void tvf(const uint8_t *d, int key, Buf *g, int *cnt);
static double hpf_loss(int si, const uint8_t *d, int key) {
    int base, res = d[38];
    static struct { int si, base, res; double db; } c[512]; static int nc;
    int i, n; float *x; double f, q, lo = 0, bp = 0, ein = 0, eout = 0, hz;
    tvf(d, key, NULL, NULL); base = (int)lrint(tvf_c);
    for (i = 0; i < nc; i++) if (c[i].si == si && c[i].base == base && c[i].res == res) return c[i].db;
    n = decode(&smp[si], NULL); if (n <= 0) return 0;
    x = malloc(n * sizeof *x); if (!x) return 0;
    decode(&smp[si], x);
    hz = 8.176 * pow(2, base / 1200.0);
    f = 2 * sin(M_PI * (hz < 15000 ? hz : 15000) / 32000.0); q = 1.0 / (0.5 + res / 64.0);
    for (i = 0; i < n; i++) { double hp = x[i] - lo - q * bp; bp += f * hp; lo += f * bp; ein += (double)x[i] * x[i]; eout += hp * hp; }
    free(x);
    f = ein > 0 && eout > 0 ? 10 * log10(eout / ein) : 0; if (f > 0) f = 0; if (f < -40) f = -40;
    if (getenv("SC55SF_DBG")) fprintf(stderr, "hpf si %d cents %d (%.0f Hz) res %d loss %.1f\n", si, base, hz, res, f);
    if (nc < 512) { c[nc].si = si; c[nc].base = base; c[nc].res = res; c[nc].db = f; nc++; }
    return f;
}

/* one zone: partial pp of instrument, keys lo..hi, sample si, played as key K (drums: K fixed) */
static void zone(const Inst *in, const Part *pp, int lo, int hi, int si, int dkey, const Drum *dr, int dn) {
    const uint8_t *d = pp->d; const Smp *m = &smp[si]; int sfi = sf_sample(si), kf, idx;
    double st, T, peak, sus, t5, att, dec, pan, hold;
    if (sfi < 0) return;
    put16(&ibag, nigen); put16(&ibag, 0); nibag++;
    genr(&igen, &nigen, 43, lo, hi);
    kf = d[13] - 0x40;
    st = be16(cpu + LUT[0] + 2 * clampi(abs(kf), 0, 20)) * 200.0 / 65536.0; if (kf < 0) st = -st;
    T = (d[10] - 0x40) + 60 - m->root + (m->pinit - 1024) / 1000.0 + (d[11] - 0x40) / 100.0;
    if (dr) { T += st / 100.0 * (dkey - 60); st = 0; }
    /* TVA levels */
    idx = 0xFF - cpu[LUT[25] + d[69]] - cpu[LUT[25] + m->vol] - cpu[LUT[25] + in->vol];
    if (dr) idx -= cpu[LUT[25] + dr->vol[dn]];
    if (idx < 1) idx = 1;
    envfit(d, idx, &peak, &att, &dec, &sus, &t5, dr && !(dr->flg[dn] & 1));
    hold = -1;
    if (tva_mode == 2 || (dr && tva_mode == 1)) {
        tva_trace(d + 4, dr && !(dr->flg[dn] & 1), &att, &hold, &dec, &sus, &t5);
        if (sus < 1.0) sus = 0;
        peak = db_lvl(d[69]) + db_lvl(m->vol) + db_lvl(in->vol) + db_lvl(max4(d + 74)) + (dr ? db_lvl(dr->vol[dn]) : 0) + trace_off;
    }
    if (d[39] && d[37] && hpf_on) peak += hpf_loss(si, d, dr ? dkey : 60);
    gen(&igen, &nigen, 48, clampi((int)lrint(-(peak + gain_db) * atten_cb), 0, 1440));   /* EMU/TSF play initialAttenuation x0.4 */
    pan = (d[9] - 0x40) * 500.0 / 64; if (dr && dr->pan[dn]) pan = (dr->pan[dn] - 0x40) * 500.0 / 64;
    if (pan) gen(&igen, &nigen, 17, clampi((int)lrint(pan), -500, 500));
    if (dr) {
        if (dr->rev[dn]) gen(&igen, &nigen, 16, dr->rev[dn] * 1000 / 127);
        if (dr->cho[dn]) gen(&igen, &nigen, 15, dr->cho[dn] * 1000 / 127);
        if (dr->grp[dn]) gen(&igen, &nigen, 57, dr->grp[dn]);
    }
    if (d[39] == 0 && !nofilt) tvf(d, dr ? dkey : 60, &igen, &nigen);
    gen(&igen, &nigen, 34, (int)lrint(sec2tc(att)));
    if (hold > 0) gen(&igen, &nigen, 35, (int)lrint(sec2tc(hold)));
    if (sus > 0.5) { gen(&igen, &nigen, 36, (int)lrint(sec2tc(dec))); gen(&igen, &nigen, 37, clampi((int)lrint(sus * 10), 0, 1440)); }
    gen(&igen, &nigen, 38, (int)lrint(sec2tc(t5)));
    {   int ct = (int)floor(T), ft = (int)lrint((T - ct) * 100);
        if (ft >= 100) { ct++; ft -= 100; }
        if (ct) gen(&igen, &nigen, 51, ct);
        if (ft) gen(&igen, &nigen, 52, ft);
    }
    if (!dr && fabs(st - 100) > 0.5) gen(&igen, &nigen, 56, (int)lrint(st));
    if (dr) gen(&igen, &nigen, 56, 0);
    gen(&igen, &nigen, 58, dr ? lo : 60);
    gen(&igen, &nigen, 54, m->mode == 2 ? 0 : 1);
    gen(&igen, &nigen, 53, sfi);
}
/* sample chosen by the firmware for partial pp at key K (libEmuSC pitch.cc) */
static int pick(const Part *pp, int K) {
    const uint8_t *d = pp->d; int pi = be16(d + 2), kf = d[13] - 0x40, bc, bp, j; double st;
    if (pi == 0xFFFF || pi >= nprt) return -1;
    st = be16(cpu + LUT[0] + 2 * clampi(abs(kf), 0, 20)) / 32768.0; if (kf < 0) st = -st;
    bc = clampi((int)floor(60 + st * (K - 60) + 1e-9) + d[10] - 0x40, 0, 127);
    bp = bc - d[1] + 0x40;
    for (j = 0; j < 16; j++)
        if (prt[pi].breaks[j] >= bp || prt[pi].breaks[j] == 0x7F) {
            int s = prt[pi].smp[j]; return (s == 0xFFFF || s >= nsmp) ? -1 : s;
        }
    return -1;
}
static int sf_inst_mel[1024];
static int make_inst(int ii) {
    Inst *in = &inst[ii]; int p, k;
    if (sf_inst_mel[ii]) return sf_inst_mel[ii] - 1;
    putname(&inst_, in->name); put16(&inst_, nibag);
    for (p = 0; p < 2; p++) {
        if (!(in->used & (1 << p))) continue;
        for (k = 0; k < 128;) {
            int s = pick(&in->p[p], k), e = k;
            while (e + 1 < 128 && pick(&in->p[p], e + 1) == s) e++;
            if (s >= 0) zone(in, &in->p[p], k, e, s, 0, NULL, 0);
            k = e + 1;
        }
    }
    sf_inst_mel[ii] = ++ninst_; return ninst_ - 1;
}
static int make_drum(int di) {
    Drum *d = &drum[di]; int n, p;
    putname(&inst_, d->name); put16(&inst_, nibag);
    for (n = 0; n < 128; n++) {
        int ii = d->preset[n]; Inst *in;
        if (ii == 0xFFFF || ii >= ninst) continue;
        in = &inst[ii];
        for (p = 0; p < 2; p++) {
            int s;
            if (!(in->used & (1 << p))) continue;
            s = pick(&in->p[p], d->key[n]);
            if (s >= 0) zone(in, &in->p[p], n, n, s, d->key[n], d, n);
        }
    }
    return ninst_++;
}
static void preset(const char *name, int prog, int bank, int sfinst) {
    putname(&phdr, name); put16(&phdr, prog); put16(&phdr, bank); put16(&phdr, npbag); put32(&phdr, 0); put32(&phdr, 0); put32(&phdr, 0); nphdr++;
    put16(&pbag, npgen); put16(&pbag, 0); npbag++;
    gen(&pgen, &npgen, 41, sfinst);
}
static void chunk(Buf *o, const char *id, const Buf *b) { put(o, id, 4); put32(o, b->n); put(o, b->b, b->n); }

/* ---- SBK (SoundFont 1, samples in AWE sample RAM) from the SF2 tables above: testrig/awe/sf2sbk.py in C.
   Banks 0 + 128 only; when the samples do not fit /AWERAM, the longest ones are decimated (x2, x4, x8). ---- */
static long rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((long)p[3] << 24); }
static int rd16s(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static int sbkv(int o, double v) {
#define CL(x, a, b) clampi((int)lrint(x), a, b)
    if (o == 8) return v >= 14400 ? 127 : CL((v - 4366) / 59, 0, 126);
    if (o == 9) return CL(v * 2 / 3, 0, 127);
    if (o == 15 || o == 16) return CL(v * 256 / 1000, 0, 255);
    if (o == 17) return CL((v + 500) * 127 / 1000, 0, 127);
    if (o == 48) return v >= 1000 ? 0 : CL(127 * pow(10, -v / 2000), 1, 127);
    if (o == 56) return v <= 75 ? 1 : 0;
    if (o == 21 || o == 23 || o == 25 || o == 26 || o == 27 || o == 28 || o == 30 || (o >= 33 && o <= 36) || o == 38)
        return CL(1000 * pow(2, v / 1200), 1, 32767);
    if (o == 31 || o == 32 || o == 39 || o == 40) return CL(v / 5.55, -127, 127);
    if (o == 22 || o == 24) return CL(pow(2, (v + 7925) / 1200), 1, 255);
    if (o >= 5 && o <= 7) return CL(v * 64 / 600, -127, 127);
    if (o == 10) return CL(v * 64 / 3600, -127, 127);
    if (o == 11) return CL(v * 64 / 7200, -127, 127);
    if (o == 13) return CL(v * 64 / 120, -127, 127);
    if (o == 29) return CL(96 - v * 96 / 1000, 0, 96);
    if (o == 37) return CL((2000 - 2 * v) / 21, 0, 96);
#undef CL
    return (int)v;
}
static void write_sbk(const char *path, long ramkb) {
    int np = nphdr, ni = ninst_, i, z, k, *used = calloc(nshdr, sizeof(int)), *fac = calloc(nshdr, sizeof(int)), *pos = calloc(nshdr, sizeof(int));
    int *inst_used = calloc(ni + 1, sizeof(int)), *imap = calloc(ni + 1, sizeof(int)), nsel = 0, nimap = 0;
    long budget = ramkb * 512L - 32768, total = 0, w; Buf sp = { 0 }, sn = { 0 }, sm = { 0 }, ph = { 0 }, pb = { 0 }, pg = { 0 }, in = { 0 }, ib = { 0 }, ig = { 0 }, sh = { 0 };
    Buf info = { 0 }, sdta = { 0 }, pdta = { 0 }, riff = { 0 }, t = { 0 }, mod = { 0 }; FILE *f; int pass;
    (void)sp;
    /* presets on banks 0 / 128 and the samples they use */
    for (i = 0; i < np; i++) {
        const uint8_t *r = phdr.b + 38 * i; int bank = r[22] | (r[23] << 8), bag = r[24] | (r[25] << 8), ii;
        if (bank != 0 && bank != 128) continue;
        ii = rd16s(pgen.b + 4 * (pbag.b[4 * bag] | (pbag.b[4 * bag + 1] << 8)) + 2);
        if (!inst_used[ii]) { inst_used[ii] = 1; imap[ii] = nimap++; }
        nsel++;
    }
    for (i = 0; i < ni; i++) if (inst_used[i]) {
        int b0 = inst_.b[22 * i + 20] | (inst_.b[22 * i + 21] << 8), b1 = inst_.b[22 * (i + 1) + 20] | (inst_.b[22 * (i + 1) + 21] << 8);
        for (z = b0; z < b1; z++) {
            int g0 = ibag.b[4 * z] | (ibag.b[4 * z + 1] << 8), g1 = ibag.b[4 * (z + 1)] | (ibag.b[4 * (z + 1) + 1] << 8);
            for (k = g0; k < g1; k++) if ((igen.b[4 * k] | (igen.b[4 * k + 1] << 8)) == 53) used[rd16s(igen.b + 4 * k + 2)] = 1;
        }
    }
    for (i = 0; i < nshdr; i++) if (used[i]) { const uint8_t *r = shdr.b + 46 * i; fac[i] = 1; total += rd32(r + 24) - rd32(r + 20) + 48; }
    while (total > budget) {   /* decimate the biggest sample still allowed */
        long bl = 0; int bi = -1;
        for (i = 0; i < nshdr; i++) if (used[i] && fac[i] < 8) {
            const uint8_t *r = shdr.b + 46 * i; long l = (rd32(r + 24) - rd32(r + 20)) / fac[i];
            if (l > bl) { bl = l; bi = i; }
        }
        if (bi < 0) { printf("SC55SF: samples do not fit %ld KB of AWE RAM\n", ramkb); free(used); return; }
        total -= bl - bl / 2; fac[bi] *= 2;
    }
    /* sample data: decimated by box average, 48 zero words after each */
    sm.cap = total * 2 + 64; sm.b = malloc(sm.cap); if (!sm.b) die("out of memory");
    for (i = 0, w = 0; i < nshdr; i++) if (used[i]) {
        const uint8_t *r = shdr.b + 46 * i; long s0 = rd32(r + 20), s1 = rd32(r + 24), ls = rd32(r + 28), le = rd32(r + 32), j, n;
        int fc = fac[i], m; char nm[20];
        n = (s1 - s0) / fc; pos[i] = w;
        for (j = 0; j < n; j++) { long a = 0; for (m = 0; m < fc; m++) a += pcm[s0 + j * fc + m]; put16(&sm, (uint16_t)(int16_t)(a / fc)); }
        for (j = 0; j < 48; j++) put16(&sm, 0);
        memcpy(nm, r, 20); put(&sn, nm, 20);
        put32(&sh, w); put32(&sh, w + n); put32(&sh, w + (ls - s0) / fc - 1); put32(&sh, w + (le - s0) / fc - 2);
        w += n + 48;
    }
    /* instruments */
    for (pass = 0, k = 0; pass < 1; pass++) for (i = 0; i < ni; i++) if (inst_used[i]) {
        int b0 = inst_.b[22 * i + 20] | (inst_.b[22 * i + 21] << 8), b1 = inst_.b[22 * (i + 1) + 20] | (inst_.b[22 * (i + 1) + 21] << 8);
        put(&in, inst_.b + 22 * i, 20); put16(&in, ib.n / 4);
        for (z = b0; z < b1; z++) {
            int g0 = ibag.b[4 * z] | (ibag.b[4 * z + 1] << 8), g1 = ibag.b[4 * (z + 1)] | (ibag.b[4 * (z + 1) + 1] << 8), gg;
            int have[64] = { 0 }, val[64], si = -1, root = 60;
            put16(&ib, ig.n / 4); put16(&ib, 0);
            for (gg = g0; gg < g1; gg++) { int o = igen.b[4 * gg] | (igen.b[4 * gg + 1] << 8); if (o < 64) { have[o] = 1; val[o] = rd16s(igen.b + 4 * gg + 2); } }
            if (have[43]) { put16(&ig, 43); put16(&ig, (uint16_t)val[43]); }
            for (gg = 0; gg < 64; gg++) if (have[gg] && gg != 43 && gg != 44 && gg != 53 && gg != 58 && gg != 41) { put16(&ig, gg); put16(&ig, (uint16_t)(int16_t)sbkv(gg, val[gg])); }
            if (!have[29]) { put16(&ig, 29); put16(&ig, sbkv(29, 0)); }
            if (!have[37]) { put16(&ig, 37); put16(&ig, sbkv(37, 0)); }
            if (!have[22]) { put16(&ig, 22); put16(&ig, sbkv(22, 0)); }
            if (!have[24]) { put16(&ig, 24); put16(&ig, sbkv(24, 0)); }
            si = val[53]; if (have[58]) root = val[58];
            {   int k2, idx = 0; for (k2 = 0; k2 < si; k2++) idx += used[k2];
                put16(&ig, 55); put16(&ig, (uint16_t)(int16_t)lrint(root * 100 + 1200 * log2(44100.0 * fac[si] / 32000)));
                put16(&ig, 53); put16(&ig, idx);
            }
        }
        k++;
    }
    putname(&in, "EOI"); put16(&in, ib.n / 4); put16(&ib, ig.n / 4); put16(&ib, 0); put32(&ig, 0);
    for (i = 0; i < np; i++) {
        const uint8_t *r = phdr.b + 38 * i; int bank = r[22] | (r[23] << 8), bag = r[24] | (r[25] << 8), ii;
        if (bank != 0 && bank != 128) continue;
        ii = rd16s(pgen.b + 4 * (pbag.b[4 * bag] | (pbag.b[4 * bag + 1] << 8)) + 2);
        put(&ph, r, 24); put16(&ph, pb.n / 4); put32(&ph, 0); put32(&ph, 0); put32(&ph, 0);
        put16(&pb, pg.n / 4); put16(&pb, 0); put16(&pg, 41); put16(&pg, imap[ii]);
    }
    putname(&ph, "EOP"); put16(&ph, 255); put16(&ph, 255); put16(&ph, pb.n / 4); put32(&ph, 0); put32(&ph, 0); put32(&ph, 0);
    put16(&pb, pg.n / 4); put16(&pb, 0); put32(&pg, 0);
    put32(&mod, 0); put32(&mod, 0); put16(&mod, 0);
    put(&info, "INFO", 4);
    put16(&t, 1); put16(&t, 0); chunk(&info, "ifil", &t);
    t.n = 0; put(&t, "SC-55 (from your ROMs)\0\0", 24); chunk(&info, "INAM", &t);
    t.n = 0; put(&t, "1MGM", 4); put16(&t, 0); chunk(&info, "irom", &t);
    put(&pdta, "pdta", 4);
    chunk(&pdta, "phdr", &ph); chunk(&pdta, "pbag", &pb); chunk(&pdta, "pmod", &mod); chunk(&pdta, "pgen", &pg);
    chunk(&pdta, "inst", &in); chunk(&pdta, "ibag", &ib); chunk(&pdta, "imod", &mod); chunk(&pdta, "igen", &ig); chunk(&pdta, "shdr", &sh);
    f = fopen(path, "wb"); if (!f) die("cannot write the SBK");
    put(&riff, "RIFF", 4); put32(&riff, 4 + 8 + info.n + 8 + 4 + 8 + sn.n + 8 + sm.n + 8 + pdta.n); put(&riff, "sfbk", 4);
    chunk(&riff, "LIST", &info);
    put(&riff, "LIST", 4); put32(&riff, 4 + 8 + sn.n + 8 + sm.n); put(&riff, "sdta", 4); chunk(&riff, "snam", &sn);
    put(&riff, "smpl", 4); put32(&riff, sm.n);
    fwrite(riff.b, 1, riff.n, f);
    if (fwrite(sm.b, 1, sm.n, f) != (size_t)sm.n) die("cannot write the SBK (disk full?)");
    free(sm.b); riff.n = 0; chunk(&riff, "LIST", &pdta); fwrite(riff.b, 1, riff.n, f);
    if (fclose(f)) die("cannot write the SBK (disk full?)");
    (void)sdta;
    {   int dec = 0; for (i = 0; i < nshdr; i++) dec += used[i] && fac[i] > 1;
        printf("SC55SF: %s written: %d presets, %d instruments, %ld KB of AWE RAM (%d samples at reduced rate) for /AWERAM:%ld\n",
               path, nsel, nimap, w * 2 / 1024, dec, ramkb); }
}

int main(int argc, char **argv) {
    const char *dir = NULL, *out = "SC55.SF2", *sbkout = NULL; long ramkb = 8192; time_t t_start = time(NULL); char path[512]; DIR *dp; struct dirent *de;
    char *wf[MAXF]; int nwf = 0, i, j, b, p, ai;
    uint8_t *cand_cpu[MAXF]; int ncpu = 0;
    for (ai = 1; ai < argc; ai++) {
        if (!strcmp(argv[ai], "-v")) verbose = 1;
        else if (!strcmp(argv[ai], "-q")) quiet = 1;
        else if (!strncmp(argv[ai], "-t", 2)) envT_scale = atoi(argv[ai] + 2);
        else if (!strcmp(argv[ai], "-nf")) nofilt = 1;
        else if (!strcmp(argv[ai], "-s") && ai + 1 < argc) sbkout = argv[++ai];
        else if (!strcmp(argv[ai], "-k") && ai + 1 < argc) ramkb = atol(argv[++ai]);
        else if (!strncmp(argv[ai], "-r", 2)) envR_scale = atoi(argv[ai] + 2);
        else if (!strncmp(argv[ai], "-g", 2)) gain_db = atof(argv[ai] + 2);
        else if (!strncmp(argv[ai], "-o", 2)) tva_mode = atoi(argv[ai] + 2);
        else if (!strncmp(argv[ai], "-L", 2)) trace_off = atof(argv[ai] + 2);
        else if (!strncmp(argv[ai], "-A", 2)) atten_cb = atof(argv[ai] + 2);
        else if (!strcmp(argv[ai], "-nh")) hpf_on = 0;
        else if (!dir) dir = argv[ai]; else out = argv[ai];
    }
    if (!dir) { printf("SC55SF ROMDIR [OUT.SF2] [-s OUT.SBK [-k AWERAM_KB]] [-q] [-v]\n  builds an SF2 from your SC-55 (mk1 1.21 / mk2 1.01) ROMs, found by content\n"); return 2; }
    dp = opendir(dir); if (!dp) die("cannot open the ROM folder");
    while ((de = readdir(dp))) {
        long sz; uint8_t *b2; struct stat st;
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
        if (st.st_size != 32768 && st.st_size != 262144 && st.st_size != 524288 && st.st_size != 0x100000 && st.st_size != 0x200000) continue;
        b2 = loadf(path, &sz); if (!b2) continue;
        if (sz == 262144 && !ctrl && !memcmp(b2 + 0xF380, "Ver1.21", 7)) { ctrl = b2; mk2 = 0; if (verbose) printf("control ROM (SC-55 1.21): %s\n", de->d_name); continue; }
        if (sz == 524288 && !ctrl && !memcmp(b2 + 0x3D148, "GS-28 VER=2.00  SC  ", 20)) { ctrl = b2; mk2 = 1; if (verbose) printf("control ROM (SC-55mkII): %s\n", de->d_name); continue; }
        if (sz == 32768 && ncpu < MAXF) { cand_cpu[ncpu++] = b2; continue; }
        if ((sz == 0x100000 || sz == 0x200000) && (!memcmp(b2, "ROLAND GSS", 10) || !memcmp(b2, "Roland GSS", 10)) && nwf < MAXF) {
            wf[nwf++] = strdup(path); free(b2); continue; }
        free(b2);
    }
    closedir(dp);
    if (!ctrl) { if (quiet) return 3; die("no SC-55 control ROM found (mk1 1.21 or mk2)"); }
    LUT = mk2 ? LUT2 : LUT1;
    for (i = 0; i < ncpu && !cpu; i++) if (cpu_ok(cand_cpu[i], LUT)) cpu = cand_cpu[i];
    if (!cpu) die("no matching SC-55 CPU ROM found");
    parse_ctrl();
    printf("SC55SF: %d instruments, %d partials, %d samples, %d drum kits\n", ninst, nprt, nsmp, ndrum);
    {   /* wave ROMs: mk1 = 3 x 1 MB, mk2 = 2 MB + 1 MB; try every order of the candidates, keep the best fit */
        int need = mk2 ? 2 : 3, best[3] = { -1, -1, -1 }, ord[3]; double bs = 1e9; uint8_t *raw[MAXF]; long rsz[MAXF];
        if (nwf < need) die("SC-55 wave ROMs not found");
        for (i = 0; i < nwf; i++) raw[i] = loadf(wf[i], &rsz[i]);
        wave = calloc(1, 0x300000); wavesz = 0x300000;
        for (j = 0; j < need; j++) {   /* each slot on its own: try every file there, keep the best fit */
            double bsj = 1e9; int f;
            for (f = 0; f < nwf; f++) {
                long want = (mk2 && j == 0) ? 0x200000 : 0x100000, m; double sc;
                if (rsz[f] != want) continue;
                for (m = 0; m < want; m += 0x100000) unscramble(raw[f] + m, wave + (j ? (mk2 ? 0x200000 : j * 0x100000) : 0) + m);
                sc = 0; for (m = 0; m < want / 0x100000; m++) sc += wave_score((j ? (mk2 ? 2 : j) : 0) + m);
                if (verbose) printf("  wave slot %d: %s score %.5f\n", j, wf[f], sc);
                if (sc < bsj) { bsj = sc; best[j] = f; }
            }
            if (best[j] < 0) break;
            if (bsj > bs || bs == 1e9) bs = bsj;
        }
        (void)ord;
        if (best[0] < 0) die("SC-55 wave ROMs not found (sizes)");
        { long off = 0; for (j = 0; j < need; j++) { long m; for (m = 0; m < rsz[best[j]]; m += 0x100000) { unscramble(raw[best[j]] + m, wave + off); off += 0x100000; } } }
        if (verbose) for (j = 0; j < need; j++) printf("wave ROM %d: %s\n", j + 1, wf[best[j]]);
        if (bs > 0.05) printf("SC55SF: warning, wave ROMs fit badly (%.4f)\n", bs);
        for (i = 0; i < nwf; i++) free(raw[i]);
    }
    /* presets: bank 0 + GS variations (only where they differ from the fallback), drum kits on bank 128 */
    for (b = 0; b < 128; b++) for (p = 0; p < 128; p++) {
        int ii = var[b][p], fb = -1, k;
        if (ii == 0xFFFF || ii >= ninst) continue;
        for (k = b - 1; k >= 0 && fb < 0; k--) if (var[k][p] != 0xFFFF) fb = var[k][p];
        if (b && ii == fb) continue;
        preset(inst[ii].name, p, b, make_inst(ii));
    }
    {   int done[32] = { 0 };
        for (p = 0; p < 128; p++) {
            int di = drumlut[p];
            if (di >= ndrum || (p && drumlut[p - 1] == di)) continue;
            if (!done[di]) done[di] = make_drum(di) + 1;
            preset(drum[di].name, p, 128, done[di] - 1);
        }
    }
    /* terminal records */
    putname(&phdr, "EOP"); put16(&phdr, 0); put16(&phdr, 0); put16(&phdr, npbag); put32(&phdr, 0); put32(&phdr, 0); put32(&phdr, 0);
    put16(&pbag, npgen); put16(&pbag, 0); put32(&pgen, 0);
    putname(&inst_, "EOI"); put16(&inst_, nibag); put16(&ibag, nigen); put16(&ibag, 0); put32(&igen, 0);
    putname(&shdr, "EOS"); { uint8_t z[26] = { 0 }; put(&shdr, z, 26); }
    {   Buf info = { 0 }, sdta = { 0 }, pdta = { 0 }, riff = { 0 }, t = { 0 }, mod = { 0 }; FILE *f;
        put(&info, "INFO", 4);
        t.n = 0; put16(&t, 2); put16(&t, 1); chunk(&info, "ifil", &t);
        t.n = 0; put(&t, "EMU8000\0", 8); chunk(&info, "isng", &t);
        t.n = 0; put(&t, "SC-55 (from your ROMs)\0\0", 24); chunk(&info, "INAM", &t);
        t.n = 0; put(&t, "SC55SF (NUKED SB-EMU)\0", 22); chunk(&info, "ISFT", &t);
        put(&pdta, "pdta", 4);
        put32(&mod, 0); put32(&mod, 0); put16(&mod, 0);   /* terminal modulator records */
        chunk(&pdta, "phdr", &phdr); chunk(&pdta, "pbag", &pbag); chunk(&pdta, "pmod", &mod); chunk(&pdta, "pgen", &pgen);
        chunk(&pdta, "inst", &inst_); chunk(&pdta, "ibag", &ibag); chunk(&pdta, "imod", &mod); chunk(&pdta, "igen", &igen);
        chunk(&pdta, "shdr", &shdr);
        /* streamed: RIFF sfbk { LIST INFO, LIST sdta { smpl = pcm }, LIST pdta } (no copy of the sample data) */
        f = fopen(out, "wb"); if (!f) die("cannot write the SF2");
        put(&riff, "RIFF", 4); put32(&riff, 4 + 8 + info.n + 8 + 12 + npcm * 2 + 8 + pdta.n); put(&riff, "sfbk", 4);
        chunk(&riff, "LIST", &info);
        put(&riff, "LIST", 4); put32(&riff, 12 + npcm * 2); put(&riff, "sdta", 4); put(&riff, "smpl", 4); put32(&riff, npcm * 2);
        fwrite(riff.b, 1, riff.n, f);
        if (fwrite(pcm, 2, npcm, f) != (size_t)npcm) die("cannot write the SF2 (disk full?)");
        riff.n = 0; chunk(&riff, "LIST", &pdta); fwrite(riff.b, 1, riff.n, f);
        if (fclose(f)) die("cannot write the SF2 (disk full?)");
        (void)sdta;
        printf("SC55SF: %s written: %d presets, %d instruments, %d samples (%ld KB of sample data)\n",
               out, nphdr, ninst_, nshdr, npcm * 2 / 1024);
    }
    if (sbkout) write_sbk(sbkout, ramkb);
    printf("SC55SF: done in %ld s\n", (long)(time(NULL) - t_start));
    return 0;
}
