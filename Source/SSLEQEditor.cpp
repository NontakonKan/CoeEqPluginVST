#include "SSLEQEditor.h"
#include "SSLEQPlugin.h"

static const char* PARAM_IDS[7] = {
    "band200", "band300", "band500", "band1k",
    "band2k5", "band8k", "output"
};
static const char* PARAM_LABELS[7] = {
    "200 Hz", "300 Hz", "500 Hz", "1 kHz",
    "2.5 kHz", "8 kHz", "Output"
};

// Colours matching the SSL hardware aesthetic
static const juce::Colour BG_DARK   { 0xff1a1a1a };
static const juce::Colour BG_PANEL  { 0xff242424 };
static const juce::Colour GREEN_LED { 0xff00e676 };
static const juce::Colour AMBER_LED { 0xffffd740 };
static const juce::Colour BLUE_ACC  { 0xff40c4ff };
static const juce::Colour TEXT_DIM  { 0xff888888 };

// ─── Custom LookAndFeel for SSL-style knobs ──────────────────
class SSLKnobLF : public juce::LookAndFeel_V4
{
public:
    explicit SSLKnobLF(juce::Colour accent) : accentColour(accent) {}

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        const float radius = (float)juce::jmin(width, height) * 0.5f - 4.0f;
        const float centreX = (float)x + (float)width  * 0.5f;
        const float centreY = (float)y + (float)height * 0.5f;

        // Track arc
        {
            juce::Path track;
            track.addArc(centreX - radius, centreY - radius,
                         radius * 2.0f, radius * 2.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(juce::Colour(0xff333333));
            g.strokePath(track, juce::PathStrokeType(3.0f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Value arc
        const float angle = rotaryStartAngle
            + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float zeroAngle = rotaryStartAngle
            + 0.5f * (rotaryEndAngle - rotaryStartAngle);
        {
            juce::Path val;
            float a1 = juce::jmin(zeroAngle, angle);
            float a2 = juce::jmax(zeroAngle, angle);
            if (a2 - a1 > 0.01f) {
                val.addArc(centreX - radius, centreY - radius,
                           radius * 2.0f, radius * 2.0f, a1, a2, true);
                g.setColour(accentColour);
                g.strokePath(val, juce::PathStrokeType(3.0f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // Knob body
        g.setColour(juce::Colour(0xff2e2e2e));
        g.fillEllipse(centreX - radius * 0.6f, centreY - radius * 0.6f,
                      radius * 1.2f, radius * 1.2f);
        g.setColour(juce::Colour(0xff555555));
        g.drawEllipse(centreX - radius * 0.6f, centreY - radius * 0.6f,
                      radius * 1.2f, radius * 1.2f, 0.5f);

        // Pointer dot
        const float dotR  = radius * 0.55f;
        const float dotX  = centreX + dotR * std::cos(angle - juce::MathConstants<float>::halfPi);
        const float dotY  = centreY + dotR * std::sin(angle - juce::MathConstants<float>::halfPi);
        g.setColour(accentColour);
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
    }

private:
    juce::Colour accentColour;
};

// ─── Editor ──────────────────────────────────────────────────
SSLEQEditor::SSLEQEditor(SSLEQAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(640, 340);

    auto& apvts = audioProcessor.getAPVTS();

    for (int i = 0; i < NUM_SLIDERS; i++) {
        bool isOutput = (i == NUM_SLIDERS - 1);
        juce::Colour accent = isOutput ? AMBER_LED
                            : (i == 0 ? BLUE_ACC : GREEN_LED);

        auto* lf = new SSLKnobLF(accent);
        sliders[i].setLookAndFeel(lf);
        sliders[i].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 16);
        sliders[i].setColour(juce::Slider::textBoxTextColourId, accent);
        sliders[i].setColour(juce::Slider::textBoxBackgroundColourId,
                             juce::Colour(0xff1a1a1a));
        sliders[i].setColour(juce::Slider::textBoxOutlineColourId,
                             juce::Colour(0x00000000));
        addAndMakeVisible(sliders[i]);

        labels[i].setText(PARAM_LABELS[i], juce::dontSendNotification);
        labels[i].setFont(juce::Font(juce::FontOptions(10.0f)));
        labels[i].setColour(juce::Label::textColourId, TEXT_DIM);
        labels[i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(labels[i]);

        attachments[i] = std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, PARAM_IDS[i], sliders[i]);
    }

    startTimerHz(30);
}

SSLEQEditor::~SSLEQEditor()
{
    stopTimer();
    for (auto& s : sliders)
        s.setLookAndFeel(nullptr);
}

void SSLEQEditor::timerCallback()
{
    buildFreqCurve();
    repaint(displayArea.toNearestInt());
}

void SSLEQEditor::buildFreqCurve()
{
    freqCurve.clear();
    const float W = displayArea.getWidth();
    const float H = displayArea.getHeight();
    const float cx = displayArea.getX();
    const float cy = displayArea.getY();

    const double fMin = 20.0, fMax = 22000.0;
    const double dbMin = -16.0, dbMax = 16.0;

    auto toX = [&](double f) -> float {
        return cx + W * (float)(std::log10(f / fMin) / std::log10(fMax / fMin));
    };
    auto toY = [&](double db) -> float {
        return cy + H * 0.5f - (float)(db / (dbMax - dbMin) * 0.85 * H);
    };

    const int steps = 256;
    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        double f = fMin * std::pow(fMax / fMin, t);
        double db = audioProcessor.getMagnitude(f);
        float px = toX(f);
        float py = toY(db);
        if (i == 0) freqCurve.startNewSubPath(px, py);
        else        freqCurve.lineTo(px, py);
    }
}

void SSLEQEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(BG_DARK);

    // Header bar
    g.setColour(BG_PANEL);
    g.fillRect(0, 0, getWidth(), 36);
    g.setColour(juce::Colour(0xff333333));
    g.drawHorizontalLine(36, 0.0f, (float)getWidth());

    // Title
    g.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("bold")));
    g.setColour(GREEN_LED);
    g.drawText("SSL  \xc2\xb7  6-BAND EQ", 16, 0, 260, 36,
               juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.setColour(TEXT_DIM);
    g.drawText("PROPORTIONAL-Q  \xc2\xb7  LOW SHELF / 4\xc3\x97 PEAK / HIGH SHELF",
               0, 0, getWidth() - 12, 36, juce::Justification::centredRight);

    // Display area background
    g.setColour(juce::Colour(0xff0a120a));
    g.fillRoundedRectangle(displayArea, 4.0f);
    g.setColour(juce::Colour(0xff1a2e1a));
    g.drawRoundedRectangle(displayArea, 4.0f, 0.5f);

    // Grid lines
    g.setColour(juce::Colour(0x18004000));
    const float W = displayArea.getWidth();
    const float H = displayArea.getHeight();
    const float cx = displayArea.getX();
    const float cy = displayArea.getY();
    auto toX = [&](double f) -> float {
        return cx + W * (float)(std::log10(f / 20.0) / std::log10(22000.0 / 20.0));
    };
    auto toY = [&](double db) -> float {
        return cy + H * 0.5f - (float)(db / 32.0 * 0.85f * H);
    };

    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        g.drawVerticalLine((int)toX(f), cy, cy + H);
    for (double db : { -15.0, -10.0, -5.0, 0.0, 5.0, 10.0, 15.0 })
        g.drawHorizontalLine((int)toY(db), cx, cx + W);

    // Zero line slightly brighter
    g.setColour(juce::Colour(0x2800c850));
    g.drawHorizontalLine((int)toY(0.0), cx, cx + W);

    // Freq response curve
    if (!freqCurve.isEmpty()) {
        // Fill under curve
        juce::Path filled(freqCurve);
        filled.lineTo(cx + W, cy + H);
        filled.lineTo(cx, cy + H);
        filled.closeSubPath();
        juce::ColourGradient grad(GREEN_LED.withAlpha(0.18f), 0, cy,
                                  GREEN_LED.withAlpha(0.0f),  0, cy + H, false);
        g.setGradientFill(grad);
        g.fillPath(filled);

        // Line
        g.setColour(GREEN_LED.withAlpha(0.9f));
        g.strokePath(freqCurve, juce::PathStrokeType(1.5f));
    }

    // Freq labels at bottom of display
    g.setFont(juce::Font(juce::FontOptions(8.0f)));
    g.setColour(juce::Colour(0xff2a4a2a));
    for (auto [f, lbl] : std::initializer_list<std::pair<double, const char*>>{
            {50,"50"},{100,"100"},{200,"200"},{500,"500"},
            {1000,"1k"},{2000,"2k"},{5000,"5k"},{10000,"10k"}}) {
        float fx = toX(f);
        g.drawText(lbl, (int)fx - 10, (int)(cy + H) - 12, 20, 12,
                   juce::Justification::centred);
    }
}

void SSLEQEditor::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // Display: top area below header
    displayArea = juce::Rectangle<float>(12.0f, 44.0f,
                                         (float)W - 24.0f, 110.0f);
    buildFreqCurve();

    // Knobs row at bottom
    const int knobAreaTop = 164;
    const int knobW = W / NUM_SLIDERS;

    for (int i = 0; i < NUM_SLIDERS; i++) {
        const int x = i * knobW;
        labels[i].setBounds(x, knobAreaTop, knobW, 16);
        sliders[i].setBounds(x + 4, knobAreaTop + 16, knobW - 8, knobW - 8 + 20);
    }
}
