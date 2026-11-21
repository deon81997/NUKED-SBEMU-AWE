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
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "vawe.h"
#ifdef __DJGPP__
#define VAWE_SEP "\\"
#else
#define VAWE_SEP "/"   /* host tests (testrig/awe/awempu_host.sh) */
#endif
#include "../awe/synth/emu8k.h"
#include "../awe/synth/awemidi.h"
#include "vdiag.h"

static int      VAWE_On;
static int      VAWE_BasePort;
static uint8_t  VAWE_WLatch[6];     /* low byte of a word being written, per word port */
static uint8_t  VAWE_RLatch[6];     /* high byte of the word last read, per word port */
static uint8_t  VAWE_RValid[6];
static volatile uint32_t VAWE_Count;


#ifdef AWEREC
/* Test build only (make AWEREC=1): every committed EMU8000 word access, with the TSC, into a 16 MB buffer;
 * SBDIAG FLUSH (MPU command A9h) writes it to C:\AWEREC.BIN (testrig/awenative). Format: "AWEREC1\0", u32 TSC Hz / 256,
 * u32 count, then count x { u32 tsc >> 8, u16 off | 0x8000 = write, u16 value }. Replay: awe/synth/test/harness -s. */
#include "dpmi/dpmi.h"
#define REC_N (2u << 20)
typedef struct { uint32_t t; uint16_t off, val; } RecEv;
/* the buffer starts with a 16-byte header "AWErec" 01 02, Hz, count (kept current): a host rig can also copy it out of
 * guest memory while the game runs (testrig/awe/recmem.py - for games that never get back to DOS, e.g. ROTT) */
static RecEv* Rec; static uint32_t* Rec_Hdr; static uint32_t Rec_W; static int Rec_Done;
static inline uint32_t rec_tsc(void) { uint32_t lo, hi; __asm__ volatile(".byte 0x0f, 0x31" : "=a"(lo), "=d"(hi)); return (lo >> 8) | (hi << 24); }
static inline void rec(uint32_t off, uint16_t v, int w) { if(Rec && Rec_W < REC_N) { RecEv* e = &Rec[Rec_W++]; e->t = rec_tsc(); e->off = (uint16_t)(off | (w ? 0x8000 : 0)); e->val = v; Rec_Hdr[3] = Rec_W; } }
static int rec_dos(DPMI_REG* r) { return DPMI_CallRealModeINT(0x21, r) == 0 && !(r->w.flags & 1); }
void VAWE_RecFlush(void)
{
    static uint32_t seg; DPMI_REG r; uint32_t lin, done, n, hz; int h; char hdr[16];
    if(!Rec || Rec_Done) return;
    if(!seg && !(seg = DPMI_DOSMalloc(4096 / 16))) return;
    lin = (seg & 0xFFFF) << 4;
    DPMI_LMemcpy(lin, DPMI_PTR2L((void*)"C:\\AWEREC.BIN"), 14);
    memset(&r, 0, sizeof r); r.h.ah = 0x3C; r.w.ds = (uint16_t)seg; r.w.dx = 0;
    if(!rec_dos(&r)) return;
    h = r.w.ax; n = Rec_W; hz = (uint32_t)(DIAG_CyclesPerSec() / 256);
    memcpy(hdr, "AWEREC1", 8); memcpy(hdr + 8, &hz, 4); memcpy(hdr + 12, &n, 4);
    for(done = 0; done < 16 + n * 8; )
    {
        uint32_t k = 16 + n * 8 - done; if(k > 4096) k = 4096;
        if(done < 16) { k = 16; DPMI_LMemcpy(lin, DPMI_PTR2L(hdr), 16); }
        else DPMI_LMemcpy(lin, DPMI_PTR2L((char*)Rec + done - 16), k);
        memset(&r, 0, sizeof r); r.h.ah = 0x40; r.w.bx = (uint16_t)h; r.w.cx = (uint16_t)k; r.w.ds = (uint16_t)seg; r.w.dx = 0;
        if(!rec_dos(&r)) break;
        done += k;
    }
    memset(&r, 0, sizeof r); r.h.ah = 0x3E; r.w.bx = (uint16_t)h; rec_dos(&r);
    Rec_Done = 1;
}
#define REC(o, v, w) rec(o, v, w)
#else
#define REC(o, v, w) ((void)0)
#endif
/* word port index 0..5 for offsets 000 002 400 402 800 802 (+1 = its high byte) */
static int VAWE_Index(uint32_t off) { return (int)((off >> 10) * 2 + ((off >> 1) & 1)); }

#ifndef VAWE_TRACE
#define VAWE_TRACE 0   /* 1 = test build: every EMU8000 byte access as a dword OUT to port 5E0h (86Box capture build) */
#endif
#if VAWE_TRACE
#include <pc.h>
static uint32_t VAWE_IO2(uint32_t port, uint32_t val, uint32_t out);
static uint32_t VAWE_IO(uint32_t port, uint32_t val, uint32_t out)
{
    uint32_t off = (port & 0xFFFF) - (uint32_t)VAWE_BasePort, r = VAWE_IO2(port, val, out);
    outportl(0x5E0, (out ? 0x80000000u : 0) | ((off & 0xFFF) << 16) | ((out ? val : r) & 0xFF));
    return r;
}
#define VAWE_IO VAWE_IO2
#endif
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
            if(woff == 0x802) { REC(woff, (uint16_t)(val & 0xFF), 1); emu8k_io_write((int)woff, (uint16_t)(val & 0xFF)); }
        }
        else
        {
            REC(woff, (uint16_t)(VAWE_WLatch[idx] | ((val & 0xFF) << 8)), 1);
            emu8k_io_write((int)woff, (uint16_t)(VAWE_WLatch[idx] | ((val & 0xFF) << 8)));
        }
        VAWE_RValid[idx] = 0;
        return val;
    }
    if(!(off & 1))
    {
        uint16_t w = emu8k_io_read((int)woff);
        REC(woff, w, 0);
        VAWE_RLatch[idx] = (uint8_t)(w >> 8);
        VAWE_RValid[idx] = 1;
        return w & 0xFF;
    }
    if(VAWE_RValid[idx]) { VAWE_RValid[idx] = 0; return VAWE_RLatch[idx]; }
    { uint16_t w = emu8k_io_read((int)woff); REC(woff, w, 0); return w >> 8; }   /* a lone read of the high byte */
}

void VAWE_GetIODT(QEMM_IODT* iodt)
{
    static const uint16_t off[VAWE_PORTS] = { 0x000,0x001,0x002,0x003, 0x400,0x401,0x402,0x403, 0x800,0x801,0x802,0x803 };
    for(int i = 0; i < VAWE_PORTS; ++i)
    {
        iodt[i].port = (uint32_t)(VAWE_BasePort + off[i]) | QEMM_IODT_SPLIT;
#if VAWE_TRACE
#undef VAWE_IO
#endif
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
        snprintf(p, sizeof p, "%s" VAWE_SEP "%s", dir, e->d_name);
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
#ifdef AWEREC
    if(!Rec_Hdr && (Rec_Hdr = (uint32_t*)malloc(16 + REC_N * sizeof(RecEv))) != NULL)
    {
        Rec = (RecEv*)(Rec_Hdr + 4);
        Rec_Hdr[0] = 0x72455741; Rec_Hdr[1] = 0x0201 << 16 | 0x6365;      /* "AWErec" 01 02 */
        Rec_Hdr[2] = (uint32_t)(DIAG_CyclesPerSec() / 256);
    }
    Rec_W = 0; Rec_Done = 0; if(Rec_Hdr) Rec_Hdr[3] = 0;
#endif
    return 0;
}

static void VAWEM_Close(void);
void VAWE_Close(void)
{
    VAWEM_Close();
    if(VAWE_On) emu8k_close();
    VAWE_On = 0;
}

int VAWE_Enabled(void) { return VAWE_On; }
void VAWE_SetFx(int on) { emu8k_set_fx(on); }   /* reverb + chorus; works before VAWE_Init too */
int VAWE_Base(void) { return VAWE_BasePort; }
uint32_t VAWE_Accesses(void) { return VAWE_Count; }

/* ---- /AWEMPU:GM|GS|MT32: the MPU-401 plays on the EMU8000 (beta.130-awe.6) ---------------------------------------
 * As AWEUTIL /EM:GM on a real AWE32 (which needs the card's NMI for the MPU-401 port, so SBEMU does it itself):
 * the game's MPU-401 bytes go to awe/synth/awemidi.c's player (Creative's SYNTHGM/GS/MT.SBK found by content in the
 * ROM folder, else NUKED's own GM map over the ROM). A game's own AWE driver wins: its first EMU8000 access
 * releases the chip (awem_release); it is taken back (awem_reclaim: chip start, RAM samples uploaded again) when
 * MIDI notes come in after VAWEM_IDLE_S seconds without any EMU8000 access - a game playing AWE music sends no
 * MPU-401 bytes, so its samples are never overwritten while it runs. Everything runs with interrupts off (trap,
 * VAWE_MixInto). */
#define VAWEM_IDLE_S 3
static int VAWEM_On, VAWEM_Rate = 44100;
static uint32_t VAWEM_SeenCnt, VAWEM_IdleFrames, VAWEM_Releases, VAWEM_Reclaims;
static int VAWEM_Base, VAWEM_Ack;
static int VAWEM_Started, VAWEM_Touched;   /* chip written by the player yet / a program used the EMU8000 */
static AwemBank* VAWEM_Banks[4];
static int VAWEM_NBanks;

static void VAWEM_Watch(int frames, int rate)
{
    VAWEM_Rate = rate;
    if(VAWE_Count != VAWEM_SeenCnt)
    {   /* a program touched the EMU8000 since the last interrupt: it is its chip now */
        VAWEM_SeenCnt = VAWE_Count;
        VAWEM_IdleFrames = 0;
        VAWEM_Touched = 1;
        if(!awem_released()) { awem_release(); ++VAWEM_Releases; }
    }
    else if(VAWEM_IdleFrames < 0x40000000u) VAWEM_IdleFrames += (uint32_t)frames;
}

/* a whole quick look: RIFF....sfbk */
static int VAWEM_IsSbk(const char* p)
{
    unsigned char h[12];
    FILE* f = fopen(p, "rb");
    int ok = 0;
    if(!f) return 0;
    ok = fread(h, 1, 12, f) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "sfbk", 4);
    fclose(f);
    return ok;
}

static const char* const VAWEM_Kinds[3] = { "GM", "GS", "MT-32" };

int VAWE_MidiStart(int mode, int ram_kb, const char* dir, char* desc, int desclen)
{
    char best[3][260]; int bestrc[3] = { 0, 0, 0 }, n = 0, found = 0, files = 0;
    DIR* d;
    struct dirent* e;
    if(!VAWE_On || mode < 0 || mode > 2) return -1;
    if(!dir || !dir[0]) dir = "ROMS";
    if((d = opendir(dir)) != NULL)
    {   /* Creative's banks by content: SoundFont 1 that plays from the GM ROM; GS/MT-32 told by the presets */
        while((e = readdir(d)) != NULL && files < 2000)
        {
            char p[260], nm[64], u[16]; int kind = -1, rc, k;
            if(e->d_name[0] == '.') continue;
            ++files;
            snprintf(p, sizeof p, "%s" VAWE_SEP "%s", dir, e->d_name);
            if(!VAWEM_IsSbk(p)) continue;
            rc = awem_probe(p, &kind, nm, sizeof nm);
            if(rc != 1 && rc != 2) continue;
            if(kind == -2) continue;   /* a small bank (Special Effects SYNTHGM.SBK): never the GM/GS/MT-32 bank */
            ++found;
            if(kind < 0)
            {   /* the presets did not tell: the file name, else GM */
                for(k = 0; k < 15 && e->d_name[k]; ++k) u[k] = (char)toupper((unsigned char)e->d_name[k]);
                u[k] = 0;
                kind = strstr(u, "MT") ? AWEM_MT32 : strstr(u, "GS") ? AWEM_GS : AWEM_GM;
            }
            if(!bestrc[kind] || (rc == 1 && bestrc[kind] != 1)) { snprintf(best[kind], sizeof best[kind], "%s", p); bestrc[kind] = rc; }
        }
        closedir(d);
    }
    VAWEM_On = 0;
    while(VAWEM_NBanks > 0) awem_bank_free(VAWEM_Banks[--VAWEM_NBanks]);
    {
        int want[2] = { mode, mode != AWEM_GM ? AWEM_GM : -1 }, i, len = 0;
        desc[0] = 0;
        for(i = 0; i < 2; ++i)
        {
            char err[96] = "";
            AwemBank* b;
            if(want[i] < 0 || !bestrc[want[i]]) continue;
            b = awem_bank_load(best[want[i]], (uint32_t)ram_kb * 512u, err, sizeof err);
            if(b && VAWEM_NBanks && awem_bank_ram_used(b) && awem_bank_ram_used(VAWEM_Banks[0]))
            {   /* both banks start their samples at the bottom of the sample RAM: the GM fallback would overwrite */
                awem_bank_free(b);
                continue;
            }
            if(!b) { len += snprintf(desc + len, desclen > len ? desclen - len : 0, "%s%s: %s", len ? ", " : "", best[want[i]], err); continue; }
            VAWEM_Banks[VAWEM_NBanks++] = b;
            len += snprintf(desc + len, desclen > len ? desclen - len : 0, "%s%s (%s)", len ? " + " : "", best[want[i]], awem_bank_name(b));
            if(awem_bank_skipped(b))
                len += snprintf(desc + len, desclen > len ? desclen - len : 0, " - %d sounds left out, /AWERAM too small", awem_bank_skipped(b));
        }
        if(!VAWEM_NBanks)
        {
            AwemBank* b = awem_bank_builtin();
            if(!b) return -1;
            VAWEM_Banks[VAWEM_NBanks++] = b;
            len += snprintf(desc + len, desclen > len ? desclen - len : 0, "%sNUKED's own GM map (no SYNTH%s.SBK in %s)",
                len ? ", " : "", mode == AWEM_GS ? "GS" : mode == AWEM_MT32 ? "MT" : "GM", dir);
        }
    }
    (void)found;
    /* nothing is written to the chip until the first note: a game that only uses its own AWE32 driver finds the
       EMU8000 exactly as without /AWEMPU (awe.6 test: Duke's AWE music was 3 dB louder after a chip start) */
    awem_release();
    awem_set_banks(VAWEM_Banks, VAWEM_NBanks, mode);
    awem_reset();
    VAWEM_Started = VAWEM_Touched = 0;
    VAWEM_SeenCnt = VAWE_Count;
    VAWEM_IdleFrames = 0;
    VAWEM_On = 1;
    return 0;
}
static void VAWEM_Close(void)
{
    VAWEM_On = 0;
    while(VAWEM_NBanks > 0) awem_bank_free(VAWEM_Banks[--VAWEM_NBanks]);
}
int VAWE_MidiOn(void) { return VAWEM_On; }
const char* VAWE_MidiKind(int mode) { return mode >= 0 && mode < 3 ? VAWEM_Kinds[mode] : "?"; }
void VAWE_MidiStats(uint32_t* releases, uint32_t* reclaims, int* released)
{
    *releases = VAWEM_Releases; *reclaims = VAWEM_Reclaims; *released = VAWEM_On && awem_released();
}

/* the MPU-401 in front of it (UART mode, as VMPU's): command port ACKs, data bytes to the player */
void VAWE_MidiSetBase(int base) { VAWEM_Base = base; }
uint32_t VAWE_MPU(uint32_t port, uint32_t val, uint32_t out)
{
    if((int)port == VAWEM_Base + 1)
    {
        if(out)
        {
            if(DIAG_ResetFlush((uint8_t)val)) { VAWEM_Ack = 1; return val; }
            if(DIAG_Command((uint8_t)val)) return val;     /* SBDIAG side channel (A6-A8h) */
            VAWEM_Ack = 1;                                 /* every command is ACKed (FFh reset, 3Fh UART) */
            return val;
        }
        return (val & ~0xffu) | 0x3F | (VAWEM_Ack ? 0 : 0x80);   /* DSR clear while an ACK waits; never busy */
    }
    if(out)
    {
        uint8_t b = (uint8_t)val;
        DIAG_Midi(0);
        if(VAWEM_On)
        {
            if(awem_released() && (b & 0xF0) == 0x90
                && (VAWEM_IdleFrames >= (uint32_t)VAWEM_Rate * VAWEM_IDLE_S || (!VAWEM_Started && !VAWEM_Touched)))
            {   /* the first notes, or notes again after no program has used the EMU8000 for a while: (re)start it */
                awem_reclaim();
                if(VAWEM_Started) ++VAWEM_Reclaims;
                VAWEM_Started = 1;
                VAWEM_SeenCnt = VAWE_Count;
            }
            awem_byte(b);
        }
        return val;
    }
    {
        uint8_t b;
        if(DIAG_ReadByte(&b)) return (val & ~0xffu) | b;
    }
    VAWEM_Ack = 0;
    return (val & ~0xffu) | 0xFE;
}

void VAWE_MixInto(int16_t* pcm, int frames, int rate, int vol)
{
    static int16_t buf[512 * 2];
    if(!VAWE_On) return;
    if(VAWEM_On) VAWEM_Watch(frames, rate);
    while(frames > 0)
    {
        int n = frames > 512 ? 512 : frames;
        emu8k_render(buf, n, rate);
        if(VAWEM_On) awem_advance(n);
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
