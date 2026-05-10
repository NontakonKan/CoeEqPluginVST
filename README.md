# SSL EQ VST Plugin — C++

6-Band Parametric EQ พร้อม SSL Proportional-Q Algorithm

## ไฟล์ในโปรเจกต์

```
SSL_EQ_VST/
├── include/
│   └── SSLEQProcessor.h        ← DSP core: Biquad filters + SSL math
├── Source/
│   ├── SSLEQPlugin.cpp         ← Plugin logic + parameter management
│   └── SSLEQAudioProcessor.h   ← JUCE AudioProcessor wrapper template
└── CMakeLists.txt              ← Build config (standalone + JUCE)
```

## สูตร DSP ที่ใช้ (SSL-Style)

### 1. Peak/Bell Filter — Bands 300Hz, 500Hz, 1kHz, 2.5kHz
```
SSL Proportional-Q:  Q = baseQ × √A
  A     = 10^(gainDb/40)        ← amplitude (half gain)
  omega = 2π × fc / Fs
  alpha = sin(ω) / (2Q)

  b0 = 1 + alpha×A
  b1 = -2×cos(ω)
  b2 = 1 - alpha×A
  a0 = 1 + alpha/A
  a1 = -2×cos(ω)
  a2 = 1 - alpha/A
```
> Q ขยายตาม sqrt(gain) — Q สูงขึ้นเมื่อ boost มาก เหมือน SSL 4000 G

### 2. Low Shelf — 200 Hz
```
  alpha = sin(ω)/2 × √((A + 1/A)(1/S − 1) + 2)
  b0 = A[(A+1) − (A−1)cos(ω) + 2√A·alpha]
  ...
```

### 3. High Shelf — 8 kHz
Mirror ของ Low Shelf ด้วย cosine sign flip

### 4. Output Gain
```
  linear = 10^(gainDb/20)
  y[n]   = x[n] × linear
```

## Build Instructions

### A) Standalone Test (ไม่ต้องการ JUCE)
```bash
mkdir build && cd build
cmake .. -DBUILD_STANDALONE_TEST=ON
cmake --build .
./ssl_eq_test
```

### B) Full VST3 / AU Plugin (ต้องการ JUCE 7+)
1. Clone JUCE: `git clone https://github.com/juce-framework/JUCE.git`
2. แก้ไข CMakeLists.txt: uncomment บล็อก JUCE
3. เพิ่มไฟล์ `Source/SSLEQEditor.cpp` สำหรับ GUI
4. Build:
```bash
cmake -B build -DJUCE_PATH=/path/to/JUCE
cmake --build build --config Release
```
Plugin จะอยู่ที่: `build/SSL_EQ_Plugin_artefacts/VST3/`

## Parameters

| ID | ชื่อ | Range | Default | ชนิด |
|----|------|-------|---------|------|
| 0 | EQ 200 Hz  | ±15 dB | 0 dB | Low Shelf |
| 1 | EQ 300 Hz  | ±15 dB | 0 dB | Peak (Proportional-Q) |
| 2 | EQ 500 Hz  | ±15 dB | 0 dB | Peak |
| 3 | EQ 1 kHz   | ±15 dB | 0 dB | Peak |
| 4 | EQ 2.5 kHz | ±15 dB | 0 dB | Peak |
| 5 | EQ 8 kHz   | ±15 dB | 0 dB | High Shelf |
| 6 | Output Gain | ±20 dB | 0 dB | Linear Gain |

## Requirements
- C++17 หรือใหม่กว่า
- CMake 3.22+
- JUCE 7+ (สำหรับ VST3 build เท่านั้น)
- macOS / Windows / Linux
