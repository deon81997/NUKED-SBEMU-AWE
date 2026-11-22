/* sc_emujack.h - Audigy shared "Digital Out / Center-Sub" jack bits (A_IOCFG + HCFG), pure so the host test
 * (testrig/spdif/jacktest.c) runs the exact code the DOS driver runs. Used by sc_sbliv.c only.
 *
 * beta.132: Audigy 2 ZS. Facts and where they come from:
 *  - Linux sound/pci/emu10k1/emu10k1_main.c card table: SB0350 (20021102), SB0350a (20051102), SB0350b (20061102,
 *    "SB0350 written on it"), SB0360 (20011102) and the Dell SB0353 (10031102) are CA0102 + CA0151 with
 *    invert_shared_spdif ("digital/analog switch swapped").
 *  - Linux emumixer.c snd_emu10k1_shared_spdif_put: the jack switch on every Audigy is A_IOCFG_GPOUT0 = 44h as ONE
 *    field (+ HCFG GPOUT0); inverted cards: digital = 44h clear.
 *  - Linux emu10k1_main.c snd_emu10k1_audio_enable + kX driver/p16v.cpp ("HCFG_K2_DISABLE_ANALOG; NOTE: 'ENABLE'"):
 *    on CA0151 cards 40h = analog DACs on. kX driver/hal.cpp: chiprev-4 cards (is_a2) have no separate
 *    "digital output only" bit at all - 04h alone is no jack switch there.
 *  So beta.131's 04h-only switch never made the ZS jack digital (40h stayed set = ALSA's "analog" state): no S/PDIF on
 *  the mini-jack in any /SPDIF mode. On the ZS the jack goes digital only with 40h clear, which also mutes the analog
 *  DACs (= Creative's "Digital Output Only"); the Live!Drive / front-panel digital out works with the jack analog.
 *  Every other Audigy keeps the beta.131 writes bit for bit. */
#ifndef SC_EMUJACK_H
#define SC_EMUJACK_H

#define EMUJACK_IO_JACK    0x0004u      /* A_IOCFG_ENABLE_DIGITAL */
#define EMUJACK_IO_GPOUT0  0x0044u      /* A_IOCFG_GPOUT0 (ALSA's whole switch field) */
#define EMUJACK_HC_GPOUT0  0x00001000u  /* HCFG_GPOUT0 */

static int emujack_is_zs(unsigned long ssid)
{
 return ssid == 0x20011102UL || ssid == 0x20021102UL || ssid == 0x20051102UL || ssid == 0x20061102UL || ssid == 0x10031102UL;
}

/* Audigy 1/2/ZS/4 PRO (not CA0108): new A_IOCFG/HCFG for mode 0/1/2 (analog / analog+S/PDIF / S/PDIF only).
 * inv_card = the model's own polarity (CA0151 except SB0380), inv_user = /SPDIFINV:1. */
static void emujack_bits(unsigned long ssid, int inv_card, int inv_user, int mode, unsigned long *io, unsigned long *hc)
{
 const int dig = mode != 0;
 const int sw = dig ^ inv_card ^ (inv_user ? 1 : 0);
 if(emujack_is_zs(ssid)){   /* ALSA: the whole 44h field, set = analog jack (+ DACs on), clear = digital jack */
  *io = sw ? (*io | EMUJACK_IO_GPOUT0) : (*io & ~(unsigned long)EMUJACK_IO_GPOUT0);
 }else{                     /* beta.131 (Audigy 1 / 2 / 2 Platinum / 4 PRO): only the jack bit, 40h untouched */
  *io = sw ? (*io | EMUJACK_IO_JACK) : (*io & ~(unsigned long)EMUJACK_IO_JACK);
 }
 *hc = sw ? (*hc | EMUJACK_HC_GPOUT0) : (*hc & ~(unsigned long)EMUJACK_HC_GPOUT0);
}

#endif
