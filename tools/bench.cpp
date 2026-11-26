#include "../Source/dsp/TzarEngine.h"
#include <chrono>
#include <cstdio>
#include <vector>
int main()
{
    const double sr = 48000; tzar::Engine e; e.prepare (sr, 512);
    tzar::Params p; p.fallSec = 1; p.density = 150; p.grainMs = 400; p.debrisSec = 15; p.tinSec = 30;
    e.setParams (p);
    std::vector<float> L (512, 0.1f), R (512, 0.1f);
    const int blocks = (int) (sr * 20 / 512);
    auto t0 = std::chrono::high_resolution_clock::now();
    e.trigger (0);
    int maxG = 0;
    for (int b = 0; b < blocks; ++b) { e.process (L.data(), R.data(), 512, nullptr); maxG = std::max (maxG, e.activeGrains()); }
    const double el = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
    std::printf ("20 s of audio in %.3f s  ->  %.1f%% of one core (max %d grains)\n", el, 100.0 * el / 20.0, maxG);
}
