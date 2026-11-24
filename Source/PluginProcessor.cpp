#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

const juce::StringArray AutoChainProAudioProcessor::rateNames { "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T" };

double AutoChainProAudioProcessor::rateInBeats (int index)
{
    switch (index)
    {
        case 0:  return 2.0;
        case 1:  return 1.0;
        case 2:  return 0.5;
        case 3:  return 0.25;
        case 4:  return 2.0 / 3.0;
        case 5:  return 1.0 / 3.0;
        default: return 1.0;
    }
}

AutoChainProAudioProcessor::AutoChainProAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoChainProState", createParameterLayout())
{
    pTrigger = apvts.getRawParameterValue ("trigger");
    pRate    = apvts.getRawParameterValue ("rate");
    pShape   = apvts.getRawParameterValue ("shape");
    pTarget  = apvts.getRawParameterValue ("target");
    pMix     = apvts.getRawParameterValue ("mix");
    pAttack  = apvts.getRawParameterValue ("attack");
    pHold    = apvts.getRawParameterValue ("hold");
    pRelease = apvts.getRawParameterValue ("release");
    pSens    = apvts.getRawParameterValue ("sens");
    pLowOnly = apvts.getRawParameterValue ("lowonly");
    pXover   = apvts.getRawParameterValue ("xover");
    pCutoff  = apvts.getRawParameterValue ("cutoff");

    setDrawPoints (defaultDrawPoints());
    setLicenceInfo (licence.check());
    licenceWatcher = std::make_unique<hz::LicenceWatcher> (licence, licenceInfo, [this] (const hz::LicenceInfo& i) { setLicenceInfo (i); });
}

bool AutoChainProAudioProcessor::usesUpgradeSettings() const
{
    return isProValue ("trigger", (int) pTrigger->load()) || isProValue ("shape", (int) pShape->load())
        || isProValue ("target", (int) pTarget->load());
}

juce::AudioProcessorValueTreeState::ParameterLayout AutoChainProAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };

    l.add (std::make_unique<AudioParameterChoice> (id ("trigger"), "Trigger", StringArray { "Sidechain", "Tempo" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (id ("rate"), "Rate", rateNames, 1));
    l.add (std::make_unique<AudioParameterChoice> (id ("shape"), "Shape", StringArray { "Classic", "Hard", "Smooth", "Gate", "Draw" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (id ("target"), "Env", StringArray { "Volume", "Filter" }, 0));

    l.add (std::make_unique<AudioParameterFloat> (id ("mix"), "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    NormalisableRange<float> att (1.0f, 30.0f, 0.01f); att.setSkewForCentre (6.0f);
    l.add (std::make_unique<AudioParameterFloat> (id ("attack"), "Attack", att, 6.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (id ("hold"), "Hold", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 15.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (id ("release"), "Release", NormalisableRange<float> (10.0f, 200.0f, 0.1f), 90.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (id ("sens"), "Sensitivity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterBool> (id ("lowonly"), "Low Only", false));
    NormalisableRange<float> xo (40.0f, 300.0f, 0.1f); xo.setSkewForCentre (120.0f);
    l.add (std::make_unique<AudioParameterFloat> (id ("xover"), "Low Freq", xo, 120.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    NormalisableRange<float> co (100.0f, 4000.0f, 1.0f); co.setSkewForCentre (700.0f);
    l.add (std::make_unique<AudioParameterFloat> (id ("cutoff"), "Cutoff", co, 800.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    return l;
}

bool AutoChainProAudioProcessor::isProValue (const juce::String& id, int v)
{
    return (id == "trigger" && v == kick) || (id == "shape" && v == draw) || (id == "target" && v == filter);
}

// ── Shapes ────────────────────────────────────────────────────────────────────
float AutoChainProAudioProcessor::releaseShape (int shape, float x)
{
    x = juce::jlimit (0.0f, 1.0f, x);
    switch (shape)
    {
        case hard:   return 1.0f - std::pow (x, 4.0f);                                    // stays down, snaps back
        case smooth: return 0.5f * (1.0f + std::cos (x * juce::MathConstants<float>::pi)); // S-curve
        case gate:   return x < 0.85f ? 1.0f : 1.0f - (x - 0.85f) / 0.15f;                  // chopped
        default:
        {
            static const float e = std::exp (-5.5f);
            return (std::exp (-5.5f * x) - e) / (1.0f - e);                                  // classic pump
        }
    }
}

std::vector<juce::Point<float>> AutoChainProAudioProcessor::defaultDrawPoints()
{
    return { { 0.0f, 1.0f }, { 0.03f, 0.0f }, { 0.08f, 0.0f }, { 0.4f, 0.7f }, { 1.0f, 1.0f } };
}

float AutoChainProAudioProcessor::gainAt (const std::vector<juce::Point<float>>& pts, float x)
{
    if (pts.empty()) return 1.0f;
    if (x <= pts.front().x) return pts.front().y;
    for (size_t i = 1; i < pts.size(); ++i)
        if (x <= pts[i].x)
        {
            const auto a = pts[i - 1], b = pts[i];
            const float span = b.x - a.x;
            if (span <= 1.0e-6f) return b.y;
            const float t = (x - a.x) / span;
            return a.y + (b.y - a.y) * t * t * (3.0f - 2.0f * t);   // eased between points: no corners, no clicks
        }
    return pts.back().y;
}

void AutoChainProAudioProcessor::rebuildTable (const std::vector<juce::Point<float>>& pts)
{
    const int next = 1 - activeTable.load();
    for (int i = 0; i < tableSize; ++i)
        tables[(size_t) next][(size_t) i] = 1.0f - juce::jlimit (0.0f, 1.0f, gainAt (pts, (float) i / (tableSize - 1)));
    activeTable.store (next);
}

void AutoChainProAudioProcessor::setDrawPoints (std::vector<juce::Point<float>> pts)
{
    std::sort (pts.begin(), pts.end(), [] (auto a, auto b) { return a.x < b.x; });
    if (pts.size() < 2) pts = defaultDrawPoints();
    pts.front().x = 0.0f;
    pts.back().x  = 1.0f;
    drawPoints = pts;
    rebuildTable (pts);

    juce::StringArray s;
    for (auto p : pts) s.add (juce::String (p.x, 4) + "," + juce::String (p.y, 4));
    apvts.state.setProperty ("drawPoints", s.joinIntoString (";"), nullptr);
}

void AutoChainProAudioProcessor::loadDrawPointsFromState()
{
    const auto str = apvts.state.getProperty ("drawPoints").toString();
    std::vector<juce::Point<float>> pts;
    for (auto& pair : juce::StringArray::fromTokens (str, ";", ""))
    {
        auto xy = juce::StringArray::fromTokens (pair, ",", "");
        if (xy.size() == 2)
            pts.push_back ({ juce::jlimit (0.0f, 1.0f, xy[0].getFloatValue()), juce::jlimit (0.0f, 1.0f, xy[1].getFloatValue()) });
    }
    setDrawPoints (pts.size() >= 2 ? pts : defaultDrawPoints());
}

float AutoChainProAudioProcessor::stepSeconds() const
{
    const double bpm = juce::jlimit (20.0, 400.0, lastBpm.load());
    const bool sc = proUnlocked.load() && (int) pTrigger->load() == kick;
    const double beats = sc ? 1.0 : rateInBeats ((int) pRate->load());
    return (float) (beats * 60.0 / bpm);
}

float AutoChainProAudioProcessor::envLengthSeconds() const
{
    const double step = stepSeconds();
    const double rel  = juce::jmax (0.005, step * pRelease->load() / 100.0);
    if ((int) pShape->load() == draw && proUnlocked.load()) return (float) rel;
    return (float) (pAttack->load() / 1000.0 + pHold->load() / 1000.0 + rel);
}

void AutoChainProAudioProcessor::commitCapture()
{
    if (capBin < 0) return;
    float pk = 0.0f;
    for (int b = 0; b < capMax; ++b) pk = juce::jmax (pk, capCur[(size_t) b]);
    if (capValid && capMax > 20 && pk > 1.0e-3f)
    {
        const bool first = kickCaptures.load() == 0;
        for (int b = 0; b < capMax; ++b)
        {
            auto& k = kickShape[(size_t) b];
            k.store (first ? capCur[(size_t) b] : 0.6f * k.load() + 0.4f * capCur[(size_t) b]);
        }
        if (first) for (int b = capMax; b < kickBins; ++b) kickShape[(size_t) b].store (0.0f);
        kickLenBins.store (first ? capMax : juce::jmax (capMax, (int) (0.9f * (float) kickLenBins.load())));
        kickCaptures.store (kickCaptures.load() + 1);
    }
    capBin = -1;
}

bool AutoChainProAudioProcessor::autoShapeFromKick()
{
    const int L = juce::jmin (kickLenBins.load(), kickBins);
    if (kickCaptures.load() == 0 || L < 30) return false;

    std::vector<float> raw ((size_t) L), e ((size_t) L);
    for (int b = 0; b < L; ++b) raw[(size_t) b] = kickShape[(size_t) b].load();
    for (int b = 0; b < L; ++b)                  // short max filter = smooth envelope of the kick
    {
        float m = 0.0f;
        for (int j = juce::jmax (0, b - 6); j <= juce::jmin (L - 1, b + 6); ++j) m = juce::jmax (m, raw[(size_t) j]);
        e[(size_t) b] = m;
    }
    int tPeak = 0;
    for (int b = 0; b < juce::jmin (L, 60); ++b) if (e[(size_t) b] > e[(size_t) tPeak]) tPeak = b;
    const float P = e[(size_t) tPeak];
    if (P < 1.0e-4f) return false;
    auto dB = [&] (int b) { return juce::Decibels::gainToDecibels (e[(size_t) juce::jlimit (0, L - 1, b)] / P, -100.0f); };

    int tBody = tPeak;                           // the punch: until the kick is 6 dB down
    while (tBody < L - 1 && dB (tBody) > -6.0f) ++tBody;
    int tEnd = tBody;                            // the tail: until it is 26 dB down
    while (tEnd < L - 1 && dB (tEnd) > -26.0f) ++tEnd;
    tEnd = juce::jmax (tEnd, tBody + 20);

    // Release sets the curve length: make the curve exactly as long as the kick.
    const float step = juce::jmax (0.01f, stepSeconds());
    const float relPct = juce::jlimit (10.0f, 200.0f, (float) tEnd / 1000.0f / step * 100.0f);
    const float E = step * relPct / 100.0f * 1000.0f;       // curve length in ms

    std::vector<juce::Point<float>> pts;
    pts.push_back ({ 0.0f, 0.0f });
    pts.push_back ({ juce::jmin (0.95f, (float) tBody / E), 0.0f });
    float g = 0.0f;
    for (int k = 1; k <= 6; ++k)
    {
        const int t = tBody + (tEnd - tBody) * k / 7;
        g = juce::jmax (g, juce::jlimit (0.0f, 1.0f, (-dB (t) - 6.0f) / 20.0f));
        const float x = (float) t / E;
        if (x > pts.back().x + 0.01f && x < 0.99f) pts.push_back ({ x, std::pow (g, 0.85f) });
    }
    const float xEnd = juce::jmin (1.0f, (float) tEnd / E);
    if (xEnd > pts.back().x + 0.01f) pts.push_back ({ xEnd, 1.0f });
    if (pts.back().x < 1.0f) pts.push_back ({ 1.0f, 1.0f });
    setDrawPoints (pts);

    if (auto* r = apvts.getParameter ("release")) r->setValueNotifyingHost (r->convertTo0to1 (relPct));
    if (auto* s = apvts.getParameter ("shape"))   s->setValueNotifyingHost (s->convertTo0to1 ((float) draw));
    return true;
}

// Attenuation 0..1 at time t after the trigger.
float AutoChainProAudioProcessor::envelopeAt (double t, double stepSec, int shape) const
{
    const double rel = juce::jmax (0.005, stepSec * pRelease->load() / 100.0);

    if (shape == draw)
    {
        const double x = t / rel;
        if (x >= 1.0) return 0.0f;
        const auto& tab = tables[(size_t) activeTable.load()];
        const double pos = x * (tableSize - 1);
        const int i = (int) pos;
        const float f = (float) (pos - i);
        const float v = tab[(size_t) i] + (tab[(size_t) juce::jmin (i + 1, tableSize - 1)] - tab[(size_t) i]) * f;
        // Restarting mid-curve: glide from where we were instead of jumping.
        const double fadeT = 0.004;
        if (t < fadeT)
        {
            const float s = 0.5f * (1.0f - std::cos ((float) (t / fadeT) * juce::MathConstants<float>::pi));
            return startAmount + (v - startAmount) * s;
        }
        return v;
    }

    const double a = pAttack->load() / 1000.0;
    const double h = pHold->load() / 1000.0;
    if (t < a)
    {
        const float s = 0.5f * (1.0f - std::cos ((float) (t / a) * juce::MathConstants<float>::pi));
        return startAmount + (1.0f - startAmount) * s;
    }
    if (t < a + h) return 1.0f;
    const double x = (t - a - h) / rel;
    return x >= 1.0 ? 0.0f : releaseShape (shape, (float) x);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void AutoChainProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const auto chans = (juce::uint32) juce::jmax (1, getMainBusNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), chans };

    kickLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 150.0f);
    kickLP.reset();
    envFast = peakTrack = hitPeak = valley = 0.0f; armed = true; lockout = 0; silentSamples = (int) (2 * sr) + 1;

    lookahead = (int) std::round (lookaheadSec * sampleRate);
    setLatencySamples (lookahead);
    delayBuf.setSize ((int) chans, juce::jmax (1, lookahead));
    delayBuf.clear();
    delayPos = 0;
    scDelay.assign ((size_t) juce::jmax (1, lookahead), 0.0f);
    scDelayPos = 0;
    capBin = -1;

    splitLP.prepare (spec); splitHP.prepare (spec);
    splitLP.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    splitHP.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lastXover = -1.0f;

    lowCut.prepare (spec);
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowCut.setResonance (0.707f);
    lowCut.setCutoffFrequency (20.0f);

    tSince = 1.0e9; startAmount = 0.0f; lastAtten = 0.0f;
    amt = 0.0f;
    maxStep = (float) (1.0 / (0.003 * sampleRate));   // full duck can't happen faster than 3 ms
    pendingHit = -1; selfBlocks = 0;
    currentPath = fadeFromPath = 0; pathFade = 1.0f;
    pathFadeStep = (float) (1.0 / (0.02 * sampleRate));
    splitLP.reset(); splitHP.reset(); lowCut.reset();

    scopeHop = juce::jmax (1, (int) (0.005 * sampleRate));
    scopeCount = 0; scopeLevel = scopeAmount = 0.0f; scopeHit = false;
}

bool AutoChainProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.inputBuses[1];
        if (sc != juce::AudioChannelSet::disabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

// ── Audio ─────────────────────────────────────────────────────────────────────
void AutoChainProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin (getMainBusNumOutputChannels(), buffer.getNumChannels(), delayBuf.getNumChannels());

    // Sidechain (may be absent or disabled: then KICK mode never ducks)
    juce::AudioBuffer<float> sc;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
        sc = getBusBuffer (buffer, true, 1);
    int scCh = sc.getNumChannels();

    // Logic (and some hosts) feed the track's own audio into the sidechain when none is picked.
    // Ducking a bass on its own notes sounds like clicks, so treat that as "no sidechain".
    if (scCh > 0 && buffer.getNumChannels() > 0)
    {
        const float* a = buffer.getReadPointer (0);
        const float* b = sc.getReadPointer (0);
        float diff = 0.0f, energy = 0.0f;
        for (int i = 0; i < n; ++i) { diff = juce::jmax (diff, std::abs (a[i] - b[i])); energy = juce::jmax (energy, std::abs (b[i])); }
        selfBlocks = (energy > 1.0e-5f && diff < 1.0e-6f) ? juce::jmin (selfBlocks + 1, 1000) : 0;
    }
    const bool scIsSelf = selfBlocks >= 2;
    sidechainIsSelf.store (scIsSelf);
    if (scIsSelf) scCh = 0;

    // Locked features fall back to their free equivalents (so automation can't bypass the lock).
    const bool pro     = proUnlocked.load();
    const int  trig    = pro ? (int) pTrigger->load() : (int) tempo;
    const int  shapeP  = (int) pShape->load();
    const int  shape   = (pro || shapeP != draw) ? shapeP : (int) classic;
    const int  target  = pro ? (int) pTarget->load() : (int) volume;
    const bool lowOnly = pLowOnly->load() > 0.5f;
    const float mixRaw = juce::jlimit (0.0f, 1.0f, pMix->load() / 100.0f);
    const float mix    = target == volume ? std::pow (mixRaw, 0.7f) : mixRaw;
    const float sens   = juce::jlimit (0.0f, 1.0f, pSens->load() / 100.0f);
    const float absThresh = juce::Decibels::decibelsToGain (-24.0f - 30.0f * sens);
    const float relThresh = 0.6f - 0.4f * sens;
    const float cutMax    = pCutoff->load();

    const float xo = pXover->load();
    if (std::abs (xo - lastXover) > 0.01f)
    {
        splitLP.setCutoffFrequency (xo); splitHP.setCutoffFrequency (xo); lastXover = xo;
    }

    // Transport
    double bpm = 120.0, ppqStart = -1.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())         bpm = *b;
            if (auto p = pos->getPpqPosition()) ppqStart = *p;
            playing = pos->getIsPlaying();
        }
    if (wrapperType == wrapperType_Standalone)
    {
        // No DAW transport: run a 120 BPM clock so TEMPO mode still moves.
        playing = true; bpm = 120.0; ppqStart = freePpq;
        freePpq += n * bpm / (60.0 * sr);
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    lastBpm.store (bpm);

    const double beatsPerSample = bpm / (60.0 * sr);
    const double laBeats  = lookahead * beatsPerSample;   // host delays everyone else by our latency
    const double rateBeat = rateInBeats ((int) pRate->load());
    const double stepSec  = (trig == tempo ? rateBeat : 1.0) * 60.0 / bpm;
    const double relSec   = juce::jmax (0.005, stepSec * pRelease->load() / 100.0);
    const double envLen   = shape == draw ? relSec : pAttack->load() / 1000.0 + pHold->load() / 1000.0 + relSec;

    const double atkSec    = shape == draw ? 0.0 : pAttack->load() / 1000.0;
    const int    hitDelay  = juce::jmax (0, lookahead - (int) std::round (atkSec * sr)); // start the attack early
    const double atkBeats  = atkSec / 60.0 * bpm;
    const float atk = std::exp (-1.0f / (0.0005f * (float) sr));
    const float rls = std::exp (-1.0f / (0.03f * (float) sr));
    const float pkd = std::exp (-1.0f / (2.0f * (float) sr));
    const int lockoutLen = (int) (0.04 * sr);

    for (int i = 0; i < n; ++i)
    {
        bool hit = false;

        // ── Triggers ──
        if (trig == kick)
        {
            float x = 0.0f;
            for (int c = 0; c < scCh; ++c) x += sc.getSample (c, i);
            if (scCh > 0) x /= (float) scCh;
            const float r = std::abs (kickLP.processSample (x));
            envFast = r > envFast ? atk * envFast + (1 - atk) * r : rls * envFast + (1 - rls) * r;
            peakTrack = juce::jmax (envFast, peakTrack * pkd);
            silentSamples = std::abs (x) > 1.0e-4f ? 0 : silentSamples + 1;

            const float level = juce::jmax (absThresh, peakTrack * relThresh);
            if (lockout > 0) --lockout;
            // Onset detection: after a hit, re-arm once the kick has fallen 4 dB from its own peak;
            // then a new hit needs to clear the trigger level AND jump 6 dB above the lowest point
            // since re-arming. So a kick landing in the previous kick's tail still counts, and a
            // decaying tail never retriggers itself.
            if (! armed)
            {
                hitPeak = juce::jmax (hitPeak, envFast);
                if (lockout == 0 && envFast < 0.6f * hitPeak) { armed = true; valley = envFast; }
            }
            else
            {
                valley = juce::jmin (valley, envFast);
                if (lockout == 0 && envFast > level && envFast > 2.0f * valley)
                { if (pendingHit < 0) pendingHit = hitDelay; armed = false; lockout = lockoutLen; hitPeak = envFast; }
            }
            if (pendingHit >= 0 && pendingHit-- == 0) hit = true;
        }
        else if (playing && ppqStart >= 0.0)
        {
            const double p = ppqStart + i * beatsPerSample - laBeats + juce::jmin (atkBeats, laBeats);
            if (std::floor (p / rateBeat) > std::floor ((p - beatsPerSample) / rateBeat)) hit = true;
        }

        // Sidechain, delayed by our lookahead so it lines up with the ducked audio (and the curve).
        float scDel = 0.0f;
        if (scCh > 0)
        {
            float x = 0.0f;
            for (int c = 0; c < scCh; ++c) x += sc.getSample (c, i);
            x /= (float) scCh;
            scPeakDecay = juce::jmax (std::abs (x), scPeakDecay * 0.99995f);
            if (lookahead > 0) { scDel = scDelay[(size_t) scDelayPos]; scDelay[(size_t) scDelayPos] = x; scDelayPos = (scDelayPos + 1) % lookahead; }
            else scDel = x;
        }

        if (hit)
        {
            commitCapture();
            capCur.fill (0.0f); capBin = 0; capMax = 0; capValid = scCh > 0;
            startAmount = lastAtten; tSince = 0.0; scopeHit = true;
        }
        if (capBin >= 0)
        {
            const int b = (int) (tSince * 1000.0);
            if (b >= kickBins) commitCapture();
            else { capCur[(size_t) b] = juce::jmax (capCur[(size_t) b], std::abs (scDel)); capMax = juce::jmax (capMax, b + 1); }
        }

        const float atten = tSince < envLen ? envelopeAt (tSince, stepSec, shape) : 0.0f;
        lastAtten = atten;
        tSince += 1.0 / sr;

        const float targetAmt = juce::jlimit (0.0f, 1.0f, atten * mix);
        amt += juce::jlimit (-maxStep, maxStep, targetAmt - amt);

        const int wantPath = target == filter ? 2 : (lowOnly ? 1 : 0);
        if (wantPath != currentPath) { fadeFromPath = currentPath; currentPath = wantPath; pathFade = 0.0f; }
        if (pathFade < 1.0f) pathFade = juce::jmin (1.0f, pathFade + pathFadeStep);

        if ((i & 3) == 0)
            lowCut.setCutoffFrequency (20.0f * std::pow (cutMax / 20.0f, amt));

        // ── Audio (with lookahead delay) ──
        const float g = 1.0f - amt;
        for (int c = 0; c < numCh; ++c)
        {
            float in = buffer.getSample (c, i);
            if (lookahead > 0)
            {
                const float d = delayBuf.getSample (c, delayPos);
                delayBuf.setSample (c, delayPos, in);
                in = d;
            }
            scopeLevel = juce::jmax (scopeLevel, std::abs (in));

            // Run every path every sample so filters never wake up with stale state.
            const float lp = splitLP.processSample (c, in), hp = splitHP.processSample (c, in);
            const float paths[3] = { in * g, lp * g + hp, lowCut.processSample (c, in) };
            float out = paths[currentPath];
            if (pathFade < 1.0f) out = paths[fadeFromPath] + (out - paths[fadeFromPath]) * pathFade;
            buffer.setSample (c, i, out);
        }
        if (lookahead > 0) delayPos = (delayPos + 1) % lookahead;

        // ── Scope ──
        scopeAmount = juce::jmax (scopeAmount, amt);
        if (++scopeCount >= scopeHop)
        {
            const int w = scopeWrite.load();
            scope[(size_t) w].level.store (scopeLevel);
            scope[(size_t) w].amount.store (scopeAmount);
            scope[(size_t) w].hit.store (scopeHit);
            scopeWrite.store ((w + 1) % scopeSize);
            scopeCount = 0; scopeLevel = scopeAmount = 0.0f; scopeHit = false;
        }
    }

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (scCh == 0) scPeakDecay = 0.0f;
    scInLevel.store (scPeakDecay);
    if (trig == kick && (scCh == 0 || scIsSelf)) silentSamples = (int) (2 * sr) + 1;
    sidechainSilent.store (silentSamples > 2 * sr);
    currentAmount.store (amt);
    envPosition.store (tSince < envLen ? (float) (tSince / envLen) : 1.0f);
}

// ── State ─────────────────────────────────────────────────────────────────────
void AutoChainProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AutoChainProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        loadDrawPointsFromState();
    }
}

juce::AudioProcessorEditor* AutoChainProAudioProcessor::createEditor() { return new AutoChainProAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutoChainProAudioProcessor(); }
