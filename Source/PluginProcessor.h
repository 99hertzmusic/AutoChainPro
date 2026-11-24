#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "Licence/Licence.h"

// AutoChain Pro
//  - SIDECHAIN trigger: ducks only when the kick on the sidechain input actually hits (no kick = no duck).
//    The plugin can't know which track the kick is on: pick it in the host's sidechain menu.
//  - TEMPO trigger: ducks on a tempo-synced rate (1/2 .. 1/16, triplets), like the free AutoChain.
//  - Shapes: Classic / Hard / Smooth / Gate, or DRAW your own curve.
//  - ENV target: VOLUME (optionally low band only) or FILTER (sweeps a low-cut, LFO Tool style).
//  - 10 ms lookahead: the attack starts early so the duck bottoms out exactly as the kick lands (reported as latency).
class AutoChainProAudioProcessor : public juce::AudioProcessor
{
public:
    enum Trigger { kick = 0, tempo };
    enum Shape   { classic = 0, hard, smooth, gate, draw };
    enum Target  { volume = 0, filter };

    static const juce::StringArray rateNames;   // "1/2", "1/4", ...
    static double rateInBeats (int index);

    AutoChainProAudioProcessor();
    ~AutoChainProAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AutoChain Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ── Free / Pro ──────────────────────────────────────────────────────────
    // Free: tempo ducking, preset shapes, envelope, depth, low-only.
    // Pro (paid or 7-day trial): sidechain from the kick, kick view + AUTO, drawn curves, filter ducking.
    hz::Licence licence { "autochainpro" };
    hz::LicenceInfo licenceInfo;                 // message thread
    std::atomic<bool> proUnlocked { false };     // audio thread reads this
    std::atomic<int> licenceVersion { 0 };       // bumps on every change, so open editors can follow
    void setLicenceInfo (const hz::LicenceInfo& i) { licenceInfo = i; proUnlocked.store (i.unlocked()); ++licenceVersion; }
    std::unique_ptr<hz::LicenceWatcher> licenceWatcher;   // picks up keys saved by other instances / trial expiry
    static bool isProValue (const juce::String& paramId, int value);
    // True if the current settings include upgrade-only values (kept in the session, played as free).
    bool usesUpgradeSettings() const;

    // ── Shapes ──────────────────────────────────────────────────────────────
    // Attenuation 0 (no duck) .. 1 (full duck) over the release, x in [0,1].
    static float releaseShape (int shape, float x);

    // Drawn curve: points are (x, gain) with x in [0,1], gain 1 = full volume. Message thread only.
    std::vector<juce::Point<float>> getDrawPoints() const { return drawPoints; }
    void setDrawPoints (std::vector<juce::Point<float>> pts);
    static std::vector<juce::Point<float>> defaultDrawPoints();
    static float gainAt (const std::vector<juce::Point<float>>& pts, float x);

    // ── For the editor ──────────────────────────────────────────────────────
    static constexpr int scopeSize = 600;              // ~3 s of history at 5 ms per column
    struct ScopeColumn { std::atomic<float> level { 0 }, amount { 0 }; std::atomic<bool> hit { false }; };
    std::array<ScopeColumn, scopeSize> scope;
    std::atomic<int> scopeWrite { 0 };

    std::atomic<float> envPosition   { 1.0f };  // 0..1 through the current envelope, 1 = idle
    std::atomic<float> currentAmount { 0.0f };
    std::atomic<double> lastBpm      { 120.0 };
    std::atomic<bool> sidechainSilent { true }; // no sidechain signal for 2 s
    std::atomic<bool> sidechainIsSelf { false }; // host is feeding our own input as the sidechain (Logic "None")

    // Seconds for the current settings (used by the editor to draw non-draw shapes on a time axis).
    float stepSeconds() const;
    float envLengthSeconds() const;     // length of the whole duck envelope (curve x = 0..1)

    // ── Kick capture (sidechain, lined up with the curve: bin b = b ms after the duck starts) ──
    static constexpr int kickBins = 800;
    std::array<std::atomic<float>, kickBins> kickShape {};  // averaged |kick| per ms
    std::atomic<int>   kickLenBins  { 0 };                  // how many bins hold real kick data
    std::atomic<int>   kickCaptures { 0 };                  // 0 = no kick seen yet
    std::atomic<float> scInLevel    { 0.0f };               // sidechain input peak (for the KICK IN meter)

    // AUTO: fit a drawn curve to the captured kick (duck through the punch, open as the tail dies).
    // Message thread. Returns false if no kick has been captured yet.
    bool autoShapeFromKick();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void rebuildTable (const std::vector<juce::Point<float>>& pts);
    void loadDrawPointsFromState();
    float envelopeAt (double t, double stepSec, int shape) const;

    std::vector<juce::Point<float>> drawPoints;
    static constexpr int tableSize = 512;
    std::array<std::array<float, tableSize>, 2> tables {};
    std::atomic<int> activeTable { 0 };
    static constexpr double lookaheadSec = 0.010;

    // Params
    std::atomic<float>* pTrigger = nullptr; std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pShape = nullptr;   std::atomic<float>* pTarget = nullptr;
    std::atomic<float>* pMix = nullptr;     std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pHold = nullptr;    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pSens = nullptr;    std::atomic<float>* pLowOnly = nullptr;
    std::atomic<float>* pXover = nullptr;   std::atomic<float>* pCutoff = nullptr;

    double sr = 44100.0;
    double freePpq = 0.0;     // free-running beat clock when there's no host transport (standalone)

    // Envelope state
    double tSince = 1.0e9;      // seconds since last trigger
    float  startAmount = 0.0f;  // attenuation at the moment of retrigger (click-free restarts)
    float  lastAtten = 0.0f;
    float  amt = 0.0f;          // applied attenuation after the slew limiter
    float  maxStep = 0.01f;     // slew limit per sample (no instant jumps = no clicks)
    int    pendingHit = -1;     // samples until a detected kick fires the envelope (-1 = none)
    int    selfBlocks = 0;      // consecutive blocks where sidechain == main input

    // Path crossfade (Volume / Low only / Filter) so switching never clicks
    int   currentPath = 0, fadeFromPath = 0;
    float pathFade = 1.0f, pathFadeStep = 0.001f;

    // Kick detector (sidechain, low-passed)
    juce::dsp::IIR::Filter<float> kickLP;
    float envFast = 0.0f, peakTrack = 0.0f, hitPeak = 0.0f, valley = 0.0f;
    bool  armed = true;
    int   lockout = 0;
    int   silentSamples = 0;

    // Lookahead
    int lookahead = 0;
    juce::AudioBuffer<float> delayBuf;
    int delayPos = 0;

    // Low-only split and low-cut filter
    juce::dsp::LinkwitzRileyFilter<float> splitLP, splitHP;
    juce::dsp::StateVariableTPTFilter<float> lowCut;
    float lastXover = -1.0f;

    // Kick capture
    std::array<float, kickBins> capCur {};
    int   capBin = -1, capMax = 0;   // capBin < 0 = not capturing
    bool  capValid = false;
    std::vector<float> scDelay; int scDelayPos = 0;
    float scPeakDecay = 0.0f;
    void commitCapture();

    // Scope accumulation
    int   scopeCount = 0, scopeHop = 220;
    float scopeLevel = 0.0f, scopeAmount = 0.0f;
    bool  scopeHit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoChainProAudioProcessor)
};
