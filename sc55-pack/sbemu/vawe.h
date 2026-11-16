/* vawe.h - AWE32/AWE64 EMU8000 for SBEMU: port traps (byte level) on top of awe/synth/emu8k.c.
 * SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU */
#ifndef VAWE_H
#define VAWE_H
#include <stdint.h>
#include "../qemm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VAWE_PORTS 12   /* base+0..3, base+400h..403h, base+800h..803h */

/* base 620h/640h/660h/680h; ram_kb 0..28672; rom = file, folder (searched by content) or NULL.
 * Returns 0 = ok (rom_found tells whether ROM samples are there), -1 = out of memory. */
void VAWE_SetFx(int on);
int  VAWE_Init(int base, int ram_kb, const char* rom, int* rom_found, char* rom_used, int rom_used_len);
void VAWE_Close(void);
int  VAWE_Enabled(void);
int  VAWE_Base(void);
/* fills iodt[VAWE_PORTS]; entries for one 4-port group are consecutive (0-3, 4-7, 8-11) */
void VAWE_GetIODT(QEMM_IODT* iodt);
/* adds the EMU8000 output to pcm (stereo int16, frames), volume 0..256 (256 = unity) */
void VAWE_MixInto(int16_t* pcm, int frames, int rate, int vol);
/* /AWEMPU:GM|GS|MT32 (beta.130-awe.6): the MPU-401 plays on the EMU8000 (awe/synth/awemidi.c). After VAWE_Init:
   mode 0 GM, 1 GS, 2 MT-32; banks found by content in dir (Creative's SYNTHGM/GS/MT.SBK), else NUKED's GM map.
   desc gets what is used. 0 = ok. */
int  VAWE_MidiStart(int mode, int ram_kb, const char* dir, char* desc, int desclen);
int  VAWE_MidiOn(void);
const char* VAWE_MidiKind(int mode);
void VAWE_MidiSetBase(int base);
/* the MPU-401 port handler (data + command port) in front of the player */
uint32_t VAWE_MPU(uint32_t port, uint32_t val, uint32_t out);
/* a program's own AWE driver took the chip (releases), MIDI took it back (reclaims), released now */
void VAWE_MidiStats(uint32_t* releases, uint32_t* reclaims, int* released);
/* trapped accesses since start (diagnostics) */
uint32_t VAWE_Accesses(void);

#ifdef __cplusplus
}
#endif
#endif
