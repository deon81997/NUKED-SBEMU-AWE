# EMU8000 synth engine: host tests and DOS (DJGPP) builds.
#   make            host harness + bench (test/harness, test/bench)
#   make test       full comparison against 86Box (needs git, python3+numpy)
#   make dos        EMU8KBEN.EXE + EMU8KHAR.EXE with DJGPP (DJGPP_CC=...)
CC       ?= cc
DJGPP_CC ?= i586-pc-msdosdjgpp-gcc
CFLAGS   ?= -std=c99 -O2 -Wall -Wextra
DOSFLAGS ?= -std=gnu99 -O2 -march=pentium2 -Wall -Wextra
SRC = emu8k.c emu8k_rom.c

all: test/harness test/bench

test/harness: test/harness.c $(SRC) emu8k.h
	$(CC) $(CFLAGS) -I. test/harness.c $(SRC) -lm -o $@

test/bench: test/bench.c $(SRC) emu8k.h
	$(CC) $(CFLAGS) -I. test/bench.c $(SRC) -lm -o $@

test:
	sh test/run_tests.sh

dos: EMU8KBEN.EXE EMU8KHAR.EXE

EMU8KBEN.EXE: test/bench.c $(SRC) emu8k.h
	$(DJGPP_CC) $(DOSFLAGS) -I. test/bench.c $(SRC) -lm -o $@

EMU8KHAR.EXE: test/harness.c $(SRC) emu8k.h
	$(DJGPP_CC) $(DOSFLAGS) -I. test/harness.c $(SRC) -lm -o $@

clean:
	rm -f test/harness test/bench test/harness_ref test/ref86box/snd_emu8k_fixed.c \
	      test/ref86box/inc/86box/snd_emu8k.h EMU8KBEN.EXE EMU8KHAR.EXE

.PHONY: all test dos clean
