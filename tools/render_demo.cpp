// Offline harness: renders a synthetic club loop through TZAR and writes a WAV.
//   g++ -O2 -std=c++17 render_demo.cpp ../Source/dsp/TzarEngine.cpp -o render_demo
#include "../Source/dsp/TzarEngine.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const int n = (int) L.size(), ch = 2, bps = 16;
    const int dataBytes = n * ch * bps / 8;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (ch); w32 (sr); w32 (sr * ch * bps / 8); w16 (ch * bps / 8); w16 (bps);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (int i = 0; i < n; ++i)
        for (float v : { L[i], R[i] })
        {
            int s = (int) std::lrint (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f);
            w16 ((uint16_t) (int16_t) s);
        }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const int sr = 48000;
    const float bpm = 128.0f, beat = 60.0f / bpm;
    const float lenSec = argc > 1 ? std::atof (argv[1]) : 24.0f;
    const int N = (int) (lenSec * sr);
    std::vector<float> L (N), R (N);

    // --- a little techno loop: kick, offbeat bass, hats, detuned chord pad
    tzar::Rng rng;
    double padPh[6] = {};
    const float padF[6] = { 220.0f, 261.63f, 329.63f, 220.0f * 1.004f, 261.63f * 0.996f, 329.63f * 1.003f };
    float hatZ = 0;
    for (int i = 0; i < N; ++i)
    {
        const float t = (float) i / sr;
        const float tb = std::fmod (t, beat);
        const int beatIdx = (int) (t / beat);
        // kick
        float kf = 45.0f + 120.0f * std::exp (-tb / 0.03f);
        static double kph = 0; kph += kf / sr;
        float kick = std::sin (6.2831853f * (float) kph) * std::exp (-tb / 0.28f);
        if (tb < 0.0005f) kph = 0;
        // offbeat bass (A1 / F1)
        const float half = std::fmod (t + beat * 0.5f, beat);
        const float bf = ((beatIdx / 8) % 2 == 0) ? 55.0f : 43.65f;
        static double bph = 0; bph += bf / sr;
        float saw = 2.0f * (float) (bph - std::floor (bph)) - 1.0f;
        float bass = saw * std::exp (-half / 0.12f) * 0.35f;
        // hats on 8ths
        const float t8 = std::fmod (t, beat * 0.5f);
        float wn = rng.nextBi(); float hp = wn - hatZ; hatZ = wn;
        float hat = hp * std::exp (-t8 / 0.025f) * 0.18f;
        // pad
        float pad = 0;
        for (int k = 0; k < 6; ++k) { padPh[k] += padF[k] / sr; pad += (2.0f * (float) (padPh[k] - std::floor (padPh[k])) - 1.0f); }
        pad *= 0.035f * (0.7f + 0.3f * std::sin (6.2831853f * 0.25f * t));
        const float mono = kick * 0.55f + bass;
        L[i] = mono + hat * 0.8f + pad;
        R[i] = mono + hat * 1.0f + pad * 0.9f;
    }

    tzar::Engine eng;
    eng.prepare (sr, 512);
    tzar::Params p;              // factory defaults
    p.bpm = bpm;
    p.sync = true;  p.fallBeats = 8;   // impact lands exactly two bars after detonate
    bool muteMusic = false; float musicStop = -1.0f;
    for (int a = 3; a < argc; ++a)
    {
        float v; char k[64];
        if (std::sscanf (argv[a], "%63[^=]=%f", k, &v) != 2) continue;
        std::string key (k);
        if (key == "whistle") p.whistleDb = v;   else if (key == "synth") p.synthDb = v;
        else if (key == "sub") p.subDb = v;      else if (key == "debris") p.debrisDb = v;
        else if (key == "residue") p.residueDb = v; else if (key == "tin") p.tinDb = v;
        else if (key == "crush") p.crushDb = v;  else if (key == "nomusic") muteMusic = v > 0;
        else if (key == "limiter") p.limiter = v > 0;
        else if (key == "musicstop") musicStop = v;
    }
    if (musicStop > 0) for (int i = (int) (musicStop * sr); i < N; ++i) L[i] = R[i] = 0.0f;
    if (muteMusic) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); }
    eng.setParams (p);

    const int trigAt = (int) (beat * 8 * sr);        // detonate at bar 3
    const int block = 512;
    std::FILE* log = std::fopen ("phases.txt", "w");
    int lastPhase = -1;
    for (int pos = 0; pos < N; pos += block)
    {
        const int n = std::min (block, N - pos);
        int off = -1;
        if (trigAt >= pos && trigAt < pos + n) off = trigAt - pos;
        if (off >= 0) eng.trigger (off);
        eng.process (&L[pos], &R[pos], n, nullptr);
        int ph = (int) eng.getPhase();
        if (ph != lastPhase) { std::fprintf (log, "%.3f phase %d grains %d\n", (double) pos / sr, ph, eng.activeGrains()); lastPhase = ph; }
    }
    std::fclose (log);
    writeWav (argc > 2 ? argv[2] : "tzar_demo.wav", L, R, sr);
    std::printf ("ok\n");
}
