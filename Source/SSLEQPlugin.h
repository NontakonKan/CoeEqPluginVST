#pragma once
#include <JuceHeader.h>

// ─── 1. DSP ENGINE (ประมวลผลเสียงจริง) ─────────────────────────
class SSLEQProcessor {
public:
    SSLEQProcessor() {
        // กำหนดความถี่คงที่สำหรับ 6 Band สไตล์ SSL
        frequencies[0] = 200.0f;  // Low
        frequencies[1] = 300.0f;  // Mid 1
        frequencies[2] = 500.0f;  // Mid 2
        frequencies[3] = 1000.0f; // Mid 3
        frequencies[4] = 2500.0f; // Mid 4
        frequencies[5] = 8000.0f; // High
    }

    void prepareToPlay(double sr, int samples) {
        currentSampleRate = sr;
        reset();
    }

    void setSampleRate(double sampleRate) { currentSampleRate = sampleRate; }
    
    void reset() {
        for (int i = 0; i < 6; ++i) {
            filterL[i].reset();
            filterR[i].reset();
        }
    }

    void setBandGain(int index, float gainDB) {
        if (index < 0 || index >= 6 || currentSampleRate <= 0) return;
        
        auto gain = juce::Decibels::decibelsToGain(gainDB);
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs;

        if (index == 0) // Low Shelf
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, frequencies[index], 0.707f, gain);
        else if (index == 5) // High Shelf
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, frequencies[index], 0.707f, gain);
        else // Peak Filter
            coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, frequencies[index], 1.0f, gain);

        if (coeffs) {
            filterL[index].coefficients = coeffs;
            filterR[index].coefficients = coeffs;
        }
    }

    void setOutputGain(float gainDB) {
        outputGain = juce::Decibels::decibelsToGain(gainDB);
    }

    void processBlock(float* L, float* R, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            float sL = L[i];
            float sR = R[i];

            for (int j = 0; j < 6; ++j) {
                sL = filterL[j].processSample(sL);
                sR = filterR[j].processSample(sR);
            }

            L[i] = sL * outputGain;
            R[i] = sR * outputGain;
        }
    }

    double getMagnitudeAtFreq(double freq) const {
        double mag = 1.0;
        for (int i = 0; i < 6; ++i)
            mag *= filterL[i].coefficients->getMagnitudeForFrequency(freq, currentSampleRate);
        return juce::Decibels::gainToDecibels(mag);
    }

private:
    double currentSampleRate = 44100.0;
    float outputGain = 1.0f;
    float frequencies[6];
    
    juce::dsp::IIR::Filter<float> filterL[6];
    juce::dsp::IIR::Filter<float> filterR[6];
};

// ─── 2. PARAMETERS & AUDIO PROCESSOR ──────────────────────────
static const juce::StringArray PARAM_ID_LIST { "band200", "band300", "band500", "band1k", "band2k5", "band8k", "output" };
static const juce::StringArray PARAM_NAME_LIST { "EQ 200 Hz", "EQ 300 Hz", "EQ 500 Hz", "EQ 1 kHz", "EQ 2.5 kHz", "EQ 8 kHz", "Output" };

class SSLEQAudioProcessor : public juce::AudioProcessor
{
public:
    SSLEQAudioProcessor();

    void prepareToPlay(double sr, int samples) override;

    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SSL EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& d) override;
    void setStateInformation(const void* d, int s) override;

    double getMagnitude(double freq) const { return eqEngine.getMagnitudeAtFreq(freq); }
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    SSLEQProcessor eqEngine;
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    void syncParamsToProcessor();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SSLEQAudioProcessor)
};