/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
/* awemidi.c - General MIDI on the EMU8000 engine, see awemidi.h.
 *
 * Adapted (GPL-2.0-or-later, as this file) from:
 *  - awesfx 0.5 by Takashi Iwai (awelib/awe_parm.c, sbkconv.c, sffile.c, parsesf.c, sample.c, sfitem.c):
 *    SoundFont 1/2 parsing, SBK -> SF2 units, SF2 units -> EMU8000 register values ("ADIP" formulas);
 *  - Linux sound/synth/emux/emux_synth.c, sound/isa/sb/emu8000_callback.c and emu8000.c (Takashi Iwai,
 *    Steve Ratcliffe, Jaroslav Kysela): voice allocation, volume/pitch/pan, the register sequence of a note,
 *    the init arrays and Creative's 8 reverb / 8 chorus parameter sets.
 * Rewritten for one in-process EMU8000 (emu8k.c), no kernel interfaces. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emu8k.h"
#include "awemidi.h"
#include "awegm.h"

#define LIMIT(x, a, b) do { if ((x) < (a)) (x) = (a); else if ((x) > (b)) (x) = (b); } while (0)
#define LO_BYTE(v) ((v) & 0xff)
#define HI_BYTE(v) (((v) >> 8) & 0xff)
#define TO_WORD(hi, lo) ((uint16_t)(((unsigned)(hi) << 8) | (unsigned)(lo)))

/* ---- registers (Linux EMU8000_xxx_WRITE -> emu8k_write) ----------------------------------------------- */
#define W(reg, ch, v) emu8k_write((reg), (ch), (uint32_t)(v))
#define R(reg, ch)    emu8k_read((reg), (ch))
#define REG_Z2 EMU8K_REG(0, 4)   /* "0080" */
#define REG_Z1 EMU8K_REG(0, 5)   /* "00A0" */
static void hwcf(int sel, uint32_t v) { W(EMU8K_CFG1, sel, v); }   /* 29-31 HWCF1-3, 9/10/13/14 HWCF4-7, 20-23 SMA* */

/* ======================================================================================================== */
/* awe_parm.c (ADIP formulas, awesfx's default)                                                              */
/* ======================================================================================================== */
static int msec_to_timecent(int msec) { if (msec <= 0) msec = 1; return (int)(log((double)msec / 1000.0) / log(2.0) * 1200.0); }
static int16_t calc_rate_offset(int hz) { return hz == 44100 ? 0 : (int16_t)(log((double)hz / 44100) / log(2.0) * 4096.0); }

static uint16_t calc_delay(int amount)
{
    int delay, temp;
    if (amount <= -12000) return 0x8000;
    delay = (amount + 0x30E4) * 65536;
    delay /= 1200;
    temp = 0x10000 | (delay & 0xffff);
    delay >>= 16;
    temp >>= (16 - (delay & 0xff)) & 31;   /* x86 masks the count, as awesfx ran */
    delay = 0x8000 - temp;
    if (delay < -32768) delay = -32768;
    return (uint16_t)delay;
}
static int calc_attack(int amount)
{
    int attack, temp1, temp2;
    if (amount >= 4300) return 1;
    if (amount > -600) {
        attack = (amount + 500) / 150;
        temp1 = ((~attack) & 7) | 8;
        temp2 = attack >> 16;
        attack += temp2 & 7;
        attack >>= 3;
        temp1 >>= attack;
        if (temp1 < 1) temp1 = 1;
        return temp1;
    }
    attack = (37 - amount) / 75 + 8;
    if (attack >= 128) attack = 127;
    return attack;
}
static int calc_hold(int amount)
{
    int hold, temp;
    if (amount < -5368) return 127;
    hold = (amount + 4130) * 65536;
    hold /= 1200;
    temp = 0x10000 | (hold & 0xffff);
    hold >>= 16;
    temp >>= (16 - (hold & 0xff)) & 31;
    hold = 127 - temp;
    if (hold < 0) hold = 0;
    return hold;
}
static uint16_t calc_atkhld(int atk, int hld) { return (uint16_t)(calc_attack(atk) | (calc_hold(hld) << 8)); }
static uint8_t calc_sustain(int amount)
{
    int v = 127 - (amount * 127) / 1000;
    LIMIT(v, 0, 127);
    return (uint8_t)v;
}
static uint8_t calc_decay(int amount)
{
    int decay, temp1, temp2;
    if (amount > 1800) {
        decay = (amount - 1800) / 150;
        temp1 = ((~decay) & 7) | 8;
        temp2 = decay >> 3;
        temp1 >>= temp2;
        decay = temp1;
        if (decay < 1) decay = 1;
        return (uint8_t)decay;
    }
    decay = (37 - amount) / 75 + 39;
    if (decay >= 128) decay = 127;
    return (uint8_t)decay;
}
static uint8_t calc_cutoff(int amount)
{
    int c = (amount + 0xf) / 0x1d - 0x99;
    LIMIT(c, 0, 255);
    return (uint8_t)c;
}
static uint8_t calc_filterQ(int amount)
{
    int q = amount / 12;
    LIMIT(q, 0, 15);
    return (uint8_t)q;
}
static uint8_t calc_pitch_shift(int amount)
{
    int v = (amount * 0x1b4f) >> 16;
    LIMIT(v, -128, 127);
    return (uint8_t)(v < 0 ? 0x100 + v : v);
}
static uint8_t calc_cutoff_shift(int amount, int octave_shift)
{
    int v = octave_shift == 3 ? (amount * 0x0919) >> 16 : (amount * 0x048D) >> 16;
    LIMIT(v, -128, 127);
    return (uint8_t)(v < 0 ? 0x100 + v : v);
}
static uint8_t calc_tremolo(int amount)
{
    int v = (amount * 128) / 0x78;
    LIMIT(v, -128, 127);
    return (uint8_t)(v < 0 ? 0x100 + v : v);
}
static uint8_t calc_freq(int amount)
{
    int freq, temp;
    if (amount <= -16000) return 0;
    freq = (amount + 0x23A6) * 65536;
    freq /= 1200;
    temp = 0x10000 | (freq & 0xffff);
    freq >>= 16;
    temp >>= (16 - (freq & 0xff)) & 31;
    if (temp > 255) temp = 255;
    return (uint8_t)temp;
}
static int calc_pan(int v) { if (v < -500) return 0; if (v > 500) return 127; return (v + 500) * 127 / 1000; }
static uint8_t calc_send(int v) { LIMIT(v, 0, 1000); return (uint8_t)(v * 255 / 1000); }
static uint8_t calc_attenuation(int amount)
{
    int a = (amount + 12) / 24;
    a = a * 8 / 3;
    LIMIT(a, 0, 255);
    return (uint8_t)a;
}

/* ======================================================================================================== */
/* generators (SF2 numbering; 55 = SBK sample pitch), awesfx sfitem.c                                       */
/* ======================================================================================================== */
enum {
    G_startAddrs = 0, G_endAddrs, G_startloopAddrs, G_endloopAddrs, G_startAddrsHi, G_lfo1ToPitch, G_lfo2ToPitch,
    G_env1ToPitch, G_initialFilterFc, G_initialFilterQ, G_lfo1ToFilterFc, G_env1ToFilterFc, G_endAddrsHi,
    G_lfo1ToVolume, G_unused1, G_chorus, G_reverb, G_pan, G_u2, G_u3, G_u4, G_delayLfo1, G_freqLfo1, G_delayLfo2,
    G_freqLfo2, G_delayEnv1, G_attackEnv1, G_holdEnv1, G_decayEnv1, G_sustainEnv1, G_releaseEnv1, G_autoHoldEnv1,
    G_autoDecayEnv1, G_delayEnv2, G_attackEnv2, G_holdEnv2, G_decayEnv2, G_sustainEnv2, G_releaseEnv2,
    G_autoHoldEnv2, G_autoDecayEnv2, G_instrument, G_nop, G_keyRange, G_velRange, G_startloopAddrsHi, G_keynum,
    G_velocity, G_initAtten, G_keyTuning, G_endloopAddrsHi, G_coarseTune, G_fineTune, G_sampleId, G_sampleFlags,
    G_samplePitch, G_scaleTuning, G_keyExclusiveClass, G_rootKey, G_EOF
};
enum { L_INHRT, L_OVWRT, L_RANGE, L_PRSET, L_INSTR };
enum { T_NOP, T_NOCONV, T_OFFSET, T_HI_OFF, T_RANGE, T_CUTOFF, T_FILTERQ, T_TENPCT, T_PANPOS, T_ATTEN, T_SCALE,
       T_TIME, T_TM_KEY, T_FREQ, T_PSHIFT, T_CSHIFT, T_TREMOLO, T_MODSUST, T_VOLSUST };
#define RANGE(lo, hi) ((((hi) & 0xff) << 8) | ((lo) & 0xff))
static const struct { uint8_t copy, type; int16_t defv; } gitem[G_EOF] = {
    {L_INHRT, T_OFFSET, 0}, {L_INHRT, T_OFFSET, 0}, {L_INHRT, T_OFFSET, 0}, {L_INHRT, T_OFFSET, 0},
    {L_INHRT, T_HI_OFF, 0}, {L_INHRT, T_PSHIFT, 0}, {L_INHRT, T_PSHIFT, 0}, {L_INHRT, T_PSHIFT, 0},
    {L_INHRT, T_CUTOFF, 13500}, {L_INHRT, T_FILTERQ, 0}, {L_INHRT, T_CSHIFT, 0}, {L_INHRT, T_CSHIFT, 0},
    {L_INHRT, T_HI_OFF, 0}, {L_INHRT, T_TREMOLO, 0}, {L_INHRT, T_NOP, 0}, {L_INHRT, T_TENPCT, 0},
    {L_INHRT, T_TENPCT, 0}, {L_INHRT, T_PANPOS, 0}, {L_INHRT, T_NOP, 0}, {L_INHRT, T_NOP, 0}, {L_INHRT, T_NOP, 0},
    {L_INHRT, T_TIME, -12000}, {L_INHRT, T_FREQ, 0}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_FREQ, 0},
    {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TIME, -12000},
    {L_INHRT, T_MODSUST, 0}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TM_KEY, 0}, {L_INHRT, T_TM_KEY, 0},
    {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TIME, -12000},
    {L_INHRT, T_VOLSUST, 0}, {L_INHRT, T_TIME, -12000}, {L_INHRT, T_TM_KEY, 0}, {L_INHRT, T_TM_KEY, 0},
    {L_PRSET, T_NOCONV, 0}, {L_INHRT, T_NOP, 0}, {L_RANGE, T_RANGE, RANGE(0, 127)}, {L_RANGE, T_RANGE, RANGE(0, 127)},
    {L_INHRT, T_HI_OFF, 0}, {L_OVWRT, T_NOCONV, -1}, {L_OVWRT, T_NOCONV, -1}, {L_INHRT, T_ATTEN, 0},
    {L_INHRT, T_NOP, 0}, {L_INHRT, T_HI_OFF, 0}, {L_INHRT, T_NOCONV, 0}, {L_INHRT, T_NOCONV, 0},
    {L_INSTR, T_NOCONV, 0}, {L_OVWRT, T_NOCONV, 0}, {L_OVWRT, T_NOCONV, 0}, {L_INHRT, T_SCALE, 100},
    {L_OVWRT, T_NOCONV, 0}, {L_OVWRT, T_NOCONV, -1},
};

/* sbkconv.c: SBK units -> SF2 units */
static int sbk_to_sf2(int g, int v)
{
    switch (gitem[g].type) {
    case T_CUTOFF:  return v == 127 ? 14400 : 59 * v + 4366;
    case T_FILTERQ: return v * 3 / 2;
    case T_TENPCT:  return v * 1000 / 256;
    case T_PANPOS:  return v * 1000 / 127 - 500;
    case T_ATTEN:   return v == 0 ? 1000 : (int)(-200.0 * log10((double)v / 127.0) * 10);
    case T_SCALE:   return v ? 50 : 100;
    case T_TIME:    return msec_to_timecent(v);
    case T_TM_KEY:  return (int)(v * 5.55);
    case T_FREQ:
        if (v == 0) return g == G_freqLfo1 ? -725 : -15600;
        return (int)(1200 * log10((double)v) / log10(2.0) - 7925.0);
    case T_PSHIFT:  return (1200 * v / 64 + 1) / 2;
    case T_CSHIFT:  return g == G_lfo1ToFilterFc ? (1200 * 3 * v) / 64 : (1200 * 6 * v) / 64;
    case T_TREMOLO: return (120 * v) / 64;
    case T_MODSUST: return v < 96 ? 1000 * (96 - v) / 96 : 0;
    case T_VOLSUST: return v < 96 ? (2000 - 21 * v) / 2 : 0;
    default:        return v;
    }
}

/* ======================================================================================================== */
/* banks                                                                                                     */
/* ======================================================================================================== */
typedef struct {
    uint16_t moddelay, modatkhld, moddcysus, modrelease, voldelay, volatkhld, voldcysus, volrelease;
    uint16_t lfo1delay, lfo2delay, pefe, fmmod, tremfrq, fm2frq2;
    uint8_t cutoff, filterQ, chorus, reverb;
} Parm;
typedef struct {
    uint32_t start, end, loopstart, loopend;     /* absolute EMU8000 word addresses */
    int16_t rate_offset, tune, scaleTuning;
    int8_t root, fixkey, fixvel, pan;            /* pan -1 = none, 0-127 */
    uint8_t attenuation, amplitude, exclass, keylo, keyhi, vello, velhi;
    uint8_t ram;                                 /* addresses are words into the bank's own RAM block */
    Parm parm;
} Zone;
typedef struct { uint16_t bank, prog; int nz; Zone* z; char name[21]; const struct AwemBank* owner; } Preset;
struct AwemBank {
    char name[64];
    int npresets;
    Preset* p;
    uint16_t* ram;          /* RAM sample words (NULL = ROM only), uploaded at RAM start + ram_base */
    uint32_t ram_words, skipped;
    uint32_t ram_base;      /* where the RAM layout put it (words from RAM start) */
    int loaded;             /* 1 = its RAM samples are in the sample RAM (layout found room) */
};
const char* awem_bank_name(const AwemBank* b) { return b ? b->name : ""; }
int awem_bank_presets(const AwemBank* b) { return b ? b->npresets : 0; }
uint32_t awem_bank_ram_used(const AwemBank* b) { return b ? b->ram_words : 0; }
int awem_bank_skipped(const AwemBank* b) { return b ? (int)b->skipped : 0; }

void awem_bank_free(AwemBank* b)
{
    int i;
    if (!b) return;
    for (i = 0; i < b->npresets; i++) free(b->p[i].z);
    free(b->p);
    free(b->ram);
    free(b);
}

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

typedef struct {
    const uint8_t *phdr, *pbag, *pgen, *inst, *ibag, *igen, *shdr, *smpl, *inam, *irom;
    uint32_t nphdr, npbag, npgen, ninst, nibag, nigen, nshdr, smpl_bytes, inam_len, irom_len;
    int version;
} Riff;

/* walk the RIFF sfbk tree; 0 = ok */
static int riff_parse(const uint8_t* d, uint32_t n, Riff* r)
{
    uint32_t pos = 12;
    memset(r, 0, sizeof *r);
    if (n < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "sfbk", 4)) return -1;
    if (rd32(d + 4) + 8 < n) n = rd32(d + 4) + 8;
    while (pos + 12 <= n) {
        uint32_t sz = rd32(d + pos + 4), end, p;
        if (memcmp(d + pos, "LIST", 4)) { pos += 8 + sz + (sz & 1); continue; }
        end = pos + 8 + sz;
        if (end > n) end = n;
        for (p = pos + 12; p + 8 <= end;) {
            const uint8_t* id = d + p;
            uint32_t s = rd32(d + p + 4);
            const uint8_t* q = d + p + 8;
            if (p + 8 + s > end) s = end - p - 8;
            if (!memcmp(id, "ifil", 4) && s >= 2) r->version = rd16(q);
            else if (!memcmp(id, "INAM", 4)) { r->inam = q; r->inam_len = s; }
            else if (!memcmp(id, "irom", 4)) { r->irom = q; r->irom_len = s; }
            else if (!memcmp(id, "smpl", 4)) { r->smpl = q; r->smpl_bytes = s; }
            else if (!memcmp(id, "phdr", 4)) { r->phdr = q; r->nphdr = s / 38; }
            else if (!memcmp(id, "pbag", 4)) { r->pbag = q; r->npbag = s / 4; }
            else if (!memcmp(id, "pgen", 4)) { r->pgen = q; r->npgen = s / 4; }
            else if (!memcmp(id, "inst", 4)) { r->inst = q; r->ninst = s / 22; }
            else if (!memcmp(id, "ibag", 4)) { r->ibag = q; r->nibag = s / 4; }
            else if (!memcmp(id, "igen", 4)) { r->igen = q; r->nigen = s / 4; }
            else if (!memcmp(id, "shdr", 4)) r->shdr = q, r->nshdr = s;   /* bytes for now */
            p += 8 + s + (s & 1);
        }
        pos = end + (sz & 1);
    }
    if (!r->version || !r->phdr || !r->pbag || !r->pgen || !r->inst || !r->ibag || !r->igen || !r->shdr) return -2;
    r->nshdr /= r->version > 1 ? 46 : 16;
    return 0;
}

typedef struct { uint32_t start, end, startloop, endloop, rate; int pitch, corr, rom, loopshot; } Smp;
typedef struct { int16_t val[G_EOF]; uint8_t set[G_EOF]; } Tbl;

static void tbl_from_layer(Tbl* t, const uint8_t* gen, uint32_t g0, uint32_t g1, int level)
{
    uint32_t i;
    for (i = g0; i < g1; i++) {
        unsigned oper = rd16(gen + i * 4);
        if (oper >= G_EOF) continue;
        t->val[oper] = (int16_t)rd16(gen + i * 4 + 2);
        t->set[oper] = (uint8_t)level;
    }
}
static int layer_has(const uint8_t* gen, uint32_t g0, uint32_t g1, unsigned oper)
{
    uint32_t i;
    for (i = g0; i < g1; i++) if (rd16(gen + i * 4) == oper) return 1;
    return 0;
}
static void merge(int v1, Tbl* dst, const Tbl* src)
{
    int i;
    for (i = 0; i < G_EOF; i++) {
        if (!src->set[i]) continue;
        if (v1) {
            if (!dst->set[i] || i == G_keyRange || i == G_velRange) dst->val[i] = src->val[i];
        } else {
            switch (gitem[i].copy) {
            case L_INHRT: dst->val[i] = (int16_t)(dst->val[i] + src->val[i]); break;
            case L_OVWRT: dst->val[i] = src->val[i]; break;
            case L_PRSET: case L_INSTR: if (!dst->set[i]) dst->val[i] = src->val[i]; break;
            case L_RANGE:
                if (!dst->set[i]) dst->val[i] = src->val[i];
                else {
                    int lo = dst->val[i] & 0xff, hi = (dst->val[i] >> 8) & 0xff, l2 = src->val[i] & 0xff, h2 = (src->val[i] >> 8) & 0xff;
                    if (l2 > lo) lo = l2;
                    if (h2 < hi) hi = h2;
                    dst->val[i] = (int16_t)RANGE(lo, hi);
                }
                break;
            }
        }
        dst->set[i] = 1;
    }
}

static void zone_from_tbl(int v1, const Tbl* t, const Smp* sp, uint32_t base, Zone* z)
{
    int val[G_EOF], i;
    Parm* p = &z->parm;
    for (i = 0; i < G_EOF; i++) {
        val[i] = t->set[i] ? t->val[i] : gitem[i].defv;
        if (v1) {
            if (!t->set[i]) {   /* SBK defaults (parsesf.c init_layer_items) */
                if (i == G_sustainEnv1 || i == G_sustainEnv2) val[i] = 1000;
                else if (i == G_freqLfo1) val[i] = -725;
                else if (i == G_freqLfo2) val[i] = -15600;
            } else val[i] = sbk_to_sf2(i, val[i]);
        } else if (i == G_keyRange || i == G_velRange) val[i] = (uint16_t)val[i];
    }
    if (v1) { val[G_keyRange] = (uint16_t)t->val[G_keyRange]; val[G_velRange] = (uint16_t)t->val[G_velRange];
              if (!t->set[G_keyRange]) val[G_keyRange] = RANGE(0, 127);
              if (!t->set[G_velRange]) val[G_velRange] = RANGE(0, 127); }
    /* set_sample_info */
    {
        int32_t so = (val[G_startAddrsHi] << 15) + val[G_startAddrs];
        int32_t eo = (val[G_endAddrsHi] << 15) + val[G_endAddrs];
        int32_t lso = (val[G_startloopAddrsHi] << 15) + val[G_startloopAddrs];
        int32_t leo = (val[G_endloopAddrsHi] << 15) + val[G_endloopAddrs];
        z->start = base + (uint32_t)((int32_t)sp->start + so);
        z->end = base + (uint32_t)((int32_t)sp->end + eo);
        if (val[G_sampleFlags] == 1 || val[G_sampleFlags] == 3) {
            z->loopstart = base + (uint32_t)((int32_t)sp->startloop + lso);
            z->loopend = base + (uint32_t)((int32_t)sp->endloop + leo);
        } else {     /* one shot: loop in the blank after the sample */
            z->loopstart = base + sp->end + 8;
            z->loopend = base + sp->end + (uint32_t)sp->loopshot - 8;
        }
        z->rate_offset = calc_rate_offset((int)sp->rate);
    }
    /* set_init_info */
    z->keylo = (uint8_t)(val[G_keyRange] & 0x7f); z->keyhi = (uint8_t)((val[G_keyRange] >> 8) & 0x7f);
    z->vello = (uint8_t)(val[G_velRange] & 0x7f); z->velhi = (uint8_t)((val[G_velRange] >> 8) & 0x7f);
    z->fixkey = (int8_t)(val[G_keynum] > 127 ? -1 : val[G_keynum]);
    z->fixvel = (int8_t)(val[G_velocity] > 127 ? -1 : val[G_velocity]);
    z->pan = (int8_t)calc_pan(val[G_pan]);
    z->amplitude = 127;
    z->attenuation = calc_attenuation(val[G_initAtten]);
    p->chorus = calc_send(t->set[G_chorus] ? val[G_chorus] : 0);
    p->reverb = calc_send(t->set[G_reverb] ? val[G_reverb] : 0);
    p->cutoff = calc_cutoff(val[G_initialFilterFc]);
    p->filterQ = calc_filterQ(val[G_initialFilterQ]);
    z->exclass = (uint8_t)val[G_keyExclusiveClass];
    /* set_rootkey */
    {
        int root, tune, scale = val[G_scaleTuning];
        if (v1 && t->set[G_samplePitch]) {
            root = val[G_samplePitch] / 100;
            tune = -val[G_samplePitch] % 100;
            if (tune <= -50) { root++; tune = 100 + tune; }
            if (scale == 50) tune /= 2;
        } else { root = sp->pitch; tune = sp->corr; }
        if (t->set[G_rootKey] && val[G_rootKey] >= 0) root += val[G_rootKey] - sp->pitch;
        if (v1) tune += val[G_coarseTune] * scale + val[G_fineTune] * scale / 100;
        else tune += val[G_coarseTune] * 100 + val[G_fineTune];
        if (root >= z->keyhi + 60) root -= 60;
        z->root = (int8_t)root; z->tune = (int16_t)tune; z->scaleTuning = (int16_t)scale;
    }
    /* set_modenv / set_volenv / set_lfo1 / set_lfo2 */
    p->moddelay = calc_delay(val[G_delayEnv1]);
    p->modatkhld = calc_atkhld(val[G_attackEnv1], val[G_holdEnv1]);
    p->moddcysus = TO_WORD(calc_sustain(val[G_sustainEnv1]), calc_decay(val[G_decayEnv1]));
    p->modrelease = TO_WORD(0x80, calc_decay(val[G_releaseEnv1]));
    p->pefe = TO_WORD(calc_pitch_shift(val[G_env1ToPitch]), calc_cutoff_shift(val[G_env1ToFilterFc], 6));
    p->voldelay = calc_delay(val[G_delayEnv2]);
    p->volatkhld = calc_atkhld(val[G_attackEnv2], val[G_holdEnv2]);
    p->voldcysus = TO_WORD(calc_sustain(val[G_sustainEnv2]), calc_decay(val[G_decayEnv2]));
    p->volrelease = TO_WORD(0x80, calc_decay(val[G_releaseEnv2]));
    p->lfo1delay = calc_delay(val[G_delayLfo1]);
    p->fmmod = TO_WORD(calc_pitch_shift(val[G_lfo1ToPitch]), calc_cutoff_shift(val[G_lfo1ToFilterFc], 3));
    p->tremfrq = TO_WORD(calc_tremolo(val[G_lfo1ToVolume]), calc_freq(val[G_freqLfo1]));
    p->lfo2delay = calc_delay(val[G_delayLfo2]);
    p->fm2frq2 = TO_WORD(calc_pitch_shift(val[G_lfo2ToPitch]), calc_freq(val[G_freqLfo2]));
}

static uint8_t* read_file(const char* path, uint32_t* n)
{
    FILE* f = fopen(path, "rb");
    long sz;
    uint8_t* d;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 256L * 1024 * 1024 || !(d = (uint8_t*)malloc((size_t)sz))) { fclose(f); return NULL; }
    if (fread(d, 1, (size_t)sz, f) != (size_t)sz) { free(d); fclose(f); return NULL; }
    fclose(f);
    *n = (uint32_t)sz;
    return d;
}

static void load_samples(const Riff* r, Smp* s)
{
    uint32_t i;
    int in_rom = 1, v1 = r->version == 1;
    for (i = 0; i < r->nshdr; i++) {
        const uint8_t* q = r->shdr + i * (v1 ? 16 : 46) + (v1 ? 0 : 20);
        s[i].start = rd32(q); s[i].end = rd32(q + 4); s[i].startloop = rd32(q + 8); s[i].endloop = rd32(q + 12);
        if (!v1) {
            s[i].rate = rd32(q + 16); s[i].pitch = q[20]; s[i].corr = (int8_t)q[21];
            s[i].rom = (rd16(q + 24) & 0x8000) != 0;
            if (!s[i].rate) s[i].rate = 44100;
            if (s[i].pitch > 127) s[i].pitch = 60;
        } else {
            s[i].rate = 44100; s[i].pitch = 60; s[i].corr = 0;
            if (s[i].start == 0) in_rom = 0;         /* the first RAM sample starts at 0 */
            s[i].rom = in_rom;
            s[i].startloop++; s[i].endloop += 2;     /* sample.c: SBK positions */
        }
    }
    for (i = 0; i < r->nshdr; i++) {
        int ls = 48;
        if (i + 1 < r->nshdr) { ls = (int)(s[i + 1].start - s[i].end); if (ls < 0 || ls > 48) ls = 48; }
        s[i].loopshot = ls;
    }
}

static int name_cmp_ci(const char* a, const char* b)
{
    for (; *b; a++, b++) {
        char x = *a, y = *b;
        if (!x) return 0;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return 1;
}
static int name_has(const char* s, const char* w) { for (; *s; s++) if (name_cmp_ci(s, w)) return 1; return 0; }

/* kind from presets: GS = melodic variation banks, MT-32 = MT-32 tone names on the first programs */
static int guess_kind(const Riff* r, char* bname)
{
    uint32_t i;
    int var = 0, mt = 0, gm = 0;
    if (bname[0]) {
        if (name_has(bname, "MT-32") || name_has(bname, "MT32") || name_has(bname, "MT 32")) return AWEM_MT32;
        if (name_has(bname, " GS") || name_has(bname, "GS ") || name_has(bname, "(GS")) return AWEM_GS;
    }
    for (i = 0; i + 1 < r->nphdr; i++) {
        const uint8_t* q = r->phdr + i * 38;
        int prog = rd16(q + 20), bank = rd16(q + 22);
        char n[21];
        memcpy(n, q, 20); n[20] = 0;
        if (bank > 0 && bank < 128) var++;
        if (bank != 0) continue;
        if (prog == 8) { if (name_has(n, "org")) mt++; if (name_has(n, "cel")) gm++; }
        if (prog == 16) { if (name_has(n, "hrp") || name_has(n, "harps") || name_has(n, "hpsi")) mt++; if (name_has(n, "org")) gm++; }
        if (prog == 7) { if (name_has(n, "honk")) mt++; if (name_has(n, "clav")) gm++; }
        if (prog == 24) { if (name_has(n, "cel")) mt++; if (name_has(n, "guit") || name_has(n, "nylon") || name_has(n, "gtr")) gm++; }
    }
    if (mt >= 2 && mt > gm) return AWEM_MT32;
    if (var >= 8) return AWEM_GS;
    if (gm >= 2) return AWEM_GM;
    return -1;
}

int awem_probe(const char* path, int* kind, char* name, int namelen)
{
    uint32_t n;
    uint8_t* d = read_file(path, &n);
    Riff r;
    int rc = 0;
    char bn[64] = "";
    if (kind) *kind = -1;
    if (name && namelen > 0) name[0] = 0;
    if (!d) return 0;
    if (riff_parse(d, n, &r) == 0) {
        if (r.inam) { uint32_t k = r.inam_len < 63 ? r.inam_len : 63; memcpy(bn, r.inam, k); bn[k] = 0; }
        if (r.version == 1) rc = (r.irom && r.irom_len >= 4) ? 1 : 2;
        else rc = 3;
        if (rc == 1) {   /* plays from ROM: at least one sample before the RAM part */
            Smp* s = (Smp*)calloc(r.nshdr + 1, sizeof(Smp));
            uint32_t i, rom = 0;
            if (s) { load_samples(&r, s); for (i = 0; i < r.nshdr; i++) rom += (uint32_t)s[i].rom; free(s); }
            if (!rom) rc = 2;
        }
        if (kind) *kind = guess_kind(&r, bn);
        if (name && namelen > 0) { strncpy(name, bn, (size_t)namelen - 1); name[namelen - 1] = 0; }
    }
    free(d);
    return rc;
}

static int preset_order(const void* a, const void* b)
{
    const Preset *x = (const Preset*)a, *y = (const Preset*)b;
    return x->bank != y->bank ? x->bank - y->bank : x->prog - y->prog;
}

AwemBank* awem_bank_load(const char* path, uint32_t ram_words, char* err, int errlen)
{
    uint32_t n, i, k;
    uint8_t* d = read_file(path, &n);
    Riff r;
    Smp* smp = NULL;
    AwemBank* b = NULL;
    int v1;
    uint32_t ram_need = 0;
#define FAIL(m) do { if (err && errlen > 0) { strncpy(err, m, (size_t)errlen - 1); err[errlen - 1] = 0; } goto out; } while (0)
    if (!d) FAIL("cannot read the file");
    if (riff_parse(d, n, &r)) FAIL("not a SoundFont bank");
    v1 = r.version == 1;
    if (!(b = (AwemBank*)calloc(1, sizeof *b))) FAIL("out of memory");
    if (r.inam) { uint32_t m = r.inam_len < 63 ? r.inam_len : 63; memcpy(b->name, r.inam, m); b->name[m] = 0; }
    if (!b->name[0]) { const char* s = strrchr(path, '\\'); const char* s2 = strrchr(path, '/'); if (s2 > s) s = s2;
                       strncpy(b->name, s ? s + 1 : path, 63); }
    smp = (Smp*)calloc(r.nshdr + 1, sizeof(Smp));
    if (!smp) FAIL("out of memory");
    load_samples(&r, smp);
    /* RAM: the smpl data as it is, from RAM start (SBK: RAM samples count from 0; SF2: every sample) */
    if (r.smpl && r.smpl_bytes >= 2) {
        int any = 0;
        for (i = 0; i < r.nshdr; i++) if (!smp[i].rom) { any = 1; if (smp[i].end + 48 > ram_need) ram_need = smp[i].end + 48; }
        if (any) {
            uint32_t words = r.smpl_bytes / 2;
            if (ram_need > words) ram_need = words;
            if (ram_need > ram_words) ram_need = ram_words;
            if (ram_need) {
                b->ram = (uint16_t*)malloc(ram_need * 2);
                if (!b->ram) FAIL("out of memory");
                for (k = 0; k < ram_need; k++) b->ram[k] = rd16(r.smpl + k * 2);
                b->ram_words = ram_need;
            }
        }
    }
    b->npresets = r.nphdr > 1 ? (int)r.nphdr - 1 : 0;
    b->p = (Preset*)calloc((size_t)b->npresets + 1, sizeof(Preset));
    if (!b->p) FAIL("out of memory");
    for (i = 0; i < (uint32_t)b->npresets; i++) {
        const uint8_t* q = r.phdr + i * 38;
        Preset* pr = &b->p[i];
        uint32_t bag0 = rd16(q + 24), bag1 = rd16(q + 38 + 24), lb, cap = 8;
        int have_global = 0;
        Tbl gl;
        memcpy(pr->name, q, 20); pr->name[20] = 0;
        pr->prog = rd16(q + 20); pr->bank = rd16(q + 22);
        pr->z = (Zone*)malloc(cap * sizeof(Zone));
        if (!pr->z) FAIL("out of memory");
        memset(&gl, 0, sizeof gl);
        for (lb = bag0; lb < bag1 && lb + 1 < r.npbag; lb++) {
            uint32_t g0 = rd16(r.pbag + lb * 4), g1 = rd16(r.pbag + (lb + 1) * 4), ib, inst;
            Tbl pt;
            if (g1 > r.npgen) g1 = r.npgen;
            if (lb == bag0 && !layer_has(r.pgen, g0, g1, G_instrument)) { tbl_from_layer(&gl, r.pgen, g0, g1, 1); have_global = 1; continue; }
            memset(&pt, 0, sizeof pt);
            if (have_global) pt = gl;
            tbl_from_layer(&pt, r.pgen, g0, g1, 2);
            if (!pt.set[G_instrument]) continue;
            inst = (uint16_t)pt.val[G_instrument];
            if (inst + 1 >= r.ninst) continue;
            {
                uint32_t ib0 = rd16(r.inst + inst * 22 + 20), ib1 = rd16(r.inst + (inst + 1) * 22 + 20);
                Tbl ig;
                int ihave = 0;
                memset(&ig, 0, sizeof ig);
                for (ib = ib0; ib < ib1 && ib + 1 < r.nibag; ib++) {
                    uint32_t h0 = rd16(r.ibag + ib * 4), h1 = rd16(r.ibag + (ib + 1) * 4);
                    Tbl ct;
                    unsigned sid;
                    const Smp* sp;
                    Zone z;
                    if (h1 > r.nigen) h1 = r.nigen;
                    if (ib == ib0 && !layer_has(r.igen, h0, h1, G_sampleId)) { tbl_from_layer(&ig, r.igen, h0, h1, 1); ihave = 1; continue; }
                    memset(&ct, 0, sizeof ct);
                    if (ihave) ct = ig;
                    tbl_from_layer(&ct, r.igen, h0, h1, 2);
                    if (!ct.set[G_sampleId]) continue;
                    merge(v1, &ct, &pt);
                    {
                        int lo = ct.val[G_keyRange] & 0xff, hi = (ct.val[G_keyRange] >> 8) & 0xff;
                        if (ct.set[G_keyRange] && (lo > 127 || hi > 127 || hi < lo)) continue;
                    }
                    sid = (uint16_t)ct.val[G_sampleId];
                    if (sid >= r.nshdr) continue;
                    sp = &smp[sid];
                    if (!sp->rom && sp->end + (uint32_t)sp->loopshot > b->ram_words) { b->skipped++; continue; }
                    memset(&z, 0, sizeof z);
                    zone_from_tbl(v1, &ct, sp, 0, &z);
                    z.ram = !sp->rom;
                    if (z.keylo > z.keyhi || z.vello > z.velhi) continue;
                    if (pr->nz == (int)cap) {
                        Zone* nz2 = (Zone*)realloc(pr->z, (cap *= 2) * sizeof(Zone));
                        if (!nz2) FAIL("out of memory");
                        pr->z = nz2;
                    }
                    pr->z[pr->nz++] = z;
                }
            }
        }
    }
    qsort(b->p, (size_t)b->npresets, sizeof(Preset), preset_order);
    for (i = 0; i < (uint32_t)b->npresets; i++) b->p[i].owner = b;
    b->loaded = 1;
    free(smp); free(d);
    return b;
out:
    free(smp); free(d);
    awem_bank_free(b);
    return NULL;
#undef FAIL
}

/* the built-in map (awegm.h: samples found in the ROM by testrig/awe/awegm.py, programs grouped by GM family) */
AwemBank* awem_bank_builtin(void)
{
    AwemBank* b = (AwemBank*)calloc(1, sizeof *b);
    int i, j;
    if (!b) return NULL;
    strcpy(b->name, "NUKED GM map (AWE32 ROM)");
    b->npresets = 129;
    b->p = (Preset*)calloc(129, sizeof(Preset));
    if (!b->p) { free(b); return NULL; }
    for (i = 0; i < 129; i++) {
        Preset* pr = &b->p[i];
        const AwegmProg* gp = &awegm_prog[i];
        pr->bank = i < 128 ? 0 : 128; pr->prog = i < 128 ? (uint16_t)i : 0;
        snprintf(pr->name, sizeof pr->name, "%s", gp->name);
        pr->nz = gp->nz;
        pr->z = (Zone*)calloc((size_t)gp->nz + 1, sizeof(Zone));
        if (!pr->z) { awem_bank_free(b); return NULL; }
        for (j = 0; j < gp->nz; j++) {
            const AwegmZone* g = &gp->z[j];
            const AwegmSample* s = &awegm_sample[g->sample];
            Zone* z = &pr->z[j];
            Tbl t;
            Smp sp;
            memset(&t, 0, sizeof t);
            memset(&sp, 0, sizeof sp);
            sp.start = s->start; sp.end = s->end; sp.rate = 44100; sp.pitch = s->root; sp.corr = s->corr;
            sp.rom = 1; sp.loopshot = 48;
            if (s->loopstart) { sp.startloop = s->loopstart; sp.endloop = s->loopend; t.val[G_sampleFlags] = 1; t.set[G_sampleFlags] = 1; }
            t.val[G_keyRange] = (int16_t)RANGE(g->keylo, g->keyhi); t.set[G_keyRange] = 1;
            t.val[G_attackEnv2] = g->atk; t.set[G_attackEnv2] = 1;
            t.val[G_decayEnv2] = g->dcy; t.set[G_decayEnv2] = 1;
            t.val[G_sustainEnv2] = g->sus; t.set[G_sustainEnv2] = 1;
            t.val[G_releaseEnv2] = g->rel; t.set[G_releaseEnv2] = 1;
            t.val[G_initAtten] = g->atten; t.set[G_initAtten] = 1;
            t.val[G_reverb] = 200; t.set[G_reverb] = 1;
            t.val[G_initialFilterFc] = g->cutoff ? g->cutoff : 13500; t.set[G_initialFilterFc] = 1;
            if (g->vib) { t.val[G_lfo2ToPitch] = g->vib; t.set[G_lfo2ToPitch] = 1; t.val[G_freqLfo2] = 0; t.set[G_freqLfo2] = 1;
                          t.val[G_delayLfo2] = -3000; t.set[G_delayLfo2] = 1; }
            if (g->fixkey >= 0) { t.val[G_keynum] = g->fixkey; t.set[G_keynum] = 1; }
            if (g->pan) { t.val[G_pan] = g->pan; t.set[G_pan] = 1; }
            if (g->exclass) { t.val[G_keyExclusiveClass] = g->exclass; t.set[G_keyExclusiveClass] = 1; }
            if (g->tune) { t.val[G_coarseTune] = (int16_t)(g->tune / 100); t.val[G_fineTune] = (int16_t)(g->tune % 100);
                           t.set[G_coarseTune] = t.set[G_fineTune] = 1; }
            zone_from_tbl(0, &t, &sp, 0, z);
        }
        pr->owner = b;
    }
    b->loaded = 1;
    return b;
}

/* ======================================================================================================== */
/* chip set-up, effects (Linux emu8000.c)                                                                    */
/* ======================================================================================================== */
static const uint16_t init1[128] = {
    0x03ff, 0x0030, 0x07ff, 0x0130, 0x0bff, 0x0230, 0x0fff, 0x0330, 0x13ff, 0x0430, 0x17ff, 0x0530, 0x1bff, 0x0630, 0x1fff, 0x0730,
    0x23ff, 0x0830, 0x27ff, 0x0930, 0x2bff, 0x0a30, 0x2fff, 0x0b30, 0x33ff, 0x0c30, 0x37ff, 0x0d30, 0x3bff, 0x0e30, 0x3fff, 0x0f30,
    0x43ff, 0x0030, 0x47ff, 0x0130, 0x4bff, 0x0230, 0x4fff, 0x0330, 0x53ff, 0x0430, 0x57ff, 0x0530, 0x5bff, 0x0630, 0x5fff, 0x0730,
    0x63ff, 0x0830, 0x67ff, 0x0930, 0x6bff, 0x0a30, 0x6fff, 0x0b30, 0x73ff, 0x0c30, 0x77ff, 0x0d30, 0x7bff, 0x0e30, 0x7fff, 0x0f30,
    0x83ff, 0x0030, 0x87ff, 0x0130, 0x8bff, 0x0230, 0x8fff, 0x0330, 0x93ff, 0x0430, 0x97ff, 0x0530, 0x9bff, 0x0630, 0x9fff, 0x0730,
    0xa3ff, 0x0830, 0xa7ff, 0x0930, 0xabff, 0x0a30, 0xafff, 0x0b30, 0xb3ff, 0x0c30, 0xb7ff, 0x0d30, 0xbbff, 0x0e30, 0xbfff, 0x0f30,
    0xc3ff, 0x0030, 0xc7ff, 0x0130, 0xcbff, 0x0230, 0xcfff, 0x0330, 0xd3ff, 0x0430, 0xd7ff, 0x0530, 0xdbff, 0x0630, 0xdfff, 0x0730,
    0xe3ff, 0x0830, 0xe7ff, 0x0930, 0xebff, 0x0a30, 0xefff, 0x0b30, 0xf3ff, 0x0c30, 0xf7ff, 0x0d30, 0xfbff, 0x0e30, 0xffff, 0x0f30,
};
static const uint16_t init2[128] = {
    0x03ff, 0x8030, 0x07ff, 0x8130, 0x0bff, 0x8230, 0x0fff, 0x8330, 0x13ff, 0x8430, 0x17ff, 0x8530, 0x1bff, 0x8630, 0x1fff, 0x8730,
    0x23ff, 0x8830, 0x27ff, 0x8930, 0x2bff, 0x8a30, 0x2fff, 0x8b30, 0x33ff, 0x8c30, 0x37ff, 0x8d30, 0x3bff, 0x8e30, 0x3fff, 0x8f30,
    0x43ff, 0x8030, 0x47ff, 0x8130, 0x4bff, 0x8230, 0x4fff, 0x8330, 0x53ff, 0x8430, 0x57ff, 0x8530, 0x5bff, 0x8630, 0x5fff, 0x8730,
    0x63ff, 0x8830, 0x67ff, 0x8930, 0x6bff, 0x8a30, 0x6fff, 0x8b30, 0x73ff, 0x8c30, 0x77ff, 0x8d30, 0x7bff, 0x8e30, 0x7fff, 0x8f30,
    0x83ff, 0x8030, 0x87ff, 0x8130, 0x8bff, 0x8230, 0x8fff, 0x8330, 0x93ff, 0x8430, 0x97ff, 0x8530, 0x9bff, 0x8630, 0x9fff, 0x8730,
    0xa3ff, 0x8830, 0xa7ff, 0x8930, 0xabff, 0x8a30, 0xafff, 0x8b30, 0xb3ff, 0x8c30, 0xb7ff, 0x8d30, 0xbbff, 0x8e30, 0xbfff, 0x8f30,
    0xc3ff, 0x8030, 0xc7ff, 0x8130, 0xcbff, 0x8230, 0xcfff, 0x8330, 0xd3ff, 0x8430, 0xd7ff, 0x8530, 0xdbff, 0x8630, 0xdfff, 0x8730,
    0xe3ff, 0x8830, 0xe7ff, 0x8930, 0xebff, 0x8a30, 0xefff, 0x8b30, 0xf3ff, 0x8c30, 0xf7ff, 0x8d30, 0xfbff, 0x8e30, 0xffff, 0x8f30,
};
static const uint16_t init3[128] = {
    0x0C10, 0x8470, 0x14FE, 0xB488, 0x167F, 0xA470, 0x18E7, 0x84B5, 0x1B6E, 0x842A, 0x1F1D, 0x852A, 0x0DA3, 0x8F7C, 0x167E, 0xF254,
    0x0000, 0x842A, 0x0001, 0x852A, 0x18E6, 0x8BAA, 0x1B6D, 0xF234, 0x229F, 0x8429, 0x2746, 0x8529, 0x1F1C, 0x86E7, 0x229E, 0xF224,
    0x0DA4, 0x8429, 0x2C29, 0x8529, 0x2745, 0x87F6, 0x2C28, 0xF254, 0x383B, 0x8428, 0x320F, 0x8528, 0x320E, 0x8F02, 0x1341, 0xF264,
    0x3EB6, 0x8428, 0x3EB9, 0x8528, 0x383A, 0x8FA9, 0x3EB5, 0xF294, 0x3EB7, 0x8474, 0x3EBA, 0x8575, 0x3EB8, 0xC4C3, 0x3EBB, 0xC5C3,
    0x0000, 0xA404, 0x0001, 0xA504, 0x141F, 0x8671, 0x14FD, 0x8287, 0x3EBC, 0xE610, 0x3EC8, 0x8C7B, 0x031A, 0x87E6, 0x3EC8, 0x86F7,
    0x3EC0, 0x821E, 0x3EBE, 0xD208, 0x3EBD, 0x821F, 0x3ECA, 0x8386, 0x3EC1, 0x8C03, 0x3EC9, 0x831E, 0x3ECA, 0x8C4C, 0x3EBF, 0x8C55,
    0x3EC9, 0xC208, 0x3EC4, 0xBC84, 0x3EC8, 0x8EAD, 0x3EC8, 0xD308, 0x3EC2, 0x8F7E, 0x3ECB, 0x8219, 0x3ECB, 0xD26E, 0x3EC5, 0x831F,
    0x3EC6, 0xC308, 0x3EC3, 0xB2FF, 0x3EC9, 0x8265, 0x3EC9, 0x8319, 0x1342, 0xD36E, 0x3EC7, 0xB3FF, 0x0000, 0x8365, 0x1420, 0x9570,
};
static const uint16_t init4[128] = {
    0x0C10, 0x8470, 0x14FE, 0xB488, 0x167F, 0xA470, 0x18E7, 0x84B5, 0x1B6E, 0x842A, 0x1F1D, 0x852A, 0x0DA3, 0x0F7C, 0x167E, 0x7254,
    0x0000, 0x842A, 0x0001, 0x852A, 0x18E6, 0x0BAA, 0x1B6D, 0x7234, 0x229F, 0x8429, 0x2746, 0x8529, 0x1F1C, 0x06E7, 0x229E, 0x7224,
    0x0DA4, 0x8429, 0x2C29, 0x8529, 0x2745, 0x07F6, 0x2C28, 0x7254, 0x383B, 0x8428, 0x320F, 0x8528, 0x320E, 0x0F02, 0x1341, 0x7264,
    0x3EB6, 0x8428, 0x3EB9, 0x8528, 0x383A, 0x0FA9, 0x3EB5, 0x7294, 0x3EB7, 0x8474, 0x3EBA, 0x8575, 0x3EB8, 0x44C3, 0x3EBB, 0x45C3,
    0x0000, 0xA404, 0x0001, 0xA504, 0x141F, 0x0671, 0x14FD, 0x0287, 0x3EBC, 0xE610, 0x3EC8, 0x0C7B, 0x031A, 0x07E6, 0x3EC8, 0x86F7,
    0x3EC0, 0x821E, 0x3EBE, 0xD208, 0x3EBD, 0x021F, 0x3ECA, 0x0386, 0x3EC1, 0x0C03, 0x3EC9, 0x031E, 0x3ECA, 0x8C4C, 0x3EBF, 0x0C55,
    0x3EC9, 0xC208, 0x3EC4, 0xBC84, 0x3EC8, 0x0EAD, 0x3EC8, 0xD308, 0x3EC2, 0x8F7E, 0x3ECB, 0x0219, 0x3ECB, 0xD26E, 0x3EC5, 0x031F,
    0x3EC6, 0xC308, 0x3EC3, 0x32FF, 0x3EC9, 0x0265, 0x3EC9, 0x8319, 0x1342, 0xD36E, 0x3EC7, 0x33FF, 0x0000, 0x8365, 0x1420, 0x9570,
};
static void send_array(const uint16_t* p)
{
    int i;
    for (i = 0; i < 32; i++) W(EMU8K_INIT1, i, *p++);
    for (i = 0; i < 32; i++) W(EMU8K_INIT2, i, *p++);
    for (i = 0; i < 32; i++) W(EMU8K_INIT3, i, *p++);
    for (i = 0; i < 32; i++) W(EMU8K_INIT4, i, *p++);
}

static const uint16_t chorus_parm[8][3] = {   /* feedback, delay offset, LFO depth; + right delay, LFO freq below */
    {0xE600, 0x03F6, 0xBC2C}, {0xE608, 0x031A, 0xBC6E}, {0xE610, 0x031A, 0xBC84}, {0xE620, 0x0269, 0xBC6E},
    {0xE680, 0x04D3, 0xBCA6}, {0xE6E0, 0x044E, 0xBC37}, {0xE600, 0x0B06, 0xBC00}, {0xE6C0, 0x0B06, 0xBC00},
};
static const uint32_t chorus_parm2[8][2] = {
    {0, 0x6D}, {0, 0x17C}, {0, 0x83}, {0, 0x17C}, {0, 0x5B}, {0, 0x26}, {0x6E000, 0x83}, {0x6E000, 0x83},
};
static const uint16_t reverb_parm[8][28] = {
    {0xB488, 0xA450, 0x9550, 0x84B5, 0x383A, 0x3EB5, 0x72F4, 0x72A4, 0x7254, 0x7204, 0x7204, 0x7204, 0x4416, 0x4516,
     0xA490, 0xA590, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528},
    {0xB488, 0xA458, 0x9558, 0x84B5, 0x383A, 0x3EB5, 0x7284, 0x7254, 0x7224, 0x7224, 0x7254, 0x7284, 0x4448, 0x4548,
     0xA440, 0xA540, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528},
    {0xB488, 0xA460, 0x9560, 0x84B5, 0x383A, 0x3EB5, 0x7284, 0x7254, 0x7224, 0x7224, 0x7254, 0x7284, 0x4416, 0x4516,
     0xA490, 0xA590, 0x842C, 0x852C, 0x842C, 0x852C, 0x842B, 0x852B, 0x842B, 0x852B, 0x842A, 0x852A, 0x842A, 0x852A},
    {0xB488, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7284, 0x7254, 0x7224, 0x7224, 0x7254, 0x7284, 0x4448, 0x4548,
     0xA440, 0xA540, 0x842B, 0x852B, 0x842B, 0x852B, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529},
    {0xB488, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7254, 0x7234, 0x7224, 0x7254, 0x7264, 0x7294, 0x44C3, 0x45C3,
     0xA404, 0xA504, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528},
    {0xB4FF, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7234, 0x7234, 0x7234, 0x7234, 0x7234, 0x7234, 0x4448, 0x4548,
     0xA440, 0xA540, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528},
    {0xB4FF, 0xA470, 0x9500, 0x84B5, 0x333A, 0x39B5, 0x7204, 0x7204, 0x7204, 0x7204, 0x7204, 0x72F4, 0x4400, 0x4500,
     0xA4FF, 0xA5FF, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520},
    {0xB4FF, 0xA490, 0x9590, 0x8474, 0x333A, 0x39B5, 0x7204, 0x7204, 0x7204, 0x7204, 0x7204, 0x72F4, 0x4400, 0x4500,
     0xA4FF, 0xA5FF, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520, 0x8420, 0x8520},
};
/* INIT1 = Data1 r2, INIT2 = Data2 r2, INIT3 = Data1 r3, INIT4 = Data2 r3 */
static const uint8_t reverb_cmd[28][2] = {
    {1, 0x03}, {1, 0x05}, {4, 0x1F}, {1, 0x07}, {2, 0x14}, {2, 0x16}, {1, 0x0F}, {1, 0x17}, {1, 0x1F}, {2, 0x07},
    {2, 0x0F}, {2, 0x17}, {2, 0x1D}, {2, 0x1F}, {3, 0x01}, {3, 0x03}, {1, 0x09}, {1, 0x0B}, {1, 0x11}, {1, 0x13},
    {1, 0x19}, {1, 0x1B}, {2, 0x01}, {2, 0x03}, {2, 0x09}, {2, 0x0B}, {2, 0x11}, {2, 0x13},
};
static const int init_reg[5] = {0, EMU8K_INIT1, EMU8K_INIT2, EMU8K_INIT3, EMU8K_INIT4};

/* ======================================================================================================== */
/* MIDI channels and voices                                                                                  */
/* ======================================================================================================== */
enum { ST_OFF = 0, ST_ON = 1, ST_SUSTAINED = 2, ST_RELEASED = 3, ST_PENDING = 4, ST_STANDBY = 5 };
typedef struct {
    uint8_t cc[128];
    uint8_t prog, drum, pressure;
    int bend;                   /* -8192..8191 */
    int bend_range;             /* 128 = 1 semitone */
    int fine, coarse;           /* RPN 1 (-8192..8191), RPN 2 (x128 per semitone) */
} Chan;
typedef struct {
    int state, chan, key, note, vel;
    uint32_t time, ontime;
    Zone reg;
    int avol, apitch, apan, aaux, acutoff, ptarget, ftarget, vtarget;
} Voice;

static struct {
    AwemBank* banks[4];
    int nbanks, mode;
    int reverb, chorus, gs_reverb, gs_chorus;   /* gs_*: set by GS SysEx (-1 = Settings) */
    int master;                                 /* GS/GM master volume 0-127 */
    Chan ch[16];
    Voice v[AWEM_VOICES];
    uint32_t use_time, now;
    int released;
    int started;                                /* awem_start() done: the chip is ours to write */
    uint8_t st, rs_need, buf[3], nbuf, sx[64];
    int sxn, in_sysex;
    AwemStats stats;
} S;

/* user banks 1-127 and single-preset replacements (Creative's bank manager), sample RAM layout */
#define AWEM_MAXOVR 256
typedef struct { int slot, prog; AwemBank* b; } Ovr;      /* slot 0-127 = bank select, 128 = drum kit */
static struct {
    AwemBank* u[128];           /* [1..127] */
    Ovr ovr[AWEM_MAXOVR];
    int novr;
    uint32_t ram_size;          /* words of sample RAM */
    int ram_set;                /* 0 = not told yet: no limit */
    uint32_t ram_used;
} U;

static const unsigned char pan_volumes[256] = {
0x00,0x03,0x06,0x09,0x0c,0x0f,0x12,0x14,0x17,0x1a,0x1d,0x20,0x22,0x25,0x28,0x2a,0x2d,0x30,0x32,0x35,0x37,0x3a,0x3c,0x3f,0x41,0x44,0x46,0x49,0x4b,0x4d,0x50,0x52,
0x54,0x57,0x59,0x5b,0x5d,0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6f,0x71,0x73,0x75,0x77,0x79,0x7b,0x7c,0x7e,0x80,0x82,0x84,0x86,0x88,0x89,0x8b,0x8d,0x8f,0x90,0x92,
0x94,0x96,0x97,0x99,0x9a,0x9c,0x9e,0x9f,0xa1,0xa2,0xa4,0xa5,0xa7,0xa8,0xaa,0xab,0xad,0xae,0xaf,0xb1,0xb2,0xb3,0xb5,0xb6,0xb7,0xb9,0xba,0xbb,0xbc,0xbe,0xbf,0xc0,
0xc1,0xc2,0xc3,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdc,0xdd,0xde,0xdf,
0xdf,0xe0,0xe1,0xe2,0xe2,0xe3,0xe4,0xe4,0xe5,0xe6,0xe6,0xe7,0xe8,0xe8,0xe9,0xe9,0xea,0xeb,0xeb,0xec,0xec,0xed,0xed,0xee,0xee,0xef,0xef,0xf0,0xf0,0xf1,0xf1,0xf1,
0xf2,0xf2,0xf3,0xf3,0xf3,0xf4,0xf4,0xf5,0xf5,0xf5,0xf6,0xf6,0xf6,0xf7,0xf7,0xf7,0xf7,0xf8,0xf8,0xf8,0xf9,0xf9,0xf9,0xf9,0xf9,0xfa,0xfa,0xfa,0xfa,0xfb,0xfb,0xfb,
0xfb,0xfb,0xfc,0xfc,0xfc,0xfc,0xfc,0xfc,0xfc,0xfd,0xfd,0xfd,0xfd,0xfd,0xfd,0xfd,0xfd,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};
static const unsigned char voltab1[128] = {
   0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x2b, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22,
   0x21, 0x20, 0x1f, 0x1e, 0x1e, 0x1d, 0x1c, 0x1b, 0x1b, 0x1a, 0x19, 0x19, 0x18, 0x17, 0x17, 0x16, 0x16, 0x15, 0x15, 0x14,
   0x14, 0x13, 0x13, 0x13, 0x12, 0x12, 0x11, 0x11, 0x11, 0x10, 0x10, 0x10, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0e, 0x0d,
   0x0d, 0x0d, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x09, 0x09, 0x09,
   0x08, 0x08, 0x08, 0x08, 0x08, 0x07, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 0x04,
   0x04, 0x04, 0x04, 0x04, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const unsigned char voltab2[128] = {
   0x32, 0x31, 0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x2a, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x24, 0x23, 0x22, 0x21,
   0x21, 0x20, 0x1f, 0x1e, 0x1e, 0x1d, 0x1c, 0x1c, 0x1b, 0x1a, 0x1a, 0x19, 0x19, 0x18, 0x18, 0x17, 0x16, 0x16, 0x15, 0x15,
   0x14, 0x14, 0x13, 0x13, 0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x10, 0x10, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d,
   0x0d, 0x0c, 0x0c, 0x0c, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x09, 0x09, 0x09, 0x08, 0x08, 0x08,
   0x08, 0x08, 0x07, 0x07, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
   0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01,
   0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const unsigned char expressiontab[128] = {
   0x7f, 0x6c, 0x62, 0x5a, 0x54, 0x50, 0x4b, 0x48, 0x45, 0x42, 0x40, 0x3d, 0x3b, 0x39, 0x38, 0x36, 0x34, 0x33, 0x31, 0x30,
   0x2f, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x24, 0x23, 0x22, 0x21, 0x21, 0x20, 0x1f, 0x1e, 0x1e,
   0x1d, 0x1d, 0x1c, 0x1b, 0x1b, 0x1a, 0x1a, 0x19, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16, 0x15, 0x15, 0x15, 0x14, 0x14, 0x13,
   0x13, 0x12, 0x12, 0x11, 0x11, 0x11, 0x10, 0x10, 0x0f, 0x0f, 0x0f, 0x0e, 0x0e, 0x0e, 0x0d, 0x0d, 0x0d, 0x0c, 0x0c, 0x0c,
   0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x09, 0x09, 0x08, 0x08, 0x08, 0x07, 0x07, 0x07, 0x07, 0x06, 0x06, 0x06,
   0x06, 0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 0x04, 0x04, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01,
   0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const unsigned char sf_vol_table[128] = {
    255,111,95,86,79,74,70,66,63,61,58,56,54,52,50,49,47,46,45,43,42,41,40,39,38,37,36,35,34,34,33,32,
    31,31,30,29,29,28,27,27,26,26,25,24,24,23,23,22,22,21,21,21,20,20,19,19,18,18,18,17,17,16,16,16,
    15,15,15,14,14,14,13,13,13,12,12,12,11,11,11,10,10,10,10,9,9,9,8,8,8,8,7,7,7,7,6,6,
    6,6,5,5,5,5,5,4,4,4,4,3,3,3,3,3,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0,
};

#define MOD_SENSE 18
#define CC_BANK 0
#define CC_MOD 1
#define CC_DATA 6
#define CC_VOL 7
#define CC_PAN 10
#define CC_EXPR 11
#define CC_BANK_LSB 32
#define CC_DATA_LSB 38
#define CC_SUSTAIN 64
#define CC_REVERB 91
#define CC_CHORUS 93
#define CC_NRPN_LSB 98
#define CC_NRPN_MSB 99
#define CC_RPN_LSB 100
#define CC_RPN_MSB 101

static int is_drum_bank(int bank) { return bank == 128; }
static int chan_bank(const Chan* c) { return c->drum ? 128 : (S.mode == AWEM_GS ? c->cc[CC_BANK] : 0); }

static int calc_volume(Voice* vp)
{
    const Chan* c = &S.ch[vp->chan];
    int vol, main_vol, expr = c->cc[CC_EXPR];
    if (vp->vel > 127) vp->vel = 127;
    main_vol = c->cc[CC_VOL] * vp->reg.amplitude / 127;
    LIMIT(main_vol, 0, 127);
    vol = voltab1[main_vol] + voltab2[vp->vel];
    vol = vol * 8 / 3;
    vol += vp->reg.attenuation;
    vol += ((0x100 - vol) * expressiontab[expr]) / 128;
    vol += sf_vol_table[S.master & 127];
    LIMIT(vol, 0, 255);
    if (vp->avol == vol) return 0;
    vp->avol = vol;
    if (!is_drum_bank(chan_bank(c)) && LO_BYTE(vp->reg.parm.volatkhld) < 0x7d) {
        int a = vp->vel < 70 ? 70 : vp->vel;
        vp->acutoff = (a * vp->reg.parm.cutoff + 0xa0) >> 7;
    } else vp->acutoff = vp->reg.parm.cutoff;
    return 1;
}
static int calc_pitch(Voice* vp)
{
    const Chan* c = &S.ch[vp->chan];
    int off = ((vp->reg.fixkey >= 0 ? vp->reg.fixkey : vp->note) - vp->reg.root) * 4096 / 12;
    off = off * vp->reg.scaleTuning / 100;
    off += vp->reg.tune * 4096 / 1200;
    if (c->bend) off += c->bend * c->bend_range / 3072;
    off += c->coarse * 4096 / (12 * 128);
    off += c->fine / 24;
    off += 0xe000 + vp->reg.rate_offset;
    LIMIT(off, 0, 0xffff);
    if (off == vp->apitch) return 0;
    vp->apitch = off;
    return 1;
}
static int calc_pan_(Voice* vp)
{
    int pan = S.ch[vp->chan].cc[CC_PAN] - 64;
    if (vp->reg.pan >= 0) pan += vp->reg.pan - 64;
    pan = 127 - pan * 2;
    LIMIT(pan, 0, 255);
    if (vp->apan != (int)pan_volumes[pan]) { vp->apan = pan_volumes[pan]; vp->aaux = pan_volumes[255 - pan]; return 1; }
    return 0;
}
static void setup_voice(Voice* vp)
{
    Parm* p = &vp->reg.parm;
    int pitch;
    vp->apan = vp->avol = vp->apitch = -1;
    calc_volume(vp); calc_pitch(vp); calc_pan_(vp);
    if (LO_BYTE(p->modatkhld) >= 0x80 && p->moddelay >= 0x8000) {
        p->moddelay = 0xbfff;
        pitch = (HI_BYTE(p->pefe) << 4) + vp->apitch;
        if (pitch > 0xffff) pitch = 0xffff;
        vp->ftarget = p->cutoff + LO_BYTE(p->pefe);
        LIMIT(vp->ftarget, 0, 255);
        vp->ftarget <<= 8;
    } else {
        vp->ftarget = p->cutoff << 8;
        pitch = vp->apitch;
    }
    if (pitch != 0xffff) {
        vp->ptarget = 1 << (pitch >> 12);
        if (pitch & 0x800) vp->ptarget += (vp->ptarget * 0x102e) / 0x2710;
        if (pitch & 0x400) vp->ptarget += (vp->ptarget * 0x764) / 0x2710;
        if (pitch & 0x200) vp->ptarget += (vp->ptarget * 0x389) / 0x2710;
        vp->ptarget += vp->ptarget >> 1;
        if (vp->ptarget > 0xffff) vp->ptarget = 0xffff;
    } else vp->ptarget = 0xffff;
    if (LO_BYTE(p->modatkhld) >= 0x80) { p->modatkhld &= ~0xff; p->modatkhld |= 0x7f; }
    vp->vtarget = 0;
    if (LO_BYTE(p->volatkhld) >= 0x80) { p->volatkhld &= ~0xff; p->volatkhld |= 0x7f; }
}

/* emu8000_callback.c */
static void set_fmmod(const Voice* vp)
{
    const Chan* c = &S.ch[vp->chan];
    int pitch = (int8_t)(vp->reg.parm.fmmod >> 8), mod = c->cc[CC_MOD] + c->pressure;
    pitch += MOD_SENSE * mod / 1200;
    LIMIT(pitch, -128, 127);
    W(EMU8K_FMMOD, vp - S.v, ((uint8_t)pitch << 8) | (vp->reg.parm.fmmod & 0xff));
}
static void set_fm2frq2(const Voice* vp)
{
    const Chan* c = &S.ch[vp->chan];
    int pitch = (int8_t)(vp->reg.parm.fm2frq2 >> 8), mod = c->cc[CC_MOD] + c->pressure;
    pitch += MOD_SENSE * mod / 1200;
    LIMIT(pitch, -128, 127);
    W(EMU8K_FM2FRQ2, vp - S.v, ((uint8_t)pitch << 8) | (vp->reg.parm.fm2frq2 & 0xff));
}
static void set_volume(const Voice* vp) { W(EMU8K_IFATN, vp - S.v, ((uint8_t)vp->acutoff << 8) | (uint8_t)vp->avol); }
static void set_pitch(const Voice* vp) { W(EMU8K_IP, vp - S.v, vp->apitch); }
static void set_pan(const Voice* vp) { W(EMU8K_PSST, vp - S.v, ((uint32_t)vp->apan << 24) | (vp->reg.loopstart - 1)); }

static void start_voice(Voice* vp)
{
    int ch = (int)(vp - S.v);
    const Chan* c = &S.ch[vp->chan];
    uint32_t t;
    W(EMU8K_DCYSUSV, ch, 0x0080);
    W(EMU8K_VTFT, ch, 0x0000FFFF);
    W(EMU8K_CVCF, ch, 0x0000FFFF);
    W(EMU8K_PTRX, ch, 0);
    W(EMU8K_CPF, ch, 0);
    set_pitch(vp);
    W(EMU8K_ENVVAL, ch, vp->reg.parm.moddelay);
    W(EMU8K_ATKHLD, ch, vp->reg.parm.modatkhld);
    W(EMU8K_DCYSUS, ch, vp->reg.parm.moddcysus);
    W(EMU8K_ENVVOL, ch, vp->reg.parm.voldelay);
    W(EMU8K_ATKHLDV, ch, vp->reg.parm.volatkhld);
    set_volume(vp);
    W(EMU8K_PEFE, ch, vp->reg.parm.pefe);
    W(EMU8K_LFO1VAL, ch, vp->reg.parm.lfo1delay);
    W(EMU8K_LFO2VAL, ch, vp->reg.parm.lfo2delay);
    set_fmmod(vp);
    W(EMU8K_TREMFRQ, ch, vp->reg.parm.tremfrq);
    set_fm2frq2(vp);
    set_pan(vp);
    t = vp->reg.parm.chorus + c->cc[CC_CHORUS] * 9 / 10;
    if (t > 255) t = 255;
    W(EMU8K_CSL, ch, (t << 24) | (vp->reg.loopend - 1));
    W(EMU8K_CCCA, ch, ((uint32_t)vp->reg.parm.filterQ << 28) | (vp->reg.start - 1));
    W(REG_Z1, ch, 0);
    W(REG_Z2, ch, 0);
    t = (uint32_t)vp->vtarget << 16;
    W(EMU8K_VTFT, ch, t | (uint32_t)vp->ftarget);
    W(EMU8K_CVCF, ch, t | 0xff00);
}
static void trigger_voice(Voice* vp)
{
    int ch = (int)(vp - S.v);
    uint32_t t = vp->reg.parm.reverb + S.ch[vp->chan].cc[CC_REVERB] * 9 / 10;
    if (t > 255) t = 255;
    W(EMU8K_PTRX, ch, (t << 8) | ((uint32_t)vp->ptarget << 16) | (uint32_t)vp->aaux);
    W(EMU8K_CPF, ch, (uint32_t)vp->ptarget << 16);
    W(EMU8K_DCYSUSV, ch, vp->reg.parm.voldcysus);
}
static void release_voice(Voice* vp)
{
    int ch = (int)(vp - S.v);
    W(EMU8K_DCYSUS, ch, 0x8000 | (uint8_t)vp->reg.parm.modrelease);
    W(EMU8K_DCYSUSV, ch, 0x8000 | (uint8_t)vp->reg.parm.volrelease);
}
static void terminate_voice(Voice* vp)
{
    if (!S.released) W(EMU8K_DCYSUSV, (int)(vp - S.v), 0x807F);
    vp->state = ST_OFF;
}

/* a free voice, else the oldest released, else the oldest sustained, else the oldest playing (Linux emu8000
   get_voice); voices of the note being started (STANDBY) are skipped */
static Voice* get_voice(void)
{
    enum { OFF, REL, SUS, PLAY, END };
    uint32_t age[END];
    int bv[END], i;
    for (i = 0; i < END; i++) { age[i] = 0; bv[i] = -1; }
    for (i = 0; i < AWEM_VOICES; i++) {
        Voice* vp = &S.v[i];
        int b;
        uint32_t a = S.use_time - vp->time;
        if (vp->state == ST_STANDBY) continue;
        if (vp->state == ST_OFF) b = OFF;
        else if (vp->state == ST_RELEASED || vp->state == ST_PENDING) {
            b = REL;
            if (!((R(EMU8K_CVCF, i) >> 16) & 0xffff)) b = OFF;
        } else if (vp->state == ST_SUSTAINED) b = SUS;
        else b = PLAY;
        if (vp->state != ST_OFF && vp->reg.loopstart > vp->reg.end &&       /* one shot played to its end */
            (R(EMU8K_CCCA, i) & 0xffffff) >= vp->reg.loopstart - 1) b = OFF;
        if (bv[b] < 0 || a > age[b]) { age[b] = a; bv[b] = i; }
    }
    for (i = 0; i < END; i++) if (bv[i] >= 0) {
        if (i >= SUS) S.stats.stolen++;
        return &S.v[bv[i]];
    }
    return NULL;
}

static const Preset* find_preset_in(const AwemBank* b, int bank, int prog)
{
    int lo = 0, hi = b->npresets - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        const Preset* p = &b->p[m];
        int d = p->bank != bank ? p->bank - bank : p->prog - prog;
        if (!d) return p->nz ? p : NULL;
        if (d < 0) lo = m + 1; else hi = m - 1;
    }
    return NULL;
}
static const Preset* find_ovr(int slot, int prog)
{
    int i;
    for (i = 0; i < U.novr; i++)
        if (U.ovr[i].slot == slot && U.ovr[i].prog == prog && U.ovr[i].b->loaded && U.ovr[i].b->npresets) return &U.ovr[i].b->p[0];
    return NULL;
}
/* user bank select: CC0, or CC32 when CC0 = 0 (sequencers that send only the LSB) */
static int chan_ubank(const Chan* c) { return c->cc[CC_BANK] ? c->cc[CC_BANK] : c->cc[CC_BANK_LSB]; }
static const Preset* find_user(const Chan* c)
{
    int ub = chan_ubank(c), i;
    const AwemBank* b;
    const Preset* p;
    if (c->drum) { if ((p = find_ovr(128, c->prog)) != NULL) return p; }
    else if ((p = find_ovr(ub, c->prog)) != NULL) return p;
    if (ub < 1 || ub > 127 || !(b = U.u[ub]) || !b->loaded) return NULL;
    if (c->drum) return find_preset_in(b, 128, c->prog);
    if ((p = find_preset_in(b, 0, c->prog)) != NULL) return p;
    for (i = 0; i < b->npresets; i++) if (b->p[i].bank < 128 && b->p[i].prog == c->prog && b->p[i].nz) return &b->p[i];
    return NULL;
}
static const Preset* find_preset(int bank, int prog)
{
    int i;
    for (i = 0; i < S.nbanks; i++) { const Preset* p = S.banks[i]->loaded ? find_preset_in(S.banks[i], bank, prog) : NULL; if (p) return p; }
    if (is_drum_bank(bank)) { for (i = 0; i < S.nbanks; i++) { const Preset* p = find_preset_in(S.banks[i], 128, 0); if (p) return p; } }
    else if (bank) { for (i = 0; i < S.nbanks; i++) { const Preset* p = find_preset_in(S.banks[i], 0, prog); if (p) return p; } }
    return NULL;
}

static void note_off(int ch, int key)
{
    int i;
    Chan* c = &S.ch[ch];
    for (i = 0; i < AWEM_VOICES; i++) {
        Voice* vp = &S.v[i];
        if ((vp->state == ST_ON) && vp->chan == ch && vp->key == key) {
            if (c->cc[CC_SUSTAIN] >= 64 && !c->drum) { vp->state = ST_SUSTAINED; continue; }
            if (S.now - vp->ontime < 64) { vp->state = ST_PENDING; continue; }   /* too soon after the trigger */
            vp->state = ST_RELEASED;
            if (!S.released) release_voice(vp);
        }
    }
}
static void note_on(int ch, int key, int vel)
{
    Chan* c = &S.ch[ch];
    const Preset* pr;
    int i, n = 0;
    Voice* started[16];
    S.stats.notes++;
    if (S.released) { S.stats.released_notes++; return; }
    pr = find_user(c);
    if (pr) S.stats.user_notes++;
    else pr = find_preset(chan_bank(c), c->prog);
    if (!pr) { S.stats.missing++; return; }
    for (i = 0; i < pr->nz; i++) {
        const Zone* z = &pr->z[i];
        if (key < z->keylo || key > z->keyhi || vel < z->vello || vel > z->velhi) continue;
        if (z->exclass) {
            int j;
            for (j = 0; j < AWEM_VOICES; j++) {
                Voice* vp = &S.v[j];
                if (vp->state != ST_OFF && vp->chan == ch && vp->reg.exclass == z->exclass) terminate_voice(vp);
            }
        }
    }
    for (i = 0; i < pr->nz && n < 16; i++) {
        const Zone* z = &pr->z[i];
        Voice* vp;
        if (key < z->keylo || key > z->keyhi || vel < z->vello || vel > z->velhi) continue;
        vp = get_voice();
        if (!vp) break;
        if (vp->state != ST_OFF) terminate_voice(vp);
        vp->time = S.use_time++;
        vp->chan = ch; vp->key = key; vp->note = key;
        vp->vel = z->fixvel >= 0 ? z->fixvel : vel;
        vp->reg = *z;
        if (z->ram) {   /* the bank's place in the sample RAM */
            uint32_t a = EMU8K_RAM_START + pr->owner->ram_base;
            vp->reg.start += a; vp->reg.end += a; vp->reg.loopstart += a; vp->reg.loopend += a;
        }
        S.stats.last_addr = vp->reg.start;
        setup_voice(vp);
        start_voice(vp);
        vp->state = ST_STANDBY;   /* not triggered yet */
        started[n++] = vp;
    }
    if (!n) { S.stats.missing++; return; }
    for (i = 0; i < n; i++) { trigger_voice(started[i]); started[i]->state = ST_ON; started[i]->ontime = S.now; }
    {
        uint32_t a = 0;
        for (i = 0; i < AWEM_VOICES; i++) a += S.v[i].state != ST_OFF;
        if (a > S.stats.voices_max) S.stats.voices_max = a;
    }
}

enum { U_VOL = 1, U_PITCH = 2, U_PAN = 4, U_MOD = 8 };
static void update_chan(int ch, int what)
{
    int i;
    if (S.released) return;
    for (i = 0; i < AWEM_VOICES; i++) {
        Voice* vp = &S.v[i];
        if (vp->state == ST_OFF || vp->chan != ch) continue;
        if ((what & U_VOL) && calc_volume(vp)) set_volume(vp);
        if ((what & U_PITCH) && calc_pitch(vp)) set_pitch(vp);
        if ((what & U_PAN) && calc_pan_(vp)) set_pan(vp);
        if (what & U_MOD) { set_fmmod(vp); set_fm2frq2(vp); }
    }
}
static void all_off(int ch, int now)
{
    int i;
    for (i = 0; i < AWEM_VOICES; i++) {
        Voice* vp = &S.v[i];
        if (vp->state == ST_OFF || (ch >= 0 && vp->chan != ch)) continue;
        if (now) terminate_voice(vp);
        else if (vp->state != ST_RELEASED) { vp->state = ST_RELEASED; if (!S.released) release_voice(vp); }
    }
}
static void reset_ctrl(Chan* c)
{
    c->cc[CC_MOD] = 0; c->cc[CC_EXPR] = 127; c->cc[CC_SUSTAIN] = 0;
    c->cc[CC_RPN_LSB] = c->cc[CC_RPN_MSB] = c->cc[CC_NRPN_LSB] = c->cc[CC_NRPN_MSB] = 127;
    c->pressure = 0; c->bend = 0;
}
static void reset_chan(int i)
{
    Chan* c = &S.ch[i];
    memset(c, 0, sizeof *c);
    reset_ctrl(c);
    c->cc[CC_VOL] = 100; c->cc[CC_PAN] = 64; c->cc[CC_REVERB] = 40; c->cc[CC_CHORUS] = 0;
    c->bend_range = S.mode == AWEM_MT32 ? 12 * 128 : 256;
    c->drum = i == 9;
}
static void apply_fx(void);
void awem_reset(void)
{
    int i;
    all_off(-1, 1);
    for (i = 0; i < 16; i++) reset_chan(i);
    S.master = 127;
    S.gs_reverb = S.gs_chorus = -1;
    apply_fx();
}

static void control(int ch, int cc, int v)
{
    Chan* c = &S.ch[ch];
    switch (cc) {
    case 120: all_off(ch, 1); return;
    case 121: reset_ctrl(c); update_chan(ch, U_VOL | U_PITCH | U_MOD); return;
    case 123: case 124: case 125: case 126: case 127: all_off(ch, 0); return;
    }
    c->cc[cc] = (uint8_t)v;
    switch (cc) {
    case CC_MOD: update_chan(ch, U_MOD); break;
    case CC_VOL: case CC_EXPR: update_chan(ch, U_VOL); break;
    case CC_PAN: update_chan(ch, U_PAN); break;
    case CC_SUSTAIN:
        if (v < 64) {
            int i;
            for (i = 0; i < AWEM_VOICES; i++) {
                Voice* vp = &S.v[i];
                if (vp->state == ST_SUSTAINED && vp->chan == ch) { vp->state = ST_RELEASED; if (!S.released) release_voice(vp); }
            }
        }
        break;
    case CC_NRPN_LSB: case CC_NRPN_MSB: c->cc[CC_RPN_LSB] = c->cc[CC_RPN_MSB] = 127; break;
    case CC_RPN_LSB: case CC_RPN_MSB: c->cc[CC_NRPN_LSB] = c->cc[CC_NRPN_MSB] = 127; break;
    case CC_DATA: case CC_DATA_LSB:
        if (c->cc[CC_RPN_MSB] == 0) {
            int msb = c->cc[CC_DATA], lsb = c->cc[CC_DATA_LSB];
            switch (c->cc[CC_RPN_LSB]) {
            case 0: c->bend_range = msb * 128 + lsb * 128 / 100; update_chan(ch, U_PITCH); break;
            case 1: c->fine = ((msb << 7) | lsb) - 8192; update_chan(ch, U_PITCH); break;
            case 2: c->coarse = (msb - 64) << 7; update_chan(ch, U_PITCH); break;
            }
        }
        if (cc == CC_DATA) c->cc[CC_DATA_LSB] = 0;
        break;
    }
}

static int gs_part(int x) { x &= 0x0f; return x == 0 ? 9 : x <= 9 ? x - 1 : x; }
static void sysex(const uint8_t* m, int n)
{
    /* universal: GM on, master volume */
    if (n >= 6 && m[1] == 0x7E && m[3] == 0x09 && m[4] == 0x01) { awem_reset(); return; }
    if (n >= 8 && m[1] == 0x7F && m[3] == 0x04 && m[4] == 0x01) {
        S.master = m[6] & 127;
        { int i; for (i = 0; i < 16; i++) update_chan(i, U_VOL); }
        return;
    }
    /* Roland GS: 41 dev 42 12 addr(3) data */
    if (n >= 10 && m[1] == 0x41 && m[3] == 0x42 && m[4] == 0x12) {
        int a1 = m[5], a2 = m[6], a3 = m[7], d = m[8];
        if (a1 == 0x40 && a2 == 0x00 && a3 == 0x7F) { awem_reset(); return; }
        if (a1 == 0x40 && a2 == 0x00 && a3 == 0x04) { S.master = d & 127; { int i; for (i = 0; i < 16; i++) update_chan(i, U_VOL); } return; }
        if (S.mode != AWEM_GS) return;
        if (a1 == 0x40 && a2 == 0x01 && a3 == 0x30 && d < 8) { S.gs_reverb = d; apply_fx(); return; }
        if (a1 == 0x40 && a2 == 0x01 && a3 == 0x38 && d < 8) { S.gs_chorus = d; apply_fx(); return; }
        if (a1 == 0x40 && (a2 & 0xF0) == 0x10 && a3 == 0x15) { S.ch[gs_part(a2)].drum = d != 0; return; }
    }
}

static void message(const uint8_t* m, int n)
{
    int st = m[0] & 0xF0, ch = m[0] & 0x0F;
    switch (st) {
    case 0x80: note_off(ch, m[1]); break;
    case 0x90: if (m[2]) note_on(ch, m[1], m[2]); else note_off(ch, m[1]); break;
    case 0xB0: control(ch, m[1] & 127, m[2] & 127); break;
    case 0xC0: S.ch[ch].prog = m[1] & 127; break;
    case 0xD0: S.ch[ch].pressure = m[1] & 127; update_chan(ch, U_MOD); break;
    case 0xE0: S.ch[ch].bend = ((m[2] << 7) | m[1]) - 8192; update_chan(ch, U_PITCH); break;
    }
    (void)n;
}

void awem_byte(uint8_t b)
{
    S.stats.bytes++;
    if (b >= 0xF8) { if (b == 0xFF) awem_reset(); return; }   /* realtime; FF = reset */
    if (b == 0xF0) { S.in_sysex = 1; S.sxn = 0; S.sx[S.sxn++] = b; S.st = 0; return; }
    if (S.in_sysex) {
        if (b == 0xF7) { S.in_sysex = 0; if (S.sxn < (int)sizeof S.sx) S.sx[S.sxn++] = b; sysex(S.sx, S.sxn); return; }
        if (b & 0x80) S.in_sysex = 0;          /* a status byte ends it */
        else { if (S.sxn < (int)sizeof S.sx) S.sx[S.sxn++] = b; return; }
    }
    if (b & 0x80) {
        if (b >= 0xF0) { S.st = 0; return; }   /* system common: ignored, clears running status */
        S.st = b; S.nbuf = 0;
        S.rs_need = (b & 0xE0) == 0xC0 ? 1 : 2;
        return;
    }
    if (!S.st) return;
    S.buf[S.nbuf++] = b;
    if (S.nbuf == S.rs_need) {
        uint8_t m[3];
        m[0] = S.st; m[1] = S.buf[0]; m[2] = S.nbuf > 1 ? S.buf[1] : 0;
        message(m, S.nbuf + 1);
        S.nbuf = 0;
    }
}

void awem_advance(int frames)
{
    int i;
    S.now += (uint32_t)frames;
    for (i = 0; i < AWEM_VOICES; i++) {
        Voice* vp = &S.v[i];
        if (vp->state == ST_PENDING && S.now - vp->ontime >= 64) {
            if (vp->chan >= 0 && S.ch[vp->chan].cc[CC_SUSTAIN] >= 64 && !S.ch[vp->chan].drum) vp->state = ST_SUSTAINED;
            else { vp->state = ST_RELEASED; if (!S.released) release_voice(vp); }
        }
    }
}

static void apply_fx(void)
{
    int r = S.gs_reverb >= 0 ? S.gs_reverb : S.reverb, c = S.gs_chorus >= 0 ? S.gs_chorus : S.chorus, i;
    if (S.released) return;
    for (i = 0; i < 28; i++) W(init_reg[reverb_cmd[i][0]], reverb_cmd[i][1], reverb_parm[r & 7][i]);
    W(EMU8K_INIT3, 0x09, chorus_parm[c & 7][0]);
    W(EMU8K_INIT3, 0x0c, chorus_parm[c & 7][1]);
    W(EMU8K_INIT4, 0x03, chorus_parm[c & 7][2]);
    hwcf(9, chorus_parm2[c & 7][0]);
    hwcf(10, chorus_parm2[c & 7][1]);
    hwcf(13, 0x8000);
    hwcf(14, 0x0000);
}
void awem_set_fx(int reverb, int chorus)
{
    S.reverb = reverb & 7; S.chorus = chorus & 7;
    apply_fx();
}

/* RAM layout: the GM/GS/MT-32 banks first, then user banks 1-127, then single presets; one that does not fit
   any more (the RAM size went down) is left out (loaded = 0) */
static void layout(void)
{
    uint32_t pos = 0;
    int i;
#define PLACE(b) do { if (!(b)->ram_words) (b)->loaded = 1; \
        else if (!U.ram_set || pos + (b)->ram_words <= U.ram_size) { (b)->ram_base = pos; pos += (b)->ram_words; (b)->loaded = 1; } \
        else (b)->loaded = 0; } while (0)
    for (i = 0; i < S.nbanks; i++) { int j, dup = 0; for (j = 0; j < i; j++) dup |= S.banks[j] == S.banks[i]; if (!dup) PLACE(S.banks[i]); }
    for (i = 1; i < 128; i++) if (U.u[i]) PLACE(U.u[i]);
    for (i = 0; i < U.novr; i++) PLACE(U.ovr[i].b);
#undef PLACE
    U.ram_used = pos;
}
static void upload_bank(const AwemBank* b)
{
    uint32_t k;
    if (!b->ram || !b->loaded) return;
    for (k = 0; k < b->ram_words; k++) emu8k_ram_write(EMU8K_RAM_START + b->ram_base + k, b->ram[k]);
}
static void upload_ram(void)
{
    int i;
    layout();
    for (i = 0; i < S.nbanks; i++) upload_bank(S.banks[i]);
    for (i = 1; i < 128; i++) if (U.u[i]) upload_bank(U.u[i]);
    for (i = 0; i < U.novr; i++) upload_bank(U.ovr[i].b);
}

static void chip_start(void)
{
    int ch;
    hwcf(29, 0x0059); hwcf(30, 0x0020); hwcf(31, 0);
    for (ch = 0; ch < 32; ch++) W(EMU8K_DCYSUSV, ch, 0x80);
    for (ch = 0; ch < 32; ch++) {
        W(EMU8K_ENVVOL, ch, 0); W(EMU8K_ENVVAL, ch, 0); W(EMU8K_DCYSUS, ch, 0); W(EMU8K_ATKHLDV, ch, 0);
        W(EMU8K_LFO1VAL, ch, 0); W(EMU8K_ATKHLD, ch, 0); W(EMU8K_LFO2VAL, ch, 0); W(EMU8K_IP, ch, 0);
        W(EMU8K_IFATN, ch, 0); W(EMU8K_PEFE, ch, 0); W(EMU8K_FMMOD, ch, 0); W(EMU8K_TREMFRQ, ch, 0);
        W(EMU8K_FM2FRQ2, ch, 0); W(EMU8K_PTRX, ch, 0); W(EMU8K_VTFT, ch, 0); W(EMU8K_PSST, ch, 0);
        W(EMU8K_CSL, ch, 0); W(EMU8K_CCCA, ch, 0);
    }
    for (ch = 0; ch < 32; ch++) { W(EMU8K_CPF, ch, 0); W(EMU8K_CVCF, ch, 0); }
    hwcf(20, 0); hwcf(21, 0); hwcf(22, 0); hwcf(23, 0);
    send_array(init1);
    send_array(init2);
    send_array(init3);
    hwcf(9, 0); hwcf(10, 0x83); hwcf(13, 0x8000);
    send_array(init4);
    for (ch = 0; ch < 32; ch++) W(EMU8K_DCYSUSV, ch, 0x807F);
    hwcf(31, 4);
    upload_ram();
    apply_fx();
}

static void init_state(void);
void awem_start(void)
{
    int i;
    init_state();
    S.released = 0;
    S.started = 1;
    for (i = 0; i < AWEM_VOICES; i++) S.v[i].state = ST_OFF;
    chip_start();
}

void awem_set_banks(AwemBank* const* list, int n, int mode)
{
    int i;
    init_state();
    all_off(-1, 1);
    if (n > 4) n = 4;
    for (i = 0; i < n; i++) S.banks[i] = list[i];
    S.nbanks = n;
    S.mode = mode;
    if (!S.released && S.started) upload_ram(); else layout();
    for (i = 0; i < 16; i++) {
        S.ch[i].bend_range = mode == AWEM_MT32 ? 12 * 128 : 256;
    }
}

void awem_release(void)
{
    int i;
    if (S.released) return;
    for (i = 0; i < AWEM_VOICES; i++) if (S.v[i].state != ST_OFF) W(EMU8K_DCYSUSV, i, 0x807F);
    for (i = 0; i < AWEM_VOICES; i++) S.v[i].state = ST_OFF;
    S.released = 1;
}
int awem_released(void) { return S.released; }
void awem_reclaim(void)
{
    if (!S.released) return;
    awem_start();
}

void awem_stats(AwemStats* s)
{
    int i;
    *s = S.stats;
    s->active = 0;
    for (i = 0; i < AWEM_VOICES; i++)
        s->active += S.v[i].state != ST_OFF && !S.released && ((R(EMU8K_CVCF, i) >> 16) & 0xffff) != 0;
}

/* static init: channels in a sane state before the first awem_reset() */
static int init_done;
static void init_state(void)
{
    int i;
    if (init_done) return;
    init_done = 1;
    S.reverb = 4; S.chorus = 2; S.gs_reverb = S.gs_chorus = -1; S.master = 127;
    for (i = 0; i < 16; i++) reset_chan(i);
}

/* ======================================================================================================== */
/* bank manager: user banks 1-127, single presets, sample RAM (host-neutral: Windows helper and DOS SBEMU)  */
/* ======================================================================================================== */
const char* awem_bank_preset(const AwemBank* b, int i, int* bank, int* prog)
{
    if (!b || i < 0 || i >= b->npresets) return NULL;
    if (bank) *bank = b->p[i].bank;
    if (prog) *prog = b->p[i].prog;
    return b->p[i].name;
}
int awem_bank_loaded(const AwemBank* b) { return b && b->loaded; }

static void relocate(void)          /* voices off, new layout, RAM written again (not while a DOS box has the chip) */
{
    all_off(-1, 1);
    if (!S.released && S.started) upload_ram(); else layout();   /* not started yet: awem_start() uploads */
}
void awem_ram_set(uint32_t words) { U.ram_size = words; U.ram_set = 1; relocate(); }
uint32_t awem_ram_total(void) { return U.ram_size; }
uint32_t awem_ram_used(void) { layout(); return U.ram_used; }
uint32_t awem_ram_free(void) { uint32_t u = awem_ram_used(); return U.ram_size > u ? U.ram_size - u : 0; }

/* RAM the layout would need with `b` in place of `old` (old may be NULL) */
static int fits(const AwemBank* b, const AwemBank* old, char* err, int errlen)
{
    uint32_t used, need = b->ram_words, have;
    layout();
    used = U.ram_used - (old && old->loaded ? old->ram_words : 0);
    have = U.ram_size > used ? U.ram_size - used : 0;
    if (!U.ram_set || need <= have) return 1;
    if (err && errlen > 0)
        snprintf(err, (size_t)errlen, "%s needs %lu KB of sample RAM, %lu KB free of %lu KB (/AWERAM)", b->name,
                 (unsigned long)((need + 511) / 512), (unsigned long)(have / 512), (unsigned long)(U.ram_size / 512));
    return 0;
}
static void set_err(char* err, int errlen, const char* m) { if (err && errlen > 0) { strncpy(err, m, (size_t)errlen - 1); err[errlen - 1] = 0; } }
static AwemBank* load_full(const char* path, char* err, int errlen)
{
    AwemBank* b = awem_bank_load(path, 0xFFFFFFFFu, err, errlen);   /* no RAM cut: fits() says if there is room */
    if (b && !b->npresets) { set_err(err, errlen, "the bank has no presets"); awem_bank_free(b); b = NULL; }
    return b;
}

int awem_ubank_put(int slot, AwemBank* b, char* err, int errlen)
{
    if (slot < 1 || slot > 127 || !b) { set_err(err, errlen, "user banks are 1-127"); return -1; }
    if (!fits(b, U.u[slot], err, errlen)) return -1;
    awem_bank_free(U.u[slot]);
    U.u[slot] = b;
    relocate();
    return 0;
}
int awem_ubank_load(int slot, const char* path, char* err, int errlen)
{
    AwemBank* b;
    if (slot < 1 || slot > 127) { set_err(err, errlen, "user banks are 1-127"); return -1; }
    if (!(b = load_full(path, err, errlen))) return -1;
    if (awem_ubank_put(slot, b, err, errlen)) { awem_bank_free(b); return -1; }
    return 0;
}
void awem_ubank_remove(int slot)
{
    if (slot < 1 || slot > 127 || !U.u[slot]) return;
    all_off(-1, 1);
    awem_bank_free(U.u[slot]);
    U.u[slot] = NULL;
    relocate();
}
const AwemBank* awem_ubank(int slot) { return slot >= 1 && slot <= 127 ? U.u[slot] : NULL; }

/* a bank holding only preset i of src, with only the RAM words its zones use */
static AwemBank* extract(const AwemBank* src, int i)
{
    const Preset* sp = &src->p[i];
    AwemBank* b = (AwemBank*)calloc(1, sizeof *b);
    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    int j;
    if (!b) return NULL;
    snprintf(b->name, sizeof b->name, "%s", sp->name);
    b->p = (Preset*)calloc(1, sizeof(Preset));
    if (!b->p) { free(b); return NULL; }
    b->npresets = 1;
    b->p[0] = *sp;
    b->p[0].owner = b;
    b->p[0].z = (Zone*)malloc((size_t)(sp->nz ? sp->nz : 1) * sizeof(Zone));
    if (!b->p[0].z) { awem_bank_free(b); return NULL; }
    memcpy(b->p[0].z, sp->z, (size_t)sp->nz * sizeof(Zone));
    for (j = 0; j < sp->nz; j++) {
        const Zone* z = &sp->z[j];
        uint32_t e;
        if (!z->ram) continue;
        if (z->start < lo) lo = z->start;
        if (z->loopstart < lo) lo = z->loopstart;
        e = z->end; if (z->loopend > e) e = z->loopend;
        e += 48;
        if (e > hi) hi = e;
    }
    if (hi > src->ram_words) hi = src->ram_words;
    if (lo < hi && src->ram) {
        b->ram_words = hi - lo;
        b->ram = (uint16_t*)malloc(b->ram_words * 2);
        if (!b->ram) { awem_bank_free(b); return NULL; }
        memcpy(b->ram, src->ram + lo, b->ram_words * 2);
        for (j = 0; j < sp->nz; j++) {
            Zone* z = &b->p[0].z[j];
            if (!z->ram) continue;
            z->start -= lo; z->end -= lo; z->loopstart -= lo; z->loopend -= lo;
        }
    }
    b->loaded = 1;
    return b;
}
static int ovr_find(int slot, int prog) { int i; for (i = 0; i < U.novr; i++) if (U.ovr[i].slot == slot && U.ovr[i].prog == prog) return i; return -1; }

int awem_preset_put(int slot, int prog, const AwemBank* src, int src_bank, int src_prog, char* err, int errlen)
{
    int i, k;
    AwemBank* b;
    if (slot < 0 || slot > 128 || prog < 0 || prog > 127 || !src) { set_err(err, errlen, "bank 0-127 (128 = drum kits), program 0-127"); return -1; }
    for (i = 0; i < src->npresets; i++)
        if ((src_bank < 0 || src->p[i].bank == src_bank) && (src_prog < 0 || src->p[i].prog == src_prog) && src->p[i].nz) break;
    if (i == src->npresets) { set_err(err, errlen, "that preset is not in the file"); return -1; }
    if (!(b = extract(src, i))) { set_err(err, errlen, "out of memory"); return -1; }
    k = ovr_find(slot, prog);
    if (k < 0 && U.novr == AWEM_MAXOVR) { set_err(err, errlen, "too many single presets"); awem_bank_free(b); return -1; }
    if (!fits(b, k >= 0 ? U.ovr[k].b : NULL, err, errlen)) { awem_bank_free(b); return -1; }
    if (k >= 0) awem_bank_free(U.ovr[k].b);
    else { k = U.novr++; U.ovr[k].slot = slot; U.ovr[k].prog = prog; }
    U.ovr[k].b = b;
    relocate();
    return 0;
}
int awem_preset_load(int slot, int prog, const char* path, int src_bank, int src_prog, char* err, int errlen)
{
    AwemBank* src = load_full(path, err, errlen);
    int rc;
    if (!src) return -1;
    rc = awem_preset_put(slot, prog, src, src_bank, src_prog, err, errlen);
    awem_bank_free(src);
    return rc;
}
void awem_preset_remove(int slot, int prog)
{
    int k = ovr_find(slot, prog);
    if (k < 0) return;
    all_off(-1, 1);
    awem_bank_free(U.ovr[k].b);
    U.ovr[k] = U.ovr[--U.novr];
    relocate();
}
int awem_preset_count(void) { return U.novr; }
const AwemBank* awem_preset_get(int i, int* slot, int* prog)
{
    if (i < 0 || i >= U.novr) return NULL;
    if (slot) *slot = U.ovr[i].slot;
    if (prog) *prog = U.ovr[i].prog;
    return U.ovr[i].b;
}
void awem_user_clear(void)
{
    int i;
    all_off(-1, 1);
    for (i = 1; i < 128; i++) { awem_bank_free(U.u[i]); U.u[i] = NULL; }
    for (i = 0; i < U.novr; i++) awem_bank_free(U.ovr[i].b);
    U.novr = 0;
    relocate();
}
