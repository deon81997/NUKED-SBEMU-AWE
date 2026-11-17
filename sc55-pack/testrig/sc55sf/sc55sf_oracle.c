/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE
 *
  * sc55sf_oracle.c - the oracle chat's converter (reference, not shipped; tools/sc55sf.c is the tool). SC55SF - makes SC55.SF2 (for SBEMU /VSF) and SC55.SBK (for the AWE32 player, /AWEMPU) from the user's own SC-55
 * ROMs: the SC-55's samples and its instrument / partial / drum-kit tables, converted on the user's PC. Nothing
 * Roland is shipped: the tool only knows the table layout (docs/SC55ROM.md) and conversion curves measured on
 * Nuked-SC55 (testrig/sc55sf: oracle + probe.py + exp_*.py).
 *
 * Usage: SC55SF [romdir] [/SF2:file] [/SBK:file] [/RAM:kb] [/NOSF2] [/NOSBK] [/Q] [/LIST]
 *   romdir   folder with the SC-55 (mk1) control + wave ROMs, found by content (default ROMS)
 *   /SF2:    SoundFont 2 output (default romdir\SC55.SF2)     /SBK: SoundFont 1 output (default romdir\SC55.SBK)
 *   /RAM:kb  AWE sample RAM the SBK must fit (512..28672, default 8192); long samples are halved in rate to fit
 * Exit code 0 = written, 2 = no SC-55 ROM set found, 3 = write error.
 *
  * Build (host): gcc -O2 -o sc55sf_oracle testrig/sc55sf/sc55sf_oracle.c upstream/Nuked-SC55/src/backend/sha/sha224-256.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../../upstream/Nuked-SC55/src/backend/sha/sha.h"

#ifdef __DJGPP__
#define SEP "\\"
#else
#define SEP "/"
#endif

/* ---------- ROM set ---------- */
static uint8_t *ctl;          /* control ROM (256 KB, mk1) */
static uint8_t *wave;         /* 3 MB unscrambled wave ROM space: A | B | C */
static uint32_t T_INST[2], T_PART[2], T_SMPL[2], T_VAR, T_DRUM; /* table addresses (docs/SC55ROM.md) */
static int q_mode;
static char romname[64] = "SC-55";

static const char *WAVE_SHA[3] = {   /* mk1 wave ROMs A/B/C (R15209276/277/281), SHA-256 of the file as dumped */
    "5655509a531804f97ea2d7ef05b8fec20ebf46216b389a84c44169257a4d2007",
    "c655b159792d999b90df9e4fa782cf56411ba1eaa0bb3ac2bdaf09e1391006b1",
    "334b2d16be3c2362210fdbec1c866ad58badeb0f84fd9bf5d0ac599baf077cc2"};

static void msg(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 1, 2)))
#endif
    ;
#include <stdarg.h>
static void msg(const char *fmt, ...) { va_list a; if (q_mode) return; va_start(a, fmt); vprintf(fmt, a); va_end(a); fflush(stdout); }

static uint8_t *load_file(const char *fn, long *size)
{
    FILE *f = fopen(fn, "rb"); long n; uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc(n > 0 ? n : 1);
    if (!b || fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); *size = n; return b;
}
static void sha_hex(const uint8_t *d, long n, char *out)
{
    SHA256Context c; uint8_t h[32]; int i;
    SHA256Reset(&c); SHA256Input(&c, d, (unsigned)n); SHA256Result(&c, h);
    for (i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", h[i]);
}
/* waverom unscrambling (address + data bit permutation), as Nuked-SC55's rom_io.cpp */
static void unscramble(const uint8_t *src, uint8_t *dst, int len)
{
    static const int aa[20] = {2, 0, 3, 4, 1, 9, 13, 10, 18, 17, 6, 15, 11, 16, 8, 5, 12, 7, 14, 19};
    static const int dd[8] = {2, 0, 4, 5, 7, 6, 3, 1};
    static uint32_t lo[1024], hi[1024]; static uint8_t dl[256]; int i, j;
    for (i = 0; i < 1024; i++) { lo[i] = hi[i] = 0; for (j = 0; j < 10; j++) if (i & (1 << j)) { lo[i] |= 1u << aa[j]; hi[i] |= 1u << aa[j + 10]; } }
    for (i = 0; i < 256; i++) { dl[i] = 0; for (j = 0; j < 8; j++) if (i & (1 << dd[j])) dl[i] |= (uint8_t)(1 << j); }
    for (i = 0; i < len; i++) dst[i] = dl[src[(i & ~0xfffff) | lo[i & 0x3ff] | hi[(i >> 10) & 0x3ff]]];
}
/* control ROM: find the instrument table by its structure (name, 12 bytes, then 216-byte records) */
static int find_tables(const uint8_t *c, long n)
{
    long a;
    for (a = 0; a + 0x30000 <= n; a += 0x10) {
        if (memcmp(c + a, "Piano 1     ", 12) || memcmp(c + a + 0xD8, "Piano 2     ", 12)) continue;
        if (a + 0x2C000 > n) continue;
        T_INST[0] = a; T_PART[0] = a + 224 * 0xD8; T_SMPL[0] = T_PART[0] + 144 * 0x3C;
        T_INST[1] = a + 0x10000; T_PART[1] = T_PART[0] + 0x10000; T_SMPL[1] = T_SMPL[0] + 0x10000;
        T_VAR = a + 0x20000; T_DRUM = a + 0x28080;
        if (T_DRUM + 14 * 0x48C > (uint32_t)n) return 0;
        if (memcmp(c + T_DRUM + 0x480, "STANDARD", 8)) return 0;
        return 1;
    }
    return 0;
}
static int find_roms(const char *dir)
{
    DIR *d = opendir(dir); struct dirent *e; char fn[600], h[65]; int got = 0, i; long n;
    if (!d) return 0;
    wave = (uint8_t *)calloc(1, 0x300000);
    while ((e = readdir(d))) {
        uint8_t *b; struct stat st;
        snprintf(fn, sizeof fn, "%s" SEP "%s", dir, e->d_name);
        if (stat(fn, &st) || (st.st_size != 0x40000 && st.st_size != 0x100000)) continue;
        if (!(b = load_file(fn, &n))) continue;
        if (n == 0x40000 && !ctl) {
            if (find_tables(b, n)) { ctl = b; got |= 8; msg("  control ROM: %s\n", e->d_name); continue; }
        } else if (n == 0x100000) {
            sha_hex(b, n, h);
            for (i = 0; i < 3; i++) if (!strcmp(h, WAVE_SHA[i]) && !(got & (1 << i))) {
                unscramble(b, wave + i * 0x100000, 0x100000); got |= 1 << i; msg("  wave ROM %c:   %s\n", 'A' + i, e->d_name);
            }
        }
        free(b);
    }
    closedir(d);
    if (ctl) { const uint8_t *v = ctl; long a; for (a = 0; a < 0x40000 - 16; a++) if (!memcmp(v + a, "Ver1.", 5) || !memcmp(v + a, "Ver2.", 5)) { snprintf(romname, sizeof romname, "SC-55 %.7s", (const char *)v + a); break; } }
    return got == 15;
}

/* ---------- table access (docs/SC55ROM.md) ---------- */
static unsigned be16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
typedef struct { const uint8_t *rec; char name[13]; } Inst;      /* 0xD8 bytes */
static const uint8_t *inst_rec(unsigned i) { return ctl + T_INST[i / 224] + (i % 224) * 0xD8; }
static const uint8_t *part_rec(unsigned i) { return ctl + T_PART[i / 144] + (i % 144) * 0x3C; }
static const uint8_t *smpl_rec(unsigned i) { return ctl + T_SMPL[i / 532] + (i % 532) * 0x10; }
static unsigned variation(unsigned bank, unsigned prog) { return be16(ctl + T_VAR + bank * 256 + prog * 2); }
static const uint8_t *drum_rec(unsigned k) { return ctl + T_DRUM + k * 0x48C; }
static void cname(const uint8_t *p, char *o) { int i; memcpy(o, p, 12); o[12] = 0; for (i = 11; i >= 0 && (o[i] == ' ' || !o[i]); i--) o[i] = 0; for (i = 0; o[i]; i++) if ((unsigned char)o[i] < 32) o[i] = ' '; }
#define NUM_INST 448
#define NUM_SMPL 1064
static int inst_ok(unsigned i) { return i < NUM_INST && inst_rec(i)[0] > ' '; }

/* ---------- samples: DPCM wave data -> 16-bit ---------- */
typedef struct {
    int16_t *d; uint32_t n, ls, le;     /* data, length, loop start / end (end exclusive) in samples */
    int mode, root, made, sf2;          /* mode 0 = loop, 1 = ping-pong (unrolled), 2 = one shot; sf2 = index in shdr */
    double tune;                        /* cents from the sample record */
    double gain_db;                     /* 40log of the sample level byte */
    double peak;                        /* largest |value| (20-bit domain) */
    int halved;
} Smp;
static Smp smp[NUM_SMPL];
static double g_scale = 1.0 / 2.0;      /* 20-bit accumulator -> 16 bit: set from the largest peak */
static int32_t *decode_raw(unsigned si, uint32_t *n_out, uint32_t *loop_out, int *mode_out)
{
    const uint8_t *r = smpl_rec(si);
    uint32_t off = (uint32_t)r[1] << 16 | r[2] << 8 | r[3], len = be16(r + 6), lp = be16(r + 8);
    uint32_t bank = ((off >> 21) & 7) * 0x100000, a0 = off & 0xfffff, a, n = len + 1; int32_t ref = 0, *o;
    if (bank > 0x200000 || a0 + len > 0xfffff) return NULL;
    o = (int32_t *)malloc(n * 4);
    for (a = 0; a < n; a++) {
        uint32_t ad = a0 + a; uint8_t e = wave[bank | (ad >> 5)]; int nib = (ad & 0x10) ? e >> 4 : e & 15;
        ref += (int32_t)(int8_t)wave[bank | ad] * (1 << nib);
        if (ref > 0x7ffff) ref = 0x7ffff; if (ref < -0x80000) ref = -0x80000;
        o[a] = ref;
    }
    *n_out = n; *loop_out = lp <= len ? len - lp : 0; *mode_out = r[10];
    return o;
}
static void make_sample(unsigned si)
{
    Smp *s = &smp[si]; uint32_t n, lp, i, tot; int mode; int32_t *raw; const uint8_t *r = smpl_rec(si);
    if (s->made) return;
    s->made = 1; s->root = r[11]; s->tune = ((int)be16(r + 12) - 1024) / 10.0; s->gain_db = r[0] ? 40.0 * log10(r[0] / 127.0) : -100;
    raw = decode_raw(si, &n, &lp, &mode);
    if (!raw) { s->d = NULL; return; }
    s->mode = mode > 2 ? 2 : mode;
    tot = n; if (s->mode == 1) tot = n + (n - 1 - lp) - 1;  /* forward, then back from end-1 to loop+1 */
    s->d = (int16_t *)malloc((tot + 1) * 2); s->peak = 0;
    for (i = 0; i < tot; i++) {
        int32_t v = i < n ? raw[i] : raw[n - 2 - (i - n)];
        double x = v * g_scale; if (fabs((double)v) > s->peak) s->peak = fabs((double)v);
        if (x > 32767) x = 32767; if (x < -32768) x = -32768;
        s->d[i] = (int16_t)lrint(x);
    }
    s->n = tot;
    if (s->mode == 0) { s->ls = lp; s->le = n; }
    else if (s->mode == 1) { s->ls = lp; s->le = tot; }
    else { s->ls = 0; s->le = 0; }
    free(raw);
}
static double global_peak(void)
{
    unsigned si; double pk = 1; uint32_t n, lp, i; int m;
    for (si = 0; si < NUM_SMPL; si++) {
        int32_t *raw = decode_raw(si, &n, &lp, &m);
        if (!raw) continue;
        for (i = 0; i < n; i++) if (fabs((double)raw[i]) > pk) pk = fabs((double)raw[i]);
        free(raw);
    }
    return pk;
}

/* ---------- conversion curves (measured on Nuked-SC55, testrig/sc55sf/exp_times.py, exp_tvf.py) ---------- */
/* stage time byte (0-127) -> ms: linear stages (bit 7 set) last D[t]; exponential stages approach the target with
 * time constant TAU[t]. Values every 8 steps, interpolated in log. */
static const double D_LIN[17] = {0, 60, 142, 258, 413, 626, 912, 1308, 1841, 2553, 3522, 4860, 6642, 9210, 12498, 17000, 23000};
static const double D_TAU[17] = {0, 7.2, 17.3, 26.0, 36.1, 57.7, 83.7, 124.1, 170.2, 239.5, 334.7, 464.5, 639.1, 890.1, 1191.7, 1612.9, 2174};
static double tab8(const double *t, int x)
{
    int i = x >> 3; double f = (x & 7) / 8.0;
    if (x <= 0) return 0; if (i >= 16) return t[16];
    if (t[i] <= 0) return t[i + 1] * f;
    return exp(log(t[i]) * (1 - f) + log(t[i + 1]) * f);
}
static double stage_ms(int tb) { return (tb & 0x80) ? tab8(D_LIN, tb & 0x7f) : 4.6 * tab8(D_TAU, tb & 0x7f); }
/* TVF cutoff index (0-127) -> filter coefficient g (Q14 of the chip's 2-pole SVF), measured every 8 */
static const double G_CUT[17] = {35, 56, 88, 140, 223, 354, 562, 892, 1415, 2245, 3560, 5634, 8875, 13821, 20887, 29160, 29440};
static double cutoff_hz(double x)
{
    double g, f; int i; double fr;
    if (x < 0) x = 0; if (x > 127) x = 127;
    i = (int)(x / 8); fr = x / 8 - i; if (i >= 16) { i = 15; fr = 1; }
    g = exp(log(G_CUT[i]) * (1 - fr) + log(G_CUT[i + 1]) * fr);
    f = g / 16384.0; if (f > 1.99) f = 1.99;
    return 64000.0 / M_PI * asin(f / 2);   /* the SVF runs at the chip rate (64 kHz), the voices at 32 kHz */
}
static int hz_cents(double hz) { return (int)lrint(1200 * log2(hz / 8.176)); }
static int ms_tc(double ms) { if (ms < 1) return -12000; return (int)lrint(1200 * log2(ms / 1000.0)); }
static double amp_lvl(int L) { return (L / 127.0) * (L / 127.0); }          /* level bytes are squared amplitude */
static double db_lvl(int L) { return L <= 0 ? -100 : 40 * log10(L / 127.0); }

/* envelope model: 4 stages from level a[0] (start) through targets; returns amplitude at t (ms) */
typedef struct { double lv[5]; int tb[5]; } Env;  /* lv[0] = start, lv[1..4] = stage targets, tb[0..3] = times T1..T4 */
static double env_at(const Env *e, double t)
{
    double a = e->lv[0]; int s;
    for (s = 0; s < 4; s++) {
        double d = stage_ms(e->tb[s]), b = e->lv[s + 1];
        if (t <= d) {
            if (e->tb[s] & 0x80) return d > 0 ? a + (b - a) * t / d : b;
            { double tau = tab8(D_TAU, e->tb[s] & 0x7f); return tau > 0 ? b + (a - b) * exp(-t / tau) : b; }
        }
        t -= d; a = b;
    }
    return a;
}
static double todb(double a) { return a <= 1e-5 ? -100 : 20 * log10(a); }

/* ---------- SF2 builder ---------- */
enum { G_startAddrsOffset = 0, G_initialFilterFc = 8, G_initialFilterQ = 9, G_modEnvToFilterFc = 11, G_chorus = 15, G_reverb = 16,
       G_pan = 17, G_delayVibLFO = 23, G_freqVibLFO = 24, G_vibLfoToPitch = 6, G_attackModEnv = 26, G_holdModEnv = 27,
       G_decayModEnv = 28, G_sustainModEnv = 29, G_releaseModEnv = 30, G_attackVolEnv = 34, G_holdVolEnv = 35,
       G_decayVolEnv = 36, G_sustainVolEnv = 37, G_releaseVolEnv = 38, G_instrument = 41, G_keyRange = 43, G_velRange = 44,
       G_initialAttenuation = 48, G_coarseTune = 51, G_fineTune = 52, G_sampleID = 53, G_sampleModes = 54,
       G_scaleTuning = 56, G_exclusiveClass = 57, G_overridingRootKey = 58 };
typedef struct { uint16_t op; int16_t v; } Gen;
typedef struct { Gen g[40]; int ng; int smp; } Zone;
typedef struct { char name[21]; Zone *z; int nz, cap; } SInst;
typedef struct { char name[21]; int bank, prog, inst[2]; } SPre;
static SInst *sinst; static int n_sinst, cap_sinst;
static SPre *spre; static int n_spre, cap_spre;
static int inst_map[NUM_INST][2], drum_map[16];
static void gen(Zone *z, int op, int v) { if (v > 32767) v = 32767; if (v < -32768) v = -32768; z->g[z->ng].op = (uint16_t)op; z->g[z->ng].v = (int16_t)v; z->ng++; }
static void gen_range(Zone *z, int op, int lo, int hi) { z->g[z->ng].op = (uint16_t)op; z->g[z->ng].v = (int16_t)(lo | hi << 8); z->ng++; }
static Zone *new_zone(SInst *I) { if (I->nz == I->cap) { I->cap = I->cap ? I->cap * 2 : 8; I->z = (Zone *)realloc(I->z, I->cap * sizeof(Zone)); } memset(&I->z[I->nz], 0, sizeof(Zone)); return &I->z[I->nz++]; }
static SInst *new_inst(const char *name) { SInst *I; if (n_sinst == cap_sinst) { cap_sinst = cap_sinst ? cap_sinst * 2 : 64; sinst = (SInst *)realloc(sinst, cap_sinst * sizeof(SInst)); } I = &sinst[n_sinst++]; memset(I, 0, sizeof *I); strncpy(I->name, name, 20); return I; }

static int max4(const uint8_t *p) { int m = p[0], i; for (i = 1; i < 4; i++) if (p[i] > m) m = p[i]; return m; }
static int pitch_kf(int b9) { int i = b9 - 0x40; if (i <= 10 && i >= -10) return i * 10; if (i == 11) return 120; if (i == 12) return 150; if (i >= 13) return 200; return -100; }

typedef struct { int vol, pan, rev, cho, excl, noteoff; int is_drum; int kitkey; } DrumCtx;

/* one partial -> one zone per key split of its part (or one zone for a drum note) */
static void add_partial(SInst *I, const uint8_t *ir, int pn, int klo, int khi, const DrumCtx *dc)
{
    const uint8_t *pr = ir + 32 + 92 * pn, *pp = pr + 4, *hdr = ir + 12;
    unsigned pidx = be16(pr + 2), y; const uint8_t *part; int lo = 0;
    if (pidx == 0xffff || pidx >= 288) return;
    part = part_rec(pidx);
    for (y = 0; y < 16; y++) {
        int hi = part[12 + y]; unsigned si = be16(part + 28 + 2 * y); int zlo, zhi; Zone *z; Smp *s;
        if (si >= NUM_SMPL) break;
        if (dc && dc->is_drum) { /* drum: the one split that holds the kit's key */
            if (!(dc->kitkey >= lo && dc->kitkey <= hi)) { lo = hi + 1; if (hi >= 127) break; continue; }
            zlo = klo; zhi = khi;
        } else { zlo = lo > klo ? lo : klo; zhi = hi < khi ? hi : khi; }
        make_sample(si); s = &smp[si];
        if (!s->d || zlo > zhi) { lo = hi + 1; if (hi >= 127) break; continue; }
        z = new_zone(I);
        gen_range(z, G_keyRange, zlo, zhi);
        {   /* pitch: cents = 100*(60-root) + kf*(key-60) + tune */
            int kf = pitch_kf(pp[9]);
            double tune = 100.0 * (pp[6] - 64) + 0.87 * (pp[7] - 64) + s->tune - 1.3, c;
            if (dc && dc->is_drum) { c = 100.0 * (60 - s->root) + kf * (dc->kitkey - 60) + tune; gen(z, G_scaleTuning, 0); }
            else { c = 100.0 * (60 - s->root) + tune; gen(z, G_scaleTuning, kf); }
            gen(z, G_overridingRootKey, 60);
            { int ct = (int)floor(c / 100.0 + 0.5); double f = c - 100.0 * ct; gen(z, G_coarseTune, ct); gen(z, G_fineTune, (int)lrint(f)); }
        }
        {   /* level */
            double db = db_lvl(hdr[0]) + db_lvl(pp[65]) + s->gain_db + db_lvl(max4(pp + 70));
            if (dc && dc->is_drum) db += db_lvl(dc->vol);
            if (db < -96) db = -96;
            gen(z, G_initialAttenuation, (int)lrint(-db * 10));
        }
        {   /* TVA: start 0 (or pp69), T1 -> L1, T2 -> L2, T3 -> L3, T4 -> L4 (sustain), T5 = release */
            Env e; int i; double pk, th = 0, t, sus_db, t30 = -1, rel;
            e.lv[0] = amp_lvl(pp[69]); for (i = 0; i < 4; i++) { e.lv[i + 1] = amp_lvl(pp[70 + i]); e.tb[i] = pp[74 + i]; }
            pk = 1e-6; for (i = 1; i < 5; i++) if (e.lv[i] > pk) pk = e.lv[i];   /* SF2 peaks at 0 dB: scale to the loudest stage */
            for (i = 0; i < 5; i++) e.lv[i] /= pk;
            gen(z, G_attackVolEnv, ms_tc((e.tb[0] & 0x80) ? stage_ms(e.tb[0]) : 2.3 * tab8(D_TAU, e.tb[0] & 0x7f)));
            {   /* hold: until 1 dB down after the attack; decay: SF2 dB-linear slope through the half-way point */
                double t0 = stage_ms(e.tb[0]), tend = t0 + stage_ms(e.tb[1]) + stage_ms(e.tb[2]) + stage_ms(e.tb[3]);
                sus_db = todb(e.lv[4]); if (sus_db < -100) sus_db = -100;
                for (t = t0; t < tend && todb(env_at(&e, t)) > -1.0; t += 2) ; th = t - t0;
                if (sus_db > -1.0) { gen(z, G_holdVolEnv, -12000); gen(z, G_decayVolEnv, -12000); }
                else {
                    double target = sus_db < -60 ? -30 : sus_db / 2, tx = -1;
                    for (t = t0 + th; t < tend + 1; t += 2) if (todb(env_at(&e, t)) <= target) { tx = t; break; }
                    if (tx < 0) tx = tend;
                    gen(z, G_holdVolEnv, ms_tc(th));
                    gen(z, G_decayVolEnv, ms_tc((tx - t0 - th) * 100.0 / (-target + 1e-9)));
                }
                gen(z, G_sustainVolEnv, (int)lrint(-sus_db * 10));
            }
            /* release: stage T5 from the sustain level to 0: SF2 time for 100 dB through the -30 dB point */
            {
                int tb = pp[78]; double d = stage_ms(tb);
                if (tb & 0x80) t30 = d * (1 - pow(10, -30 / 20.0)); else t30 = tab8(D_TAU, tb & 0x7f) * log(pow(10, 30 / 20.0));
                rel = t30 * 100.0 / 30.0;
                if (dc && dc->is_drum && !dc->noteoff) { /* drums that ignore note-off: release = the natural decay */
                    double nat = 0; for (i = 0; i < 4; i++) nat += stage_ms(e.tb[i]); if (nat > rel) rel = nat;
                }
                gen(z, G_releaseVolEnv, ms_tc(rel < 5 ? 5 : rel));
            }
        }
        if (pp[35] == 0 && !getenv("SC55SF_NOTVF")) {   /* TVF (pp35: 0 = low-pass, else high-pass: SF2 has none, left out);: cutoff index x = pp33 + (L-64)*pp40/64*0.75; resonance pp34 */
            double x[5], c[5]; int i, Tf[5], q;
            for (i = 0; i < 5; i++) { x[i] = pp[33] + (pp[41 + i] - 64) * pp[40] / 64.0 * 0.75; c[i] = hz_cents(cutoff_hz(x[i])); }
            for (i = 0; i < 5; i++) Tf[i] = pp[46 + i];
            gen(z, G_initialFilterFc, (int)c[4]);
            if (fabs(c[1] - c[4]) > 20 && !getenv("SC55SF_NOMODENV")) {
                double dsum = stage_ms(Tf[1]) + stage_ms(Tf[2]) + stage_ms(Tf[3]);
                gen(z, G_modEnvToFilterFc, (int)(c[1] - c[4]));
                gen(z, G_attackModEnv, ms_tc(stage_ms(Tf[0])));
                gen(z, G_decayModEnv, ms_tc(dsum > 1 ? dsum : 1));
                gen(z, G_sustainModEnv, 1000);
                gen(z, G_releaseModEnv, ms_tc(stage_ms(Tf[4]) + 1));
            }
            q = pp[34]; if (q < 8) q = 8; if (q > 58) q = 58;
            gen(z, G_initialFilterQ, (int)lrint(200 * log10(64.0 / q)));
        }
        {   /* pan, vibrato, sends, sample */
            int pan = (dc && dc->is_drum && dc->pan) ? dc->pan : pp[5];
            if (pan != 0x40) gen(z, G_pan, (int)lrint((pan - 64) / 64.0 * 500));
            if (pp[10] & 0x7f) { gen(z, G_vibLfoToPitch, pp[10] & 0x7f); gen(z, G_freqVibLFO, -687); gen(z, G_delayVibLFO, -2786); }
            if (dc && dc->is_drum) { gen(z, G_reverb, dc->rev * 1000 / 127); gen(z, G_chorus, dc->cho * 1000 / 127); if (dc->excl) gen(z, G_exclusiveClass, dc->excl); }
            gen(z, G_sampleModes, s->mode == 2 ? 0 : 1);
            z->smp = (int)si; gen(z, G_sampleID, (int)si);
        }
        lo = hi + 1;
        if (hi >= 127 || (dc && dc->is_drum)) break;
    }
}
static int melodic_inst(unsigned ii, int pn)
{
    char nm[21]; SInst *I; const uint8_t *r;
    if (inst_map[ii][pn] != -1) return inst_map[ii][pn];
    r = inst_rec(ii); cname(r, nm); if (pn) strcat(nm, " 2");
    I = new_inst(nm); add_partial(I, r, pn, 0, 127, NULL);
    inst_map[ii][pn] = I->nz ? n_sinst - 1 : -2;
    if (!I->nz) n_sinst--;
    return inst_map[ii][pn];
}
static int drum_inst(unsigned k)
{
    const uint8_t *d = drum_rec(k); char nm[13]; SInst *I; int n;
    if (drum_map[k] >= 0) return drum_map[k];
    cname(d + 0x480, nm); I = new_inst(nm);
    for (n = 0; n < 128; n++) {
        unsigned ii = be16(d + 2 * n); DrumCtx dc; int pn;
        if (!inst_ok(ii)) continue;
        dc.is_drum = 1; dc.vol = d[256 + n]; dc.kitkey = d[384 + n]; dc.excl = d[512 + n]; dc.pan = d[640 + n];
        dc.rev = d[768 + n]; dc.cho = d[896 + n]; dc.noteoff = d[1024 + n] & 1;
        for (pn = 0; pn < 2; pn++) add_partial(I, inst_rec(ii), pn, n, n, &dc);
    }
    drum_map[k] = n_sinst - 1;
    return drum_map[k];
}
static void add_preset(const char *name, int bank, int prog, int inst0, int inst1)
{
    if (inst0 < 0 && inst1 < 0) return;
    if (n_spre == cap_spre) { cap_spre = cap_spre ? cap_spre * 2 : 256; spre = (SPre *)realloc(spre, cap_spre * sizeof(SPre)); }
    strncpy(spre[n_spre].name, name, 20); spre[n_spre].name[20] = 0; spre[n_spre].bank = bank; spre[n_spre].prog = prog;
    spre[n_spre].inst[0] = inst0; spre[n_spre].inst[1] = inst1; n_spre++;
}
static const int KIT_PROG[14] = {0, 8, 16, 24, 25, 32, 40, 48, 56, -1, -1, -1, -1, 127};
static void build(void)
{
    unsigned b, p, k;
    for (p = 0; p < NUM_INST; p++) inst_map[p][0] = inst_map[p][1] = -1;
    for (k = 0; k < 16; k++) drum_map[k] = -1;
    for (b = 0; b < 128; b++) for (p = 0; p < 128; p++) {
        unsigned ii = variation(b, p); char nm[13];
        if (!inst_ok(ii)) continue;
        if (b && ii == variation(0, p)) continue;     /* same as the capital tone: the player falls back to bank 0 */
        cname(inst_rec(ii), nm);
        add_preset(nm, b, p, melodic_inst(ii, 0), melodic_inst(ii, 1));
    }
    for (k = 0; k < 14; k++) if (KIT_PROG[k] >= 0) { char nm[13]; cname(drum_rec(k) + 0x480, nm); add_preset(nm, 128, KIT_PROG[k], drum_inst(k), -1); }
}

/* ---------- writer (SF2 = version 2.01; SBK = SoundFont 1 for the AWE32 player) ---------- */
typedef struct { uint8_t *b; size_t n, cap; } Buf;
static void put(Buf *o, const void *d, size_t n) { if (o->n + n > o->cap) { o->cap = (o->n + n) * 2 + 4096; o->b = (uint8_t *)realloc(o->b, o->cap); } memcpy(o->b + o->n, d, n); o->n += n; }
static void p16(Buf *o, unsigned v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; put(o, b, 2); }
static void p32(Buf *o, uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; put(o, b, 4); }
static void pname(Buf *o, const char *s, int n) { char t[40] = {0}; strncpy(t, s, n); put(o, t, n); }
static void chunk(Buf *o, const char *id, const Buf *c) { put(o, id, 4); p32(o, (uint32_t)c->n); put(o, c->b, c->n); if (c->n & 1) { uint8_t z = 0; put(o, &z, 1); } }
static void list(Buf *o, const char *id, const Buf *c) { Buf t = {0}; put(&t, id, 4); put(&t, c->b, c->n); chunk(o, "LIST", &t); free(t.b); }
static void cstr(Buf *o, const char *id, const char *s) { Buf t = {0}; put(&t, s, strlen(s) + 1); if (t.n & 1) { uint8_t z = 0; put(&t, &z, 1); } chunk(o, id, &t); free(t.b); }

static int is_default(int op, int v)
{
    switch (op) {
    case G_attackVolEnv: case G_holdVolEnv: case G_decayVolEnv: case G_attackModEnv: case G_holdModEnv: case G_decayModEnv: return v <= -12000;
    case G_sustainVolEnv: case G_sustainModEnv: case G_pan: case G_coarseTune: case G_fineTune: case G_initialFilterQ: case G_sampleModes:
    case G_modEnvToFilterFc: case G_vibLfoToPitch: case G_initialAttenuation: case G_exclusiveClass: case G_reverb: case G_chorus: return v == 0;
    case G_initialFilterFc: return v >= 13500;
    case G_scaleTuning: return v == 100;
    }
    return 0;
}
/* SF2 value -> SoundFont 1 (SBK) value: the inverse of awesfx's sbkconv.c (as testrig/awe/sf2sbk.py) */
static int sbkv(int o, int v)
{
#define CL(x, a, b) ((int)fmax((a), fmin((b), floor((x) + 0.5))))
    switch (o) {
    case 8: return v >= 14400 ? 127 : CL((v - 4366) / 59.0, 0, 126);
    case 9: return CL(v * 2 / 3.0, 0, 127);
    case 15: case 16: return CL(v * 256 / 1000.0, 0, 255);
    case 17: return CL((v + 500) * 127 / 1000.0, 0, 127);
    case 48: return v >= 1000 ? 0 : CL(127 * pow(10, -v / 2000.0), 1, 127);
    case 56: return v <= 75 ? 1 : 0;
    case 21: case 23: case 25: case 26: case 27: case 28: case 30: case 33: case 34: case 35: case 36: case 38:
        return CL(1000 * pow(2, v / 1200.0), 1, 32767);
    case 31: case 32: case 39: case 40: return CL(v / 5.55, -127, 127);
    case 22: case 24: return CL(pow(2, (v + 7925) / 1200.0), 1, 255);
    case 5: case 6: case 7: return CL(v * 64 / 600.0, -127, 127);
    case 10: return CL(v * 64 / 3600.0, -127, 127);
    case 11: return CL(v * 64 / 7200.0, -127, 127);
    case 13: return CL(v * 64 / 120.0, -127, 127);
    case 29: return CL(96 - v * 96 / 1000.0, 0, 96);
    case 37: return CL((2000 - 2 * v) / 21.0, 0, 96);
    }
    return v;
#undef CL
}
static int write_bank(const char *fn, int sbk, long ram_kb)
{
    Buf info = {0}, sdta = {0}, smpl = {0}, pdta = {0}, ph = {0}, pb = {0}, pg = {0}, ih = {0}, ib = {0}, ig = {0}, im = {0}, sh = {0}, sn = {0}, all = {0}, body = {0};
    int *pos = (int *)calloc(NUM_SMPL, sizeof(int)), *used = (int *)calloc(NUM_SMPL, sizeof(int)), i, j, nb = 0, ng = 0, ni = 0, ns = 0, *order = (int *)calloc(NUM_SMPL, sizeof(int));
    uint32_t words = 0, total = 0; FILE *f; uint8_t zero[96] = {0};
    for (i = 0; i < n_sinst; i++) for (j = 0; j < sinst[i].nz; j++) used[sinst[i].z[j].smp] = 1;
    for (i = 0; i < NUM_SMPL; i++) if (used[i] && smp[i].d) { order[ns++] = i; total += smp[i].n + 46; }
    if (sbk) {   /* fit the AWE sample RAM: halve the rate of the longest samples until it fits */
        uint32_t lim = (uint32_t)(ram_kb * 512) - 64;
        while (total > lim) {
            int best = -1; uint32_t bn = 0, k;
            for (i = 0; i < ns; i++) { Smp *s = &smp[order[i]]; if (s->halved < 2 && s->n > bn) { bn = s->n; best = order[i]; } }
            if (best < 0) break;
            { Smp *s = &smp[best]; uint32_t nn = s->n / 2; for (k = 0; k < nn; k++) s->d[k] = (int16_t)((s->d[2 * k] + s->d[2 * k + 1]) / 2);
              total -= s->n - nn; s->n = nn; s->ls /= 2; s->le /= 2; s->halved++; }
        }
        if (total > lim) { msg("  SBK: samples need %lu KB, more than /RAM:%ld\n", (unsigned long)total / 512, ram_kb); }
    }
    /* sample data */
    for (i = 0; i < ns; i++) {
        Smp *s = &smp[order[i]]; uint32_t k;
        pos[order[i]] = (int)words; smp[order[i]].sf2 = i;
        for (k = 0; k < s->n; k++) p16(&smpl, (uint16_t)s->d[k]);
        put(&smpl, zero, 92); words += s->n + 46;
        if (sbk) put(&sn, "SC55 sample         ", 20);
        else {
            char nm[21]; snprintf(nm, sizeof nm, "SC55 s%d", order[i]); pname(&sh, nm, 20);
            p32(&sh, pos[order[i]]); p32(&sh, pos[order[i]] + s->n);
            p32(&sh, pos[order[i]] + (s->mode == 2 ? 0 : s->ls)); p32(&sh, pos[order[i]] + (s->mode == 2 ? s->n - 1 : s->le));
            p32(&sh, s->halved ? 32000 >> s->halved : 32000); { uint8_t pr[2] = {(uint8_t)s->root, 0}; put(&sh, pr, 2); }
            p16(&sh, 0); p16(&sh, 1);
        }
        if (sbk) { p32(&sh, pos[order[i]]); p32(&sh, pos[order[i]] + s->n); p32(&sh, pos[order[i]] + (s->mode == 2 ? 0 : s->ls) - 1); p32(&sh, pos[order[i]] + (s->mode == 2 ? s->n - 1 : s->le) - 2); }
    }
    if (!sbk) { pname(&sh, "EOS", 20); p32(&sh, 0); p32(&sh, 0); p32(&sh, 0); p32(&sh, 0); p32(&sh, 0); p16(&sh, 0); p16(&sh, 0); p16(&sh, 0); }
    /* instruments */
    for (i = 0; i < n_sinst; i++) {
        SInst *I = &sinst[i]; char common[64];
        pname(&ih, I->name, 20); p16(&ih, nb);
        memset(common, 0, sizeof common);
        if (!sbk) {   /* global zone: generators equal in every zone; CC91 / CC93 set the sends directly, as on the SC-55 */
            int k, op; Zone *z0 = &I->z[0];
            for (k = 0; k < z0->ng; k++) {
                op = z0->g[k].op; if (op == G_keyRange || op == G_sampleID) continue;
                for (j = 1; j < I->nz; j++) { int m, f = 0; for (m = 0; m < I->z[j].ng; m++) if (I->z[j].g[m].op == op) { f = I->z[j].g[m].v == z0->g[k].v; break; } if (!f) break; }
                if (j == I->nz) common[op] = 1;
            }
            p16(&ib, ng); p16(&ib, ni); nb++;
            for (k = 0; k < z0->ng; k++) if (common[z0->g[k].op] && !is_default(z0->g[k].op, z0->g[k].v)) { p16(&ig, z0->g[k].op); p16(&ig, (uint16_t)z0->g[k].v); ng++; }
            p16(&im, 0x00DB); p16(&im, G_reverb); p16(&im, 1000); p16(&im, 0); p16(&im, 0);
            p16(&im, 0x00DD); p16(&im, G_chorus); p16(&im, 1000); p16(&im, 0); p16(&im, 0); ni += 2;
        }
        for (j = 0; j < I->nz; j++) {
            Zone *z = &I->z[j]; int k, k2, klo = 0, khi = 127, ct = 0, ft = 0, st = 100, key, kfrom, kto; Smp *s = &smp[z->smp];
            for (k2 = 0; k2 < z->ng; k2++) {
                if (z->g[k2].op == G_keyRange) { klo = z->g[k2].v & 255; khi = (z->g[k2].v >> 8) & 255; }
                if (z->g[k2].op == G_coarseTune) ct = z->g[k2].v; if (z->g[k2].op == G_fineTune) ft = z->g[k2].v; if (z->g[k2].op == G_scaleTuning) st = z->g[k2].v;
            }
            /* SBK keeps only 100 (or 50) cents per key: other key follows get one zone per key with the exact pitch */
            kfrom = kto = -1; if (sbk && st != 100 && khi - klo < 4) { kfrom = klo; kto = khi; }
            for (key = kfrom; key <= kto; key++) {
                p16(&ib, ng); p16(&ib, ni); nb++;
                if (key >= 0) { p16(&ig, G_keyRange); p16(&ig, (uint16_t)(key | key << 8)); ng++; }
                else for (k = 0; k < z->ng; k++) if (z->g[k].op == G_keyRange) { p16(&ig, G_keyRange); p16(&ig, (uint16_t)z->g[k].v); ng++; }
                for (k = 0; k < z->ng; k++) {
                    int op = z->g[k].op, v = z->g[k].v;
                    if (op == G_keyRange || op == G_sampleID) continue;
                    if (!sbk && (common[op] || is_default(op, v))) continue;
                    if (sbk) {
                        if (op == G_overridingRootKey || op == G_coarseTune || op == G_fineTune || op == G_scaleTuning) continue;
                        v = sbkv(op, v);
                    }
                    p16(&ig, op); p16(&ig, (uint16_t)v); ng++;
                }
                if (sbk) {   /* SBK: the zone pitch becomes "sample pitch" (gen 55): key k plays k*100 - P cents from 44.1 kHz */
                    double r = 32000 >> s->halved, c44, P;
                    int kk = key >= 0 ? key : 60;
                    c44 = st * (kk - 60) + 100.0 * ct + ft + 1200 * log2(r / 44100.0);
                    P = kk * 100 - c44;
                    p16(&ig, 56); p16(&ig, 0); ng++;
                    p16(&ig, 55); p16(&ig, (uint16_t)(int)lrint(P)); ng++;
                }
                p16(&ig, G_sampleID); p16(&ig, s->sf2); ng++;
            }
        }
    }
    if (ng > 65535 || nb > 65535 || ni > 65535) { msg("  %s: too many generators (%d) / zones (%d)\n", fn, ng, nb); return 0; }
    pname(&ih, "EOI", 20); p16(&ih, nb); p16(&ib, ng); p16(&ib, ni); p16(&ig, 0); p16(&ig, 0);
    for (i = 0; i < 10; i++) { uint8_t z = 0; put(&im, &z, 1); }
    /* presets */
    {
        int k, pbn = 0, pgn = 0;
        for (k = 0; k < n_spre; k++) {
            if (sbk && spre[k].bank != 0 && spre[k].bank != 128 && spre[k].bank > 16) continue;   /* SBK: GM + GS variations + kits */
            pname(&ph, spre[k].name, 20); p16(&ph, spre[k].prog); p16(&ph, spre[k].bank); p16(&ph, pbn); p32(&ph, 0); p32(&ph, 0); p32(&ph, 0);
            { int q; for (q = 0; q < 2; q++) if (spre[k].inst[q] >= 0) { p16(&pb, pgn); p16(&pb, 0); pbn++; p16(&pg, G_instrument); p16(&pg, spre[k].inst[q]); pgn++; } }
        }
        pname(&ph, "EOP", 20); p16(&ph, 255); p16(&ph, 255); p16(&ph, pbn); p32(&ph, 0); p32(&ph, 0); p32(&ph, 0);
        p16(&pb, pgn); p16(&pb, 0); p16(&pg, 0); p16(&pg, 0);
    }
    { Buf t = {0}; p16(&t, sbk ? 1 : 2); p16(&t, sbk ? 0 : 1); chunk(&info, "ifil", &t); free(t.b); }
    cstr(&info, "isng", "EMU8000");
    { char nm[100]; snprintf(nm, sizeof nm, "%s (made by SC55SF from your ROMs)", romname); cstr(&info, "INAM", nm); }
    if (sbk) cstr(&info, "irom", "1MGM");
    cstr(&info, "ISFT", "NUKED SB-EMU SC55SF");
    if (sbk) chunk(&sdta, "snam", &sn);
    chunk(&sdta, "smpl", &smpl);
    chunk(&pdta, "phdr", &ph); chunk(&pdta, "pbag", &pb);
    { Buf t = {0}; for (i = 0; i < 10; i++) { uint8_t z = 0; put(&t, &z, 1); } chunk(&pdta, "pmod", &t); free(t.b); }
    chunk(&pdta, "pgen", &pg); chunk(&pdta, "inst", &ih); chunk(&pdta, "ibag", &ib); chunk(&pdta, "imod", &im); chunk(&pdta, "igen", &ig); chunk(&pdta, "shdr", &sh);
    put(&body, "sfbk", 4); list(&body, "INFO", &info); list(&body, "sdta", &sdta); list(&body, "pdta", &pdta);
    chunk(&all, "RIFF", &body);
    f = fopen(fn, "wb");
    if (!f || fwrite(all.b, 1, all.n, f) != all.n) { if (f) fclose(f); return 0; }
    fclose(f);
    msg("  %s: %s, %d samples, %lu KB samples, %lu KB\n", fn, sbk ? "SoundFont 1 (AWE32)" : "SoundFont 2", ns, (unsigned long)words / 512, (unsigned long)all.n / 1024);
    free(pos); free(used); free(order);
    return 1;
}

int main(int argc, char **argv)
{
    const char *dir = "ROMS"; char sf2[600] = "", sbk[600] = ""; long ram = 8192; int do_sf2 = 1, do_sbk = 1, i, list_only = 0;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] == '/' || a[0] == '-') {
            if (!strncasecmp(a + 1, "SF2:", 4)) snprintf(sf2, sizeof sf2, "%s", a + 5);
            else if (!strncasecmp(a + 1, "SBK:", 4)) snprintf(sbk, sizeof sbk, "%s", a + 5);
            else if (!strncasecmp(a + 1, "RAM:", 4)) ram = atol(a + 5);
            else if (!strcasecmp(a + 1, "NOSF2")) do_sf2 = 0;
            else if (!strcasecmp(a + 1, "NOSBK")) do_sbk = 0;
            else if (!strcasecmp(a + 1, "Q")) q_mode = 1;
            else if (!strcasecmp(a + 1, "LIST")) list_only = 1;
            else if (a[1] == '?' || !strcasecmp(a + 1, "H")) { puts("SC55SF [romdir] [/SF2:file] [/SBK:file] [/RAM:kb] [/NOSF2] [/NOSBK] [/Q] [/LIST]"); return 0; }
            else dir = a;   /* a path (host builds) */
        } else dir = a;
    }
    if (ram < 512) ram = 512; if (ram > 28672) ram = 28672;
    if (!sf2[0]) snprintf(sf2, sizeof sf2, "%s" SEP "SC55.SF2", dir);
    if (!sbk[0]) snprintf(sbk, sizeof sbk, "%s" SEP "SC55.SBK", dir);
    msg("SC55SF: SC-55 instruments from your ROMs -> SoundFont\n");
    if (!find_roms(dir)) { if (!q_mode) printf("  no complete SC-55 (mk1) ROM set in %s (control ROM + 3 wave ROMs)\n", dir); return 2; }
    { double pk = global_peak(); g_scale = 32000.0 / pk; }
    build();
    if (list_only) { int k; for (k = 0; k < n_spre; k++) printf("%3d:%3d %s\n", spre[k].bank, spre[k].prog, spre[k].name); return 0; }
    msg("  %d presets, %d instruments\n", n_spre, n_sinst);
    if (do_sf2 && !write_bank(sf2, 0, 0)) { printf("  cannot write %s\n", sf2); return 3; }
    if (do_sbk && !write_bank(sbk, 1, ram)) { printf("  cannot write %s\n", sbk); return 3; }
    return 0;
}
