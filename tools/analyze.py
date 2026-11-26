import sys, numpy as np, matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy.io import wavfile
sr, x = wavfile.read(sys.argv[1]); x = x.astype(np.float32)/32768
m = x.mean(1)
fig, ax = plt.subplots(3,1, figsize=(14,9), sharex=True)
hop=480; n=len(m)//hop
rms = [20*np.log10(np.sqrt(np.mean(m[i*hop:(i+1)*hop]**2))+1e-9) for i in range(n)]
pk  = [20*np.log10(np.max(np.abs(x[i*hop:(i+1)*hop]))+1e-9) for i in range(n)]
tt = np.arange(n)*hop/sr
ax[0].plot(tt, rms, lw=.7, label="RMS dB"); ax[0].plot(tt, pk, lw=.5, label="peak dB"); ax[0].set_ylim(-90,3); ax[0].legend(); ax[0].grid(alpha=.3)
ax[1].specgram(m, NFFT=2048, Fs=sr, noverlap=1536, cmap="magma", vmin=-130); ax[1].set_ylim(0,9000)
ax[2].specgram(m, NFFT=8192, Fs=sr, noverlap=7168, cmap="magma", vmin=-130); ax[2].set_ylim(0,200)
plt.tight_layout(); plt.savefig(sys.argv[2], dpi=80)
print("max abs", np.abs(x).max())
# click detector: big sample-to-sample jumps
d = np.abs(np.diff(m)); idx = np.argsort(d)[-8:]
print("largest jumps", [(round(i/sr,3), round(float(d[i]),3)) for i in sorted(idx)])
