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

void be_io_w(int off, uint16_t v);
/* the state emu8k.c starts in (a card after Creative's AWEUTIL /S): voices 0-29 released, 30/31 envelope off,
   reverb Hall 2, chorus Chorus 3 - so both engines start alike */
static void wr(int reg, int ch, int port, uint16_t v) { be_io_w(0x802, (uint16_t) ((reg << 5) | ch)); be_io_w(port, v); }
static void aweutil(void)
{
    static const uint16_t hall2[28] = {
        0xB488, 0xA470, 0x9570, 0x84B5, 0x383A, 0x3EB5, 0x7254, 0x7234, 0x7224, 0x7254, 0x7264, 0x7294, 0x44C3, 0x45C3,
        0xA404, 0xA504, 0x842A, 0x852A, 0x842A, 0x852A, 0x8429, 0x8529, 0x8429, 0x8529, 0x8428, 0x8528, 0x8428, 0x8528};
    static const unsigned char cmd[28][2] = {
        {1, 0x03}, {1, 0x05}, {4, 0x1F}, {1, 0x07}, {2, 0x14}, {2, 0x16}, {1, 0x0F}, {1, 0x17}, {1, 0x1F}, {2, 0x07},
        {2, 0x0F}, {2, 0x17}, {2, 0x1D}, {2, 0x1F}, {3, 0x01}, {3, 0x03}, {1, 0x09}, {1, 0x0B}, {1, 0x11}, {1, 0x13},
        {1, 0x19}, {1, 0x1B}, {2, 0x01}, {2, 0x03}, {2, 0x09}, {2, 0x0B}, {2, 0x11}, {2, 0x13}};
    int i;
    for (i = 0; i < 32; i++) wr(5, i, 0x400, i < 30 ? 0x807F : 0x0080);          /* DCYSUSV */
    for (i = 0; i < 28; i++)   /* INIT1 = Data1 r2, INIT2 = Data2 r2, INIT3 = Data1 r3, INIT4 = Data2 r3 */
        wr(cmd[i][0] <= 2 ? 2 : 3, cmd[i][1], (cmd[i][0] & 1) ? 0x400 : 0x402, hall2[i]);
    wr(3, 0x09, 0x400, 0xE610); wr(3, 0x0C, 0x400, 0x031A); wr(3, 0x03, 0x402, 0xBC84);   /* Chorus 3 */
    wr(1, 9, 0x400, 0); wr(1, 9, 0x402, 0); wr(1, 10, 0x400, 0x83); wr(1, 10, 0x402, 0);
    wr(1, 13, 0x400, 0x8000); wr(1, 13, 0x402, 0); wr(1, 14, 0x400, 0); wr(1, 14, 0x402, 0);
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
    aweutil();
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
void be_probe(int ch, uint32_t *pitch, uint32_t *vol, uint32_t *cut, uint32_t *addr)
{
    emu8k_voice_t *v = &ref->voice[ch & 31];
    *pitch = v->cpf_curr_pitch; *vol = v->cvcf_curr_volume; *cut = v->cvcf_curr_filt_ctoff;
    *addr = v->addr.int_address & 0xFFFFFF;
}
