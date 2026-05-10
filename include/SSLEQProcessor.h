#pragma once
#include <cmath>
#include <array>
#include <string>

// ============================================================
//  SSL-Style Parametric EQ  — 6-Band + Output Gain
//  DSP: Biquad filters with SSL "proportional-Q" behavior
//  Each peak band uses constant-Q scaling: Q rises with gain
// ============================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Biquad State (Direct Form II Transposed) ───────────────
struct BiquadCoeffs {
    double b0, b1, b2;   // numerator
    double a1, a2;       // denominator (a0 normalised to 1)
};

struct BiquadState {
    double z1 = 0.0, z2 = 0.0;
};

// ─── Single-sample Biquad process ───────────────────────────
inline double processBiquad(double x, BiquadCoeffs& c, BiquadState& s) {
    double y = c.b0 * x + s.z1;
    s.z1     = c.b1 * x - c.a1 * y + s.z2;
    s.z2     = c.b2 * x - c.a2 * y;
    return y;
}

// ─── Filter Design Helpers ──────────────────────────────────

// Peak/Bell  (SSL-style proportional Q: Q = baseQ * sqrt(|gainLinear|))
inline BiquadCoeffs makePeakEQ(double freqHz, double gainDb, double baseQ, double sampleRate) {
    double A     = std::pow(10.0, gainDb / 40.0);           // sqrt of linear gain
    double omega = 2.0 * M_PI * freqHz / sampleRate;
    double sinW  = std::sin(omega);
    double cosW  = std::cos(omega);

    // SSL proportional-Q: Q scales with sqrt(A)  (classic SSL 4000 behaviour)
    double Q     = baseQ * std::sqrt(A);
    double alpha = sinW / (2.0 * Q);

    double b0 =  1.0 + alpha * A;
    double b1 = -2.0 * cosW;
    double b2 =  1.0 - alpha * A;
    double a0 =  1.0 + alpha / A;
    double a1 = -2.0 * cosW;
    double a2 =  1.0 - alpha / A;

    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

// Low-Shelf  (for 200 Hz band — SSL LF shelf option)
inline BiquadCoeffs makeLowShelf(double freqHz, double gainDb, double S, double sampleRate) {
    double A     = std::pow(10.0, gainDb / 40.0);
    double omega = 2.0 * M_PI * freqHz / sampleRate;
    double sinW  = std::sin(omega);
    double cosW  = std::cos(omega);
    double alpha = sinW / 2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);

    double b0 =  A * ((A+1) - (A-1)*cosW + 2*std::sqrt(A)*alpha);
    double b1 =  2*A * ((A-1) - (A+1)*cosW);
    double b2 =  A * ((A+1) - (A-1)*cosW - 2*std::sqrt(A)*alpha);
    double a0 =       (A+1) + (A-1)*cosW + 2*std::sqrt(A)*alpha;
    double a1 = -2*  ((A-1) + (A+1)*cosW);
    double a2 =       (A+1) + (A-1)*cosW - 2*std::sqrt(A)*alpha;

    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

// High-Shelf (for 8 kHz band — SSL HF shelf option)
inline BiquadCoeffs makeHighShelf(double freqHz, double gainDb, double S, double sampleRate) {
    double A     = std::pow(10.0, gainDb / 40.0);
    double omega = 2.0 * M_PI * freqHz / sampleRate;
    double sinW  = std::sin(omega);
    double cosW  = std::cos(omega);
    double alpha = sinW / 2.0 * std::sqrt((A + 1.0/A) * (1.0/S - 1.0) + 2.0);

    double b0 =  A * ((A+1) + (A-1)*cosW + 2*std::sqrt(A)*alpha);
    double b1 = -2*A * ((A-1) + (A+1)*cosW);
    double b2 =  A * ((A+1) + (A-1)*cosW - 2*std::sqrt(A)*alpha);
    double a0 =       (A+1) - (A-1)*cosW + 2*std::sqrt(A)*alpha;
    double a1 =  2*  ((A-1) - (A+1)*cosW);
    double a2 =       (A+1) - (A-1)*cosW - 2*std::sqrt(A)*alpha;

    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

// ─── EQ Band descriptor ─────────────────────────────────────
struct EQBand {
    double freqHz;
    double gainDb;      // ±15 dB
    double baseQ;
    bool   isLowShelf;
    bool   isHighShelf;

    BiquadCoeffs coeffL, coeffR;
    BiquadState  stateL, stateR;

    void update(double sampleRate) {
        if (isLowShelf) {
            coeffL = coeffR = makeLowShelf(freqHz, gainDb, 0.7, sampleRate);
        } else if (isHighShelf) {
            coeffL = coeffR = makeHighShelf(freqHz, gainDb, 0.7, sampleRate);
        } else {
            coeffL = coeffR = makePeakEQ(freqHz, gainDb, baseQ, sampleRate);
        }
    }

    void reset() { stateL = {}; stateR = {}; }

    // process stereo sample
    void process(double& left, double& right) {
        left  = processBiquad(left,  coeffL, stateL);
        right = processBiquad(right, coeffR, stateR);
    }
};

// ─── Main Processor ─────────────────────────────────────────
class SSLEQProcessor {
public:
    static constexpr int NUM_BANDS = 6;

    // Band center frequencies matching SSL 4000 G channel strip
    static constexpr double BAND_FREQS[NUM_BANDS] = {
        200.0, 300.0, 500.0, 1000.0, 2500.0, 8000.0
    };

    // Base Q for each band (SSL proportional-Q style)
    static constexpr double BAND_BASE_Q[NUM_BANDS] = {
        0.71, 0.71, 0.71, 0.71, 0.71, 0.71
    };

    SSLEQProcessor() {
        sampleRate = 44100.0;
        outputGainDb = 0.0;

        for (int i = 0; i < NUM_BANDS; i++) {
            bands[i].freqHz     = BAND_FREQS[i];
            bands[i].gainDb     = 0.0;
            bands[i].baseQ      = BAND_BASE_Q[i];
            bands[i].isLowShelf = (i == 0);          // 200 Hz = Low Shelf
            bands[i].isHighShelf = (i == NUM_BANDS-1); // 8 kHz = High Shelf
        }
        updateAll();
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        updateAll();
    }

    // gain in dB, range [-15, +15]
    void setBandGain(int band, double gainDb) {
        if (band < 0 || band >= NUM_BANDS) return;
        bands[band].gainDb = std::max(-15.0, std::min(15.0, gainDb));
        bands[band].update(sampleRate);
    }

    double getBandGain(int band) const {
        if (band < 0 || band >= NUM_BANDS) return 0.0;
        return bands[band].gainDb;
    }

    // output gain range [-20, +20] dB
    void setOutputGain(double gainDb) {
        outputGainDb = std::max(-20.0, std::min(20.0, gainDb));
    }

    double getOutputGain() const { return outputGainDb; }

    void reset() {
        for (auto& b : bands) b.reset();
    }

    // Process stereo block
    void processBlock(float* left, float* right, int numSamples) {
        double outLinear = std::pow(10.0, outputGainDb / 20.0);
        for (int i = 0; i < numSamples; i++) {
            double l = left[i];
            double r = right[i];
            for (auto& b : bands) b.process(l, r);
            left[i]  = static_cast<float>(l * outLinear);
            right[i] = static_cast<float>(r * outLinear);
        }
    }

    // Compute magnitude response (dB) at given frequency
    double getMagnitudeAtFreq(double freq) const {
        double totalDb = 0.0;
        for (auto& b : bands) {
            if (std::abs(b.gainDb) < 0.001) continue;
            totalDb += computeBandMagnitude(b, freq);
        }
        return totalDb + outputGainDb;
    }

    const EQBand& getBand(int i) const { return bands[i]; }

private:
    double sampleRate;
    double outputGainDb;
    std::array<EQBand, NUM_BANDS> bands;

    void updateAll() {
        for (auto& b : bands) b.update(sampleRate);
    }

    double computeBandMagnitude(const EQBand& b, double freq) const {
        // Evaluate H(z) on unit circle
        double omega = 2.0 * M_PI * freq / sampleRate;
        double cosW = std::cos(omega);
        double cos2W = std::cos(2.0 * omega);
        double sinW = std::sin(omega);
        double sin2W = std::sin(2.0 * omega);

        const auto& c = b.coeffL;

        double numRe = c.b0 + c.b1 * cosW + c.b2 * cos2W;
        double numIm =       -c.b1 * sinW - c.b2 * sin2W;
        double denRe = 1.0  + c.a1 * cosW + c.a2 * cos2W;
        double denIm =       -c.a1 * sinW - c.a2 * sin2W;

        double numMag2 = numRe*numRe + numIm*numIm;
        double denMag2 = denRe*denRe + denIm*denIm;

        if (denMag2 < 1e-30) return 0.0;
        return 10.0 * std::log10(numMag2 / denMag2);
    }
};
