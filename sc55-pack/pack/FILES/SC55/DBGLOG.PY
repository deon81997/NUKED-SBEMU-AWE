#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""DBGLOG.PY - host side of SBEMU's serial debug link (SBEMU /DBG:COM1[,baud][,level]) and of the Windows 98
part's (SC55W /DBG:COM1[:level], SC55W.VXD sends the lines).

Reads the retro PC's lines from a serial port (null-modem cable, USB-serial adapter), shows them live and writes every
line, with the host's time and the side it came from (DOS or W98), to a log file. Windows and Linux, Python 3 +
pyserial (pip install pyserial). DOS and Windows 98 captures read the same way; one log can hold both (a DOS game
after Windows was shut down to MS-DOS mode, for example).

  python DBGLOG.PY COM3                     Windows: the USB-serial adapter's COM port (Device Manager > Ports)
  python3 DBGLOG.PY /dev/ttyUSB0            Linux
  python3 DBGLOG.PY /dev/ttyUSB0 -b 57600   when SBEMU runs /DBG:COM1,57600
  python3 DBGLOG.PY COM3 -k E! -k S         show only errors, fatal lines and stats (the file still gets everything)
  python3 DBGLOG.PY COM3 -f usb -x "MENU"   show lines matching 'usb', hide lines matching 'MENU' (regex, any case)
  python3 DBGLOG.PY --list                  the serial ports this PC has
  python3 DBGLOG.PY socket://localhost:4555 an emulator's serial port (QEMU -serial tcp::4555,server,nowait)

Line format: "<seconds.ms> <source> <kind> <text>"; DOS sources SB (SBEMU), MENU, MIDPLAY, GAMETEST, SBDIAG;
Windows 98 sources VXD (SC55W.VXD) and SC55W (the helper program) - shown and logged as side "W98", the rest as "DOS";
kind E error, ! fatal, I event, S stats once a second, F per-frame summary (level 2), K keyboard byte
(level 2: T read by the game through the trap, L looked at by SBEMU, I injected, U taken back), T capture record (level 3).
  python3 DBGLOG.PY COM3 --side W98       show only the Windows 98 lines
The log file: "<host date time.ms>  <DOS|W98>  <line>", default dbg-YYYYMMDD-HHMMSS.log next to this script (-o to choose).
Ctrl+C stops. If the DOS PC freezes, the last line in the file is the last thing it said.
"""
import argparse
import datetime
import os
import re
import sys
import time

KINDS = "E!ISFT"
WIN_SOURCES = ("VXD", "SC55W")                          # Windows 98: SC55W.VXD and the SC55W helper
COLOR = {"E": "\033[91m", "!": "\033[97;41m", "S": "\033[36m", "F": "\033[90m", "T": "\033[90m", "K": "\033[33m", "I": ""}


def kind_of(line):
    """(source, kind) of a link line, kind '' for tool lines and anything else."""
    p = line.split(None, 3)
    if len(p) >= 3 and p[1] in ("SB",) + WIN_SOURCES and len(p[2]) == 1 and p[2] in KINDS:
        return p[1], p[2]
    if p and p[0] == "T":                               # level-3 capture record: "T <us> <i|o> <port> <value>"
        return "SB", "T"
    return (p[1] if len(p) > 1 else ""), ""


def main(argv=None):
    ap = argparse.ArgumentParser(description="SBEMU serial debug link logger", formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    ap.add_argument("port", nargs="?", help="COM3, /dev/ttyUSB0, /dev/ttyS0, socket://host:port")
    ap.add_argument("-b", "--baud", type=int, default=115200, help="baud rate (default 115200, as SBEMU)")
    ap.add_argument("-o", "--out", help="log file (default dbg-<date>-<time>.log)")
    ap.add_argument("-f", "--filter", action="append", default=[], help="show only lines matching this regex (repeatable: any)")
    ap.add_argument("-x", "--exclude", action="append", default=[], help="hide lines matching this regex (repeatable)")
    ap.add_argument("-k", "--kind", action="append", default=[], help="show only these kinds of SBEMU lines, e.g. E! or S")
    ap.add_argument("-s", "--source", action="append", default=[], help="show only these sources: SB MENU MIDPLAY GAMETEST SBDIAG VXD SC55W")
    ap.add_argument("--side", choices=["DOS", "W98", "dos", "w98"], help="show only the lines of one side")
    ap.add_argument("-q", "--quiet", action="store_true", help="no live display (file only)")
    ap.add_argument("--filter-file", action="store_true", help="the filters apply to the log file too")
    ap.add_argument("--until", help="stop when a line matches this regex (test rigs)")
    ap.add_argument("--timeout", type=float, default=0, help="stop after this many seconds (0 = never)")
    ap.add_argument("--idle", type=float, default=0, help="stop after this many seconds without a line (0 = never)")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    ap.add_argument("--no-color", action="store_true")
    a = ap.parse_args(argv)
    try:
        import serial
    except ImportError:
        print("pyserial is missing: pip install pyserial", file=sys.stderr)
        return 2
    if a.list:
        from serial.tools import list_ports
        for p in list_ports.comports():
            print(f"{p.device:16} {p.description}")
        return 0
    if not a.port:
        ap.error("which port? (--list shows them)")
    flt = [re.compile(x, re.I) for x in a.filter]
    exc = [re.compile(x, re.I) for x in a.exclude]
    kinds = set("".join(a.kind).upper())
    srcs = set(s.upper() for s in a.source)
    until = re.compile(a.until, re.I) if a.until else None
    color = not a.no_color and sys.stdout.isatty()
    if color and os.name == "nt":
        os.system("")                                   # enables ANSI colours in the Windows console

    cap_side = ["DOS"]                                  # T records: the side of the last "capture:" line

    def side_of(line):
        p = line.split(None, 2)
        if p and p[0] == "T":
            return cap_side[0]
        s = "W98" if len(p) > 1 and p[1] in WIN_SOURCES else "DOS"
        if "I capture:" in line:
            cap_side[0] = s
        return s

    def shown(line, side):
        if a.side and side != a.side.upper():
            return False
        src, k = kind_of(line)
        if kinds and k not in kinds:
            return False
        if srcs and src.upper() not in srcs:
            return False
        if flt and not any(r.search(line) for r in flt):
            return False
        return not any(r.search(line) for r in exc)

    out = a.out or os.path.join(os.path.dirname(os.path.abspath(__file__)), datetime.datetime.now().strftime("dbg-%Y%m%d-%H%M%S.log"))
    try:
        ser = serial.serial_for_url(a.port, baudrate=a.baud, bytesize=8, parity="N", stopbits=1, timeout=0.2)
    except Exception as e:                              # noqa: BLE001 - any open error is the user's port
        print(f"cannot open {a.port}: {e}", file=sys.stderr)
        return 2
    print(f"DBGLOG: {a.port} {a.baud} 8N1 -> {out}  (Ctrl+C stops)", file=sys.stderr)
    t0 = last = time.time()
    buf = b""
    n = shown_n = 0
    rc = 0
    with open(out, "a", encoding="utf-8", newline="\n") as f:
        f.write(f"# DBGLOG {datetime.datetime.now().isoformat(timespec='seconds')} {a.port} {a.baud}\n")
        f.flush()
        try:
            while True:
                data = ser.read(4096)
                now = time.time()
                if data:
                    buf += data
                    last = now
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.rstrip(b"\r").decode("latin-1")
                    if not line.strip():
                        continue
                    n += 1
                    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                    side = side_of(line)
                    ok = shown(line, side)
                    if ok or not a.filter_file:
                        f.write(f"{ts}  {side}  {line}\n")
                        f.flush()                       # on disk now: the PC may freeze any moment
                    if ok and not a.quiet:
                        shown_n += 1
                        k = kind_of(line)[1]
                        c = COLOR.get(k, "") if color else ""
                        print(f"{ts[11:]}  {side}  {c}{line}{chr(27) + '[0m' if c else ''}", flush=True)
                    if until and until.search(line):
                        raise StopIteration
                if a.timeout and now - t0 > a.timeout:
                    rc = 1
                    break
                if a.idle and now - last > a.idle:
                    break
        except StopIteration:
            pass
        except KeyboardInterrupt:
            pass
        except Exception as e:                          # noqa: BLE001 - a USB adapter pulled out
            print(f"\nDBGLOG: {e}", file=sys.stderr)
            rc = 3
        if buf.strip():                                 # a line cut short by a freeze: keep it, marked
            cut = buf.decode('latin-1').rstrip()
            f.write(f"{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}  {side_of(cut)}  {cut}  [cut off]\n")
    print(f"DBGLOG: {n} lines ({shown_n} shown) in {out}", file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())
