// =============================================================================
//  TZAR — AudioProcessor implementation
// =============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tzar;

TzarProcessor::TzarProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TZAR", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw = { get (pid::fall), get (pid::sync), get (pid::fallBeats), get (pid::whStart), get (pid::whEnd),
            get (pid::whCurve), get (pid::tension), get (pid::whLevel),
            get (pid::voidMs), get (pid::crush), get (pid::hold), get (pid::release), get (pid::pressHz), get (pid::pressAmt),
            get (pid::blDelay), get (pid::smpLevel), get (pid::smpPitch), get (pid::synLevel), get (pid::subLevel), get (pid::drive),
            get (pid::debTime), get (pid::grain), get (pid::density), get (pid::drop), get (pid::spread), get (pid::reverse),
            get (pid::debLevel), get (pid::residue),
            get (pid::tinHz), get (pid::tinLevel), get (pid::tinTime), get (pid::ear), get (pid::deaf), get (pid::recovery),
            get (pid::yield), get (pid::output), get (pid::limiter), get (pid::stopReset), get (pid::detonate) };

    formatManager.registerBasicFormats();
    startTimer (500);
}

TzarProcessor::~TzarProcessor()
{
    stopTimer();
    payload.store (nullptr);
}

//==============================================================================
bool TzarProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void TzarProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    telemetry.sampleRate.store (sampleRate);
    engine.prepare (sampleRate, samplesPerBlock);
    updateEngineParams (telemetry.hostBpm.load());
    lastDetonate = raw.detonate->load() > 0.5f;
}

//==============================================================================
static float dbParam (std::atomic<float>* p) noexcept
{
    const float v = p->load();
    return v <= -59.9f ? -120.0f : v;
}

Params TzarProcessor::snapshotParams() const
{
    Params p;
    p.fallSec      = raw.fall->load();
    p.sync         = raw.sync->load() > 0.5f;
    p.fallBeats    = beatValue ((int) raw.fallBeats->load());
    p.bpm          = telemetry.hostBpm.load();
    p.whistleStart = raw.whStart->load();
    p.whistleEnd   = raw.whEnd->load();
    p.whistleCurve = raw.whCurve->load();
    p.tension      = raw.tension->load() * 0.01f;
    p.whistleDb    = dbParam (raw.whLevel);

    p.voidMs       = raw.voidMs->load();
    p.crushDb      = raw.crush->load() <= -79.9f ? -120.0f : raw.crush->load();
    p.holdSec      = raw.hold->load();
    p.releaseSec   = raw.release->load();
    p.pressureHz   = raw.pressHz->load();
    p.pressureAmt  = raw.pressAmt->load() * 0.01f;

    p.blastDelayMs = raw.blDelay->load();
    p.sampleDb     = dbParam (raw.smpLevel);
    p.samplePitch  = raw.smpPitch->load();
    p.synthDb      = dbParam (raw.synLevel);
    p.subDb        = dbParam (raw.subLevel);
    p.drive        = raw.drive->load() * 0.01f;

    p.debrisSec    = raw.debTime->load();
    p.grainMs      = raw.grain->load();
    p.density      = raw.density->load();
    p.dropSemis    = raw.drop->load();
    p.spread       = raw.spread->load() * 0.01f;
    p.reverseProb  = raw.reverse->load() * 0.01f;
    p.debrisDb     = dbParam (raw.debLevel);
    p.residueDb    = dbParam (raw.residue);

    p.tinHz        = raw.tinHz->load();
    p.tinDb        = dbParam (raw.tinLevel);
    p.tinSec       = raw.tinTime->load();
    p.ear          = raw.ear->load() * 0.01f;
    p.deafHz       = raw.deaf->load();
    p.recoverySec  = raw.recovery->load();

    p.yield        = raw.yield->load() * 0.01f;
    p.outDb        = raw.output->load();
    p.limiter      = raw.limiter->load() > 0.5f;
    return p;
}

void TzarProcessor::updateEngineParams (double bpm)
{
    auto p = snapshotParams();
    p.bpm = (float) bpm;
    engine.setParams (p);
}

//==============================================================================
void TzarProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh == 0 || n == 0) return;

    for (int ch = getTotalNumInputChannels(); ch < numCh; ++ch)
        buffer.clear (ch, 0, n);

    // ---------------------------------------------------------------- host transport
    double bpm = telemetry.hostBpm.load();
    bool playing = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
        }
    }
    telemetry.hostBpm.store ((float) bpm);
    if (raw.stopReset->load() > 0.5f && wasPlaying && ! playing)
        engine.abort();
    wasPlaying = playing;

    updateEngineParams (bpm);

    // ---------------------------------------------------------------- triggers
    const bool det = raw.detonate->load() > 0.5f;
    if (det && ! lastDetonate) engine.trigger (0);          // automation rising edge
    lastDetonate = det;

    if (uiDetonate.exchange (false)) engine.trigger (0);
    if (uiAbort.exchange (false))    engine.abort();

    for (const auto meta : midi)                            // any MIDI note-on detonates (sample accurate)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) { engine.trigger (meta.samplePosition); break; }
    }

    // ---------------------------------------------------------------- DSP
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : L;
    engine.process (L, R, n, payload.load (std::memory_order_acquire));

    // ---------------------------------------------------------------- telemetry
    const int phase = (int) engine.getPhase();
    if (engine.getSequenceCount() != lastSequence)
    {
        lastSequence = engine.getSequenceCount();
        const auto& t = engine.getTimeline();
        telemetry.tFall.store ((float) t.fall);       telemetry.tImpact.store ((float) t.impact);
        telemetry.tBlast.store ((float) t.blast);     telemetry.tHoldEnd.store ((float) t.holdEnd);
        telemetry.tRelEnd.store ((float) t.releaseEnd); telemetry.tRecEnd.store ((float) t.recoveryEnd);
        telemetry.tDebStart.store ((float) t.debrisStart); telemetry.tDebEnd.store ((float) t.debrisEnd);
        telemetry.tResEnd.store ((float) t.residueEnd); telemetry.tTinEnd.store ((float) t.tinEnd);
        telemetry.tTotal.store ((float) t.total);
        telemetry.sequenceId.fetch_add (1);
    }
    telemetry.phase.store (phase);
    telemetry.elapsed.store ((float) engine.getElapsedSec());
    telemetry.grains.store (engine.activeGrains());

    float pl = 0.0f, pr = 0.0f;
    int w = telemetry.scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        pl = std::max (pl, std::abs (L[i]));
        pr = std::max (pr, std::abs (R[i]));
        if (++scopeDecim >= 2)
        {
            scopeDecim = 0;
            telemetry.scope[(size_t) w].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
            w = (w + 1) % Telemetry::scopeSize;
        }
    }
    telemetry.scopeWrite.store (w, std::memory_order_release);
    // UI consumes (exchange to 0) and applies its own ballistics
    telemetry.peakL.store (std::max (pl, telemetry.peakL.load (std::memory_order_relaxed)));
    telemetry.peakR.store (std::max (pr, telemetry.peakR.load (std::memory_order_relaxed)));
}

//==============================================================================
//  PAYLOAD (user impact sample)
//==============================================================================
bool TzarProcessor::loadPayload (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const int maxLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));
    if (maxLen <= 0) return false;

    juce::AudioBuffer<float> tmp ((int) std::min<unsigned int> (2u, reader->numChannels), maxLen);
    reader->read (&tmp, 0, maxLen, 0, true, tmp.getNumChannels() > 1);

    auto data = std::make_unique<SampleData>();
    data->rate = reader->sampleRate;
    data->L.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + maxLen);
    if (tmp.getNumChannels() > 1)
        data->R.assign (tmp.getReadPointer (1), tmp.getReadPointer (1) + maxLen);

    // thumbnail: 256 peak bins, normalised
    std::vector<float> thumb (256, 0.0f);
    float mx = 1e-9f;
    for (int i = 0; i < maxLen; ++i)
    {
        const size_t b = (size_t) ((juce::int64) i * 256 / maxLen);
        float v = std::abs (data->L[(size_t) i]);
        if (! data->R.empty()) v = std::max (v, std::abs (data->R[(size_t) i]));
        thumb[b] = std::max (thumb[b], v);
        mx = std::max (mx, v);
    }
    for (auto& v : thumb) v /= mx;

    // swap in (lock-free for the audio thread); the old one is deleted later by the timer
    payload.store (data.get(), std::memory_order_release);
    if (ownedPayload != nullptr)
        retired.push_back ({ std::move (ownedPayload), juce::Time::getMillisecondCounter() });
    ownedPayload = std::move (data);

    const juce::ScopedLock sl (payloadMetaLock);
    payloadName  = file.getFileNameWithoutExtension();
    payloadPath  = file.getFullPathName();
    payloadThumb = std::move (thumb);
    return true;
}

void TzarProcessor::clearPayload()
{
    payload.store (nullptr, std::memory_order_release);
    if (ownedPayload != nullptr)
        retired.push_back ({ std::move (ownedPayload), juce::Time::getMillisecondCounter() });
    const juce::ScopedLock sl (payloadMetaLock);
    payloadName = {};
    payloadPath = {};
    payloadThumb.clear();
}

void TzarProcessor::timerCallback()
{
    juce::String pending;
    {
        const juce::ScopedLock sl (payloadMetaLock);
        std::swap (pending, pendingPayloadPath);
    }
    if (pending.isNotEmpty()) loadPayload (juce::File (pending));

    const auto now = juce::Time::getMillisecondCounter();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [now] (const Retired& r) { return now - r.when > 2000; }),
                   retired.end());
}

juce::String TzarProcessor::getPayloadName() const
{
    const juce::ScopedLock sl (payloadMetaLock);
    return payloadName;
}

std::vector<float> TzarProcessor::getPayloadThumbnail() const
{
    const juce::ScopedLock sl (payloadMetaLock);
    return payloadThumb;
}

double TzarProcessor::getPayloadLengthSec() const
{
    if (auto* d = payload.load()) return d->length() / d->rate;
    return 0.0;
}

//==============================================================================
void TzarProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (payloadMetaLock);
        state.setProperty ("payloadPath", payloadPath, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TzarProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const juce::String path = state.getProperty ("payloadPath").toString();
            apvts.replaceState (state);
            lastDetonate = raw.detonate->load() > 0.5f;

            if (path.isNotEmpty() && juce::File (path).existsAsFile())
            {
                if (juce::MessageManager::existsAndIsCurrentThread())
                    loadPayload (juce::File (path));
                else
                {
                    const juce::ScopedLock sl (payloadMetaLock);
                    pendingPayloadPath = path;          // picked up by the timer on the message thread
                }
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* TzarProcessor::createEditor()
{
    return new TzarEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TzarProcessor();
}
