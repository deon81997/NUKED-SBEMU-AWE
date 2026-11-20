/* emu8k.c - standalone EMU8000 synth engine (AWE32/AWE64).
 *
 * Behaviour follows 86Box's EMU8000 emulation (register map, envelope,
 * LFO and modulation scaling; see REGS.md), reimplemented from scratch in
 * integer arithmetic. Floating point is used only to build tables in
 * emu8k_init(); the per-sample loop is integer only.
 *
 * Reverb and chorus (fx_*): the effect structure, register meanings and
 * parameter scaling are 86Box's (src/sound/snd_emu8k.c, the 86Box/PCem
 * contributors, GPL-2.0-or-later, as this file), in integer arithmetic,
 * programmed by the INIT1-4 / HWCF4-5 values Creative's drivers write.
 *
 * Engine rate: 44100 Hz, 32 voices. Modulation (envelopes, LFOs, targets)
 * is computed every EMU8K_CTRL samples (default 16, 1 = per sample like
 * 86Box); volume slides, pitch and filter apply per sample.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emu8k.h"

#ifndef EMU8K_CTRL
#define EMU8K_CTRL 16
#endif
#define BLOCK     64             /* frames per internal render block */
#define PAD       (BLOCK * 4 + 16) /* zero words after ROM/RAM: reads past the end in one block */
#define ADDR_MASK 0xFFFFFFu
#define ENV_MAX   (1 << 21)
#define ATT_MAX   0x1FFFFF

enum { ENV_STOPPED, ENV_DELAY, ENV_ATTACK, ENV_HOLD, ENV_RAMP_DOWN, ENV_RAMP_UP, ENV_SUSTAIN };

typedef struct {
    int     state;
    int32_t delay, hold;        /* remaining samples */
    int32_t attack_inc;         /* amplitude units (1<<21 full) per sample */
    int32_t amp;                /* linear value during attack */
    int32_t db;                 /* vol: attenuation 0..1<<21; mod: 0..1<<21 */
    int32_t sustain, ramp;
} env_t;

typedef struct {
    /* raw registers */
    uint32_t ptrx, psst, csl, ccca, z1, z2;
    uint16_t envvol, dcysusv, envval, dcysus, atkhldv, lfo1val, atkhld, lfo2val;
    uint16_t ip, ifatn, pefe, fmmod, tremfrq, fm2frq2;
    /* oscillator */
    uint32_t pos, frac;         /* 24-bit word address, 16-bit fraction */
    uint32_t loop_start, loop_end;
    uint32_t cur_pitch, pitch_t;
    int32_t  cur_vol, vol_t;
    uint32_t cur_cut, cut_t;
    int      env_on;
    env_t    venv, menv;
    int32_t  init_att, init_filt;
    int32_t  lfo1_delay, lfo2_delay;
    uint32_t lfo1_cnt, lfo2_cnt, lfo1_speed, lfo2_speed;
    int32_t  m_env_pitch, m_env_filt, m_lfo1_pitch, m_lfo1_filt, m_lfo1_trem, m_lfo2_pitch;
    /* filter: direct-form biquad, x in samples, y in samples << 4 */
    int      q;
    int32_t  x1, x2, y1, y2;
    int32_t  pan_l, pan_r;      /* 0..255 */
    int      ctrl_left;         /* samples until the next modulation update */
} voice_t;

typedef struct {
    voice_t  v[32];
    uint16_t *blk[256];         /* 64K-word blocks: 0-7 ROM, 32+ RAM */
    uint16_t *rom, *ram, *empty;
    uint32_t ram_end, ram_alloc_end;
    uint32_t smalr, smarr, smalw, smarw;
    uint16_t smld_buf, smrd_buf;
    uint32_t hwcf4, hwcf5, hwcf6, hwcf7;
    uint16_t hwcf1, hwcf2, hwcf3, id;
    uint16_t init1[32], init2[32], init3[32], init4[32];
    uint16_t wc;
    int      cur_reg, cur_ch, rnd;
    /* output resampler */
    int16_t  ob[BLOCK * 2];
    int      ob_len, ob_pos;
    uint32_t rs_phase;
    int16_t  rs_prev[2];
    int      ok;
    int      ext;               /* rom/ram belong to the caller (emu8k_init_mem) */
    struct fx_t *fx;            /* reverb + chorus state (calloc) */
    int      fx_off;            /* emu8k_set_fx(0) */
    int32_t  rev_hold, cho_hold; /* samples reverb / chorus still run (their sends seen recently: the tails) */
} emu8k_t;

static emu8k_t E;

/* ---- tables (built once) ---- */
static uint32_t t_exp2[4096];            /* 2^(i/4096) in Q16 (65536..131071) */
static uint32_t t_log2[4097];            /* 65536*log2(1+i/4096) */
static int32_t  t_attack[128];           /* attack samples */
static uint16_t t_att[256];              /* IFATN attenuation -> volume target */
static uint32_t t_lfo[256];              /* LFO speed, 16.16 table steps per sample */
static int32_t  t_filt[16][256][3];      /* biquad LPF: b (incl. Q gain), -a1, -a2; Q28 */
static int16_t  t_cubic[1024][4];        /* Catmull-Rom, Q14 */
static uint16_t t_pitch[65536];          /* IP -> pitch (0x4000 = 1.0), as 86Box */
static const int32_t t_filt_att[16] = {  /* DC gain vs Q (86Box), Q16 */
    65536, 61869, 57079, 53269, 49145, 44820, 40877, 34792, 32845, 30653, 28607,
    26392, 24630, 22463, 20487, 18470 };
/* Decay/release step per sample in 1/21845 dB (or octave) units, index =
 * DCYSUS(V) low 7 bits. Values as in 86Box (derived from the Linux AWE
 * driver's decay_time_tbl). */
static const int16_t t_decay[128] = {
       0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,
      16,   17,   18,   19,   20,   20,   21,   22,   23,   24,   25,   27,   28,   29,   30,   32,
      33,   34,   36,   38,   39,   41,   43,   45,   49,   51,   53,   55,   58,   60,   63,   66,
      69,   72,   75,   78,   82,   85,   89,   93,   97,  102,  106,  111,  116,  121,  126,  132,
     138,  144,  150,  157,  164,  171,  179,  186,  195,  203,  212,  222,  232,  243,  253,  264,
     276,  288,  301,  315,  328,  342,  358,  374,  390,  406,  425,  444,  466,  485,  506,  528,
     553,  580,  602,  634,  660,  689,  721,  755,  780,  820,  849,  897,  932,  970, 1012, 1057,
    1106, 1160, 1219, 1285, 1321, 1399, 1441, 1534, 1585, 1640, 1698, 1829, 1902, 1981, 2068, 2162 };
static int tables_done;
static int16_t t_sin[4096];                /* chorus LFO, Q15 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void build_tables(void)
{
    int i, q;
    if (tables_done)
        return;
    for (i = 0; i < 4096; i++)
        t_sin[i] = (int16_t) floor(sin(i * M_PI / 2048.0) * 32767.0 + 0.5);
    for (i = 0; i < 4096; i++)
        t_exp2[i] = (uint32_t) (pow(2.0, i / 4096.0) * 65536.0 + 0.5);
    for (i = 0; i <= 4096; i++)
        t_log2[i] = (uint32_t) (log(1.0 + i / 4096.0) / log(2.0) * 65536.0);
    for (i = 0; i < 128; i++) {
        double ms = (i == 0) ? 0 : (i < 32) ? 11878.0 / i : 360.0 * exp((i - 32) / (16.0 / log(0.5)));
        t_attack[i] = (int32_t) (44.1 * ms);
    }
    {
        double o = 65535.0;
        for (i = 0; i < 256; i++) {
            t_att[i] = (uint16_t) o;
            o /= sqrt(1.09018);
        }
        t_att[255] = 0;
    }
    for (i = 0; i < 256; i++)
        t_lfo[i] = (uint32_t) ((0.01 + 0.042 * i) * 65536.0 / 44100.0 * 65536.0);
    for (q = 0; q < 16; q++) {
        double fc = 125.0;
        /* 2-pole resonant LPF (EMU8000 docs): Q 0..15 = 0..22.5 dB peak over
         * Butterworth, DC gain lowered with Q as 86Box's filter_atten. */
        double qlin = pow(10.0, q * 1.5 / 20.0) * 0.7071, g = t_filt_att[q] / 65536.0;
        for (i = 0; i < 256; i++) {
            double w = fc / 44100.0, w0, cs, al, a0;
            if (w > 0.45)
                w = 0.45;
            w0 = 2 * 3.14159265358979 * w;
            cs = cos(w0);
            al = sin(w0) / (2 * qlin);
            a0 = 1 + al;
            t_filt[q][i][0] = (int32_t) ((1 - cs) / 2 / a0 * g * 268435456.0);
            t_filt[q][i][1] = (int32_t) (2 * cs / a0 * 268435456.0);
            t_filt[q][i][2] = (int32_t) (-(1 - al) / a0 * 268435456.0);
            fc *= 1.016378315; /* 86Box: 125 Hz, ~42.66 steps per octave */
        }
    }
    for (i = 0; i < 65536; i++) {
        double p = pow(2.0, (i - 0xE000) / 4096.0) * 16384.0;
        t_pitch[i] = (uint16_t) (i == 0 ? 0 : p > 65535.0 ? 65535 : p);
    }
    for (i = 0; i < 1024; i++) {
        double x = i / 1024.0;
        t_cubic[i][0] = (int16_t) floor((-0.5 * x * x * x + x * x - 0.5 * x) * 16384.0 + 0.5);
        t_cubic[i][1] = (int16_t) floor((1.5 * x * x * x - 2.5 * x * x + 1.0) * 16384.0 + 0.5);
        t_cubic[i][2] = (int16_t) floor((-1.5 * x * x * x + 2.0 * x * x + 0.5 * x) * 16384.0 + 0.5);
        t_cubic[i][3] = (int16_t) floor((0.5 * x * x * x - 0.5 * x * x) * 16384.0 + 0.5);
    }
    tables_done = 1;
}

/* 0xE000 = 0x4000 (no shift), +-0x1000 per octave; 0 = stopped */
#define pitch_of(ip) ((uint32_t) t_pitch[(ip) & 0xFFFF])
/* attenuation 0..0xFFFF (65535 = -96 dB) -> volume 0..65535: 65535*2^(-a/4096) */
static int32_t vol_of(uint32_t a)
{
    uint32_t f = a & 4095, base;
    if (a >= 0xFFFF)
        return 0;
    base = f ? t_exp2[4096 - f] >> 1 : 65535;
    if (base > 65535)
        base = 65535;
    return (int32_t) (base >> (a >> 12));
}
/* linear amplitude 0..65536 -> attenuation 0..65535 (-4096*log2(a/65536)) */
static int32_t amp_to_att(uint32_t a)
{
    int e = 15;
    int32_t r;
    if (a == 0)
        return 65535;
    if (a >= 65535)
        return 0;
    while (!(a & (1u << e)))
        e--;
    r = 65536 - (e * 4096 + (int32_t) ((t_log2[((a << (15 - e)) & 0x7FFF) >> 3] + 8) >> 4));
    return r < 0 ? 0 : r > 65535 ? 65535 : r;
}
/* mod env attack: linear 0..65536 -> log2(1+x)*65536 */
static int32_t hz_to_oct(uint32_t x)
{
    uint32_t i = x >> 4, f = x & 15;
    if (i >= 4096)
        return 65536;
    return (int32_t) ((t_log2[i] * (16 - f) + t_log2[i + 1] * f) >> 4);
}

/* ---- memory ---- */
static inline uint16_t rd(uint32_t a)
{
    a &= ADDR_MASK;
    return E.blk[a >> 16][a & 0xFFFF];
}

void emu8k_ram_write(uint32_t addr, uint16_t w)
{
    addr &= ADDR_MASK;
    if (!E.ram || addr < EMU8K_RAM_START || addr >= 0xFFFFE0)
        return;
    while (addr >= E.ram_end) /* wraps like the card (OpenCP sizes RAM this way) */
        addr -= E.ram_end - EMU8K_RAM_START;
    E.ram[addr - EMU8K_RAM_START] = w;
}

uint16_t emu8k_ram_read(uint32_t addr)
{
    return rd(addr);
}

/* ---- envelopes ---- */
static void env_trigger(env_t *e, uint16_t delayreg, uint16_t atkhld)
{
    e->amp   = 0;
    e->delay = (delayreg & 0x8000) ? 0 : (int32_t) ((0x8000 - (delayreg & 0x7FFF)) << 5);
    e->hold  = 4096 * (0x7F - ((atkhld >> 8) & 0x7F));
    if (e->delay)
        e->state = ENV_DELAY;
    else if (e->attack_inc == 0)
        e->state = ENV_STOPPED;
    else
        e->state = ENV_ATTACK;
}

static void set_attack(env_t *e, uint16_t atkhld)
{
    int32_t s = t_attack[atkhld & 0x7F];
    e->attack_inc = s ? ENV_MAX / s : 0;
}

/* advance volume envelope n samples; returns attenuation (0..1<<21) to add,
 * or -1 = fully muted */
static int32_t venv_step(env_t *e, int n)
{
    switch (e->state) {
    case ENV_DELAY:
        e->delay -= n;
        if (e->delay <= 0) {
            e->delay = 0;
            e->state = ENV_ATTACK;
        }
        return -1;
    case ENV_ATTACK:
        e->amp += e->attack_inc * n;
        if (e->amp >= ENV_MAX) {
            e->amp   = ENV_MAX;
            e->db    = 0;
            e->state = e->hold > 0 ? ENV_HOLD : ENV_RAMP_UP;
        }
        return amp_to_att((uint32_t) e->amp >> 5) << 5;
    case ENV_HOLD:
        e->hold -= n;
        if (e->hold <= 0)
            e->state = ENV_RAMP_UP;
        return e->db;
    case ENV_RAMP_DOWN:
        e->db -= e->ramp * n;
        if (e->db <= e->sustain) {
            e->db    = e->sustain;
            e->state = ENV_SUSTAIN;
        }
        return e->db;
    case ENV_RAMP_UP:
        e->db += e->ramp * n;
        if (e->db >= e->sustain) {
            e->db    = e->sustain;
            e->state = ENV_SUSTAIN;
        }
        return e->db;
    case ENV_SUSTAIN:
        return e->db;
    default:
        return -1;
    }
}

/* modulation envelope: value 0..1<<21 */
static int32_t menv_step(env_t *e, int n)
{
    switch (e->state) {
    case ENV_DELAY:
        e->delay -= n;
        if (e->delay <= 0) {
            e->delay = 0;
            e->state = ENV_ATTACK;
        }
        break;
    case ENV_ATTACK:
        e->amp += e->attack_inc * n;
        if (e->amp >= ENV_MAX) {
            e->amp   = ENV_MAX;
            e->db    = ENV_MAX;
            /* 86Box goes hold -> RAMP_UP (jumps to sustain); the docs say decay */
            e->state = e->hold > 0 ? ENV_HOLD : ENV_RAMP_DOWN;
        } else
            e->db = hz_to_oct((uint32_t) e->amp >> 5) << 5;
        break;
    case ENV_HOLD:
        e->hold -= n;
        if (e->hold <= 0)
            e->state = ENV_RAMP_DOWN;
        break;
    case ENV_RAMP_DOWN:
        e->db -= e->ramp * n;
        if (e->db <= e->sustain) {
            e->db    = e->sustain;
            e->state = ENV_SUSTAIN;
        }
        break;
    case ENV_RAMP_UP:
        e->db += e->ramp * n;
        if (e->db >= e->sustain) {
            e->db    = e->sustain;
            e->state = ENV_SUSTAIN;
        }
        break;
    default:
        break;
    }
    return e->db;
}

static inline int32_t tri(uint32_t cnt) /* LFO triangle, starts at 0 rising, +-32768 */
{
    int32_t d = (int32_t) (((cnt >> 16) + 16384) & 0xFFFF);
    return d >= 32768 ? 98304 - 2 * d : 2 * d - 32768;
}

static int32_t s8scale(int v) /* signed 8-bit amount -> +-0x4000 */
{
    return v < 0 ? v * 0x4000 / 0x80 : v * 0x4000 / 0x7F;
}

/* volume / filter / pitch targets from the envelopes' current values (ve = venv_step, me = menv_step) */
static inline __attribute__((always_inline)) void
mod_targets(const voice_t *v, int32_t ve, int32_t me, int32_t *vol_t, uint32_t *cut_t, uint32_t *pitch_t)
{
    int32_t att = v->init_att, cut = v->init_filt, pitch = v->ip, l1;
    att = ve < 0 ? ATT_MAX : att + ve;
    l1 = tri(v->lfo1_cnt);
    if (v->m_env_pitch)
        pitch += ((me >> 9) * v->m_env_pitch) >> 14;
    if (v->m_lfo1_pitch)
        pitch += (l1 * v->m_lfo1_pitch) >> 17;
    if (v->m_lfo2_pitch)
        pitch += (tri(v->lfo2_cnt) * v->m_lfo2_pitch) >> 17;
    if (v->m_env_filt)
        cut += ((me >> 9) * v->m_env_filt) >> 5;
    if (v->m_lfo1_filt)
        cut += (l1 * v->m_lfo1_filt) >> 9;
    if (v->m_lfo1_trem)
        att += (l1 * v->m_lfo1_trem) >> 11;
    pitch = pitch < 0 ? 0 : pitch > 0xFFFF ? 0xFFFF : pitch;
    att   = att < 0 ? 0 : att > ATT_MAX ? ATT_MAX : att;
    cut   = cut < 0 ? 0 : cut > ATT_MAX ? ATT_MAX : cut;
    *vol_t   = vol_of((uint32_t) att >> 5);
    *cut_t   = (uint32_t) cut >> 5;
    *pitch_t = pitch_of((uint32_t) pitch);
}

static void mod_update(voice_t *v, int n)
{
    int32_t ve, me;
    ve = venv_step(&v->venv, n);
    me = menv_step(&v->menv, n);
    if (v->lfo1_delay > 0) {
        v->lfo1_delay -= n;
        if (v->lfo1_delay < 0)
            v->lfo1_delay = 0;
    } else
        v->lfo1_cnt += v->lfo1_speed * (uint32_t) n;
    if (v->lfo2_delay > 0) {
        v->lfo2_delay -= n;
        if (v->lfo2_delay < 0)
            v->lfo2_delay = 0;
    } else
        v->lfo2_cnt += v->lfo2_speed * (uint32_t) n;
    mod_targets(v, ve, me, &v->vol_t, &v->cut_t, &v->pitch_t);
}

/* VTFT as the chip shows it at this moment: its envelope engine writes the targets every sample, so right after a
 * note-on the volume target is the attack's start (near 0), not the initial attenuation. The SDK drivers (HMI SOS,
 * Apogee) read VTFT to pick the voice to take; a stale value made Descent take other voices than on a real card.
 * Changes nothing (copies of the envelopes). */
static uint32_t vtft_now(const voice_t *v)
{
    env_t ve = v->venv, me = v->menv;
    int32_t vol;
    uint32_t cut, pitch;
    if (!v->env_on)
        return ((uint32_t) v->vol_t << 16) | v->cut_t;
    mod_targets(v, venv_step(&ve, 0), menv_step(&me, 0), &vol, &cut, &pitch);
    return ((uint32_t) vol << 16) | cut;
}

static void lfo_reset(voice_t *v)
{
    v->lfo1_cnt   = 0;
    v->lfo2_cnt   = 0;
    v->lfo1_delay = (v->lfo1val & 0x8000) ? 0 : (int32_t) ((0x8000 - (v->lfo1val & 0x7FFF)) << 5);
    v->lfo2_delay = (v->lfo2val & 0x8000) ? 0 : (int32_t) ((0x8000 - (v->lfo2val & 0x7FFF)) << 5);
}

/* ---- reverb and chorus (structure and scaling as 86Box's snd_emu8k.c) ---- */
#define REV_STEP  242                       /* delay unit, samples */
#define CHO_SIZE  16384                     /* chorus delay line, samples (power of 2) */
#define FX_HOLD   (44100 * 4)               /* keep running this long after the last send (tails) */
typedef struct { int32_t *b; int size, pos; int32_t fs, gain, fb, d1, d2; } comb_t;   /* Q12 coefficients */
#define REV_SH 0   /* sends as they are, clamped to +-2^18 so the Q12 products stay in 32 bits (>> 3 cost 10 dB of tail) */
typedef struct fx_t {
    comb_t  refl[6], tail[2], diff[4], damper;  /* diff: tail L = 0,1  tail R = 2,3 */
    int32_t out_mix, link_amp, in_amp;           /* 0..255 */
    int     link_type;                           /* 1 = panned (0x8474 in INIT1 ch 7) */
    int32_t cfb, central, depth;                 /* chorus: feedback 0..255, delay, LFO depth (samples) */
    int32_t right16;                             /* right channel extra delay, 16.16 samples */
    uint32_t lfo, lfo_inc;                       /* phase, 2^32 = one period */
    int     w;
    int32_t cl[CHO_SIZE], cr[CHO_SIZE];
    int32_t mem[];                               /* comb/tail/diffuser lines */
} fx_t;

static inline int32_t mulq(int32_t a, int32_t b) { return (a * b) >> 12; }
static int32_t q15(double x) { return (int32_t) floor(x * 4096.0 + 0.5); }   /* Q12 */

/* The delay elements run over a whole block each, in at most two runs without a wrap check (every line is longer
 * than a block); same arithmetic as per sample. d1 + d2 = 1, so the damped sum stays in 32 bits. */
static void comb_blk(comb_t *c, const int32_t *in, int32_t *acc, int n)
{
    int32_t fs = c->fs;
    const int32_t g = c->gain, fb = c->fb, d1 = c->d1, d2 = c->d2;
    while (n > 0) {
        int k = c->size - c->pos, i;
        int32_t *p = c->b + c->pos;
        if (k > n) k = n;
        for (i = 0; i < k; i++) {
            int32_t out = p[i];
            fs = (out * d2 + fs * d1) >> 12;                 /* low-pass in the loop */
            p[i] = in[i] - mulq(fs, fb);
            acc[i] += mulq(out, g);
        }
        c->pos += k; if (c->pos >= c->size) c->pos = 0;
        in += k; acc += k; n -= k;
    }
    c->fs = fs;
}

static void diff_blk(comb_t *c, int32_t *x, int n)     /* in place */
{
    const int32_t fb = c->fb;
    while (n > 0) {
        int k = c->size - c->pos, i;
        int32_t *p = c->b + c->pos;
        if (k > n) k = n;
        for (i = 0; i < k; i++) {
            int32_t bo = p[i], bi = mulq(bo, fb) - x[i];
            p[i] = bi;
            x[i] = bo - mulq(bi, fb);
        }
        c->pos += k; if (c->pos >= c->size) c->pos = 0;
        x += k; n -= k;
    }
}

/* tail: plain delay of (in + early reflections), two diffusers, added at link_amp */
static void tail_blk(comb_t *t, comb_t *d, const int32_t *in, int32_t *acc, int32_t amp, int n)
{
    int32_t x[BLOCK], *b = t->b;
    int pos = t->pos, size = t->size, i;
    for (i = 0; i < n; i++) {
        x[i] = b[pos];
        b[pos] = in[i] + acc[i];
        if (++pos >= size) pos = 0;
    }
    t->pos = pos;
    diff_blk(&d[0], x, n);
    diff_blk(&d[1], x, n);
    for (i = 0; i < n; i++) acc[i] += (x[i] * amp) >> 8;
}

static void fx_reset(fx_t *f)
{
    static const int rs[6] = { 2, 4, 8, 13, 19, 26 };   /* reflection lengths x REV_STEP (6th: room size) */
    int32_t *m = f->mem;
    int i;
    for (i = 0; i < 6; i++) { f->refl[i].b = m; m += 34 * REV_STEP; f->refl[i].size = rs[i] * REV_STEP; }
    for (i = 0; i < 2; i++) { f->tail[i].b = m; m += 34 * REV_STEP; f->tail[i].size = 27 * REV_STEP; }
    for (i = 0; i < 4; i++) {
        f->diff[i].b = m; m += 8 * REV_STEP + 55;
        f->diff[i].size = (i & 1) ? 4 * REV_STEP + 55 : 8 * REV_STEP + 55;   /* 86Box allpass[1], [2] */
        f->diff[i].fb = 1 << 11;   /* 0.5 until INIT1 ch 5 */
    }
}
#define FX_MEM ((6 * 34 + 2 * 34) * REV_STEP + 4 * (8 * REV_STEP + 55))

static fx_t *fx_new(void)
{
    fx_t *f = (fx_t *) calloc(1, sizeof(fx_t) + FX_MEM * sizeof(int32_t));
    if (f) fx_reset(f);
    return f;
}

/* INIT1-4 (effect registers per "channel" index) and HWCF4/5 writes, as 86Box: ignored while INIT1[0]
 * is 03FFh (the first two passes of the init sequence) */
static void fx_reg(int reg, int ch, uint16_t w)
{
    fx_t *f = E.fx;
    int i, mult;
    if (!f || E.init1[0] == 0x03FF) return;
    switch (reg) {
    case EMU8K_INIT1:
        switch (ch) {
        case 0x03: f->out_mix = w & 0xFF; break;
        case 0x05: for (i = 0; i < 4; i++) f->diff[i].fb = q15((w & 0xFF) / 255.0); break;
        case 0x07: f->link_type = w == 0x8474; break;
        case 0x0F: f->refl[0].gain = q15(((w >> 4) & 15) / 15.0); break;
        case 0x17: f->refl[1].gain = q15(((w >> 4) & 15) / 15.0); break;
        case 0x1F: f->refl[2].gain = q15(((w >> 4) & 15) / 15.0); break;
        case 0x09: f->refl[0].fb = q15((w & 15) / 15.0); break;
        case 0x11: f->refl[1].fb = q15((w & 15) / 15.0); break;
        case 0x19: f->refl[2].fb = q15((w & 15) / 15.0); break;
        }
        return;
    case EMU8K_INIT2:
        switch (ch) {
        case 0x14:
            mult = ((w >> 8) & 15) + 18;
            f->refl[5].size = mult * REV_STEP; f->refl[5].pos = 0;
            f->tail[0].size = (mult + 1) * REV_STEP; f->tail[0].pos = 0;
            if (!f->link_type) { f->tail[1].size = (mult + 1) * REV_STEP; f->tail[1].pos = 0; }
            break;
        case 0x16:
            if (f->link_type) { f->tail[1].size = (((w >> 8) & 15) + 19) * REV_STEP; f->tail[1].pos = 0; }
            break;
        case 0x07: f->refl[3].gain = q15(((w >> 4) & 15) / 15.0); break;
        case 0x0F: f->refl[4].gain = q15(((w >> 4) & 15) / 15.0); break;
        case 0x17: f->refl[5].gain = q15(((w >> 4) & 15) / 15.0); break;
        case 0x1D:
            for (i = 0; i < 6; i++) { f->refl[i].d1 = q15((w & 0xFF) / 255.0); f->refl[i].d2 = q15((0xFF - (w & 0xFF)) / 255.0); f->refl[i].fs = 0; }
            f->damper.d1 = f->refl[0].d1; f->damper.d2 = f->refl[0].d2; f->damper.fs = 0;
            break;
        case 0x01: f->refl[3].fb = q15((w & 15) / 15.0); break;
        case 0x09: f->refl[4].fb = q15((w & 15) / 15.0); break;
        case 0x11: f->refl[5].fb = q15((w & 15) / 15.0); break;
        }
        return;
    case EMU8K_INIT3:
        if (ch == 0x09) f->cfb = w & 0xFF;
        else if (ch == 0x0C) f->central = w & 0x1FFF;
        else if (ch == 0x01) f->in_amp = w & 0xFF;
        return;
    case EMU8K_INIT4:
        if (ch == 0x03) f->depth = ((w & 0xFF) * f->central) >> 8;
        else if (ch == 0x1F) f->link_amp = w & 0xFF;
        return;
    }
}

static void fx_hwcf(void)
{
    fx_t *f = E.fx;
    if (!f || E.init1[0] == 0x03FF) return;
    f->right16 = (int32_t) ((E.hwcf4 & 0x1FFFFF) << 8);                      /* 1/256 sample units */
    f->lfo_inc = (uint32_t) (E.hwcf5 * (4294967296.0 / 1000.0 / 44100.0));  /* milli-Hz */
}

static inline int32_t cho_tap(const int32_t *b, int32_t p16)
{
    int i;
    i = (p16 >> 16) & (CHO_SIZE - 1);   /* two's complement: a negative position wraps by itself */
    return b[i] + (((b[(i + 1) & (CHO_SIZE - 1)] - b[i]) * ((p16 >> 8) & 0xFF)) >> 8);   /* 1/256 sample steps */
}

static int32_t lfo_off(fx_t *f, uint32_t ph)           /* chorus LFO offset, 16.16 samples */
{
    int32_t s0 = t_sin[ph >> 20], s1 = t_sin[((ph >> 20) + 1) & 4095];
    return (s0 + (int32_t) (((int64_t) (s1 - s0) * ((ph >> 4) & 0xFFFF)) >> 16)) * f->depth * 2;
}

/* rv / cv: mono reverb and chorus sends for n (<= BLOCK) samples; the wet signal is added to mix */
static void fx_run(fx_t *f, const int32_t *rv, const int32_t *cv, int32_t *mix, int n, int do_rev, int do_cho)
{
    int32_t in[BLOCK], in2[BLOCK], a[BLOCK], b[BLOCK], fs = f->damper.fs, o0, o1, step, base0;
    const int32_t d1 = f->damper.d1, d2 = f->damper.d2, amp = f->in_amp, om = f->out_mix;
    int i, w;
    if (!do_rev) goto chorus;
    /* reverb: input low-pass, 6 early reflections (combs), two tails (delay + 2 diffusers) */
    for (i = 0; i < n; i++) {
        int32_t x = rv[i] >> REV_SH;
        x = x > 262143 ? 262143 : x < -262144 ? -262144 : x;   /* 8x full scale: the Q12 products fit */
        fs = (x * d2 + fs * d1) >> 12;
        in[i] = fs;
        in2[i] = (fs * amp) >> 8;
    }
    f->damper.fs = fs;
    memset(a, 0, sizeof(int32_t) * n);
    if (f->link_type) {                                  /* panned: reflections 3 and 5 left, the rest right */
        memset(b, 0, sizeof(int32_t) * n);
        comb_blk(&f->refl[0], in2, b, n); comb_blk(&f->refl[1], in2, b, n); comb_blk(&f->refl[2], in2, a, n);
        comb_blk(&f->refl[3], in2, b, n); comb_blk(&f->refl[4], in2, a, n); comb_blk(&f->refl[5], in2, b, n);
    } else {
        for (i = 0; i < 6; i++) comb_blk(&f->refl[i], in2, a, n);
        memcpy(b, a, sizeof(int32_t) * n);
    }
    tail_blk(&f->tail[0], &f->diff[0], in, a, f->link_amp, n);
    tail_blk(&f->tail[1], &f->diff[2], in, b, f->link_amp, n);
    for (i = 0; i < n; i++) {
        mix[2 * i] += (a[i] * om) >> (8 - REV_SH);
        mix[2 * i + 1] += (b[i] * om) >> (8 - REV_SH);
    }
chorus:
    if (!do_cho) return;
    /* chorus: two taps around the central delay moved by a sine LFO (linear over the block), the right one
       later by HWCF4; feedback into both lines */
    o0 = lfo_off(f, f->lfo);
    f->lfo += f->lfo_inc * (uint32_t) n;
    o1 = lfo_off(f, f->lfo);
    step = (o1 - o0) / n;
    w = f->w;
    base0 = (f->central << 16) + o0;
    for (i = 0; i < n; i++) {
        int32_t p = (w << 16) - base0 - step * i, l = cho_tap(f->cl, p), r = f->right16 ? cho_tap(f->cr, p - f->right16) : cho_tap(f->cr, p);
        f->cl[w] = cv[i] + ((l * f->cfb) >> 8);
        f->cr[w] = cv[i] + ((r * f->cfb) >> 8);
        w = (w + 1) & (CHO_SIZE - 1);
        mix[2 * i] += l;
        mix[2 * i + 1] += r;
    }
    f->w = w;
}

/* ---- render ---- */
static inline int32_t mul14(int32_t a, int32_t b)
{
    return (int32_t) (((int64_t) a * b) >> 14);
}

#ifndef EMU8K_LINEAR
#define EMU8K_LINEAR 0 /* 1 = linear instead of 4-point interpolation (faster) */
#endif

/* Inner loop for m samples. pitch/cut/vol hold the "current" registers:
 * the first sample uses them, after it pitch/cut jump to their targets and
 * the volume slides 0x400 per sample (as 86Box). FILT is a constant 0/1.
 * mem = sample memory with mem[pos] valid for the whole run (checked by
 * the caller), or NULL for the slow per-word lookup. */
static inline __attribute__((always_inline)) void
voice_run(voice_t *v, int32_t *mix, int32_t *rv, int32_t *cv, const uint16_t *mem, int m, const int FILT, const int FX)
{
    uint32_t pos = v->pos, frac = v->frac, pitch = v->cur_pitch, pt = v->pitch_t;
    uint32_t le = v->loop_end, span = v->loop_end - v->loop_start;
    int32_t vol = v->cur_vol, vt = v->vol_t, gl, gr;
    int32_t x1 = v->x1, x2 = v->x2, y1 = v->y1, y2 = v->y2;
    const int32_t *k = t_filt[v->q][v->cur_cut >> 8], *kt = t_filt[v->q][v->cut_t >> 8];
    /* effect sends (FX): mono, after volume, before pan - PTRX bits 15-8 reverb, CSL bits 31-24 chorus */
    const int32_t rs = (int32_t) ((v->ptrx >> 8) & 0xFF), cs = (int32_t) (v->csl >> 24);
    int32_t grv = 0, gcv = 0;
    gl = (vol * v->pan_l) >> 9;
    gr = (vol * v->pan_r) >> 9;
    if (FX) { grv = (vol * rs) >> 8; gcv = (vol * cs) >> 8; }
    while (m-- > 0) {
        if (vol) {
            int32_t d;
#if EMU8K_LINEAR
            int32_t s1, s2;
            if (mem) { s1 = (int16_t) mem[pos + 1]; s2 = (int16_t) mem[pos + 2]; }
            else { s1 = (int16_t) rd(pos + 1); s2 = (int16_t) rd(pos + 2); }
            d = s1 + (((s2 - s1) * (int32_t) (frac >> 1)) >> 15);
#else
            const int16_t *c = t_cubic[frac >> 6];
            if (mem) {
                const uint16_t *p = mem + pos;
                d = ((int16_t) p[0] * c[0] + (int16_t) p[1] * c[1] + (int16_t) p[2] * c[2] +
                     (int16_t) p[3] * c[3]) >> 14;
            } else
                d = ((int16_t) rd(pos) * c[0] + (int16_t) rd(pos + 1) * c[1] +
                     (int16_t) rd(pos + 2) * c[2] + (int16_t) rd(pos + 3) * c[3]) >> 14;
#endif
            if (FILT) {
                int32_t y = (int32_t) (((int64_t) k[0] * ((d + 2 * x1 + x2) * 16) + (int64_t) k[1] * y1 +
                                        (int64_t) k[2] * y2) >> 28);
                x2 = x1; x1 = d; y2 = y1; y1 = y;
                d = y >> 4;
                if (d > 32767) d = 32767; else if (d < -32768) d = -32768;
            }
            mix[0] += (d * gl) >> 15;
            mix[1] += (d * gr) >> 15;
            if (FX) {
                rv[0] += (d * grv) >> 16;
                cv[0] += (d * gcv) >> 16;
            }
        }
        mix += 2;
        if (FX) { rv++; cv++; }
        frac += pitch << 2;
        pos = (pos + (frac >> 16)) & ADDR_MASK;
        frac &= 0xFFFF;
        if (pos >= le)
            pos = (pos - span) & ADDR_MASK;
        pitch = pt;
        k = kt;
        if (vol != vt) {
            if (vol < vt) { vol += 0x400; if (vol > vt) vol = vt; }
            else { vol -= 0x400; if (vol < vt) vol = vt; }
            gl = (vol * v->pan_l) >> 9;
            gr = (vol * v->pan_r) >> 9;
            if (FX) { grv = (vol * rs) >> 8; gcv = (vol * cs) >> 8; }
        }
    }
    v->pos = pos; v->frac = frac; v->cur_pitch = pitch; v->cur_vol = vol;
    v->cur_cut = v->cut_t; v->x1 = x1; v->x2 = x2; v->y1 = y1; v->y2 = y2;
}

/* Sample memory such that mem[a] is valid for every word this run can read
 * (positions grow by < 4 words per sample, loops wrap back to loop_start;
 * PAD zero words follow each region), or NULL for the per-word lookup. */
static const uint16_t *mem_for(const voice_t *v, int m)
{
    uint32_t pos = v->pos, lo, hi;
    const uint16_t *base;
    if (pos < 0x80000) {
        lo = 0; hi = 0x80000; base = E.rom;
    } else if (E.ram && pos >= EMU8K_RAM_START && pos < E.ram_alloc_end) {
        lo = EMU8K_RAM_START; hi = E.ram_alloc_end; base = E.ram;
    } else
        return NULL;
    if (v->loop_end < v->loop_start)
        return NULL;
    if (v->loop_start < lo && v->loop_end <= pos + (uint32_t) m * 4 + 4)
        return NULL; /* a wrap could leave the region */
    (void) hi;
    return base - lo;
}

static void advance_only(voice_t *v, int n)
{
    uint32_t f = v->frac + (v->cur_pitch << 2) + (v->pitch_t << 2) * (uint32_t) (n - 1);
    v->pos = (v->pos + (f >> 16)) & ADDR_MASK;
    v->frac = f & 0xFFFF;
    if (v->loop_end > v->loop_start && v->pos >= v->loop_end)
        v->pos = v->loop_start + (v->pos - v->loop_end) % (v->loop_end - v->loop_start);
    v->cur_pitch = v->pitch_t;
    v->cur_cut = v->cut_t;
    if (v->cur_vol != v->vol_t) {
        int32_t d = v->vol_t - v->cur_vol, s = 0x400 * n;
        v->cur_vol = (d > s) ? v->cur_vol + s : (d < -s) ? v->cur_vol - s : v->vol_t;
    }
}

/* fx = 1: this voice adds its own effect sends to rv / cv (more voices with different sends than send groups) */
static void render_voice(voice_t *v, int32_t *mix, int32_t *rv, int32_t *cv, int n, int fx)
{
    int out = (E.hwcf3 & 4) && !(v->ccca & 0x04000000);
    while (n > 0) {
        int m = n;
        if (v->env_on) {
            if (v->ctrl_left <= 0) {
                v->ctrl_left = EMU8K_CTRL;
                mod_update(v, EMU8K_CTRL);
            }
            if (m > v->ctrl_left)
                m = v->ctrl_left;
            v->ctrl_left -= m;
        }
        if (!out || (!v->cur_vol && !v->vol_t))
            advance_only(v, m);
        else {
            const uint16_t *mem = mem_for(v, m);
            int filt = v->q || v->cut_t != 0xFFFF || v->cur_cut != 0xFFFF;
            if (fx) {
                if (filt) voice_run(v, mix, rv, cv, mem, m, 1, 1);
                else voice_run(v, mix, rv, cv, mem, m, 0, 1);
            } else if (filt)
                voice_run(v, mix, NULL, NULL, mem, m, 1, 0);
            else
                voice_run(v, mix, NULL, NULL, mem, m, 0, 0);
        }
        mix += 2 * m;
        if (fx) { rv += m; cv += m; }
        n -= m;
    }
}

/* Reverb + chorus over the block's sends; runs while any voice sends and FX_HOLD samples after (the tails) */
static void fx_process(int32_t *mix, const int32_t *rv, const int32_t *cv, int n)
{
    if (!E.fx || E.fx_off || (E.rev_hold <= 0 && E.cho_hold <= 0))
        return;
    fx_run(E.fx, rv, cv, mix, n, E.rev_hold > 0, E.cho_hold > 0);
    E.rev_hold -= n;
    E.cho_hold -= n;
}

/* Voices with the same (reverb, chorus) send levels share a stereo sub-mix; the mono send is taken from it once per
 * sample (L + R = the voice before pan), not per voice: MIDI drivers set the sends per channel, so a song has a few. */
#define FX_GROUPS 8
static int32_t g_mix[BLOCK * 2], g_rv[BLOCK], g_cv[BLOCK], g_sub[FX_GROUPS][BLOCK * 2];

static void render_block(int16_t *dst, int n)
{
    int32_t *mix = g_mix, *rv = g_rv, *cv = g_cv;
    uint16_t gkey[FX_GROUPS];
    int i, c, g, ng = 0, fx = E.fx && !E.fx_off, solo = 0;
    memset(mix, 0, sizeof(int32_t) * 2 * n);
    for (c = 0; c < 32; c++) {
        voice_t *v = &E.v[c];
        uint16_t key;
        if (!v->cur_vol && !v->vol_t &&
            (!v->env_on || v->venv.state == ENV_STOPPED ||
             (v->venv.state == ENV_SUSTAIN && v->venv.db >= ENV_MAX))) {
            advance_only(v, n); /* silent or fully released: oscillator only */
            continue;
        }
        key = (uint16_t) ((v->ptrx & 0xFF00) | (v->csl >> 24));
        if (!fx || !key) { render_voice(v, mix, NULL, NULL, n, 0); continue; }
        for (g = 0; g < ng && gkey[g] != key; g++) ;
        if (g == ng && ng < FX_GROUPS) { gkey[ng++] = key; memset(g_sub[g], 0, sizeof(int32_t) * 2 * n); }
        if (g < ng) render_voice(v, g_sub[g], NULL, NULL, n, 0);
        else {      /* more distinct sends than groups: this voice sends by itself */
            if (!solo) { memset(rv, 0, sizeof(int32_t) * n); memset(cv, 0, sizeof(int32_t) * n); solo = 1; }
            render_voice(v, mix, rv, cv, n, 1);
        }
    }
    if (ng || solo) {
        if (!solo) { memset(rv, 0, sizeof(int32_t) * n); memset(cv, 0, sizeof(int32_t) * n); }
        for (g = 0; g < ng; g++) {
            const int32_t *b = g_sub[g];
            const int32_t rs = (gkey[g] >> 8) * 257, cs = (gkey[g] & 0xFF) * 257;   /* /255 of (L + R), Q16 */
            for (i = 0; i < n; i++) {
                int32_t l = b[2 * i], r = b[2 * i + 1], m = l + r;
                mix[2 * i] += l;
                mix[2 * i + 1] += r;
                rv[i] += (int32_t) (((int64_t) m * rs) >> 16);
                cv[i] += (int32_t) (((int64_t) m * cs) >> 16);
            }
            if (rs) E.rev_hold = FX_HOLD;
            if (cs) E.cho_hold = FX_HOLD;
        }
        if (solo) E.rev_hold = E.cho_hold = FX_HOLD;
    }
    if ((E.rev_hold > 0 || E.cho_hold > 0) && !ng && !solo) { memset(rv, 0, sizeof(int32_t) * n); memset(cv, 0, sizeof(int32_t) * n); }
    fx_process(mix, rv, cv, n);
    for (i = 0; i < 2 * n; i++)
        dst[i] = (int16_t) (mix[i] > 32767 ? 32767 : mix[i] < -32768 ? -32768 : mix[i]);
    E.wc = (uint16_t) (E.wc + n);
}

void emu8k_render(int16_t *out, int frames, int rate)
{
    if (rate <= 0 || rate == 44100) {
        while (frames > 0) {
            int n = frames > BLOCK ? BLOCK : frames;
            render_block(out, n);
            out += 2 * n;
            frames -= n;
        }
        return;
    }
    {
        /* linear resampler, 16.16 phase */
        uint32_t step = (uint32_t) (((uint64_t) 44100 << 16) / (uint32_t) rate);
        while (frames-- > 0) {
            int ch;
            uint32_t f = E.rs_phase & 0xFFFF;
            if (E.ob_pos >= E.ob_len) {
                render_block(E.ob, BLOCK);
                E.ob_len = BLOCK;
                E.ob_pos = 0;
            }
            for (ch = 0; ch < 2; ch++) {
                int32_t a = E.rs_prev[ch], b = E.ob[2 * E.ob_pos + ch];
                *out++ = (int16_t) (a + (((b - a) * (int32_t) f) >> 16));
            }
            E.rs_phase += step;
            while (E.rs_phase >= 0x10000) {
                E.rs_phase -= 0x10000;
                E.rs_prev[0] = E.ob[2 * E.ob_pos];
                E.rs_prev[1] = E.ob[2 * E.ob_pos + 1];
                if (++E.ob_pos >= E.ob_len) {
                    render_block(E.ob, BLOCK);
                    E.ob_len = BLOCK;
                    E.ob_pos = 0;
                }
            }
        }
    }
}

void emu8k_clock(int samples)
{
    E.wc = (uint16_t) (E.wc + samples);
}

/* ---- registers ---- */
static void set_pan(voice_t *v)
{
    v->pan_l = (int32_t) (v->psst >> 24);
    v->pan_r = 255 - v->pan_l;
}

static void trigger_all(voice_t *v)
{
    lfo_reset(v);
    if (!(v->atkhldv & 0x8000))
        env_trigger(&v->venv, v->envvol, v->atkhldv);
    if (!(v->atkhld & 0x8000)) {
        env_trigger(&v->menv, v->envval, v->atkhld);
        v->menv.db = 0;
    }
    v->ctrl_left = 0;
}

void emu8k_write(int reg, int ch, uint32_t val)
{
    voice_t *v;
    uint16_t w = (uint16_t) val;
    if (!E.ok)
        return;
    ch &= 31;
    v = &E.v[ch];
    switch (reg) {
    case EMU8K_CPF: v->cur_pitch = val >> 16; return; /* low word (fraction) is read-only */
    case EMU8K_PTRX: v->ptrx = val; v->pitch_t = val >> 16; return;
    case EMU8K_CVCF: v->cur_vol = (int32_t) (val >> 16); v->cur_cut = val & 0xFFFF; return;
    case EMU8K_VTFT: v->vol_t = (int32_t) (val >> 16); v->cut_t = val & 0xFFFF; return;
    case EMU8K_Z2: v->z2 = val; return;
    case EMU8K_Z1: v->z1 = val; return;
    case EMU8K_PSST: v->psst = val; v->loop_start = val & ADDR_MASK; set_pan(v); return;
    case EMU8K_CSL: v->csl = val; v->loop_end = val & ADDR_MASK; return;
    case EMU8K_CCCA:
        v->ccca = val;
        v->pos  = val & ADDR_MASK;
        v->q    = (int) (val >> 28);
        return;
    case EMU8K_CFG1:
        switch (ch) {
        case 9: E.hwcf4 = val; fx_hwcf(); return;
        case 10: E.hwcf5 = val; fx_hwcf(); return;
        case 13: E.hwcf6 = val; return;
        case 14: E.hwcf7 = val; return;
        case 20: E.smalr = val & ADDR_MASK; return;
        case 21: E.smarr = val & ADDR_MASK; return;
        case 22: E.smalw = val & ADDR_MASK; return;
        case 23: E.smarw = val & ADDR_MASK; return;
        case 26: emu8k_ram_write(E.smalw, w); E.smalw = (E.smalw + 1) & ADDR_MASK; return;
        case 29: E.hwcf1 = w; return;
        case 30: E.hwcf2 = w; return;
        case 31: E.hwcf3 = w; return;
        }
        return;
    case EMU8K_CFG2:
        if (ch == 26) {
            emu8k_ram_write(E.smarw, w);
            E.smarw = (E.smarw + 1) & ADDR_MASK;
        }
        return;
    case EMU8K_INIT1: E.init1[ch] = w; fx_reg(reg, ch, w); return;
    case EMU8K_INIT2: E.init2[ch] = w; fx_reg(reg, ch, w); return;
    case EMU8K_INIT3: E.init3[ch] = w; fx_reg(reg, ch, w); return;
    case EMU8K_INIT4: E.init4[ch] = w; fx_reg(reg, ch, w); return;
    case EMU8K_ENVVOL: v->envvol = w; return;
    case EMU8K_DCYSUSV: {
        int was = v->env_on;
        v->dcysusv = w;
        v->env_on  = !(w & 0x80);
        if (v->env_on && !was) {
            if (E.hwcf3 != 4)
                E.hwcf3 = 4; /* Doom / OpenCP never init HWCF3 (86Box does the same) */
            trigger_all(v);
        }
        v->venv.sustain = ((0x7F - ((w >> 8) & 0x7F)) << 21) / 0x7F;
        v->venv.ramp    = t_decay[w & 0x7F];
        if (w & 0x8000) {
            env_t *e = &v->venv;
            if (e->state == ENV_DELAY || e->state == ENV_ATTACK || e->state == ENV_HOLD) {
                if (e->state != ENV_HOLD)
                    e->db = amp_to_att((uint32_t) e->amp >> 5) << 5;
                if (e->db > ENV_MAX)
                    e->db = ENV_MAX;
            }
            e->state = e->db >= e->sustain ? ENV_RAMP_DOWN : ENV_RAMP_UP;
        }
        return;
    }
    case EMU8K_ENVVAL: v->envval = w; return;
    case EMU8K_DCYSUS:
        v->dcysus       = w;
        v->menv.sustain = (((w >> 8) & 0x7F) << 21) / 0x7F;
        v->menv.ramp    = t_decay[w & 0x7F];
        if (w & 0x8000) {
            env_t *e = &v->menv;
            if (e->state == ENV_DELAY || e->state == ENV_ATTACK || e->state == ENV_HOLD) {
                if (e->state != ENV_HOLD)
                    e->db = hz_to_oct((uint32_t) e->amp >> 5) << 5;
                if (e->db >= ENV_MAX)
                    e->db = ENV_MAX - 1;
            }
            e->state = e->db >= e->sustain ? ENV_RAMP_DOWN : ENV_RAMP_UP;
        }
        return;
    case EMU8K_ATKHLDV:
        v->atkhldv = w;
        set_attack(&v->venv, w);
        if (!(w & 0x8000) && v->env_on) {
            lfo_reset(v);
            env_trigger(&v->venv, v->envvol, w);
            v->ctrl_left = 0;
        }
        return;
    case EMU8K_LFO1VAL: v->lfo1val = w; v->lfo1_delay = (w & 0x8000) ? 0 : (int32_t) ((0x8000 - (w & 0x7FFF)) << 5); return;
    case EMU8K_ATKHLD:
        v->atkhld = w;
        set_attack(&v->menv, w);
        if (!(w & 0x8000) && v->env_on) {
            env_trigger(&v->menv, v->envval, w);
            v->menv.db = 0;
            v->ctrl_left = 0;
        }
        return;
    case EMU8K_LFO2VAL: v->lfo2val = w; v->lfo2_delay = (w & 0x8000) ? 0 : (int32_t) ((0x8000 - (w & 0x7FFF)) << 5); return;
    case EMU8K_IP:
        v->ip = w;
        v->pitch_t = pitch_of(w);
        return;
    case EMU8K_IFATN:
        /* 86Box: ignore the all-zero "note off" some trackers write */
        if ((w & 0xFF) == 0 && v->cur_vol == 0 && v->vol_t == 0 && (v->dcysusv == 0x80 || v->dcysusv == 0x807F) && v->ip == 0)
            return;
        v->ifatn     = w;
        v->init_att  = (int32_t) (((w & 0xFF) << 21) / 0xFF);
        v->vol_t     = t_att[w & 0xFF];
        v->init_filt = (int32_t) (((w >> 8) << 21) / 0xFF);
        v->cut_t     = (w >> 8) == 0xFF ? 0xFFFF : (uint32_t) v->init_filt >> 5;
        return;
    case EMU8K_PEFE:
        v->pefe        = w;
        v->m_env_filt  = s8scale((int8_t) (w & 0xFF));
        v->m_env_pitch = s8scale((int8_t) (w >> 8));
        return;
    case EMU8K_FMMOD:
        v->fmmod        = w;
        v->m_lfo1_filt  = s8scale((int8_t) (w & 0xFF));
        v->m_lfo1_pitch = s8scale((int8_t) (w >> 8));
        return;
    case EMU8K_TREMFRQ:
        v->tremfrq     = w;
        v->lfo1_speed  = t_lfo[w & 0xFF];
        v->m_lfo1_trem = s8scale((int8_t) (w >> 8));
        return;
    case EMU8K_FM2FRQ2:
        v->fm2frq2      = w;
        v->lfo2_speed   = t_lfo[w & 0xFF];
        v->m_lfo2_pitch = s8scale((int8_t) (w >> 8));
        return;
    case EMU8K_ID: E.id = w; return;
    default: return;
    }
}

uint32_t emu8k_read(int reg, int ch)
{
    voice_t *v;
    uint16_t r;
    if (!E.ok)
        return 0xFFFF;
    ch &= 31;
    v = &E.v[ch];
    switch (reg) {
    case EMU8K_CPF: return (v->cur_pitch << 16) | (v->frac & 0xFFFF);
    case EMU8K_PTRX: return (v->ptrx & 0xFFFF) | (v->pitch_t << 16);
    case EMU8K_CVCF: return ((uint32_t) v->cur_vol << 16) | v->cur_cut;
    case EMU8K_VTFT: return vtft_now(v);
    case EMU8K_Z2: return v->z2;
    case EMU8K_Z1: return v->z1;
    case EMU8K_PSST: return v->psst;
    case EMU8K_CSL: return v->csl;
    case EMU8K_CCCA: return (v->ccca & 0xFF000000u) | v->pos;
    case EMU8K_CFG1:
        switch (ch) {
        case 9: return E.hwcf4;
        case 10: return E.hwcf5;
        case 13: return E.hwcf6;
        case 14: return E.hwcf7;
        case 20: return E.smalr;
        case 21: return E.smarr;
        case 22: return E.smalw;
        case 23: return E.smarw;
        case 26: r = E.smld_buf; E.smld_buf = rd(E.smalr); E.smalr = (E.smalr + 1) & ADDR_MASK; return r;
        case 29: return (E.hwcf1 & 0xFE) | (E.hwcf3 & 1);
        case 30:
            return ((E.hwcf2 >> 4) & 0x0E) | (E.hwcf1 & 1) | ((E.hwcf3 & 2) ? 0x10 : 0) |
                   ((E.hwcf3 & 4) ? 0x40 : 0) | ((E.hwcf3 & 8) ? 0x20 : 0) | ((E.hwcf3 & 0x10) ? 0x80 : 0);
        case 31: return E.hwcf2 & 0x1F;
        }
        return 0xFFFF;
    case EMU8K_CFG2:
        if (ch == 26) {
            r = E.smrd_buf;
            E.smrd_buf = rd(E.smarr);
            E.smarr = (E.smarr + 1) & ADDR_MASK;
            return r;
        }
        if (ch == 27)
            return E.wc;
        return 0xFFFF;
    case EMU8K_INIT1: return E.init1[ch];
    case EMU8K_INIT2: return E.init2[ch];
    case EMU8K_INIT3: return E.init3[ch];
    case EMU8K_INIT4: return E.init4[ch];
    case EMU8K_ENVVOL: return v->envvol;
    case EMU8K_DCYSUSV: return v->dcysusv;
    case EMU8K_ENVVAL: return v->envval;
    case EMU8K_DCYSUS: return v->dcysus;
    case EMU8K_ATKHLDV: return v->atkhldv;
    case EMU8K_LFO1VAL: return v->lfo1val;
    case EMU8K_ATKHLD: return v->atkhld;
    case EMU8K_LFO2VAL: return v->lfo2val;
    case EMU8K_IP: return v->ip;
    case EMU8K_IFATN: return v->ifatn;
    case EMU8K_PEFE: return v->pefe;
    case EMU8K_FMMOD: return v->fmmod;
    case EMU8K_TREMFRQ: return v->tremfrq;
    case EMU8K_FM2FRQ2: return v->fm2frq2;
    case EMU8K_REG(3, 6): return 0xFFFF;
    case EMU8K_ID: return 0x1C | ((E.id & 2) ? 0xFF02 : 0);
    }
    return 0xFFFF;
}

/* ---- port-level glue ---- */
static int is_dword1(int r, int ch) /* Data1/Data2 pair forms one dword */
{
    return r == 0 || (r == 1 && (ch == 9 || ch == 10 || ch == 13 || ch == 14 || (ch >= 20 && ch <= 23)));
}

static uint32_t raw1(int r, int ch) /* side-effect-free dword value */
{
    if (r == 0)
        return (E.v[ch].ccca & 0xFF000000u) | E.v[ch].pos;
    switch (ch) {
    case 9: return E.hwcf4;
    case 10: return E.hwcf5;
    case 13: return E.hwcf6;
    case 14: return E.hwcf7;
    case 20: return E.smalr;
    case 21: return E.smarr;
    case 22: return E.smalw;
    default: return E.smarw;
    }
}

void emu8k_io_write(int off, uint16_t val)
{
    int r = E.cur_reg, ch = E.cur_ch;
    uint32_t d;
    switch (off & 0xC02) {
    case 0x802: /* pointer: bits 0-4 channel, 5-7 register */
        E.cur_ch  = val & 31;
        E.cur_reg = (val >> 5) & 7;
        return;
    case 0x000:
    case 0x002:
        d = r == 3 ? ((uint32_t) E.v[ch].vol_t << 16) | E.v[ch].cut_t   /* VTFT halves: the stored targets */
                   : emu8k_read(EMU8K_REG(0, r), ch);
        d = (off & 2) ? (d & 0xFFFF) | ((uint32_t) val << 16) : (d & 0xFFFF0000u) | val;
        emu8k_write(EMU8K_REG(0, r), ch, d);
        return;
    case 0x400:
    case 0x402:
        if (is_dword1(r, ch)) {
            d = raw1(r, ch);
            if (off & 2) {
                if (r == 1)
                    val &= 0xFF; /* SMAxx / HWCF: top byte not addressable */
                d = (d & 0xFFFF) | ((uint32_t) val << 16);
            } else
                d = (d & 0xFFFF0000u) | val;
            emu8k_write(EMU8K_REG(1, r), ch, d);
            return;
        }
        emu8k_write(EMU8K_REG((off & 2) ? 2 : 1, r), ch, val);
        return;
    case 0x800:
        emu8k_write(EMU8K_REG(3, r), ch, val);
        return;
    }
}

uint16_t emu8k_io_read(int off)
{
    int r = E.cur_reg, ch = E.cur_ch;
    uint32_t d;
    switch (off & 0xC02) {
    case 0x802: /* high byte varies: Impulse Tracker / OpenCP detection */
        E.rnd = (E.rnd + 1) & 0x1F;
        return (uint16_t) (((0x80 | E.rnd) << 8) | (r << 5) | ch);
    case 0x000:
    case 0x002:
        d = emu8k_read(EMU8K_REG(0, r), ch);
        return (uint16_t) ((off & 2) ? d >> 16 : d);
    case 0x400:
    case 0x402:
        if (is_dword1(r, ch)) {
            d = (r == 0) ? emu8k_read(EMU8K_CCCA, ch) : raw1(r, ch);
            return (uint16_t) ((off & 2) ? d >> 16 : d);
        }
        return (uint16_t) emu8k_read(EMU8K_REG((off & 2) ? 2 : 1, r), ch);
    case 0x800:
        return (uint16_t) emu8k_read(EMU8K_REG(3, r), ch);
    }
    return 0xFFFF;
}

/* ---- init ---- */
static int fx_off_saved;   /* emu8k_set_fx() before/after emu8k_init */

void emu8k_set_fx(int on)
{
    fx_off_saved = !on;
    E.fx_off = !on;
}

/* Power-on state = a card after Creative's AWEUTIL /S (every AWE32/AWE64 install runs it from AUTOEXEC.BAT; the DOS
 * games' own drivers - Apogee Sound System in Duke 3D, HMI SOS in Descent, both from Creative's AWE32 SDK - never set
 * the effects up themselves and only send reverb/chorus amounts per voice). Without it the effects engine has no
 * program: no reverb at all, the chorus a plain copy - dry and thin next to a real card. AWEUTIL's defaults = Creative's
 * AWE Control: reverb Hall 2, chorus Chorus 3 (parameters from Linux emu8000.c, as awemidi.c's apply_fx). */
static void aweutil_fx(void)
{
    static const uint16_t hall2[28] = {
        0xB488, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7254, 0x7234, 0x7224, 0x7254, 0x7264, 0x7294, 0x44C3, 0x45C3,
        0xA404, 0xA504, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528};
    /* INIT1 = Data1 r2, INIT2 = Data2 r2, INIT3 = Data1 r3, INIT4 = Data2 r3 */
    static const uint8_t cmd[28][2] = {
        {1, 0x03}, {1, 0x05}, {4, 0x1F}, {1, 0x07}, {2, 0x14}, {2, 0x16}, {1, 0x0F}, {1, 0x17}, {1, 0x1F}, {2, 0x07},
        {2, 0x0F}, {2, 0x17}, {2, 0x1D}, {2, 0x1F}, {3, 0x01}, {3, 0x03}, {1, 0x09}, {1, 0x0B}, {1, 0x11}, {1, 0x13},
        {1, 0x19}, {1, 0x1B}, {2, 0x01}, {2, 0x03}, {2, 0x09}, {2, 0x0B}, {2, 0x11}, {2, 0x13}};
    static const int reg[5] = {0, EMU8K_INIT1, EMU8K_INIT2, EMU8K_INIT3, EMU8K_INIT4};
    int i;
    for (i = 0; i < 28; i++) emu8k_write(reg[cmd[i][0]], cmd[i][1], hall2[i]);
    emu8k_write(EMU8K_INIT3, 0x09, 0xE610);     /* Chorus 3: feedback, delay offset, LFO depth */
    emu8k_write(EMU8K_INIT3, 0x0C, 0x031A);
    emu8k_write(EMU8K_INIT4, 0x03, 0xBC84);
    emu8k_write(EMU8K_CFG1, 9, 0);              /* HWCF4: right delay, HWCF5: LFO frequency, HWCF6/7 */
    emu8k_write(EMU8K_CFG1, 10, 0x83);
    emu8k_write(EMU8K_CFG1, 13, 0x8000);
    emu8k_write(EMU8K_CFG1, 14, 0);
}

static int init_with(int ram_kb, uint16_t *rom, uint16_t *ram)
{
    int j, i;
    emu8k_close();
    build_tables();
    memset(&E, 0, sizeof(E));
    E.fx_off = fx_off_saved;
    E.fx = fx_new();           /* no memory: plays dry */
    E.ext   = rom != NULL;
    E.rom   = rom ? rom : (uint16_t *) calloc(0x80000 + PAD, 2);
    E.empty = (uint16_t *) calloc(0x10000, 2);
    if (!E.rom || !E.empty)
        return -1;
    if (ram_kb > 28672)
        ram_kb = 28672;
    if (ram_kb < 0)
        ram_kb = 0;
    for (j = 0; j < 256; j++)
        E.blk[j] = E.empty;
    for (j = 0; j < 8; j++)
        E.blk[j] = E.rom + j * 0x10000;
    E.ram_end = EMU8K_RAM_START;
    if (ram_kb) {
        uint32_t words = (uint32_t) ram_kb * 512u;
        uint32_t nblk  = (words + 0xFFFF) >> 16;
        E.ram = E.ext ? ram : (uint16_t *) calloc(nblk * 0x10000u + PAD, 2);
        if (!E.ram)
            return -1;
        for (i = 0; i < (int) nblk; i++)
            E.blk[32 + i] = E.ram + i * 0x10000;
        E.ram_end = EMU8K_RAM_START + words;
        E.ram_alloc_end = EMU8K_RAM_START + nblk * 0x10000u;
    }
    for (i = 0; i < 32; i++) {
        /* AWEUTIL /S leaves 0-29 released (807Fh, as the end of Creative's init sequence) and 30/31 envelope-off
           (0080h, its DRAM refresh voices). Drivers from Creative's SDK read DCYSUSV to find free voices and skip
           the ones with bit 7 set: with 0080h everywhere Descent's HMI driver found none and took sounding voices
           (notes cut off). Only the read-back value: the envelope stays off until a note starts. */
        E.v[i].dcysusv = i < 30 ? 0x807F : 0x80;
        E.v[i].cur_cut = E.v[i].cut_t = 0xFFFF;
        E.v[i].atkhldv = E.v[i].atkhld = 0x8000;
    }
    E.hwcf1 = 0x59;
    E.hwcf2 = 0x20;
    E.hwcf3 = 0;
    E.ok    = 1;
    aweutil_fx();
    return 0;
}

int emu8k_init(int ram_kb)
{
    return init_with(ram_kb, NULL, NULL);
}

/* rom: emu8k_mem_words(0) zeroed words, ram: emu8k_mem_words(ram_kb) - that zeroed words; both stay the
   caller's (Windows 98: a block shared with SC55W.VXD, which reads and writes the RAM itself) */
int emu8k_init_mem(int ram_kb, uint16_t *rom, uint16_t *ram)
{
    if (!rom || (ram_kb > 0 && !ram))
        return -1;
    return init_with(ram_kb, rom, ram);
}

uint32_t emu8k_rom_words(void) { return 0x80000u + PAD; }
uint32_t emu8k_ram_words(int ram_kb)
{
    uint32_t words;
    if (ram_kb <= 0)
        return 0;
    if (ram_kb > 28672)
        ram_kb = 28672;
    words = (uint32_t) ram_kb * 512u;
    return ((words + 0xFFFF) >> 16) * 0x10000u + PAD;
}

void emu8k_close(void)
{
    if (!E.ext) {
        free(E.rom);
        free(E.ram);
    }
    free(E.fx);
    free(E.empty);
    memset(&E, 0, sizeof(E));
}

/* exposed to emu8k_rom.c */
uint16_t *emu8k__rom(void)
{
    return E.rom;
}
