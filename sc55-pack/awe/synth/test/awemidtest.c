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
/* sample start of the last voice started */
static uint32_t voice_addr(void) { AwemStats t; awem_stats(&t); return t.last_addr; }
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
    if (argc < 2) { printf("usage: awemidtest ROM [GM.SBK GS.SBK MT.SBK [SF2 SMALL.SBK]]\n"); return 2; }
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
    /* G: bank manager: SF2 + SBK as user banks 1/2, bank select CC0/CC32, RAM full, replace, single presets,
       drum kit, RAM size cut, DOS box clobbering the RAM (argv[5] = SF2, argv[6] = small SBK) */
    if (argc >= 7) {
        char err[160] = "";
        uint32_t u1, u2, used;
        int rc, p2 = 0;
        awem_ram_set(8192u * 512u);
        CHECK(awem_ram_used() == 0 && awem_ram_free() == 8192u * 512u, "RAM empty at start: used %u", awem_ram_used());
        rc = awem_ubank_load(1, argv[5], err, sizeof err);
        CHECK(rc == 0 && awem_ubank(1), "bank 1 SF2: %s", err);
        rc = awem_ubank_load(2, argv[6], err, sizeof err);
        CHECK(rc == 0 && awem_ubank(2), "bank 2 SBK: %s", err);
        u1 = awem_bank_ram_used(awem_ubank(1)); u2 = awem_bank_ram_used(awem_ubank(2));
        awem_bank_preset(awem_ubank(2), 0, NULL, &p2);   /* the small bank has 12 programs: test with its first */
        CHECK(awem_ram_used() == u1 + u2 && u1 > 0 && u2 > 0, "RAM used %u != %u + %u", awem_ram_used(), u1, u2);
        printf("bank manager: bank 1 = %s (%u KB), bank 2 = %s (%u KB), %u KB used, %u KB free\n", awem_bank_name(awem_ubank(1)),
               u1 / 512, awem_bank_name(awem_ubank(2)), u2 / 512, awem_ram_used() / 512, awem_ram_free() / 512);
        {   /* bank select: CC0 1 -> bank 1's RAM, CC0 2 -> bank 2's, CC0 0 + CC32 2 -> bank 2, 0/0 -> ROM, 5 (empty) -> ROM */
            static const struct { int msb, lsb; uint32_t lo, hi; const char* what; } t[] = {
                {1, 0, 0, 1, "CC0 1"}, {2, 0, 1, 2, "CC0 2"}, {0, 2, 1, 2, "CC32 2"}, {0, 0, 9, 9, "bank 0"}, {5, 0, 9, 9, "empty bank 5"} };
            int i;
            for (i = 0; i < 5; i++) {
                uint32_t lo = t[i].lo == 9 ? 0 : EMU8K_RAM_START + (t[i].lo ? u1 : 0);
                uint32_t hi = t[i].lo == 9 ? EMU8K_RAM_START : EMU8K_RAM_START + (t[i].hi == 1 ? u1 : u1 + u2);
                uint32_t addr;
                double r;
                msg(0xB0, 0, t[i].msb); msg(0xB0, 32, t[i].lsb); msg(0xC0, p2, 0); msg(0x90, 60, 100); r = run(150);
                addr = voice_addr();
                msg(0x80, 60, 0); msg(0xB0, 120, 0); run(30);
                CHECK(r > 50 && addr >= lo && addr < hi, "%s: rms %.1f, playing at %06X (want %06X-%06X)", t[i].what, r, addr, lo, hi);
                printf("bank select %s: plays at %06X (rms %.0f)\n", t[i].what, addr, r);
            }
        }
        {   /* drum channel, CC0 1: bank 1's drum kit (SF2 preset bank 128) */
            uint32_t addr; double r;
            msg(0xB9, 0, 1); msg(0xC9, 0, 0); msg(0x99, 38, 100); r = run(100); addr = voice_addr();
            msg(0x89, 38, 0); msg(0xB9, 120, 0); msg(0xB9, 0, 0); run(30);
            CHECK(r > 20 && addr >= EMU8K_RAM_START && addr < EMU8K_RAM_START + u1, "drum kit bank 1: rms %.1f at %06X", r, addr);
            printf("drum channel CC0 1: bank 1's kit at %06X\n", addr);
        }
        /* RAM full: the whole stand-in GM SBK (5.6 MB) does not fit beside them - refused, nothing changes */
        used = awem_ram_used();
        rc = awem_ubank_load(3, argv[2], err, sizeof err);
        CHECK(rc != 0 && !awem_ubank(3) && awem_ram_used() == used && strstr(err, "needs") && strstr(err, "free"),
              "RAM full: rc %d, used %u -> %u, '%s'", rc, used, awem_ram_used(), err);
        printf("RAM full: \"%s\"\n", err);
        /* replace bank 1 (SF2) with the small SBK: room now for the big one as bank 3 */
        rc = awem_ubank_load(1, argv[6], err, sizeof err);
        CHECK(rc == 0 && awem_ram_used() == 2 * u2, "replace bank 1: rc %d used %u", rc, awem_ram_used());
        rc = awem_ubank_load(3, argv[2], err, sizeof err);
        CHECK(rc == 0 && awem_ubank(3), "bank 3 after the replace: %s", err);
        {
            uint32_t addr; double r;
            msg(0xB0, 0, 3); msg(0xB0, 32, 0); msg(0xC0, 0, 0); msg(0x90, 60, 100); r = run(150); addr = voice_addr();
            msg(0x80, 60, 0); msg(0xB0, 120, 0); run(30);
            CHECK(r > 50 && addr >= EMU8K_RAM_START + 2 * u2, "bank 3: rms %.1f at %06X", r, addr);
            printf("replaced bank 1, bank 3 = whole SBK (%u KB): plays at %06X, %u KB used\n",
                   awem_bank_ram_used(awem_ubank(3)) / 512, addr, awem_ram_used() / 512);
        }
        awem_ubank_remove(3);
        CHECK(!awem_ubank(3) && awem_ram_used() == 2 * u2, "remove bank 3: used %u", awem_ram_used());
        /* single preset: bank 0 program 0 replaced with the SF2's piano; only its samples take RAM */
        used = awem_ram_used();
        rc = awem_preset_load(0, 0, argv[5], 0, 0, err, sizeof err);
        CHECK(rc == 0 && awem_preset_count() == 1, "single preset: %s", err);
        {
            uint32_t addr, pu = awem_ram_used() - used; double r;
            msg(0xB0, 0, 0); msg(0xC0, 0, 0); msg(0x90, 60, 100); r = run(150); addr = voice_addr();
            msg(0x80, 60, 0); msg(0xB0, 120, 0); run(30);
            CHECK(r > 50 && addr >= EMU8K_RAM_START + used && pu > 0 && pu < u1 / 4, "single preset: rms %.1f at %06X, %u words", r, addr, pu);
            printf("single preset bank 0 / program 0 <- SF2 piano: %u KB RAM, plays at %06X\n", pu / 512, addr);
            msg(0xC0, 1, 0); msg(0x90, 60, 100); run(150); addr = voice_addr(); msg(0x80, 60, 0); msg(0xB0, 120, 0); run(30);
            CHECK(addr < EMU8K_RAM_START, "program 1 stays on the ROM: %06X", addr);
        }
        awem_preset_remove(0, 0);
        CHECK(awem_preset_count() == 0 && awem_ram_used() == used, "single preset removed: used %u", awem_ram_used());
        /* RAM size cut to 512 KB: both banks left out, bank select falls back to the ROM; back to 8 MB: back */
        awem_ram_set(512u * 512u);
        {
            uint32_t addr; double r;
            msg(0xB0, 0, 1); msg(0xC0, 0, 0); msg(0x90, 60, 100); r = run(150); addr = voice_addr();
            msg(0x80, 60, 0); msg(0xB0, 120, 0); run(30);
            CHECK(!awem_bank_loaded(awem_ubank(1)) && r > 50 && addr < EMU8K_RAM_START, "RAM 512 KB: loaded %d, rms %.1f at %06X",
                  awem_bank_loaded(awem_ubank(1)), r, addr);
        }
        awem_ram_set(8192u * 512u);
        CHECK(awem_bank_loaded(awem_ubank(1)) && awem_bank_loaded(awem_ubank(2)), "RAM 8 MB again: banks back");
        /* a DOS box takes the chip and writes its own samples over the RAM: banks come back with the chip */
        {
            uint32_t k, addr; double r;
            awem_release();
            for (k = 0; k < 2 * u2; k++) emu8k_ram_write(EMU8K_RAM_START + k, 0);
            awem_reclaim();
            msg(0xB0, 0, 2); msg(0xC0, p2, 0); msg(0x90, 60, 100); r = run(150); addr = voice_addr();
            msg(0x80, 60, 0); msg(0xB0, 120, 0); msg(0xB0, 0, 0); run(30);
            CHECK(r > 50 && addr >= EMU8K_RAM_START + u2, "after a DOS box: bank 2 rms %.1f at %06X", r, addr);
            printf("DOS box wrote over the sample RAM; banks back after: bank 2 rms %.0f\n", r);
        }
        awem_user_clear();
        CHECK(awem_ram_used() == 0 && !awem_ubank(1) && !awem_ubank(2), "clear: used %u", awem_ram_used());
        printf("bank manager: PASS checks done, cleared\n");
    }
    awem_stats(&st);
    printf("total: %u bytes, %u notes, %u stolen, %u without a sound\n", st.bytes, st.notes, st.stolen, st.missing);
    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
