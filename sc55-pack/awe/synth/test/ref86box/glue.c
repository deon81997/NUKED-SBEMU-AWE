/* glue.c - minimal 86Box environment so snd_emu8k.c runs inside the harness. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <86box/86box.h>
#include <86box/sound.h>
#include <86box/snd_emu8k.h>

int wavetable_pos_global;
static const char *rom_path;
static emu8k_t *ref;

void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(2);
}
void pclog_ex(const char *fmt, va_list ap) { (void) fmt; (void) ap; }
void pclog(const char *fmt, ...) { (void) fmt; }
FILE *rom_fopen(const char *fn, const char *mode)
{
    (void) fn;
    return rom_path ? fopen(rom_path, mode) : NULL;
}

/* backend API used by harness.c */
int be_init(int ram_kb, const char *rom)
{
    static const char zero_rom[] = "/tmp/emu8k_zero_rom.bin";
    if (!rom) {
        FILE *f = fopen(zero_rom, "wb");
        static char z[1 << 20];
        if (!f || fwrite(z, 1, sizeof(z), f) != sizeof(z))
            return -1;
        fclose(f);
        rom = zero_rom;
    }
    rom_path = rom;
    ref = calloc(1, sizeof(*ref));
    emu8k_init(ref, 0x620, ram_kb);
    return 0;
}
void be_io_w(int off, uint16_t v) { emu8k_outw((uint16_t) (0x620 + off), v, ref); }
uint16_t be_io_r(int off) { return emu8k_inw((uint16_t) (0x620 + off), ref); }
void be_render(int16_t *out, int frames)
{
    while (frames > 0) {
        int n = frames > WTBUFLEN ? WTBUFLEN : frames, i;
        wavetable_pos_global = n;
        emu8k_update(ref);
        for (i = 0; i < 2 * n; i++) {
            int32_t s = ref->buffer[i];
            out[i] = (int16_t) (s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
        emu8k_reset_buffer(ref);
        wavetable_pos_global = 0;
        out += 2 * n;
        frames -= n;
    }
}
