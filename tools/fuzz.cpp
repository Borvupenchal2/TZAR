#include "../Source/dsp/TzarEngine.h"
#include <cstdio>
#include <vector>
#include <cmath>
int main()
{
    tzar::Rng r; r.s = 1234;
    int bad = 0; double worst = 0;
    const double rates[] = { 22050, 44100, 48000, 96000, 192000 };
    for (int trial = 0; trial < 60; ++trial)
    {
        const double sr = rates[trial % 5];
        tzar::Engine e; e.prepare (sr, 4096);
        tzar::Params p;
        auto U = [&] (float a, float b) { return a + (b - a) * r.next01(); };
        p.fallSec = U (0.25f, 12); p.sync = r.next01() < 0.3f; p.fallBeats = U (0.5f, 16); p.bpm = U (60, 200);
        p.whistleStart = U (400, 6000); p.whistleEnd = U (60, 1500); p.whistleCurve = U (0.5f, 3); p.tension = U (0, 1);
        p.voidMs = U (0, 500); p.crushDb = U (-120, 0); p.holdSec = U (0.1f, 6); p.releaseSec = U (0.02f, 4);
        p.pressureHz = U (1, 30); p.pressureAmt = U (0, 1); p.blastDelayMs = U (0, 1000); p.samplePitch = U (-24, 12);
        p.synthDb = U (-60, 6); p.subDb = U (-60, 6); p.drive = U (0, 1); p.debrisSec = U (0.5f, 15); p.grainMs = U (10, 400);
        p.density = U (1, 150); p.dropSemis = U (0, 36); p.spread = U (0, 1); p.reverseProb = U (0, 1); p.debrisDb = U (-60, 6);
        p.residueDb = U (-60, 6); p.tinHz = U (1000, 14000); p.tinDb = U (-60, -12); p.tinSec = U (0.5f, 30); p.ear = U (-1, 1);
        p.deafHz = U (100, 20000); p.recoverySec = U (0.5f, 30); p.yield = U (0, 1.5f); p.outDb = U (-24, 6); p.limiter = r.next01() < 0.7f;
        e.setParams (p);
        tzar::SampleData smp; smp.rate = 44100; smp.L.resize (44100 * 2); for (auto& v : smp.L) v = r.nextBi() * 0.5f;
        if (trial % 2) smp.R = smp.L;
        const int total = (int) (sr * 45);
        std::vector<float> L (8192), R (8192);
        int pos = 0; int blk = (trial % 7 == 0) ? 1 : (int) U (16, 4096);
        bool triggered = false;
        while (pos < total)
        {
            const int n = std::min (blk, total - pos);
            for (int i = 0; i < n; ++i) { L[i] = std::sin (pos * 0.01f + i * 0.01f) * 0.5f; R[i] = r.nextBi() * 0.3f; }
            if (! triggered && pos > sr * 0.5) { e.trigger (n / 2); triggered = true; }
            if (trial % 3 == 0 && pos > sr * 6 && pos < sr * 6 + n) e.trigger (0);       // retrigger mid-sequence
            if (trial % 5 == 1 && pos > sr * 9 && pos < sr * 9 + n) e.abort();
            e.process (L.data(), (trial % 4 == 0) ? L.data() : R.data(), n, (trial % 3) ? &smp : nullptr);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { ++bad; break; }
                worst = std::max (worst, (double) std::max (std::fabs (L[i]), std::fabs (R[i])));
            }
            pos += n;
        }
        if (e.isActive()) { std::printf ("trial %d still active after 45 s (total %.1f)\n", trial, e.getTimeline().total); }
    }
    std::printf ("non-finite blocks: %d   worst peak: %.3f\n", bad, worst);
}
