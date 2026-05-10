#pragma once
#include <JuceHeader.h>
#include "SSLEQPlugin.h"

class SSLEQEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit SSLEQEditor(SSLEQAudioProcessor&);
    ~SSLEQEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SSLEQAudioProcessor& audioProcessor;

    // One rotary slider per parameter (6 EQ bands + output)
    static constexpr int NUM_SLIDERS = 7;
    juce::Slider sliders[NUM_SLIDERS];
    juce::Label  labels[NUM_SLIDERS];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachments[NUM_SLIDERS];

    // Frequency response display
    juce::Path   freqCurve;
    juce::Rectangle<float> displayArea;

    void buildFreqCurve();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SSLEQEditor)
};
