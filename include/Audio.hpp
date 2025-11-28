#pragma once

#include "Config.hpp"

#include <vector>
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
    void pitchBend(uint8_t value);

    bool loadSample(const char* path);

    void computeSpectrum();
    std::vector<float> getSpectrumCopy() const;

    std::vector<uint8_t> getKeyVelocitiesCopy() const;

    void decayKeysOnce(uint32_t ms = 10);

private:
    struct Voice {
        int key;
        float pos;
        float increment;
        float velocity;
        bool alive;
    };

    static int paCallback(const void* input, void* output,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);
    int processAudio(void* outputBuffer, unsigned long framesPerBuffer);

    std::vector<float> sampleData_;
    mutable std::mutex sampleMutex_;
    std::atomic<bool> sampleLoaded_;

    std::vector<Voice> activeVoices_;
    mutable std::mutex voiceMutex_;

    std::vector<float> audioSnapshot_;
    mutable std::mutex audioSnapshotMutex_;

    std::vector<float> fftSmoothed_;
    std::vector<float> hannWindow_;

    std::vector<uint8_t> keys_;
    std::atomic<uint8_t> pitch_;

    PaStream* stream_;

    void initHannWindow();
    float pitchBendFactor() const;
    float frequencyFromMidi(int key) const;

    int wavSampleRate_;
    int wavChannels_;
};
