/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sc55sf)
 * tsfplay SF2 MID OUT.WAV [rate] - render a standard MIDI file through sbemu/tsf.h (TinySoundFont, the /VSF engine)
 * the way vmpu.c drives it (channel 10 = bank 128, CC / program / bend / pressure), dry, 16-bit stereo. */
#define TSF_IMPLEMENTATION
#define TSF_FLOAT_LUT
#include "../../sbemu/tsf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { double t; unsigned char b[3]; int n; } Ev;
static unsigned char *d; static long dn;
static unsigned rd32(const unsigned char *p) { return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static int cmp(const void *a, const void *b) { const Ev *x = a, *y = b; return x->t < y->t ? -1 : x->t > y->t; }
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[2], "rb"); int ntr, div, t, rate = argc > 4 ? atoi(argv[4]) : 44100; long p = 14; Ev *ev = 0; int ne = 0, cap = 0;
    struct { unsigned long tick; int tempo; } *tm = 0; int ntm = 0;
    tsf *s = tsf_load_filename(argv[1]); if (!s || !f) { fprintf(stderr, "load failed\n"); return 1; }
    fseek(f, 0, SEEK_END); dn = ftell(f); fseek(f, 0, SEEK_SET); d = malloc(dn); fread(d, 1, dn, f); fclose(f);
    ntr = d[10] << 8 | d[11]; div = d[12] << 8 | d[13];
    tm = malloc(sizeof(*tm) * 10000); tm[0].tick = 0; tm[0].tempo = 500000; ntm = 1;
    for (t = 0; t < ntr && p + 8 <= dn; t++) {      /* pass 1: tempo map from all tracks, pass 2: events in ticks */
        long q = p + 8, e = q + rd32(d + p + 4); unsigned long tick = 0; unsigned char rs = 0;
        while (q < e) {
            unsigned long dt = 0; unsigned char x, st; do { x = d[q++]; dt = dt << 7 | (x & 127); } while (x & 128); tick += dt; st = d[q];
            if (st == 0xFF) { unsigned ty = d[q + 1]; unsigned long l = 0; q += 2; do { x = d[q++]; l = l << 7 | (x & 127); } while (x & 128);
                if (ty == 0x51 && ntm < 10000) { tm[ntm].tick = tick; tm[ntm].tempo = d[q] << 16 | d[q + 1] << 8 | d[q + 2]; ntm++; } q += l; if (ty == 0x2F) break; continue; }
            if (st == 0xF0 || st == 0xF7) { unsigned long l = 0; q++; do { x = d[q++]; l = l << 7 | (x & 127); } while (x & 128); q += l; continue; }
            if (st & 0x80) { rs = st; q++; }
            { int n = ((rs & 0xF0) == 0xC0 || (rs & 0xF0) == 0xD0) ? 1 : 2; if (ne == cap) { cap = cap ? cap * 2 : 4096; ev = realloc(ev, cap * sizeof(Ev)); }
              ev[ne].t = tick; ev[ne].b[0] = rs; ev[ne].b[1] = d[q]; ev[ne].b[2] = n > 1 ? d[q + 1] : 0; ev[ne].n = n + 1; ne++; q += n; }
        }
        p = e;
    }
    { int i, j; /* ticks -> seconds through the tempo map (sorted by tick) */
      for (i = 1; i < ntm; i++) for (j = i; j > 0 && tm[j].tick < tm[j - 1].tick; j--) { tm[9999] = tm[j]; tm[j] = tm[j - 1]; tm[j - 1] = tm[9999]; }
      for (i = 0; i < ne; i++) { double sec = 0; unsigned long tk = (unsigned long)ev[i].t; int k;
        for (k = 0; k < ntm; k++) { unsigned long a = tm[k].tick, b = k + 1 < ntm ? tm[k + 1].tick : ~0ul; if (tk <= a) break; sec += (double)((tk < b ? tk : b) - a) * tm[k].tempo / div / 1e6; }
        ev[i].t = sec; } }
    qsort(ev, ne, sizeof(Ev), cmp);
    tsf_set_output(s, TSF_STEREO_INTERLEAVED, rate, 0); tsf_set_max_voices(s, 64);
    for (t = 0; t < 16; t++) tsf_channel_midi_control(s, t, 121, 0);
    tsf_channel_set_bank_preset(s, 9, 128, 0);
    {
        FILE *o = fopen(argv[3], "wb"); unsigned char h[44] = {0}; long fr = 0; int i = 0; short buf[256 * 2]; double end = ne ? ev[ne - 1].t + 3 : 1;
        fwrite(h, 1, 44, o);
        while ((double)fr / rate < end) {
            while (i < ne && ev[i].t <= (double)fr / rate) {
                unsigned char *b = ev[i].b; int c = b[0] & 15;
                switch (b[0] & 0xF0) {
                case 0x80: tsf_channel_note_off(s, c, b[1]); break;
                case 0x90: if (b[2]) tsf_channel_note_on(s, c, b[1], b[2] / 127.0f); else tsf_channel_note_off(s, c, b[1]); break;
                case 0xB0: tsf_channel_midi_control(s, c, b[1], b[2]); break;
                case 0xC0: tsf_channel_set_presetnumber(s, c, b[1], c == 9); break;
                case 0xD0: tsf_channel_set_pressure(s, c, b[1] / 127.f); break;
                case 0xE0: tsf_channel_set_pitchwheel(s, c, (b[1] & 0x7f) | ((b[2] & 0x7f) << 7)); break;
                }
                i++;
            }
            tsf_render_short(s, buf, 256, 0); fwrite(buf, 4, 256, o); fr += 256;
        }
        { unsigned dsz = fr * 4, x = dsz + 36; unsigned short w; memcpy(h, "RIFF", 4); memcpy(h + 4, &x, 4); memcpy(h + 8, "WAVEfmt ", 8); x = 16; memcpy(h + 16, &x, 4);
          w = 1; memcpy(h + 20, &w, 2); w = 2; memcpy(h + 22, &w, 2); memcpy(h + 24, &rate, 4); x = rate * 4; memcpy(h + 28, &x, 4); w = 4; memcpy(h + 32, &w, 2); w = 16; memcpy(h + 34, &w, 2);
          memcpy(h + 36, "data", 4); memcpy(h + 40, &dsz, 4); fseek(o, 0, SEEK_SET); fwrite(h, 1, 44, o); fclose(o); }
    }
    return 0;
}
