// =============================================================================
//  TZAR — AudioProcessor
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/TzarEngine.h"
#include "Parameters.h"

class TzarProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    TzarProcessor();
    ~TzarProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TZAR"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Commands from the UI (message thread)
    void requestDetonate() noexcept { uiDetonate.store (true); }
    void requestAbort() noexcept    { uiAbort.store (true); }

    bool loadPayload (const juce::File& file);
    void clearPayload();
    juce::String getPayloadName() const;
    std::vector<float> getPayloadThumbnail() const;   // normalised peak envelope
    double getPayloadLengthSec() const;

    /** Current params as the engine would see them (for UI timeline preview). */
    tzar::Params snapshotParams() const;

    //==========================================================================
    // Telemetry (written by audio thread, read by UI)
    struct Telemetry
    {
        std::atomic<int>   phase { 0 };
        std::atomic<float> elapsed { 0.0f };
        std::atomic<int>   grains { 0 };
        std::atomic<int>   sequenceId { 0 };
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
        std::atomic<float> hostBpm { 120.0f };
        std::atomic<double> sampleRate { 48000.0 };
        // timeline snapshot of the running sequence
        std::atomic<float> tFall { 0 }, tImpact { 0 }, tBlast { 0 }, tHoldEnd { 0 }, tRelEnd { 0 },
                           tRecEnd { 0 }, tDebStart { 0 }, tDebEnd { 0 }, tResEnd { 0 }, tTinEnd { 0 }, tTotal { 0 };

        static constexpr int scopeSize = 1024;
        std::array<std::atomic<float>, scopeSize> scope {};
        std::atomic<int> scopeWrite { 0 };
    };
    Telemetry telemetry;

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;   // frees retired payloads (message thread)
    void updateEngineParams (double bpm);

    tzar::Engine engine;
    bool lastDetonate = false;
    bool wasPlaying = false;
    uint32_t lastSequence = 0;
    int  scopeDecim = 0;
    std::atomic<bool> uiDetonate { false }, uiAbort { false };

    // raw parameter pointers
    struct Raw
    {
        std::atomic<float> *fall, *sync, *fallBeats, *whStart, *whEnd, *whCurve, *tension, *whLevel,
                           *voidMs, *crush, *hold, *release, *pressHz, *pressAmt,
                           *blDelay, *smpLevel, *smpPitch, *synLevel, *subLevel, *drive,
                           *debTime, *grain, *density, *drop, *spread, *reverse, *debLevel, *residue,
                           *tinHz, *tinLevel, *tinTime, *ear, *deaf, *recovery,
                           *yield, *output, *limiter, *stopReset, *detonate;
    } raw {};

    // payload sample: lock-free handoff to the audio thread, deferred deletion
    std::atomic<tzar::SampleData*> payload { nullptr };
    struct Retired { std::unique_ptr<tzar::SampleData> data; juce::uint32 when = 0; };
    std::vector<Retired> retired;
    std::unique_ptr<tzar::SampleData> ownedPayload;
    juce::String payloadName, payloadPath, pendingPayloadPath;
    std::vector<float> payloadThumb;
    mutable juce::CriticalSection payloadMetaLock;
    juce::AudioFormatManager formatManager;
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TzarProcessor)
};
