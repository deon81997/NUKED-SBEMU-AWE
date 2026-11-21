/* emu8k.h - standalone EMU8000 (AWE32/AWE64) synth engine.
 *
 * One global instance. Plain C (C99), builds with DJGPP and host gcc/clang.
 * Register numbers/meanings follow 86Box's EMU8000 emulation; see REGS.md.
 *
 * Register code: EMU8K_REG(port, r)
 *   port 0 = Data0 (base+0x000, 32-bit regs), 1 = Data1 (base+0x400),
 *   port 2 = Data2 (base+0x402), 3 = Data3 (base+0x800), r = 0..7.
 * Dword registers (all of Data0, Data1 r0/r1 dword channels) take the
 * full 32-bit value in emu8k_write/emu8k_read. Port-level word access
 * (what a trapped game does) goes through emu8k_io_write/emu8k_io_read.
 */
#ifndef EMU8K_H
#define EMU8K_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMU8K_REG(port, r) (((port) << 3) | (r))

/* Data0 (dword) */
#define EMU8K_CPF    EMU8K_REG(0, 0)
#define EMU8K_PTRX   EMU8K_REG(0, 1)
#define EMU8K_CVCF   EMU8K_REG(0, 2)
#define EMU8K_VTFT   EMU8K_REG(0, 3)
#define EMU8K_Z2     EMU8K_REG(0, 4)
#define EMU8K_Z1     EMU8K_REG(0, 5)
#define EMU8K_PSST   EMU8K_REG(0, 6)
#define EMU8K_CSL    EMU8K_REG(0, 7)
/* Data1 */
#define EMU8K_CCCA   EMU8K_REG(1, 0) /* dword */
#define EMU8K_CFG1   EMU8K_REG(1, 1) /* channel selects: 9 HWCF4, 10 HWCF5, 13 HWCF6, 14 HWCF7,
                                        20 SMALR, 21 SMARR, 22 SMALW, 23 SMARW (dword),
                                        26 SMLD, 29 HWCF1, 30 HWCF2, 31 HWCF3 (word) */
#define EMU8K_INIT1  EMU8K_REG(1, 2)
#define EMU8K_INIT3  EMU8K_REG(1, 3)
#define EMU8K_ENVVOL EMU8K_REG(1, 4)
#define EMU8K_DCYSUSV EMU8K_REG(1, 5)
#define EMU8K_ENVVAL EMU8K_REG(1, 6)
#define EMU8K_DCYSUS EMU8K_REG(1, 7)
/* Data2 */
#define EMU8K_CFG2   EMU8K_REG(2, 1) /* ch 26 SMRD, ch 27 WC (word) */
#define EMU8K_INIT2  EMU8K_REG(2, 2)
#define EMU8K_INIT4  EMU8K_REG(2, 3)
#define EMU8K_ATKHLDV EMU8K_REG(2, 4)
#define EMU8K_LFO1VAL EMU8K_REG(2, 5)
#define EMU8K_ATKHLD EMU8K_REG(2, 6)
#define EMU8K_LFO2VAL EMU8K_REG(2, 7)
/* Data3 */
#define EMU8K_IP     EMU8K_REG(3, 0)
#define EMU8K_IFATN  EMU8K_REG(3, 1)
#define EMU8K_PEFE   EMU8K_REG(3, 2)
#define EMU8K_FMMOD  EMU8K_REG(3, 3)
#define EMU8K_TREMFRQ EMU8K_REG(3, 4)
#define EMU8K_FM2FRQ2 EMU8K_REG(3, 5)
#define EMU8K_ID     EMU8K_REG(3, 7)

#define EMU8K_RAM_START 0x200000u /* first sample-RAM word address */

/* ram_kb: on-board sample RAM in KB (0, 512, 1024 ... 28672). 0 = ok. */
int      emu8k_init(int ram_kb);
void     emu8k_close(void);
/* the same with the caller's memory (zeroed, kept by the caller): rom = emu8k_rom_words() words,
   ram = emu8k_ram_words(ram_kb) words (64K-word blocks + read pad), NULL if ram_kb = 0 */
int      emu8k_init_mem(int ram_kb, uint16_t *rom, uint16_t *ram);
uint32_t emu8k_rom_words(void);
uint32_t emu8k_ram_words(int ram_kb);
/* reverb + chorus (programmed through INIT1-4 / HWCF4-5 as on the card): 1 = on (default), 0 = dry */
void     emu8k_set_fx(int on);
/* output presence EQ (+2.75 dB at 3.6 kHz, matched to real AWE32/AWE64 recordings): 1 = on (default), 0 = off */
void     emu8k_set_tone(int on);
void     emu8k_write(int reg, int channel, uint32_t value);
uint32_t emu8k_read(int reg, int channel);
/* addr = absolute EMU8000 word address (RAM starts at 0x200000). */
void     emu8k_ram_write(uint32_t addr, uint16_t word);
uint16_t emu8k_ram_read(uint32_t addr);
/* Render interleaved stereo int16 at any output rate (engine runs at 44100). */
void     emu8k_render(int16_t *stereo, int frames, int rate);
/* AWEROM.BIN raw dump (1 MB, AWE-DUMP offset fixed) or a .SF2 whose sample
 * data is in ROM layout. 0 = ok, -1 = error (ROM stays silent). */
int      emu8k_load_rom(const char *path);

/* Optional glue for port trapping: offset from the EMU base (0x620/0x640/
 * 0x660/0x680): 0x000/0x002 Data0 lo/hi, 0x400 Data1, 0x402 Data2,
 * 0x800 Data3, 0x802 Pointer. 16-bit accesses. */
void     emu8k_io_write(int offset, uint16_t value);
uint16_t emu8k_io_read(int offset);
/* Optional: advance the 44.1 kHz sample counter (WC) without rendering,
 * for games that busy-wait on WC while no audio is pulled. */
void     emu8k_clock(int samples);

#ifdef __cplusplus
}
#endif
#endif
