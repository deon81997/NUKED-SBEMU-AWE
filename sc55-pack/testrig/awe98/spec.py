import sys, numpy as np, scipy.io.wavfile as w
for f in sys.argv[1:]:
    r,x=w.read(f); x=x.astype(float); x=x.mean(1) if x.ndim>1 else x
    x=x[int(r*5):]  # skip start
    n=4096; seg=[x[i:i+n] for i in range(0,len(x)-n,n)]
    P=np.mean([np.abs(np.fft.rfft(s*np.hanning(n)))**2 for s in seg],0); fr=np.fft.rfftfreq(n,1/r)
    tot=P.sum(); rms=20*np.log10(np.sqrt(np.mean(x**2))+1e-9)
    bands=[(0,250),(250,1000),(1000,3000),(3000,6000),(6000,11000),(11000,r/2)]
    print(f.split('/')[-2], r, 'rms %.1f dB'%rms, ' '.join('%d-%d:%.1f'%(a,b,10*np.log10(P[(fr>=a)&(fr<b)].sum()/tot+1e-12)) for a,b in bands))
