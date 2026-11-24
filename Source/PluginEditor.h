#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class HardwareLookAndFeel;

// Row/grid of hardware keys bound to a choice (or bool) parameter.
// `values` maps each key to a parameter index, so keys can be shown in any order.
class SegmentControl : public juce::Component
{
public:
    SegmentControl (juce::RangedAudioParameter& param, juce::StringArray labels, juce::Array<int> values = {});
    void resized() override;
    void setDimmed (bool d);
    void setLocked (int value, bool locked);          // draws a padlock on that key
    std::function<bool (int value)> allowValue;       // return false to refuse a click (e.g. a locked Pro value)
    int columns = 0;                 // 0 = one row
    int gap = 8;
private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::Array<int> vals;
    juce::ParameterAttachment attachment;
    bool dimmed = false;
};

// The duck shape on an LED screen, with the captured kick drawn behind it.
// DRAW mode: points can be dragged, added (click) and removed (double/right click).
// Clicking while on a preset shape copies it into DRAW so it can be tweaked.
class CurveView : public juce::Component
{
public:
    explicit CurveView (AutoChainProAudioProcessor& p) : proc (p) {}
    std::function<void()> onLockedClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> plot() const;
    juce::Point<float> toScreen (juce::Point<float> p) const;
    juce::Point<float> fromScreen (juce::Point<float> s) const;
    int hitPoint (juce::Point<float> s) const;
    bool isDraw() const;
    float displayGain (float x) const;
    AutoChainProAudioProcessor& proc;
    int dragIndex = -1;
};

// Upgrade panel: what Pro adds, start the 7-day trial, paste a key, or buy.
class ProPanel : public juce::Component
{
public:
    explicit ProPanel (AutoChainProAudioProcessor& p);
    void show (const juce::String& reason);
    void licenceChanged();   // the processor's licence changed (another instance, trial expiry): redraw if open
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;   // click outside the card closes it
    std::function<void()> onLicenceChanged;
    void setMessage (const juce::String& text, bool isError);   // (also used by the test renders)
private:
    void refresh();
    void licenceResult (const juce::String& error, const juce::String& okMessage);
    juce::Rectangle<int> card() const;
    AutoChainProAudioProcessor& proc;
    juce::String reason, message;
    bool messageIsError = false, busy = false;
    juce::TextEditor emailBox, keyBox;
    juce::ToggleButton subscribe { "Send me tips and offers" };
    juce::TextButton trialButton { "START 7-DAY FREE TRIAL" }, unlockButton { "UNLOCK" },
                     buyButton { "UPGRADE" }, closeButton { "CLOSE" },
                     activateButton { "ACTIVATE THIS COMPUTER" }, moveButton { "MOVE TO ANOTHER COMPUTER" };
};

class ScopeView : public juce::Component
{
public:
    explicit ScopeView (AutoChainProAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    juce::String warning, warningSub;   // shown over the scope when the sidechain isn't set up
private:
    AutoChainProAudioProcessor& proc;
};

class AutoChainProAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AutoChainProAudioProcessorEditor (AutoChainProAudioProcessor&);
    ~AutoChainProAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::String label, paramId;
    };
    Knob& addKnob (const juce::String& id, const juce::String& label);
    Knob* knob (const juce::String& id);
    void renderBackground();

    AutoChainProAudioProcessor& proc;
    std::unique_ptr<HardwareLookAndFeel> laf;

    SegmentControl trigger, rate, shape, target, lowOnly;
    juce::TextButton autoButton { "AUTO" };
    CurveView curve;
    ScopeView scopeView;
    juce::TextButton licenceButton;
    ProPanel proPanel;
    void openPro (const juce::String& reason);
    void updateLocks();
    juce::OwnedArray<Knob> knobs;

    juce::Image background;
    juce::Rectangle<int> ledArea, meterArea, secTrigger, secShape, secEnv, secOutput, hintArea;
    juce::String ledLabel, ledText, flashText;
    bool ledWarn = false, upgradeNudge = false;
    int seenLicenceVersion = -1;
    int flashTicks = 0;
    float meterValue = 0.0f, scMeter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoChainProAudioProcessorEditor)
};
