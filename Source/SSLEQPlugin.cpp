#include "SSLEQPlugin.h"
#include "SSLEQEditor.h"

// ─── Parameter layout ────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
SSLEQAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Input Gain
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "input", 1 },
        "Input Gain",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // 6 EQ Bands
    for (int i = 1; i < 7; i++) {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { PARAM_ID_LIST[i], 1 },
            PARAM_NAME_LIST[i],
            juce::NormalisableRange<float>(-15.0f, 15.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
    }

    // Output Gain
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "output", 1 },
        "Output Gain",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

SSLEQAudioProcessor::SSLEQAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SSL_EQ_STATE", createParams())
{
}

void SSLEQAudioProcessor::prepareToPlay(double sampleRate, int samples)
{
    eqEngine.prepareToPlay(sampleRate, samples);
}

void SSLEQAudioProcessor::releaseResources() { eqEngine.reset(); }

void SSLEQAudioProcessor::syncParamsToProcessor()
{
    // Input Gain
    float in = apvts.getRawParameterValue("input")->load();
    eqEngine.setInputGain(in);

    // 6 EQ Bands
    for (int i = 1; i < 7; i++) {
        float v = apvts.getRawParameterValue(PARAM_ID_LIST[i])->load();
        eqEngine.setBandGain(i - 1, v);
    }

    // Output Gain
    float out = apvts.getRawParameterValue("output")->load();
    eqEngine.setOutputGain(out);
}

void SSLEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    
    // 1. อัปเดตค่าจาก Slider เข้าสู่ตัวแปรใน eqEngine
    syncParamsToProcessor(); 

    // 2. ดึง Pointer ของข้อมูลเสียง
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(buffer.getNumChannels() > 1 ? 1 : 0);

    // 3. ส่งไปให้ DSP คำนวณ (ถ้าขั้นตอนนี้ว่างเปล่า เสียงจะไม่เปลี่ยน)
    eqEngine.processBlock(L, R, buffer.getNumSamples());
}
juce::AudioProcessorEditor* SSLEQAudioProcessor::createEditor()
{
    return new SSLEQEditor(*this);
}

void SSLEQAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void SSLEQAudioProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SSLEQAudioProcessor();
}
bool SSLEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // ตรวจสอบว่า DAW พยายามส่งสัญญาณแบบ Stereo มาหรือไม่ (SSL EQ ส่วนใหญ่ทำงานเป็น Stereo)
    auto stereo = juce::AudioChannelSet::stereo();
    
    // ตรวจสอบว่าทั้ง Input และ Output เป็น Stereo หรือไม่
    if (layouts.getMainInputChannelSet() != stereo ||
        layouts.getMainOutputChannelSet() != stereo)
        return false;

    return true;
}