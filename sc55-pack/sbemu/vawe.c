/* vawe.c - AWE32/AWE64 EMU8000 for SBEMU.
 * SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU
 *
 * The EMU8000 has three 4-port groups at base (620h, 640h, 660h or 680h):
 *   base+0/+2      Data0 low / high word (32-bit registers)
 *   base+400h/402h Data1 / Data2
 *   base+800h/802h Data3 / Pointer (channel 0-31 | register << 5)
 * Games use 16-bit (and a few 32-bit) IN/OUT. HDPMI's trap API calls the handler once per byte, port then
 * port+1, low byte first; the QEMM/QPIEMU path does the same in qemm.c for IODT entries marked
 * QEMM_IODT_SPLIT. So a word reaches us as two byte calls: the even byte is latched and the odd byte commits
 * the word to emu8k_io_write(); a read of the even byte reads the whole word and the odd byte returns the
 * latched high half. Only the Pointer is also committed on its even byte (some code writes it as a byte; its
 * high byte is always 0) - committing a data port twice would run its side effects (SMLD/SMRD streams) twice.
 *
 * The sound is made in SBEMU's sound card interrupt (VAWE_MixInto), with interrupts off like the trap
 * handlers, so a register write never lands in the middle of a rendered block. WC (the 44.1 kHz sample
 * counter games busy-wait on) moves with the rendered frames, i.e. in real time while SBEMU plays.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "vawe.h"
#include "../awe/synth/emu8k.h"

static int      VAWE_On;
static int      VAWE_BasePort;
static uint8_t  VAWE_WLatch[6];     /* low byte of a word being written, per word port */
static uint8_t  VAWE_RLatch[6];     /* high byte of the word last read, per word port */
static uint8_t  VAWE_RValid[6];
static volatile uint32_t VAWE_Count;

/* word port index 0..5 for offsets 000 002 400 402 800 802 (+1 = its high byte) */
static int VAWE_Index(uint32_t off) { return (int)((off >> 10) * 2 + ((off >> 1) & 1)); }

static uint32_t VAWE_IO(uint32_t port, uint32_t val, uint32_t out)
{
    uint32_t off = (port & 0xFFFF) - (uint32_t)VAWE_BasePort;   /* 0..3, 400h..403h, 800h..803h */
    uint32_t woff = off & ~1u;
    int idx = VAWE_Index(off);
    ++VAWE_Count;
    if(out)
    {
        if(!(off & 1))
        {
            VAWE_WLatch[idx] = (uint8_t)val;
            if(woff == 0x802) emu8k_io_write((int)woff, (uint16_t)(val & 0xFF));
        }
        else
            emu8k_io_write((int)woff, (uint16_t)(VAWE_WLatch[idx] | ((val & 0xFF) << 8)));
        VAWE_RValid[idx] = 0;
        return val;
    }
    if(!(off & 1))
    {
        uint16_t w = emu8k_io_read((int)woff);
        VAWE_RLatch[idx] = (uint8_t)(w >> 8);
        VAWE_RValid[idx] = 1;
        return w & 0xFF;
    }
    if(VAWE_RValid[idx]) { VAWE_RValid[idx] = 0; return VAWE_RLatch[idx]; }
    return emu8k_io_read((int)woff) >> 8;   /* a lone read of the high byte */
}

void VAWE_GetIODT(QEMM_IODT* iodt)
{
    static const uint16_t off[VAWE_PORTS] = { 0x000,0x001,0x002,0x003, 0x400,0x401,0x402,0x403, 0x800,0x801,0x802,0x803 };
    for(int i = 0; i < VAWE_PORTS; ++i)
    {
        iodt[i].port = (uint32_t)(VAWE_BasePort + off[i]) | QEMM_IODT_SPLIT;
        iodt[i].handler = &VAWE_IO;
    }
}

/* a 1 MB EMU8000 ROM dump: "1MGM" at word 2 (raw) or word 3 (AWE-DUMP image) */
static int VAWE_IsRom(const char* path)
{
    struct stat st;
    unsigned char h[12];
    FILE* f;
    int ok = 0;
    if(stat(path, &st) != 0 || S_ISDIR(st.st_mode)) return 0;
    if(st.st_size != 1048576 && st.st_size != 1048578) return 0;
    if(!(f = fopen(path, "rb"))) return 0;
    if(fread(h, 1, 12, f) == 12)
        ok = !memcmp(h + 4, "M1MG", 4) || !memcmp(h + 6, "M1MG", 4);
    fclose(f);
    return ok;
}

static int VAWE_FindRom(const char* dir, char* out, int outlen)
{
    DIR* d = opendir(dir);
    struct dirent* e;
    int found = 0;
    if(!d) return 0;
    while(!found && (e = readdir(d)) != NULL)
    {
        char p[260];
        if(e->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s\\%s", dir, e->d_name);
        if(VAWE_IsRom(p)) { snprintf(out, outlen, "%s", p); found = 1; }
    }
    closedir(d);
    return found;
}

int VAWE_Init(int base, int ram_kb, const char* rom, int* rom_found, char* rom_used, int rom_used_len)
{
    char path[260];
    struct stat st;
    VAWE_Close();
    *rom_found = 0;
    if(rom_used_len) rom_used[0] = 0;
    if(emu8k_init(ram_kb) != 0) { emu8k_close(); return -1; }
    path[0] = 0;
    if(rom && rom[0])
    {
        if(stat(rom, &st) == 0 && S_ISDIR(st.st_mode)) VAWE_FindRom(rom, path, sizeof path);
        else snprintf(path, sizeof path, "%s", rom);
    }
    else if(!VAWE_FindRom("ROMS", path, sizeof path)) VAWE_FindRom("AWE", path, sizeof path);
    if(path[0] && emu8k_load_rom(path) == 0)
    {
        *rom_found = 1;
        snprintf(rom_used, rom_used_len, "%s", path);
    }
    memset(VAWE_WLatch, 0, sizeof VAWE_WLatch);
    memset(VAWE_RValid, 0, sizeof VAWE_RValid);
    VAWE_BasePort = base;
    VAWE_On = 1;
    return 0;
}

void VAWE_Close(void)
{
    if(VAWE_On) emu8k_close();
    VAWE_On = 0;
}

int VAWE_Enabled(void) { return VAWE_On; }
void VAWE_SetFx(int on) { emu8k_set_fx(on); }   /* reverb + chorus; works before VAWE_Init too */
int VAWE_Base(void) { return VAWE_BasePort; }
uint32_t VAWE_Accesses(void) { return VAWE_Count; }

void VAWE_MixInto(int16_t* pcm, int frames, int rate, int vol)
{
    static int16_t buf[512 * 2];
    if(!VAWE_On) return;
    while(frames > 0)
    {
        int n = frames > 512 ? 512 : frames;
        emu8k_render(buf, n, rate);
        if(vol > 0)
            for(int i = 0; i < n * 2; ++i)
            {
                int s = pcm[i] + (buf[i] * vol >> 8);
                pcm[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
            }
        pcm += n * 2;
        frames -= n;
    }
}
