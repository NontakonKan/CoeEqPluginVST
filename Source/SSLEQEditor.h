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

    // One rotary slider per parameter (Input + 6 EQ bands + Output)
    static constexpr int NUM_SLIDERS = 8;
    juce::Slider sliders[NUM_SLIDERS];
    juce::Label  labels[NUM_SLIDERS];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachments[NUM_SLIDERS];

    // Frequency response display
    juce::Path   freqCurve;
    juce::Rectangle<float> displayArea;

    // Logo images shown on the top-right
    juce::Image logoImage1;
    juce::Image logoImage2;

    void buildFreqCurve();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SSLEQEditor)
};
