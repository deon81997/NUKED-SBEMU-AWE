# bursts.py WAV [thr_db] - sound bursts in a recording (SC55W RecordWav): start, length, peak dB (w98par tests)
import sys, wave, numpy as np
w = wave.open(sys.argv[1]); r = w.getframerate(); ch = w.getnchannels()
d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float).reshape(-1, ch).mean(axis=1)
thr = float(sys.argv[2]) if len(sys.argv) > 2 else -50
win = r // 20; n = len(d) // win
lv = np.array([20 * np.log10(np.sqrt(np.mean(d[i*win:(i+1)*win] ** 2)) / 32768 + 1e-9) for i in range(n)])
on = lv > thr; b = []; i = 0
while i < n:
    if on[i]:
        j = i
        while j < n and (on[j] or (j + 6 < n and on[j:j+6].any())): j += 1
        b.append((i * win / r, (j - i) * win / r, lv[i:j].max())); i = j
    else: i += 1
print(f"{len(d)/r:.1f} s at {r} Hz, {len(b)} bursts over {thr} dB")
for s, l, p in b: print(f"  {s:7.2f} s  {l:5.2f} s  peak {p:6.1f} dB")
