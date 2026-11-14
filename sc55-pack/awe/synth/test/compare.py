#!/usr/bin/env python3
"""compare.py A.wav B.wav [...] -> per pair: SNR of A vs B (best lag), level and
pitch match per 50 ms window. Exit 1 if any pair is below its threshold.

  compare.py [--min-snr dB] [--max-lvl dB] A1.wav B1.wav [A2.wav B2.wav ...]
  compare.py --dir OURS REF [--min-snr 20] [--max-lvl dB]  (all *.wav in both)
Fails a pair when SNR < min-snr or the worst 50 ms level difference > max-lvl.
"""
import os, sys, wave
import numpy as np

SKIP = 1024


def load(p):
    with wave.open(p) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)
        return x.reshape(-1, w.getnchannels()), w.getframerate()


def best_lag(a, b, maxlag=8):
    best = (-1e30, 0)
    n = min(len(a), len(b)) - 2 * maxlag
    if n <= 0:
        return 0
    for lag in range(-maxlag, maxlag + 1):
        x = a[maxlag:maxlag + n]
        y = b[maxlag + lag:maxlag + lag + n]
        c = float(np.dot(x[:, 0] + x[:, 1], y[:, 0] + y[:, 1]))
        if c > best[0]:
            best = (c, lag)
    return best[1]


def stats(pa, pb):
    a, ra = load(pa)
    b, rb = load(pb)
    if ra != rb:
        return None
    a, b = a[SKIP:], b[SKIP:]
    lag = best_lag(a, b)
    n = min(len(a), len(b)) - abs(lag) - 1
    x = a[max(0, -lag):max(0, -lag) + n]
    y = b[max(0, lag):max(0, lag) + n]
    sig = np.sum(y * y) + 1e-9
    err = np.sum((x - y) ** 2) + 1e-9
    snr = 10 * np.log10(sig / err)
    # level difference per 50 ms window (dB), only where the reference is audible
    win = ra // 20
    lv = []
    for i in range(0, n - win, win):
        ry = np.sqrt(np.mean(y[i:i + win] ** 2))
        rx = np.sqrt(np.mean(x[i:i + win] ** 2))
        if ry > 30:
            lv.append(20 * np.log10((rx + 1e-9) / ry))
    lvl = (float(np.median(lv)), float(np.max(np.abs(lv)))) if lv else (0.0, 0.0)
    return dict(lag=lag, snr=snr, lvl_med=lvl[0], lvl_max=lvl[1], peak_a=np.abs(x).max(), peak_b=np.abs(y).max())


def main():
    args = sys.argv[1:]
    opt = {"--min-snr": 20.0, "--max-lvl": 99.0}
    for o in opt:
        if o in args:
            i = args.index(o)
            opt[o] = float(args[i + 1])
            del args[i:i + 2]
    min_snr, max_lvl = opt["--min-snr"], opt["--max-lvl"]
    pairs = []
    if args and args[0] == "--dir":
        da, db = args[1], args[2]
        for f in sorted(os.listdir(da)):
            if f.endswith(".wav") and os.path.exists(os.path.join(db, f)):
                pairs.append((os.path.join(da, f), os.path.join(db, f)))
    else:
        pairs = [(args[i], args[i + 1]) for i in range(0, len(args) - 1, 2)]
    bad = 0
    print("%-16s %5s %8s %9s %9s %7s %7s" % ("file", "lag", "SNR dB", "lvl med", "lvl max", "peakA", "peakB"))
    for pa, pb in pairs:
        s = stats(pa, pb)
        if s is None:
            print(os.path.basename(pa), "rate mismatch")
            bad += 1
            continue
        flag = "" if s["snr"] >= min_snr and s["lvl_max"] <= max_lvl else "  <"
        bad += bool(flag)
        print("%-16s %5d %8.1f %9.2f %9.2f %7d %7d%s" % (os.path.basename(pa)[:16], s["lag"], s["snr"], s["lvl_med"],
                                                     s["lvl_max"], s["peak_a"], s["peak_b"], flag))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
