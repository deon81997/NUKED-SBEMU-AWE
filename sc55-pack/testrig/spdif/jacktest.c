/* beta.132 host register trace: Audigy shared-jack writes, beta.131 logic vs sc_emujack.h, per card model.
 * cc -I../../mpxplay/au_cards jacktest.c -o /tmp/jacktest && /tmp/jacktest   (exit 0 = PASS) */
#include <stdio.h>
#include "sc_emujack.h"
#define F_0151 1
static const struct { const char *name; unsigned long ssid; int f; } m[] = {
 {"Audigy 1 [SB0090]",0x00511102,0},{"Audigy 1 ES [SB0160]",0x00521102,0},{"Audigy 1 [SB0092]",0x00531102,0},
 {"Audigy 2 [SB0240]",0x10071102,F_0151},{"Audigy 2 Platinum [SB0240P]",0x10021102,F_0151},{"Audigy 2 EX [SB0280]",0x10051102,F_0151},
 {"Audigy 4 PRO [SB0380]",0x20071102,F_0151},{"Audigy 2 [unknown]",0x99991102,F_0151},
 {"Audigy 2 ZS [SB0350]",0x20021102,F_0151},{"Audigy 2 ZS [SB0350a]",0x20051102,F_0151},{"Audigy 2 [SB0350b]",0x20061102,F_0151},
 {"Audigy 2 ZS [SB0360]",0x20011102,F_0151},{"Audigy 2 ZS [SB0353]",0x10031102,F_0151}};
/* A_IOCFG / HCFG after snd_emu10k1_hw_init: chiprev 4 = 40h set (analog on); Audigy 1 = 80h set */
static void old131(int inv_card, int inv, int mode, unsigned long *io, unsigned long *hc)
{ int sw = (mode != 0) ^ inv_card ^ (inv ? 1 : 0);
  *io = sw ? (*io | 4) : (*io & ~4ul); *hc = sw ? (*hc | 0x1000) : (*hc & ~0x1000ul); }
int main(void)
{
 int i, mode, inv, bad = 0, zs_dig = 0, changed = 0;
 for (i = 0; i < (int)(sizeof m / sizeof m[0]); i++)
  for (inv = 0; inv < 2; inv++)
   for (mode = 0; mode < 3; mode++) {
    const int c = m[i].f & F_0151, inv_card = c && m[i].ssid != 0x20071102;
    unsigned long io0 = c ? 0x0040 : 0x0080, hc0 = 0x0000001d, oi = io0, oh = hc0, ni = io0, nh = hc0;
    old131(inv_card, inv, mode, &oi, &oh);
    emujack_bits(m[i].ssid, inv_card, inv, mode, &ni, &nh);
    const int zs = emujack_is_zs(m[i].ssid), same = oi == ni && oh == nh;
    printf("%-28s %08lX inv=%d mode=%d  old IOCFG=%04lX HCFG=%08lX  new IOCFG=%04lX HCFG=%08lX %s\n", m[i].name, m[i].ssid,
           inv, mode, oi, oh, ni, nh, same ? "same" : "CHANGED");
    if (!zs && !same) bad++, printf("  FAIL: non-ZS card changed\n");
    if (!same) changed++;
    if (zs) {   /* ALSA (emumixer.c shared_spdif_put, invert_shared_spdif): digital jack = 44h clear; analog = 44h set */
     const int want_dig = (mode != 0) ^ inv, is_dig = (ni & 0x44) == 0, alsa_analog = (ni & 0x44) == 0x44;
     if (want_dig ? !is_dig : !alsa_analog) bad++, printf("  FAIL: ZS jack not as ALSA\n");
     if (want_dig && is_dig) zs_dig++;
     if (want_dig && (oi & 0x44) == 0) printf("  note: old made it digital too\n");
    }
   }
 printf("%s: non-ZS identical, ZS digital states = %d (old: 0), %d of %d rows changed\n", bad ? "FAIL" : "PASS", zs_dig, changed,
        (int)(sizeof m / sizeof m[0]) * 6);
 return bad != 0;
}
