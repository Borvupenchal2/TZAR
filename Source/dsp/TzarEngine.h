// =============================================================================
//  TZAR  —  Thermonuclear Sequence Engine
//  Pure C++17 DSP core (no JUCE dependency, so it can be unit-rendered offline).
//
//  Sequence (t = 0 at DETONATE):
//    [FALL]      whistle glissando, the mix gets progressively high-passed (tension)
//    [VOID]      absolute silence (100-200 ms)
//    [IMPACT]    the mix is crushed (deep duck) with a 10 Hz pressure-wave pump,
//                a sub "residue" thump starts
//    [BLAST]     +200 ms : the explosion arrives (your sample + synth blast)
//    [DEBRIS]    granular shower made from the last seconds of YOUR mix,
//                over a strange silence still loaded with bass
//    [TINNITUS]  ear ringing; the mix comes back muffled (deafness LPF) and
//                slowly recovers
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tzar
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept { return db <= -99.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
template <typename T> inline T clampv (T v, T lo, T hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

//------------------------------------------------------------------------------
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline uint32_t nextU() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float next01() noexcept { return (float) (nextU() >> 8) * (1.0f / 16777216.0f); }
    inline float nextBi() noexcept { return next01() * 2.0f - 1.0f; }
};

//------------------------------------------------------------------------------
// Topology-preserving-transform state variable filter (Zavalishin / Simper).
// Safe under per-sample cutoff modulation.
struct Svf
{
    float ic1 = 0, ic2 = 0;
    float a1 = 1, a2 = 0, a3 = 0, k = 1.414f;
    float lp = 0, bp = 0, hp = 0;

    inline void set (float fc, float q, float fs) noexcept
    {
        fc = clampv (fc, 5.0f, fs * 0.45f);
        const float g = std::tan (kPi * fc / fs);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void tick (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline void reset() noexcept { ic1 = ic2 = lp = bp = hp = 0; }
};

//------------------------------------------------------------------------------
struct Params
{
    // 01 DESCENT
    float fallSec       = 3.0f;
    bool  sync          = false;
    float fallBeats     = 4.0f;
    float bpm           = 120.0f;
    float whistleStart  = 2400.0f;   // Hz
    float whistleEnd    = 320.0f;    // Hz
    float whistleCurve  = 1.6f;      // >1 = accelerating descent
    float tension       = 0.35f;     // 0..1 : high-pass sweep on the mix during the fall
    float whistleDb     = -10.0f;

    // 02 VOID / CRUSH
    float voidMs        = 150.0f;
    float crushDb       = -42.0f;    // duck depth
    float holdSec       = 1.5f;
    float releaseSec    = 0.7f;
    float pressureHz    = 10.0f;
    float pressureAmt   = 0.55f;     // 0..1

    // 03 BLAST
    float blastDelayMs  = 200.0f;
    float sampleDb      = -3.0f;
    float samplePitch   = 0.0f;      // semitones
    float synthDb       = -5.0f;
    float subDb         = -6.0f;
    float drive         = 0.45f;     // 0..1

    // 04 DEBRIS
    float debrisSec     = 5.0f;
    float grainMs       = 70.0f;
    float density       = 35.0f;     // grains / s
    float dropSemis     = 12.0f;
    float spread        = 0.75f;     // 0..1 stereo + pitch scatter
    float reverseProb   = 0.35f;     // 0..1
    float debrisDb      = -4.0f;
    float residueDb     = -13.0f;

    // 05 TINNITUS / DEAFNESS
    float tinHz         = 6200.0f;
    float tinDb         = -30.0f;
    float tinSec        = 9.0f;
    float ear           = 0.25f;     // -1 left .. +1 right
    float deafHz        = 380.0f;
    float recoverySec   = 7.0f;

    // SYS
    float yield         = 1.0f;      // 0..1.5 : level of everything TZAR generates
    float outDb         = 0.0f;
    bool  limiter       = true;
};

//------------------------------------------------------------------------------
struct Timeline   // all in seconds, relative to DETONATE
{
    double fall = 0, impact = 0, blast = 0, holdEnd = 0, releaseEnd = 0, recoveryEnd = 0,
           debrisStart = 0, debrisEnd = 0, residueEnd = 0, tinStart = 0, tinEnd = 0,
           blastEnd = 0, total = 0;

    static Timeline compute (const Params& p, double sampleLenSec) noexcept
    {
        Timeline t;
        t.fall        = p.sync ? p.fallBeats * 60.0 / std::max (20.0f, p.bpm) : p.fallSec;
        t.impact      = t.fall + p.voidMs * 0.001;
        t.blast       = t.impact + p.blastDelayMs * 0.001;
        t.holdEnd     = t.impact + p.holdSec;
        t.releaseEnd  = t.holdEnd + p.releaseSec;
        t.recoveryEnd = t.holdEnd + (p.deafHz < 19000.0f ? p.recoverySec : p.releaseSec);
        t.recoveryEnd = std::max (t.recoveryEnd, t.releaseEnd);
        t.debrisStart = t.blast + 0.08;
        t.debrisEnd   = t.debrisStart + p.debrisSec;
        t.residueEnd  = t.debrisEnd + 0.8;
        t.tinStart    = t.blast;
        t.tinEnd      = t.blast + p.tinSec;
        t.blastEnd    = t.blast + std::max (6.0, sampleLenSec);
        t.total = std::max ({ t.recoveryEnd, t.debrisEnd, t.residueEnd, t.tinEnd, t.blastEnd }) + 0.05;
        return t;
    }
};

//------------------------------------------------------------------------------
struct SampleData
{
    std::vector<float> L, R;   // R may be empty (mono)
    double rate = 44100.0;
    int length() const noexcept { return (int) L.size(); }
};

//------------------------------------------------------------------------------
enum class Phase : int { Idle = 0, Fall, Void, Impact, Debris, Recovery };

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void setParams (const Params& p) noexcept { params = p; }

    /** Request a detonation at the given sample offset inside the next process() call. */
    void trigger (int sampleOffset = 0) noexcept { pendingTrigger = std::max (0, sampleOffset); }
    void abort() noexcept;

    /** In-place stereo processing. R may equal L for mono. sample may be nullptr. */
    void process (float* L, float* R, int n, const SampleData* sample) noexcept;

    // --- telemetry (read on the audio thread, published by the host wrapper)
    Phase    getPhase()       const noexcept;
    double   getElapsedSec()  const noexcept { return active ? (double) t / sr : 0.0; }
    bool     isActive()       const noexcept { return active; }
    int      activeGrains()   const noexcept { return grainCount; }
    const Timeline& getTimeline() const noexcept { return tl; }
    uint32_t getSequenceCount()  const noexcept { return seqCount; }

private:
    void start (const SampleData* sample) noexcept;
    void finish() noexcept;
    void spawnGrain (float progress) noexcept;
    inline float readCapture (int ch, double pos) const noexcept;

    double sr = 48000.0;
    Params params;
    Timeline tl;
    Rng rng;

    // sequence state
    bool    active = false;
    int64_t t = 0;
    int     pendingTrigger = -1;
    uint32_t seqCount = 0;
    bool    restartAfterFade = false;
    int     fadeLeft = 0, fadeLen = 1;     // abort fade
    // timeline in samples
    int64_t sFall = 0, sImpact = 0, sBlast = 0, sHoldEnd = 0, sRelEnd = 0, sRecEnd = 0,
            sDebStart = 0, sDebEnd = 0, sResEnd = 0, sTinEnd = 0, sTotal = 0;

    // music path
    Svf  tensionHP[2], deafLP1[2], deafLP2[2];
    float gainSm = 1.0f, gainCoef = 0.02f;
    float wetSm = 0.0f;
    double pressurePhase = 0.0;

    // whistle
    double whPhase = 0.0;
    Svf whNoiseBP[2], whRushLP[2];

    // blast synth
    Svf blCrackHP[2], blBodyLP[2], blRumbleLP[2];
    double subPhase = 0.0;
    float brown[2] = { 0, 0 };
    double samplePos = -1.0;

    // residue
    double resPhase = 0.0;
    Svf resLP[2];
    float resBrown[2] = { 0, 0 };

    // tinnitus
    double tinPh1 = 0.0, tinPh2 = 0.0;

    // debris / capture
    std::vector<float> cap[2];
    int  capLen = 1, capWrite = 0, freezeIdx = 0;
    bool frozen = false, captureSilent = false;
    double nextGrainIn = 0.0;
    Svf debrisLP[2];

    struct Grain
    {
        bool on = false;
        double pos = 0, inc = 1;
        int len = 1, age = 0;
        float gL = 0, gR = 0;
        bool perc = false, noise = false;
        float lpC = 0.3f, lpZ = 0;
    };
    static constexpr int kMaxGrains = 96;
    std::array<Grain, kMaxGrains> grains {};
    int grainCount = 0;

    const SampleData* activeSample = nullptr;   // pointer held only during process()
};

} // namespace tzar
