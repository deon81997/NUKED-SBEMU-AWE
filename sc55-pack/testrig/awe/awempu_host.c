/* awempu_host.c - host test of SBEMU's /AWEMPU:GM|GS|MT32 (sbemu/vawe.c: MPU-401 front end, SBK search by content,
 * release to a program's own AWE driver, reclaim). Built by awempu_host.sh with stub qemm.h / vdiag.h.
 * awempu_host ROMDIR EMPTYDIR : ROMDIR = AWE32.RAW + stand-in SBKs (neutral names); last line PASS or FAIL. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sbemu/vawe.h"

static int fails;
#define CHECK(c, ...) do { int ok_ = (c); printf("%s ", ok_ ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if(!ok_) fails++; } while(0)
static QEMM_IODT io[VAWE_PORTS];
static double render(double secs)
{
    static int16_t pcm[1024 * 2];
    int left = (int)(secs * 44100); double e = 0; long n = 0;
    while(left > 0)
    {
        int k = left > 1024 ? 1024 : left, i;
        memset(pcm, 0, sizeof pcm);
        VAWE_MixInto(pcm, k, 44100, 256);
        for(i = 0; i < k * 2; i++) e += (double)pcm[i] * pcm[i];
        n += k * 2; left -= k;
    }
    return sqrt(e / n);
}
static void mpu(int cmd, int b) { VAWE_MPU(0x330 + cmd, (uint32_t)b, 1); }
static void note(int on, int k) { mpu(0, on ? 0x90 : 0x80); mpu(0, k); mpu(0, on ? 100 : 0); }
static void game_touch(void)
{   /* a game's AWE driver: pointer write + data read (as a probe does), through the trap handlers */
    io[10].handler(io[10].port & 0xFFFF, 0x00, 1); io[11].handler(io[11].port & 0xFFFF, 0x00, 1);
    io[0].handler(io[0].port & 0xFFFF, 0, 0); io[1].handler(io[1].port & 0xFFFF, 0, 0);
}
int main(int argc, char** argv)
{
    int found = 0; char rn[200], desc[400]; uint32_t rel, rec; int released; double a, b;
    if(argc < 3) return 2;
    CHECK(VAWE_Init(0x620, 4096, argv[1], &found, rn, sizeof rn) == 0 && found, "AWE ROM found by content: %s", rn);
    VAWE_GetIODT(io);
    VAWE_MidiSetBase(0x330);
    CHECK(VAWE_MidiStart(0, 4096, argv[1], desc, sizeof desc) == 0 && strstr(desc, "b_gm"), "GM bank by content: %s", desc);
    mpu(1, 0xFF);
    CHECK(!(VAWE_MPU(0x331, 0, 0) & 0x80) && (VAWE_MPU(0x330, 0, 0) & 0xFF) == 0xFE, "MPU reset ACKed");
    CHECK((VAWE_MPU(0x331, 0, 0) & 0xC0) == 0x80, "status: no data, ready (%02X)", VAWE_MPU(0x331, 0, 0) & 0xFF);
    mpu(1, 0x3F); VAWE_MPU(0x330, 0, 0);
    mpu(0, 0xC0); mpu(0, 0);
    note(1, 60); a = render(0.5);
    CHECK(a > 200, "note 60 sounds (rms %.0f)", a);
    note(0, 60); render(1.5); b = render(0.2);
    CHECK(b < 20, "silent after note off (rms %.1f)", b);
    game_touch(); render(0.05);
    VAWE_MidiStats(&rel, &rec, &released);
    CHECK(released && rel == 1, "a game's AWE access releases the chip (releases %u)", rel);
    note(1, 64); a = render(0.5);
    VAWE_MidiStats(&rel, &rec, &released);
    CHECK(released && rec == 0 && a < 20, "MIDI right after: still the game's chip (rms %.1f)", a);
    note(0, 64); render(3.1);
    note(1, 67); a = render(0.5);
    VAWE_MidiStats(&rel, &rec, &released);
    CHECK(!released && rec == 1 && a > 200, "3 s without AWE access + notes: taken back (reclaims %u, rms %.0f)", rec, a);
    note(0, 67);
    game_touch(); render(0.1); game_touch(); render(0.1);
    VAWE_MidiStats(&rel, &rec, &released);
    CHECK(released && rel == 2, "taken by the game again (releases %u)", rel);
    /* GS: own GS bank + GM fallback; MT-32 mode; no SBK -> built-in map */
    CHECK(VAWE_Init(0x620, 4096, argv[1], &found, rn, sizeof rn) == 0, "re-init");
    CHECK(VAWE_MidiStart(1, 4096, argv[1], desc, sizeof desc) == 0 && strstr(desc, "b_gs") && !strstr(desc, "b_gm"), "GS (both banks use RAM: no GM fallback): %s", desc);
    VAWE_Init(0x620, 4096, argv[1], &found, rn, sizeof rn);
    CHECK(VAWE_MidiStart(2, 4096, argv[1], desc, sizeof desc) == 0 && strstr(desc, "b_mt") && !strstr(desc, "b_gm"), "MT-32: %s", desc);
    VAWE_Init(0x620, 4096, argv[1], &found, rn, sizeof rn);
    CHECK(VAWE_MidiStart(0, 4096, argv[2], desc, sizeof desc) == 0 && strstr(desc, "own GM map"), "no SBK: %s", desc);
    mpu(1, 0xFF); VAWE_MPU(0x330, 0, 0); mpu(1, 0x3F); VAWE_MPU(0x330, 0, 0);
    mpu(0, 0xC0); mpu(0, 0); note(1, 60); a = render(0.5);
    CHECK(a > 200, "built-in map plays (rms %.0f)", a);
    VAWE_Init(0x620, 128, argv[1], &found, rn, sizeof rn);
    CHECK(VAWE_MidiStart(0, 128, argv[1], desc, sizeof desc) == 0 && strstr(desc, "left out"), "128 KB RAM: %s", desc);
    VAWE_Init(0x620, 4096, argv[1], &found, rn, sizeof rn); VAWE_GetIODT(io);
    VAWE_MidiStart(0, 4096, argv[2], desc, sizeof desc);
    game_touch(); render(0.1);
    mpu(0, 0xC0); mpu(0, 0); note(1, 60); a = render(0.5); note(0, 60);
    VAWE_MidiStats(&rel, &rec, &released);
    CHECK(a < 20 && released, "a game's AWE driver first: the player stays off the chip (rms %.1f)", a);
    render(3.1); note(1, 62); a = render(0.5);
    CHECK(a > 200, "...and starts 3 s after its last access (rms %.0f)", a);
    VAWE_Close();
    puts(fails ? "FAIL" : "PASS");
    return fails != 0;
}
