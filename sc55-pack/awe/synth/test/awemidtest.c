/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
/* awemidtest.c - unit test of awemidi.c on emu8k.c: built-in map (every program and drum key sounds), voice stealing
 * (30 voices, no lost note-on), a DOS box taking the chip and giving it back, GS/GM SysEx, bend range, bank probing.
 * awemidtest ROM [GM.SBK GS.SBK MT.SBK]  - the SBKs from testrig/awe/sf2sbk.py (neutral names: kind from content).
 * Last line PASS or FAIL. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emu8k.h"
#include "awemidi.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static int16_t buf[44100 * 2 * 4];
static void msg(int a, int b, int c) { awem_byte((uint8_t)a); awem_byte((uint8_t)b); if ((a & 0xE0) != 0xC0) awem_byte((uint8_t)c); }
static void sx(const uint8_t* m, int n) { int i; for (i = 0; i < n; i++) awem_byte(m[i]); }
/* render ms milliseconds, rms of the second half (whole: all of it) */
static int whole;
static double run(int ms)
{
    int n = 44100 * ms / 1000, done = 0, i;
    double e = 0; int cnt = 0;
    while (done < n) {
        int k = n - done > 1024 ? 1024 : n - done;
        emu8k_render(buf, k, 44100); awem_advance(k);
        if (whole || done >= n / 2) for (i = 0; i < k * 2; i++) { e += (double)buf[i] * buf[i]; cnt++; }
        done += k;
    }
    return cnt ? sqrt(e / cnt) : 0;
}
/* zero crossings per second of the left channel over ms (rough pitch) */
static double zc(int ms)
{
    int n = 44100 * ms / 1000, i, c = 0;
    emu8k_render(buf, n, 44100); awem_advance(n);
    for (i = 2; i < n * 2; i += 2) if ((buf[i - 2] < 0) != (buf[i] < 0)) c++;
    return c * 1000.0 / ms / 2;
}
int main(int argc, char** argv)
{
    AwemBank* b;
    AwemStats st;
    int p, k, silent = 0;
    if (argc < 2) { printf("usage: awemidtest ROM [GM.SBK GS.SBK MT.SBK]\n"); return 2; }
    if (emu8k_init(8192) || emu8k_load_rom(argv[1])) { printf("FAIL: engine/ROM\n"); return 1; }
    emu8k_set_fx(0);
    b = awem_bank_builtin();
    awem_start(); awem_set_banks(&b, 1, AWEM_GM); awem_set_fx(4, 2); awem_reset();
    /* A: every program and drum key makes a sound */
    whole = 1;
    for (p = 0; p < 128; p++) {
        double r;
        msg(0xC0, p, 0); msg(0x90, 60, 100); r = run(300); msg(0x80, 60, 0); msg(0xB0, 120, 0); run(20);
        if (r < 20) { msg(0x90, 60, 100); r = run(1200); msg(0x80, 60, 0); msg(0xB0, 120, 0); run(20); }   /* slow pads */
        if (r < 20) { printf("  program %d silent (rms %.1f)\n", p, r); silent++; }
    }
    for (k = 27; k <= 87; k++) {
        double r;
        msg(0x99, k, 100); r = run(120); msg(0x89, k, 0); msg(0xB9, 120, 0); run(20);
        if (r < 20) { printf("  drum key %d silent (rms %.1f)\n", k, r); silent++; }
    }
    whole = 0;
    awem_stats(&st);
    CHECK(silent == 0 && st.missing == 0, "built-in map: %d silent, %u without a zone", silent, st.missing);
    printf("built-in map: 128 programs + drum keys 27-87 sound, %u notes\n", st.notes);
    /* B: 40 held organ notes on 30 voices: 10 stolen, nothing lost, all quiet after notes off */
    {
        uint32_t n0 = st.notes, s0 = st.stolen;
        msg(0xC0, 16, 0);
        for (k = 0; k < 40; k++) msg(0x90, 40 + k, 90);
        run(200);
        awem_stats(&st);
        CHECK(st.notes - n0 == 40 && st.voices_max == 30 && st.stolen - s0 == 10 && st.active == 30,
              "stealing: notes %u, voices max %u, stolen %u, active %u", st.notes - n0, st.voices_max, st.stolen - s0, st.active);
        printf("voice stealing: 40 held notes -> 30 voices, %u stolen, %u playing\n", st.stolen - s0, st.active);
        for (k = 0; k < 40; k++) msg(0x80, 40 + k, 0);
        run(400);
        { double r = run(1500); awem_stats(&st); CHECK(r < 2 && st.active == 0, "after notes off: rms %.1f, %u active", r, st.active); }
    }
    /* C: a DOS box takes the chip: silent at once, notes counted but not played, state kept; back = plays again */
    {
        double r1, r2, r3;
        msg(0xC0, 0, 0); msg(0xB0, 7, 100); msg(0x90, 60, 100); r1 = run(200);
        awem_release();
        r2 = run(200);
        msg(0xC0, 73, 0);             /* program change while the DOS box has it */
        msg(0x90, 64, 100);
        awem_stats(&st);
        CHECK(awem_released() && r1 > 50 && r2 < 2 && st.released_notes == 1, "release: before %.1f, after %.1f, notes held back %u",
              r1, r2, st.released_notes);
        awem_reclaim();
        msg(0x90, 72, 100); r3 = run(300);
        CHECK(!awem_released() && r3 > 50, "reclaim: %.1f", r3);
        msg(0xB0, 123, 0); run(1500);
        printf("DOS box takes the EMU8000: Windows voices off (rms %.0f -> %.1f), back after: rms %.0f\n", r1, r2, r3);
    }
    /* D: SysEx: GS master volume 0 = silence, GM on = reset (volume back) */
    {
        static const uint8_t gs_mvol0[] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x04, 0x00, 0x3C, 0xF7};
        static const uint8_t gm_on[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
        double r1, r2;
        sx(gs_mvol0, sizeof gs_mvol0);
        msg(0x90, 60, 100); r1 = run(200); msg(0x80, 60, 0); run(800);
        sx(gm_on, sizeof gm_on);
        msg(0x90, 60, 100); r2 = run(200); msg(0x80, 60, 0); run(1500);
        CHECK(r1 < 2 && r2 > 50, "SysEx: master volume 0 -> %.1f, GM on -> %.1f", r1, r2);
        printf("SysEx: GS master volume 0 -> rms %.1f, GM on -> %.0f\n", r1, r2);
    }
    /* E: bend range 12 (RPN 0): full bend up = one octave */
    {
        double f0, f1;
        msg(0xC0, 73, 0); msg(0x90, 60, 100); run(200); f0 = zc(400);
        msg(0xB0, 101, 0); msg(0xB0, 100, 0); msg(0xB0, 6, 12); msg(0xB0, 38, 0);
        msg(0xE0, 0x7F, 0x7F); run(100); f1 = zc(400);
        msg(0x80, 60, 0); msg(0xE0, 0, 0x40); run(1000);
        CHECK(f1 / f0 > 1.9 && f1 / f0 < 2.1, "bend range 12: %.1f -> %.1f Hz", f0, f1);
        printf("bend range 12 semitones: %.1f -> %.1f Hz (x%.2f)\n", f0, f1, f1 / f0);
    }
    /* F: SBK banks: kind from the content (neutral names), RAM sample banks, too little RAM = zones left out */
    if (argc >= 5) {
        int want[3] = {AWEM_GM, AWEM_GS, AWEM_MT32}, i;
        for (i = 0; i < 3; i++) {
            int kind = -9; char nm[64]; char err[128];
            int rc = awem_probe(argv[2 + i], &kind, nm, sizeof nm);
            AwemBank* sb = awem_bank_load(argv[2 + i], 8192u * 512u, err, sizeof err);
            AwemBank* small = awem_bank_load(argv[2 + i], 256u * 512u, err, sizeof err);
            CHECK(rc == 2 && kind == want[i] && sb && awem_bank_skipped(sb) == 0 && small && awem_bank_skipped(small) > 0,
                  "%s: probe %d kind %d, skipped %d / %d", argv[2 + i], rc, kind, sb ? awem_bank_skipped(sb) : -1,
                  small ? awem_bank_skipped(small) : -1);
            printf("%s: SoundFont 1 (RAM samples), kind %s from the presets, %d presets, %u KB RAM; in 128 KB: %d zones left out\n",
                   argv[2 + i], kind == 0 ? "GM" : kind == 1 ? "GS" : kind == 2 ? "MT-32" : "?", sb ? awem_bank_presets(sb) : 0,
                   sb ? awem_bank_ram_used(sb) / 512 : 0, small ? awem_bank_skipped(small) : -1);
            awem_bank_free(small);
            if (i == 1 && sb) {   /* GS: bank select 8 finds the variation, unknown bank 5 falls back to bank 0 */
                AwemBank* l[2]; double r1, r2;
                l[0] = sb; l[1] = b;
                awem_set_banks(l, 2, AWEM_GS);
                msg(0xB0, 0, 8); msg(0xC0, 0, 0); msg(0x90, 60, 100); r1 = run(200); msg(0x80, 60, 0); run(500);
                msg(0xB0, 0, 5); msg(0xC0, 0, 0); msg(0x90, 60, 100); r2 = run(200); msg(0x80, 60, 0); run(500);
                awem_stats(&st);
                CHECK(r1 > 50 && r2 > 50, "GS banks: variation %.1f, fallback %.1f", r1, r2);
                awem_set_banks(&b, 1, AWEM_GM);
            }
            awem_bank_free(sb);
        }
    }
    awem_stats(&st);
    printf("total: %u bytes, %u notes, %u stolen, %u without a sound\n", st.bytes, st.notes, st.stolen, st.missing);
    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
