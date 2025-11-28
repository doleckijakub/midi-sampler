#pragma once

#include <cstdint>

namespace cfg {

constexpr int NUM_KEYS = 121;
constexpr int DEFAULT_WAV_SAMPLE_RATE = 44100;
constexpr int DEFAULT_WAV_CHANNELS = 1;
constexpr float OUTPUT_SAMPLE_RATE = 44100.f;
constexpr int PA_FRAMES = 256;

constexpr int FFT_SIZE = 8192;
constexpr float SMOOTHING_FACTOR = 0.75f;

constexpr int WINDOW_WIDTH = 1800;
constexpr int WINDOW_HEIGHT = 400;
constexpr char WINDOW_TITLE[] = "M-Audio Oxygen Pro Mini Sampler";

} // namespace cfg
