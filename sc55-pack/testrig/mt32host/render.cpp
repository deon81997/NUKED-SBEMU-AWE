// Host MT-32 render: the same Munt calls as sbemu/vmpu_mt32.cpp (DIGITAL_ONLY, Bit16s, native 32 kHz,
// DefaultMidiStreamParser). Input = event file from smf2ev.py: records [u32 frame][u16 len][bytes].
// MT32F=1: float renderer (as Win98 SC55W helper/synths.cpp), written as s16.
// Usage: render CTRL.ROM PCM.ROM EVENTS OUT.RAW [extra_seconds]   (OUT = s16le stereo 32000 Hz)
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "mt32emu.h"
using namespace MT32Emu;

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: render CTRL PCM EVENTS OUT [tail_s]\n"); return 2; }
    FileStream cf, pf;
    if (!cf.open(argv[1]) || !pf.open(argv[2])) { fprintf(stderr, "rom open failed\n"); return 1; }
    const ROMImage *c = ROMImage::makeROMImage(&cf), *p = ROMImage::makeROMImage(&pf);
    Synth s;
    if (!s.open(*c, *p, DEFAULT_MAX_PARTIALS, AnalogOutputMode_DIGITAL_ONLY)) { fprintf(stderr, "synth open failed\n"); return 1; }
    DefaultMidiStreamParser parser(s, 32768);
    FILE *ev = fopen(argv[3], "rb"), *out = fopen(argv[4], "wb");
    if (!ev || !out) return 1;
    std::vector<Bit16s> buf; std::vector<float> fb;
    bool fl = getenv("MT32F") && *getenv("MT32F") == '1';
#define RENDER(n) do { buf.resize((n) * 2); if (fl) { fb.resize((n) * 2); s.render(&fb[0], n); \
        for (unsigned k = 0; k < (n) * 2; k++) { float v = fb[k] * 32768.0f; buf[k] = (Bit16s)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); } } \
        else s.render(&buf[0], n); } while (0)
    unsigned long now = 0;
    unsigned char hdr[6], data[65536];
    while (fread(hdr, 1, 6, ev) == 6) {
        unsigned long fr = hdr[0] | hdr[1] << 8 | hdr[2] << 16 | (unsigned long)hdr[3] << 24;
        unsigned len = hdr[4] | hdr[5] << 8;
        if (fread(data, 1, len, ev) != len) break;
        while (now < fr) {                       // render in <= 256-frame chunks like the ISR does
            unsigned n = fr - now > 256 ? 256 : (unsigned)(fr - now);
            RENDER(n); fwrite(&buf[0], 2, n * 2, out); now += n;
        }
        parser.parseStream(data, len);
    }
    unsigned long end = now + (unsigned long)((argc > 5 ? atof(argv[5]) : 2.0) * 32000);
    while (now < end) { unsigned n = 256; RENDER(n); fwrite(&buf[0], 2, n * 2, out); now += n; }
    fclose(out); s.close();
    ROMImage::freeROMImage(c); ROMImage::freeROMImage(p);
    printf("frames=%lu\n", now);
    return 0;
}
