/* mt32emu_exp2f.h - forced include for the DOS (DJGPP) Munt build: DJGPP's libm has no exp2f. Munt 2.8.6 mmath.h uses
   exp2f() under MT32EMU_WITH_EXP2F; otherwise it falls to std::exp(float), which changes the integer tables (Tables.cpp
   exp9) vs 2.8.5. This is 2.8.5's own EXP2F formula (float LN2 * x, double exp), so the tables stay bit-identical. */
#ifndef MT32EMU_EXP2F_SHIM_H
#define MT32EMU_EXP2F_SHIM_H
#include <math.h>
static inline float mt32emu_exp2f(float x) { return (float)exp((double)(0.6931472f * x)); }
#define exp2f mt32emu_exp2f
#endif
