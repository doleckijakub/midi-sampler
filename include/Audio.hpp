#pragma once

#include "Config.hpp"

#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>

#include <portaudio.h>
#include <sndfile.h>
#include <kiss_fft.h>

class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void noteOn(uint8_t key, uint8_t velocity);
    void percOn(uint8_t idx, uint8_t velocity);
    void pitchBend(uint8_t value);

    bool loadSample(const char* path);
    bool loadPercSample(uint8_t idx, const char* path);

    void computeSpectrum();
    std::vector<float> getSpectrumCopy() const;

    std::array<uint8_t, cfg::NUM_KEYS> getKeyVelocitiesCopy() const;
    std::array<uint8_t, cfg::NUM_PERC> getPercVelocitiesCopy() const;

    void decayKeysOnce();
    void decayPercOnce();

private:
    struct Voice {
        int key;
        float pos;
        float increment;
        float velocity;
        bool alive;
    };

    struct PercVoice {
        int idx;
        float pos;
        float increment;
        float velocity;
        bool alive;
    };

    struct Sample {
        std::vector<float> data;
        mutable std::mutex mutex;
        std::atomic<bool> loaded = false;
        int rate = cfg::DEFAULT_WAV_SAMPLE_RATE;
        int channels = cfg::DEFAULT_WAV_CHANNELS;
    };

    static int paCallback(const void* input, void* output,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);
    int processAudio(void* outputBuffer, unsigned long framesPerBuffer);

    Sample pianoSample_;
    std::vector<Voice> activeVoices_;
    mutable std::mutex voiceMutex_;

    std::array<Sample, cfg::NUM_PERC> percSamples_;
    std::vector<PercVoice> activePercs_;
    mutable std::mutex percMutex_;

    std::vector<float> audioSnapshot_;
    mutable std::mutex audioSnapshotMutex_;

    std::vector<float> fftSmoothed_;
    std::vector<float> hannWindow_;

    std::array<uint8_t, cfg::NUM_KEYS> keys_;
    std::array<uint8_t, cfg::NUM_PERC> perc_;
    std::atomic<uint8_t> pitch_;

    PaStream* stream_;

    void initHannWindow();
    float pitchBendFactor() const;
    float frequencyFromMidi(int key) const;
};
