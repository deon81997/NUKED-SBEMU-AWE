/* emu8k_rom.c - load the EMU8000 1 MB GM ROM (word addresses 0..0x7FFFF).
 *
 * Accepted files:
 *  - AWEROM.BIN / awe32.raw: raw 1 MB little-endian dump.
 *  Either way, images that start 2 bytes late ("1MGM" at word 3, as AWE-DUMP
 *  writes them; 86Box's awe32.raw is one) are realigned, like 86Box.
 *  - .SF2: RIFF sfbk; its smpl data is copied to ROM word 0 onward, so an
 *    SF2 whose sample offsets are in ROM layout (made from a ROM dump)
 *    plays where the drivers (SYNTHGM.SBK) expect the ROM samples.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu8k.h"

#define ROM_WORDS 0x80000u

uint16_t *emu8k__rom(void);

static uint32_t le32(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static int load_raw(FILE *f, uint16_t *rom)
{
    static unsigned char buf[4096];
    uint32_t w = 0;
    size_t n, i;
    while (w < ROM_WORDS && (n = fread(buf, 1, sizeof(buf), f)) > 1) {
        for (i = 0; i + 1 < n && w < ROM_WORDS; i += 2)
            rom[w++] = (uint16_t) (buf[i] | (buf[i + 1] << 8));
    }
    return w < 16 ? -1 : 0;
}

/* find a chunk id inside [p, end); returns data pointer and size */
static const unsigned char *find(const unsigned char *p, const unsigned char *end, const char *id,
                                 const char *list_type, uint32_t *size)
{
    while (p + 8 <= end) {
        uint32_t sz = le32(p + 4);
        const unsigned char *d = p + 8;
        if (sz > (uint32_t) (end - d))
            return NULL;
        if (!memcmp(p, id, 4) && (!list_type || (sz >= 4 && !memcmp(d, list_type, 4)))) {
            *size = sz;
            return d;
        }
        p = d + sz + (sz & 1);
    }
    return NULL;
}

static int load_sf2(FILE *f, uint16_t *rom)
{
    long len;
    unsigned char *b;
    const unsigned char *sdta, *smpl;
    uint32_t sz, ssz, i, words;
    int rc = -1;
    if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 12 || fseek(f, 0, SEEK_SET))
        return -1;
    b = (unsigned char *) malloc((size_t) len);
    if (!b)
        return -1;
    if (fread(b, 1, (size_t) len, f) != (size_t) len || memcmp(b, "RIFF", 4) || memcmp(b + 8, "sfbk", 4))
        goto out;
    sdta = find(b + 12, b + len, "LIST", "sdta", &sz);
    if (!sdta)
        goto out;
    smpl = find(sdta + 4, sdta + sz, "smpl", NULL, &ssz);
    if (!smpl)
        goto out;
    words = ssz / 2;
    if (words > ROM_WORDS)
        words = ROM_WORDS;
    for (i = 0; i < words; i++)
        rom[i] = (uint16_t) (smpl[2 * i] | (smpl[2 * i + 1] << 8));
    rc = 0;
out:
    free(b);
    return rc;
}

int emu8k_load_rom(const char *path)
{
    uint16_t *rom = emu8k__rom();
    unsigned char hdr[12];
    FILE *f;
    int rc;
    if (!rom || !path)
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    memset(rom, 0, ROM_WORDS * 2);
    if (fread(hdr, 1, 12, f) == 12 && !memcmp(hdr, "RIFF", 4) && !memcmp(hdr + 8, "sfbk", 4))
        rc = load_sf2(f, rom);
    else {
        rewind(f);
        rc = load_raw(f, rom);
    }
    fclose(f);
    if (rc)
        memset(rom, 0, ROM_WORDS * 2);
    else if (rom[3] == 0x314d && rom[4] == 0x474d) {
        /* "1MGM" one word late: AWE-DUMP image (86Box's awe32.raw is one) */
        memmove(&rom[0], &rom[1], (ROM_WORDS - 1) * 2);
        rom[ROM_WORDS - 1] = 0;
    }
    return rc;
}
