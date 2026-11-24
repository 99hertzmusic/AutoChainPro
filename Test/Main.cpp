// AutoChain Pro offline test: click measurement + editor snapshot.
// Usage: AutoChainProTest <outdir>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "LicenceTests.h"
#include <cmath>

struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 125.0, ppq = 0.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        return p;
    }
};

static void setParam (AutoChainProAudioProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

// Kick: pitch sweep 160->48 Hz, ~250 ms decay.
static float kickSample (double t)
{
    if (t < 0 || t > 0.4) return 0.0f;
    const double f0 = 48.0, f1 = 160.0, k = 30.0;
    const double phase = 2 * juce::MathConstants<double>::pi * (f0 * t + (f1 - f0) * (1 - std::exp (-k * t)) / k);
    return (float) (0.9 * std::sin (phase) * std::exp (-t / 0.12));
}

struct Result { float hfPeakDb; float duckDb; bool self; };

// mode: 0 tempo, 1 sidechain kick, 2 sidechain == main input (Logic "None"), 3 sidechain silent
static Result run (int mode, int shape, float extraSeconds = 0.0f, bool printSteps = false, const char* wavOut = nullptr)
{
    const double sr = 48000; const int bs = 256;
    AutoChainProAudioProcessor p;
    { hz::LicenceInfo li; li.kind = hz::LicenceInfo::pro; p.setLicenceInfo (li); }
    { auto L = p.getBusesLayout(); L.inputBuses.getReference (1) = juce::AudioChannelSet::stereo(); bool ok = p.setBusesLayout (L);
      static bool once = false; if (! once) { once = true; std::printf ("layout ok=%d sc enabled=%d inCh=%d\n", (int) ok, (int) p.getBus (true, 1)->isEnabled(), p.getTotalNumInputChannels()); } }
    p.setRateAndBufferSizeDetails (sr, bs);
    FakePlayHead ph; p.setPlayHead (&ph);
    setParam (p, "trigger", mode == 0 ? (float) AutoChainProAudioProcessor::tempo : (float) AutoChainProAudioProcessor::kick);
    setParam (p, "shape", (float) shape);
    setParam (p, "rate", 1.0f); // 1/4
    p.prepareToPlay (sr, bs);

    const double secs = 4.0 + extraSeconds;
    const int total = (int) (secs * sr);
    const double beat = 60.0 / ph.bpm;
    juce::AudioBuffer<float> buf (4, bs);
    juce::AudioBuffer<float> out (2, total);
    double phase = 0;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = juce::jmin (bs, total - pos);
        buf.setSize (4, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const double t = (pos + i) / sr;
            // bass: 55 Hz saw-ish (sine + 2nd + 3rd harmonics, all < 200 Hz)
            phase += 55.0 / sr;
            const double ph2 = 2 * juce::MathConstants<double>::pi * phase;
            const float b = (float) (0.5 * std::sin (ph2) + 0.2 * std::sin (2 * ph2) + 0.1 * std::sin (3 * ph2));
            const double tb = std::fmod (t, beat);
            const float k = kickSample (tb);
            buf.setSample (0, i, b); buf.setSample (1, i, b);
            float sc = 0.0f;
            if (mode == 1) sc = k;
            if (mode == 2) sc = b;
            buf.setSample (2, i, sc); buf.setSample (3, i, sc);
        }
        ph.ppq = (pos / sr) / beat;
        juce::MidiBuffer m;
        p.processBlock (buf, m);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }

    // HF content: 2nd-order HP at 1.5 kHz twice (bass has nothing up there; anything left = clicks)
    juce::dsp::IIR::Filter<float> h1, h2;
    h1.coefficients = h2.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 1500.0f);
    float hf = 0.0f, minRms = 1e9f, maxRms = 0.0f;
    const int skip = (int) (0.5 * sr);
    double acc = 0; int cnt = 0; const int win = (int) (0.01 * sr);
    for (int i = 0; i < total; ++i)
    {
        const float x = out.getSample (0, i);
        const float y = h2.processSample (h1.processSample (x));
        if (i > skip) hf = juce::jmax (hf, std::abs (y));
        acc += x * x; if (++cnt == win) { const float r = (float) std::sqrt (acc / win); if (i > skip) { minRms = juce::jmin (minRms, r); maxRms = juce::jmax (maxRms, r); } acc = 0; cnt = 0; }
    }
    if (wavOut)
    {
        juce::File f (wavOut); f.deleteFile();
        juce::WavAudioFormat w;
        if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream().release()))
            if (auto wr = std::unique_ptr<juce::AudioFormatWriter> (w.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
            { os.release(); wr->writeFromAudioSampleBuffer (out, 0, total); }
    }
    juce::ignoreUnused (printSteps);
    return { juce::Decibels::gainToDecibels (hf, -150.0f), juce::Decibels::gainToDecibels (minRms / juce::jmax (1e-9f, maxRms), -150.0f), p.sidechainIsSelf.load() };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    const char* modes[]  = { "tempo", "sc-kick", "sc=input(None)", "sc-silent" };
    const char* shapes[] = { "classic", "hard", "smooth", "gate", "draw" };
    std::printf ("%-16s %-8s %10s %10s\n", "mode", "shape", "click dB", "duck dB");
    for (int m = 0; m < 4; ++m)
        for (int s = 0; s < 5; ++s)
        {
            auto r = run (m, s, 0.0f, false,
                          (m == 0 && s == 0) || (m == 1 && s == 0) || (m == 2 && s == 0)
                              ? outDir.getChildFile (juce::String ("render-") + modes[m] + ".wav").getFullPathName().toRawUTF8() : nullptr);
            std::printf ("%-16s %-8s %10.1f %10.1f %s\n", modes[m], shapes[s], r.hfPeakDb, r.duckDb, r.self ? "self-detected" : "");
        }

    // Editor snapshots. Each processor is fed 3 s of bass + kick sidechain first so the kick shows up.
    auto snap = [&] (const char* name, int trig, int shape, bool feedKick, bool autoFit, int licKind = hz::LicenceInfo::pro, const char* panel = nullptr)
    {
        const double sr = 48000; const int bs = 256;
        AutoChainProAudioProcessor p;
        { hz::LicenceInfo li; li.kind = (hz::LicenceInfo::Kind) licKind; li.email = "buyer@example.com";
          li.expires = licKind == hz::LicenceInfo::trial ? juce::Time::currentTimeMillis() / 1000 + 5 * 86400 - 60 : 0; p.setLicenceInfo (li); }
        { auto L = p.getBusesLayout(); L.inputBuses.getReference (1) = juce::AudioChannelSet::stereo(); p.setBusesLayout (L); }
        p.setRateAndBufferSizeDetails (sr, bs);
        FakePlayHead ph; p.setPlayHead (&ph);
        setParam (p, "trigger", (float) trig);
        setParam (p, "shape", (float) shape);
        p.prepareToPlay (sr, bs);
        const double beat = 60.0 / ph.bpm;
        juce::AudioBuffer<float> buf (4, bs);
        double phase = 0;
        const int total = (int) (3.1 * sr);
        for (int pos = 0; pos < total; pos += bs)
        {
            for (int i = 0; i < bs; ++i)
            {
                const double t = (pos + i) / sr;
                phase += 55.0 / sr;
                const float b = (float) (0.5 * std::sin (2 * juce::MathConstants<double>::pi * phase));
                const float k = feedKick ? kickSample (std::fmod (t, beat)) : 0.0f;
                buf.setSample (0, i, b); buf.setSample (1, i, b); buf.setSample (2, i, k); buf.setSample (3, i, k);
            }
            ph.ppq = (pos / sr) / beat;
            juce::MidiBuffer m;
            p.processBlock (buf, m);
        }
        if (autoFit)
        {
            const bool ok = p.autoShapeFromKick();
            std::printf ("auto fit ok=%d release=%.1f%% points:", (int) ok, p.apvts.getRawParameterValue ("release")->load());
            for (auto pt : p.getDrawPoints()) std::printf (" (%.3f,%.2f)", pt.x, pt.y);
            std::printf ("\n");
        }
        std::printf ("%s: kick captures=%d len=%d ms\n", name, p.kickCaptures.load(), p.kickLenBins.load());
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        if (auto* t = dynamic_cast<juce::Timer*> (ed.get())) t->timerCallback();
        if (panel != nullptr)
            for (auto* c : ed->getChildren())
                if (auto* pp = dynamic_cast<ProPanel*> (c)) pp->show (panel);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::FileOutputStream fo (outDir.getChildFile (name));
        fo.setPosition (0); fo.truncate();
        juce::PNGImageFormat().writeImageToStream (img, fo);
    };
    snap ("ui-default.png", AutoChainProAudioProcessor::tempo, 0, false, false);
    snap ("ui-sidechain-kick.png", AutoChainProAudioProcessor::kick, 0, true, false);
    snap ("ui-sidechain-auto.png", AutoChainProAudioProcessor::kick, 0, true, true);
    snap ("ui-sidechain-missing.png", AutoChainProAudioProcessor::kick, AutoChainProAudioProcessor::draw, false, false);
    snap ("ui-free.png", AutoChainProAudioProcessor::tempo, 0, true, false, hz::LicenceInfo::none);
    snap ("ui-free-upgrade-panel.png", AutoChainProAudioProcessor::tempo, 0, true, false, hz::LicenceInfo::none, "SIDECHAIN FROM YOUR KICK IS A PRO FEATURE");
    snap ("ui-trial.png", AutoChainProAudioProcessor::kick, 0, true, false, hz::LicenceInfo::trial);
    snap ("ui-trial-ended-panel.png", AutoChainProAudioProcessor::kick, 0, true, false, hz::LicenceInfo::trialExpired, "");
    // Licence states (Launch prep renders)
    snap ("licence-1-free.png", AutoChainProAudioProcessor::tempo, 0, false, false, hz::LicenceInfo::none);
    snap ("licence-2-free-panel.png", AutoChainProAudioProcessor::tempo, 0, false, false, hz::LicenceInfo::none, "");
    snap ("licence-3-trial.png", AutoChainProAudioProcessor::kick, 0, true, false, hz::LicenceInfo::trial);
    snap ("licence-4-trial-panel.png", AutoChainProAudioProcessor::kick, 0, true, false, hz::LicenceInfo::trial, "");
    snap ("licence-5-trial-ended.png", AutoChainProAudioProcessor::tempo, 0, false, false, hz::LicenceInfo::trialExpired);
    snap ("licence-6-trial-ended-panel.png", AutoChainProAudioProcessor::tempo, 0, false, false, hz::LicenceInfo::trialExpired, "");
    snap ("licence-7-licensed.png", AutoChainProAudioProcessor::kick, 0, true, false, hz::LicenceInfo::pro);
    snap ("licence-8-licensed-panel.png", AutoChainProAudioProcessor::kick, 0, true, false, hz::LicenceInfo::pro, "");
    snap ("licence-9-saved-upgrade-settings.png", AutoChainProAudioProcessor::kick, AutoChainProAudioProcessor::draw, true, false, hz::LicenceInfo::trialExpired);

    // Licence keys passed on the command line: name=key
    for (int a = 2; a < argc; ++a)
    {
        const juce::String arg (argv[a]);
        const auto info = hz::Licence::verify (arg.fromFirstOccurrenceOf ("=", false, false), "autochainpro");
        const char* kinds[] = { "none", "trial", "pro", "trialExpired", "invalid" };
        std::printf ("key %-14s -> %-12s email=%s daysLeft=%d\n", arg.upToFirstOccurrenceOf ("=", false, false).toRawUTF8(),
                     kinds[(int) info.kind], info.email.toRawUTF8(), info.daysLeft());
    }
    hztest::runLicenceTests<AutoChainProAudioProcessor> ("autochainpro", [] (AutoChainProAudioProcessor& p) { return p.proUnlocked.load(); });
    hztest::runSavedSettingsTests<AutoChainProAudioProcessor> ([] (AutoChainProAudioProcessor& p)
    {
        setParam (p, "trigger", (float) AutoChainProAudioProcessor::kick);
        setParam (p, "shape", (float) AutoChainProAudioProcessor::draw);
        setParam (p, "target", (float) AutoChainProAudioProcessor::filter);
    });
    {
        // and the audio still falls back to free: no licence + saved SIDECHAIN = tempo ducking
        AutoChainProAudioProcessor p;
        p.setLicenceInfo ({});
        setParam (p, "trigger", (float) AutoChainProAudioProcessor::kick);
        hztest::expect (! p.proUnlocked.load() && p.usesUpgradeSettings(), "no licence + saved SIDECHAIN: flagged, plays as free");
    }
    return hztest::finish();
}
