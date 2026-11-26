import subprocess, numpy as np
from scipy.io import wavfile
M="-120"
cfgs = {
 "music_only(crush)": ["whistle="+M,"synth="+M,"sub="+M,"debris="+M,"residue="+M,"tin="+M],
 "synth":   ["nomusic=1","whistle="+M,"sub="+M,"debris="+M,"residue="+M,"tin="+M],
 "sub":     ["nomusic=1","whistle="+M,"synth="+M,"debris="+M,"residue="+M,"tin="+M],
 "residue": ["nomusic=1","whistle="+M,"synth="+M,"sub="+M,"debris="+M,"tin="+M],
 "debris":  ["musicstop=7.6","whistle="+M,"synth="+M,"sub="+M,"residue="+M,"tin="+M],
 "whistle": ["nomusic=1","synth="+M,"sub="+M,"debris="+M,"residue="+M,"tin="+M],
 "tin":     ["nomusic=1","whistle="+M,"synth="+M,"sub="+M,"debris="+M,"residue="+M],
}
sr=48000; beat=60/128; imp=beat*16+0.15
def db(v): return 20*np.log10(v+1e-12)
for name,args in cfgs.items():
    subprocess.run(["./render_demo","26","/tmp/s.wav","limiter=0"]+args,check=True,capture_output=True)
    _,x=wavfile.read("/tmp/s.wav"); x=x.astype(np.float32)/32768; m=np.abs(x).max(1)
    def seg(a,b):
        s=x[int(a*sr):int(b*sr)]; return f"pk {db(np.abs(s).max()):6.1f} rms {db(np.sqrt((s**2).mean())):6.1f}"
    print(f"{name:18s} fall[{seg(imp-4,imp-0.15)}]  +0-1s[{seg(imp,imp+1)}]  +1-3s[{seg(imp+1,imp+3)}]  +3-6s[{seg(imp+3,imp+6)}]")
