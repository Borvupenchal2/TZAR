// =============================================================================
//  TZAR  —  DSP core implementation
// =============================================================================
#include "TzarEngine.h"

namespace tzar
{
namespace
{
    inline float softClip (float x) noexcept
    {
        // linear up to 0.85, then smooth saturation towards 1.0 (never exceeds)
        const float a = std::fabs (x);
        if (a <= 0.85f) return x;
        const float y = 0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f);
        return x < 0 ? -y : y;
    }

    inline float hermite (float xm1, float x0, float x1, float x2, float f) noexcept
    {
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    inline float readLinearSample (const std::vector<float>& d, double pos) noexcept
    {
        const int n = (int) d.size();
        const int i = (int) pos;
        if (i < 0 || i >= n) return 0.0f;
        const float f = (float) (pos - i);
        const float xm1 = d[(size_t) std::max (i - 1, 0)];
        const float x0  = d[(size_t) i];
        const float x1  = d[(size_t) std::min (i + 1, n - 1)];
        const float x2  = d[(size_t) std::min (i + 2, n - 1)];
        return hermite (xm1, x0, x1, x2, f);
    }

    inline float smoothstep (float x) noexcept { x = clampv (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
}

//==============================================================================
void Engine::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    capLen = (int) (sr * 6.0);                 // 6 s ring of the incoming mix
    for (auto& c : cap) c.assign ((size_t) capLen, 0.0f);
    capWrite = 0;
    gainCoef = 1.0f - std::exp (-1.0f / (0.0012f * (float) sr));   // ~1.2 ms de-click
    for (int c = 0; c < 2; ++c)
    {
        tensionHP[c].reset(); deafLP1[c].reset(); deafLP2[c].reset();
        whNoiseBP[c].reset(); whRushLP[c].reset();
        blCrackHP[c].reset(); blBodyLP[c].reset(); blRumbleLP[c].reset();
        resLP[c].reset(); debrisLP[c].reset();
        tensionHP[c].set (20.0f, 0.707f, (float) sr);
    }
    active = false;
    pendingTrigger = -1;
    gainSm = 1.0f;
    wetSm = 0.0f;
    for (auto& g : grains) g.on = false;
    grainCount = 0;
}

void Engine::abort() noexcept
{
    if (! active) return;
    restartAfterFade = false;
    fadeLen = fadeLeft = std::max (1, (int) (0.030 * sr));
}

Phase Engine::getPhase() const noexcept
{
    if (! active)          return Phase::Idle;
    if (t < sFall)         return Phase::Fall;
    if (t < sImpact)       return Phase::Void;
    if (t < sRelEnd)       return Phase::Impact;
    if (t < sDebEnd)       return Phase::Debris;
    return Phase::Recovery;
}

void Engine::start (const SampleData* sample) noexcept
{
    double sampleLenSec = 0.0;
    if (sample != nullptr && sample->length() > 0)
        sampleLenSec = sample->length() / sample->rate / std::pow (2.0, params.samplePitch / 12.0);

    tl = Timeline::compute (params, sampleLenSec);
    auto S = [this] (double s) { return (int64_t) std::llround (s * sr); };
    sFall     = S (tl.fall);
    sImpact   = S (tl.impact);
    sBlast    = S (tl.blast);
    sHoldEnd  = S (tl.holdEnd);
    sRelEnd   = S (tl.releaseEnd);
    sRecEnd   = S (tl.recoveryEnd);
    sDebStart = S (tl.debrisStart);
    sDebEnd   = S (tl.debrisEnd);
    sResEnd   = S (tl.residueEnd);
    sTinEnd   = S (tl.tinEnd);
    sTotal    = S (tl.total);

    active = true;
    ++seqCount;
    t = 0;
    fadeLeft = 0;
    restartAfterFade = false;
    frozen = false;
    whPhase = subPhase = resPhase = tinPh1 = tinPh2 = pressurePhase = 0.0;
    samplePos = -1.0;
    brown[0] = brown[1] = resBrown[0] = resBrown[1] = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        whNoiseBP[c].reset(); whRushLP[c].reset();
        blCrackHP[c].reset(); blBodyLP[c].reset(); blRumbleLP[c].reset();
        resLP[c].reset(); debrisLP[c].reset(); deafLP1[c].reset(); deafLP2[c].reset();
    }
    for (auto& g : grains) g.on = false;
    grainCount = 0;
    nextGrainIn = 0.0;
}

void Engine::finish() noexcept
{
    active = false;
    frozen = false;          // resume capturing the live mix
    for (auto& g : grains) g.on = false;
    grainCount = 0;
}

inline float Engine::readCapture (int ch, double pos) const noexcept
{
    const int i = (int) pos;
    const float f = (float) (pos - i);
    auto at = [&] (int k) { k %= capLen; if (k < 0) k += capLen; return cap[ch][(size_t) k]; };
    return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), f);
}

void Engine::spawnGrain (float progress) noexcept
{
    Grain* g = nullptr;
    for (auto& cand : grains) if (! cand.on) { g = &cand; break; }
    if (g == nullptr) return;

    const float fs = (float) sr;
    const float u  = rng.next01();
    float lenMs = params.grainMs * (0.5f + rng.next01()) * (1.0f + 0.8f * progress);
    g->len = std::max (16, (int) (lenMs * 0.001f * fs));

    // pitch: the debris "falls" over time, plus random scatter
    const float semis = -params.dropSemis * std::pow (progress, 0.8f) * (0.6f + 0.8f * rng.next01())
                        + params.spread * 6.0f * rng.nextBi();
    double ratio = std::pow (2.0, semis / 12.0);
    const bool reverse = rng.next01() < params.reverseProb;
    g->inc = reverse ? -ratio : ratio;

    // source: the last 3 s of the mix before impact, biased towards the most recent material
    const int region = std::min (capLen - 8, (int) (3.0 * sr));
    int back = (int) ((0.02f + 0.98f * std::pow (u, 1.8f)) * (float) region);
    const int need = (int) (g->len * ratio) + 8;
    back = clampv (back, need, region);
    double startPos = (double) (freezeIdx - back);
    if (reverse) startPos += g->len * ratio;            // read backwards, ending before freeze
    while (startPos < 0) startPos += capLen;
    while (startPos >= capLen) startPos -= capLen;
    g->pos = startPos;

    // stereo placement (equal-power)
    const float pan = clampv (0.5f + 0.5f * params.spread * rng.nextBi(), 0.0f, 1.0f);
    const float amp = 0.35f + 0.65f * rng.next01();
    g->gL = amp * std::cos (pan * kPi * 0.5f);
    g->gR = amp * std::sin (pan * kPi * 0.5f);

    g->perc  = rng.next01() < 0.5f;
    g->noise = captureSilent;
    g->lpC   = 0.04f + 0.5f * rng.next01();
    g->lpZ   = 0.0f;
    g->age   = 0;
    g->on    = true;
}

//==============================================================================
void Engine::process (float* L, float* R, int n, const SampleData* sample) noexcept
{
    const bool  stereo = (R != nullptr && R != L);
    const float fs     = (float) sr;
    const float invFs  = 1.0f / fs;
    const Params& p    = params;

    const float whistleLin = dbToGain (p.whistleDb);
    const float crushLin   = dbToGain (p.crushDb);
    const float sampleLin  = dbToGain (p.sampleDb);
    const float synthLin   = dbToGain (p.synthDb);
    const float subLin     = dbToGain (p.subDb);
    const float debrisLin  = dbToGain (p.debrisDb);
    const float residueLin = dbToGain (p.residueDb);
    const float tinLin     = dbToGain (p.tinDb);
    const float outLin     = dbToGain (p.outDb);
    const float yieldLin   = p.yield;
    const float tensionTop = 20.0f * std::pow (100.0f, clampv (p.tension, 0.0f, 1.0f)); // 20 Hz .. 2 kHz
    const double sampleInc = (sample != nullptr && sample->length() > 0)
                               ? sample->rate / sr * std::pow (2.0, p.samplePitch / 12.0) : 1.0;

    // tinnitus stereo balance (equal power, centre = unity on both ears)
    const float earAng = (clampv (p.ear, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
    const float earL = std::min (1.0f, std::cos (earAng) * 1.41421356f);
    const float earR = std::min (1.0f, std::sin (earAng) * 1.41421356f);
    const float wetCoef = 1.0f - std::exp (-1.0f / (0.02f * fs));

    for (int i = 0; i < n; ++i)
    {
        // ---------------------------------------------------------------- trigger
        if (pendingTrigger >= 0 && i >= pendingTrigger)
        {
            pendingTrigger = -1;
            if (active && t > 0)
            {
                // retrigger while running: 10 ms fade then restart, to avoid clicks
                restartAfterFade = true;
                fadeLen = fadeLeft = std::max (1, (int) (0.010 * sr));
            }
            else
                start (sample);
        }

        const float inL = L[i];
        const float inR = stereo ? R[i] : inL;

        // ---------------------------------------------------------------- capture ring (dry input)
        if (! frozen)
        {
            cap[0][(size_t) capWrite] = inL;
            cap[1][(size_t) capWrite] = inR;
            if (++capWrite >= capLen) capWrite = 0;
        }

        // always run the tension HP so its state is warm when the fall starts
        tensionHP[0].tick (inL);
        tensionHP[1].tick (inR);

        if (! active)
        {
            gainSm += (1.0f - gainSm) * gainCoef;
            wetSm  += (0.0f - wetSm) * wetCoef;
            float oL = inL * outLin, oR = inR * outLin;
            if (p.limiter) { oL = softClip (oL); oR = softClip (oR); }
            L[i] = oL;
            if (stereo) R[i] = oR;
            continue;
        }

        // ================================================================ ACTIVE SEQUENCE
        const float tau = (float) t * invFs;        // seconds since detonate
        float musicTarget = 1.0f;                   // gain on the mix
        float mL = inL, mR = inR;                   // processed mix
        float gL = 0.0f, gR = 0.0f;                 // generated (TZAR) signal

        float pressureMod = 0.0f;                   // 0..1, shared by duck & residue

        // ------------------------------------------------------------ 01 FALL
        if (t < sFall)
        {
            const float prog = (float) t / (float) std::max<int64_t> (1, sFall);

            // tension: the mix is progressively drained of its low end
            const float hpc = 20.0f * std::pow (tensionTop / 20.0f, prog * prog);
            tensionHP[0].set (hpc, 0.707f, fs);
            tensionHP[1].set (hpc, 0.707f, fs);
            const float tw = std::min (1.0f, tau / 0.05f) * (p.tension > 0.001f ? 1.0f : 0.0f);
            mL = inL + (tensionHP[0].hp - inL) * tw;
            mR = inR + (tensionHP[1].hp - inR) * tw;

            // whistle glissando
            float f = p.whistleStart * std::pow (p.whistleEnd / p.whistleStart, std::pow (prog, p.whistleCurve));
            f *= 1.0f + (0.003f + 0.006f * prog) * std::sin (kTwoPi * (5.2f + 2.0f * prog) * tau);
            whPhase += f * invFs;
            if (whPhase >= 1.0) whPhase -= 1.0;
            const float ph  = (float) whPhase * kTwoPi;
            const float osc = std::sin (ph) + 0.10f * std::sin (2.0f * ph) + 0.03f * std::sin (3.0f * ph);

            float amp = 0.10f + 0.90f * std::pow (prog, 1.4f);
            amp *= std::min (1.0f, tau / 0.03f);
            const float toEnd = (float) (sFall - t) * invFs;
            if (toEnd < 0.0015f) amp *= toEnd / 0.0015f;          // de-click the cut into VOID

            float nz[2];
            for (int c = 0; c < 2; ++c)
            {
                const float w = rng.nextBi();
                whNoiseBP[c].set (f * (c == 0 ? 0.995f : 1.005f), 14.0f, fs);
                whNoiseBP[c].tick (w);
                whRushLP[c].set (900.0f + 2600.0f * prog, 0.6f, fs);
                whRushLP[c].tick (rng.nextBi());
                nz[c] = whNoiseBP[c].bp * whNoiseBP[c].k * 3.2f       // whistling air band
                      + whRushLP[c].lp * 0.35f * prog * prog * prog;   // incoming wind rush
            }
            gL += (osc * 0.8f + nz[0]) * amp * whistleLin;
            gR += (osc * 0.8f + nz[1]) * amp * whistleLin;
        }
        // ------------------------------------------------------------ 02 VOID
        else if (t < sImpact)
        {
            musicTarget = 0.0f;
            mL = mR = 0.0f;
        }
        // ------------------------------------------------------------ 03 IMPACT and after
        else
        {
            const float ti = (float) (t - sImpact) * invFs;          // time since impact

            // duck envelope d: 1 during hold, cosine release
            float d = 0.0f;
            if (t < sHoldEnd) d = 1.0f;
            else if (t < sRelEnd)
            {
                const float pr = (float) (t - sHoldEnd) / (float) std::max<int64_t> (1, sRelEnd - sHoldEnd);
                d = 0.5f * (1.0f + std::cos (kPi * pr));
            }

            // pressure wave: a 10 Hz pump that slowly decelerates and dissipates
            if (d > 0.0f && p.pressureAmt > 0.0f)
            {
                const float span = (float) (tl.releaseEnd - tl.impact);
                const float pr   = clampv (ti / std::max (0.01f, span), 0.0f, 1.0f);
                pressurePhase += p.pressureHz * (1.0f - 0.35f * pr) * invFs;
                if (pressurePhase >= 1.0) pressurePhase -= 1.0;
                const float lfo = 0.5f - 0.5f * std::cos (kTwoPi * (float) pressurePhase);
                pressureMod = p.pressureAmt * lfo * std::sqrt (1.0f - pr);
            }
            const float dEff = d * (1.0f - pressureMod);
            musicTarget = 1.0f - dEff * (1.0f - crushLin);

            // deafness: 24 dB/oct LPF that slowly re-opens (and the level comes back with it)
            float deafGain = 1.0f;
            if (t < sRecEnd && p.deafHz < 19000.0f)
            {
                float fc = p.deafHz;
                float wTarget = 1.0f;
                deafGain = dbToGain (-9.0f);
                if (t >= sHoldEnd)
                {
                    const float pr = (float) (t - sHoldEnd) / (float) std::max<int64_t> (1, sRecEnd - sHoldEnd);
                    const float curve = std::pow (pr, 1.7f);
                    fc = p.deafHz * std::pow (20000.0f / p.deafHz, curve);
                    deafGain = dbToGain (-9.0f * (1.0f - curve));
                    if (pr > 0.85f) wTarget = 1.0f - (pr - 0.85f) / 0.15f;
                }
                wetSm += (wTarget - wetSm) * wetCoef;
                for (int c = 0; c < 2; ++c)
                {
                    deafLP1[c].set (fc, 0.54f, fs);
                    deafLP2[c].set (fc, 1.31f, fs);
                }
            }
            else
                wetSm += (0.0f - wetSm) * wetCoef;

            deafLP1[0].tick (inL); deafLP2[0].tick (deafLP1[0].lp);
            deafLP1[1].tick (inR); deafLP2[1].tick (deafLP1[1].lp);
            if (ti < 0.002f) wetSm = (p.deafHz < 19000.0f) ? 1.0f : 0.0f;   // snap: we come out of silence
            mL = inL + (deafLP2[0].lp * deafGain - inL) * wetSm;
            mR = inR + (deafLP2[1].lp * deafGain - inR) * wetSm;

            // ------------------------------------------------ residue: silence still loaded with bass
            if (t < sResEnd && residueLin > 0.0f)
            {
                const float span = (float) (tl.residueEnd - tl.impact);
                const float pr   = clampv (ti / std::max (0.01f, span), 0.0f, 1.0f);
                const float fRes = 34.0f - 5.0f * pr;
                resPhase += fRes * invFs;
                if (resPhase >= 1.0) resPhase -= 1.0;
                const float s = std::sin (kTwoPi * (float) resPhase);

                float env = std::min (1.0f, ti / 0.025f) * (1.0f + 0.9f * std::exp (-ti / 0.12f));
                if (pr > 0.45f) env *= 0.5f * (1.0f + std::cos (kPi * (pr - 0.45f) / 0.55f));
                env *= 0.78f + 0.22f * std::sin (kTwoPi * 0.45f * ti);    // slow breathing
                env *= 1.0f - 0.6f * pressureMod;                          // pumps against the mix

                float out[2];
                for (int c = 0; c < 2; ++c)
                {
                    resBrown[c] = resBrown[c] * 0.995f + rng.nextBi() * 0.05f;
                    resLP[c].set (70.0f, 0.8f, fs);
                    resLP[c].tick (resBrown[c]);
                    const float x = (s * 0.8f + resLP[c].lp * 1.6f) * env;
                    out[c] = std::tanh (1.8f * x) * 0.85f;                 // harmonics: audible on small speakers
                }
                gL += out[0] * residueLin;
                gR += out[1] * residueLin;
            }

            // ------------------------------------------------ BLAST (+delay)
            if (t >= sBlast)
            {
                const float tb = (float) (t - sBlast) * invFs;

                // user payload sample
                if (sample != nullptr && sample->length() > 0 && sampleLin > 0.0f)
                {
                    if (samplePos < 0.0 && tb < 0.001f) samplePos = 0.0;
                    if (samplePos >= 0.0 && samplePos < sample->length() - 1)
                    {
                        const float sL = readLinearSample (sample->L, samplePos);
                        const float sR = sample->R.empty() ? sL : readLinearSample (sample->R, samplePos);
                        gL += sL * sampleLin;
                        gR += sR * sampleLin;
                        samplePos += sampleInc;
                    }
                }

                // synthetic blast
                if (tb < 8.0f && (synthLin > 0.0f || subLin > 0.0f))
                {
                    const float drv = 1.0f + p.drive * 6.0f;
                    const float norm = 1.0f / std::tanh (drv);
                    const float atk = std::min (1.0f, tb / 0.002f);

                    // sub drop 90 -> 28 Hz
                    const float fSub = 28.0f + 62.0f * std::exp (-tb / 0.35f);
                    subPhase += fSub * invFs;
                    if (subPhase >= 1.0) subPhase -= 1.0;
                    const float subDrv = 1.0f + p.drive * 3.0f;
                    const float sub = std::tanh (std::sin (kTwoPi * (float) subPhase) * subDrv) / std::tanh (subDrv)
                                    * std::exp (-tb / 0.95f) * std::min (1.0f, tb / 0.004f);

                    const float bodyFc = 150.0f + 4500.0f * std::exp (-tb / 0.25f);
                    float o[2];
                    for (int c = 0; c < 2; ++c)
                    {
                        const float w = rng.nextBi();
                        float crack = 0.0f;
                        if (tb < 0.05f)
                        {
                            blCrackHP[c].set (1200.0f, 0.7f, fs);
                            blCrackHP[c].tick (w);
                            crack = blCrackHP[c].hp * std::exp (-tb / 0.006f);
                        }
                        blBodyLP[c].set (bodyFc, 0.9f, fs);
                        blBodyLP[c].tick (rng.nextBi());
                        // saturate the raw material, then apply the envelopes: drive colours, never sustains
                        const float body = std::tanh (blBodyLP[c].lp * 2.2f * drv) * norm
                                         * std::exp (-tb / 0.45f) * atk;

                        brown[c] = brown[c] * 0.997f + rng.nextBi() * 0.04f;
                        blRumbleLP[c].set (95.0f, 0.7f, fs);
                        blRumbleLP[c].tick (brown[c]);
                        const float rumble = std::tanh (blRumbleLP[c].lp * 2.0f * drv) * norm
                                           * 0.55f * std::exp (-tb / 1.25f) * std::min (1.0f, tb / 0.05f);

                        const float x = crack * 0.8f + body * 0.75f + rumble;
                        o[c] = x * synthLin + sub * subLin;
                    }
                    gL += o[0];
                    gR += o[1];
                }
            }

            // ------------------------------------------------ DEBRIS (granular from your own mix)
            if (t >= sDebStart && t < sDebEnd)
            {
                const float td  = (float) (t - sDebStart) * invFs;
                const float dur = (float) std::max (0.05, tl.debrisEnd - tl.debrisStart);
                const float pr  = clampv (td / dur, 0.0f, 1.0f);

                if (t < sDebEnd && p.density > 0.0f)
                {
                    nextGrainIn -= 1.0;
                    if (nextGrainIn <= 0.0)
                    {
                        spawnGrain (pr);
                        const float dens = p.density * (1.0f + 2.0f * std::exp (-td / 0.3f))
                                         * (1.0f - pr) + 0.5f;
                        const float u = std::max (1e-6f, rng.next01());
                        nextGrainIn = -std::log (u) / dens * sr;      // Poisson process
                    }
                }

                float dl = 0.0f, dr = 0.0f;
                int count = 0;
                for (auto& g : grains)
                {
                    if (! g.on) continue;
                    ++count;
                    const float x = (float) g.age / (float) g.len;
                    float w;
                    if (g.perc) w = x < 0.04f ? x / 0.04f : std::pow (1.0f - (x - 0.04f) / 0.96f, 3.0f);
                    else        w = 0.5f - 0.5f * std::cos (kTwoPi * x);

                    float sL, sR;
                    if (g.noise)
                    {
                        g.lpZ += (rng.nextBi() - g.lpZ) * g.lpC;
                        sL = sR = g.lpZ * 0.6f;
                    }
                    else
                    {
                        sL = readCapture (0, g.pos);
                        sR = readCapture (1, g.pos);
                    }
                    dl += sL * w * g.gL;
                    dr += sR * w * g.gR;

                    g.pos += g.inc;
                    if (g.pos >= capLen) g.pos -= capLen;
                    if (g.pos < 0)       g.pos += capLen;
                    if (++g.age >= g.len) g.on = false;
                }
                grainCount = count;

                const float env = std::min (1.0f, td / 0.06f) * std::pow (1.0f - pr, 0.8f);
                const float fc  = 700.0f + 9000.0f * std::pow (1.0f - pr, 1.5f);
                debrisLP[0].set (fc, 0.7f, fs); debrisLP[0].tick (dl);
                debrisLP[1].set (fc, 0.7f, fs); debrisLP[1].tick (dr);
                gL += debrisLP[0].lp * debrisLin * 2.0f * env;
                gR += debrisLP[1].lp * debrisLin * 2.0f * env;
            }
            else
                grainCount = 0;

            // ------------------------------------------------ TINNITUS
            if (t >= sBlast && t < sTinEnd && tinLin > 0.0f)
            {
                const float tt  = (float) (t - sBlast) * invFs;
                const float pr  = clampv (tt / std::max (0.1f, p.tinSec), 0.0f, 1.0f);
                float env = smoothstep (tt / 0.35f);
                if (pr > 0.22f) env *= std::pow ((1.0f - pr) / 0.78f, 2.2f);
                const float drift = 0.003f * std::sin (kTwoPi * 0.13f * tt) + 0.002f * std::sin (kTwoPi * 0.31f * tt + 1.0f);
                const float f = p.tinHz * (1.0f + drift);
                tinPh1 += f * invFs;           if (tinPh1 >= 1.0) tinPh1 -= 1.0;
                tinPh2 += f * 1.0011f * invFs; if (tinPh2 >= 1.0) tinPh2 -= 1.0;
                const float s = (std::sin (kTwoPi * (float) tinPh1) + 0.35f * std::sin (kTwoPi * (float) tinPh2)) * 0.74f;
                gL += s * env * tinLin * earL;
                gR += s * env * tinLin * earR;
            }
        }

        // ---------------------------------------------------------------- mix & declick
        gainSm += (musicTarget - gainSm) * gainCoef;
        float oL = mL * gainSm + gL * yieldLin;
        float oR = mR * gainSm + gR * yieldLin;

        if (fadeLeft > 0)
        {
            const float f = (float) fadeLeft / (float) fadeLen;   // 1 -> 0
            oL = inL + (oL - inL) * f;
            oR = inR + (oR - inR) * f;
            if (--fadeLeft == 0)
            {
                finish();
                if (restartAfterFade) { restartAfterFade = false; start (sample); }
                gainSm = 1.0f;
            }
        }

        oL *= outLin; oR *= outLin;
        if (p.limiter) { oL = softClip (oL); oR = softClip (oR); }
        L[i] = oL;
        if (stereo) R[i] = oR;

        // ---------------------------------------------------------------- advance
        if (active)
        {
            if (! frozen && t >= sFall)
            {
                // freeze the capture at the impact moment: debris are made of what was playing
                frozen = true;
                freezeIdx = capWrite;
                double e = 0.0;
                const int region = std::min (capLen - 8, (int) (3.0 * sr));
                for (int k = 0; k < region; k += 4)
                {
                    int idx = freezeIdx - 1 - k; if (idx < 0) idx += capLen;
                    e += (double) cap[0][(size_t) idx] * cap[0][(size_t) idx];
                }
                captureSilent = (e / (region / 4.0)) < 1e-8;
            }
            if (++t >= sTotal && fadeLeft == 0) finish();
        }
    }
}

} // namespace tzar
