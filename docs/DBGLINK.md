# Serial debug link (SBEMU /DBG) - design notes

## Stage 1 (beta.127, shipped)

- `tools/dbg.h` (shared): spec parser (`COM1|COM2|COM3|COM4|1-4|hex[,baud][,level]`, `:` = `,` for batch files),
  16550 init (8N1, FIFO on, OUT2 off, no UART interrupts), 8 KB byte ring, `dbg_pump()` = at most one FIFO load
  (16 bytes) when THRE is set, never waits; `dbg_sync()` = drain with a time limit (2x wire time + 50 ms).
  Tools: `dbg_tool_init()` takes `/DBG:` from argv or `SBDBG`; with SBEMU's link on, lines go through SBEMU
  (AMIS 0Ah ECX=1) so two writers never interleave bytes.
- `sbemu/dbglink.c`: line `"<s.ms> SB <kind> <text>"`. Hooks, each one `if (DBGL_On)`: trace.c events (E for IRQ/DMA
  problems, synchronous; I otherwise; same text within 1 s counted), virtual IRQ raised/acked, MIDI bytes/drops,
  FIFO underruns, end of the sound ISR (stats once a second from TSC, level-2 frame line, capture send, pump),
  both port-trap paths (hdpmipt.c PM, qemm.c RM: count, level-2 class counters, level-3 record, pump if pending),
  INT 28h (pump at the prompt).
- Level 3: 4096 x 8-byte records (TSC low, port, value, dir) in RAM; nothing sent while capturing. Trigger
  Ctrl+Alt+F10 (vdiag.c key watcher) / AMIS 0Ah ECX=2 / `SBDIAG DBG DUMP`: the ring is frozen and sent as `T` lines
  from the ISR, the trap and INT 28h pumps, then capturing restarts.
- Fatal: `DBGL_Fatal()` = `!` line + `dbg_sync()`. `SBDIAG DBG HANG` (AMIS ECX=3) tests it: the next ISR sends the
  line and spins with interrupts off (HDPMI virtualises ring-3 CLI/HLT, so a HLT loop does not freeze).
- AMIS 0Ah: ECX 0 status (EDX in = buffer size, out = level), 1 tool line (EDX bit 0 = sync), 2 dump, 3 hang test,
  4 set level EDX.

## Stage 2 (not built): two-way commands

- Receive side polled from the same pump: LSR bit 0 -> a small RX line buffer; a complete line (CR) becomes a
  pending command, executed at the next safe point (end of the sound ISR for state reads; INT 28h for anything that
  could call DOS). Never execute from the trap path.
- Commands (host DBGLOG.PY gets an input line; `DBGLOG.PY --cmd "..."` for scripts): `level N`, `dump`, `status`,
  `mout synth|usb`, `spdif 0|1|only`, `peek port` (read-only through UntrappedIO), `marker text`, `wd` (watchdog
  report), `trace flush` (SBTRACE.TXT now). Reply lines are kind `R`. No pokes / memory writes over the cable.
- Framing stays text; optional `#seq` prefix so the host can match replies. Flow control: none needed at 115200
  for short commands; RX buffer 128 bytes, overflow = drop line + `E rx overflow`.
- Security: a serial cable is physical access; still keep a whitelist and no DOS file commands.

## Stage 2b (not built): UDP over a packet driver

- Same line format in UDP datagrams (one or more lines per packet, <= 512 bytes) to a host:port given as
  `/DBG:UDP,192.168.1.10:4555[,level]`; DBGLOG.PY `udp://:4555` listens.
- Packet driver (Crynwr, INT 60h-80h, e.g. Marvell Yukon / RTL8139 drivers on the P5PE-VM): `access_type` for
  class 1 / type 0800+0806, minimal ARP (reply to who-has, one request for the host's MAC, cached) and IPv4/UDP
  with checksum 0. Sending = packet driver `send_pkt` (INT, real mode) - NOT callable from inside the sound ISR
  while DOS/the driver may be busy: queue in the same ring and send from INT 28h / INT 1Ch with a re-entry guard,
  or from the ISR only when the driver's busy flag is clear.
- Receive (for stage-2 commands) via the driver's receiver upcall into a DOS-memory buffer; parse at INT 28h.
- Costs: packet driver calls are far heavier than a UART byte; measure on the PII-450 and keep it behind the switch.
- D810: no DOS packet driver tested for its Broadcom 57xx yet; serial (D/Port COM1) stays the primary path there.
