/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
/* awemidi.h - a General MIDI player on the EMU8000 engine (emu8k.c), like Creative's AWE32 MIDI driver.
 *
 * Banks: SoundFont 1 (.SBK, Creative's SYNTHGM/GS/MT.SBK, samples in the 1 MB GM ROM and/or sample RAM) or
 * SoundFont 2, else the built-in map over the GM ROM (awegm.h). The MIDI side follows the Linux emux driver
 * (voice allocation, volume/pitch/pan, register sequence); bank loading follows Takashi Iwai's awesfx.
 * Drives the engine with emu8k_write(): call it from the thread that renders, and call awem_advance() with
 * the frames rendered. One global instance, plain C99. */
#ifndef AWEMIDI_H
#define AWEMIDI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { AWEM_GM = 0, AWEM_GS = 1, AWEM_MT32 = 2 };
#define AWEM_VOICES 30          /* channels 30/31: DRAM refresh / FM, as Creative's drivers */

typedef struct AwemBank AwemBank;

/* load a .SBK or .SF2; NULL + err on failure. ram_words = sample RAM the card has (RAM samples beyond it are
   skipped, their zones dropped). */
AwemBank*   awem_bank_load(const char* path, uint32_t ram_words, char* err, int errlen);
/* the built-in GM map over the AWE32 GM ROM (no Creative files) */
AwemBank*   awem_bank_builtin(void);
void        awem_bank_free(AwemBank* b);
const char* awem_bank_name(const AwemBank* b);   /* INAM, or the file name */
int         awem_bank_presets(const AwemBank* b);
uint32_t    awem_bank_ram_used(const AwemBank* b);
int         awem_bank_skipped(const AwemBank* b); /* zones dropped (RAM full) */

/* content check of a file (no full load): 1 = SoundFont 1 bank (.SBK) that plays from the GM ROM (a Creative
   AWE32/AWE64 bank), 2 = other SoundFont 1, 3 = SoundFont 2, 0 = not a SoundFont. kind = AWEM_GM/GS/MT32
   guessed from the presets (GS: variation banks; MT-32: MT-32 instrument names), -1 if unsure. */
int awem_probe(const char* path, int* kind, char* name, int namelen);

/* chip start (emu8k_init* done): HWCF, voices, effect arrays; uploads the banks' RAM samples */
void awem_start(void);
/* banks in lookup order (first = selected mode's bank, then fallbacks); mode = AWEM_GM/GS/MT32 */
void awem_set_banks(AwemBank* const* list, int n, int mode);
/* reverb 0-7 (Room 1-3, Hall 1-2, Plate, Delay, Panning delay), chorus 0-7 (Chorus 1-4, Feedback, Flanger,
   Short delay, Short delay + feedback); Creative's AWE Control types, defaults Hall 2 / Chorus 3 */
void awem_set_fx(int reverb, int chorus);
/* one MIDI byte (running status, SysEx) */
void awem_byte(uint8_t b);
/* all channels: notes off, controllers and programs reset (GM reset) */
void awem_reset(void);
/* frames rendered since the last call (note-off timing, voice bookkeeping) */
void awem_advance(int frames);
/* another program takes the chip (a DOS box): forget the voices, write nothing until awem_reclaim();
   MIDI state (programs, controllers) is still tracked */
void awem_release(void);
int  awem_released(void);
/* take the chip back: chip start again (RAM samples uploaded again), effects, channels keep their state */
void awem_reclaim(void);

typedef struct {
    uint32_t bytes, notes, voices_max, stolen, active, released_notes; /* stolen = sounding voices taken */
    uint32_t missing;                                                   /* note-ons with no preset/zone   */
} AwemStats;
void awem_stats(AwemStats* s);

#ifdef __cplusplus
}
#endif
#endif
