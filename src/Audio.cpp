#include "Audio.hpp"
#include "Config.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>

Audio::Audio()
    : sampleData_(),
      sampleLoaded_(false),
      activeVoices_(),
      fftSmoothed_(cfg::FFT_SIZE / 2, 0.f),
      hannWindow_(cfg::FFT_SIZE, 0.f),
      keys_(cfg::NUM_KEYS, 0),
      pitch_(64),
      stream_(nullptr),
      wavSampleRate_(cfg::DEFAULT_WAV_SAMPLE_RATE),
      wavChannels_(cfg::DEFAULT_WAV_CHANNELS)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("PortAudio init failed");
    }

    initHannWindow();

    PaDeviceIndex device;

    {
        int numDevices = Pa_GetDeviceCount();
        for (int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if (std::string("pipewire") == info->name) {
                device = i;
                break;
            }
        }
    }

    if (device == paNoDevice) device = Pa_GetDefaultOutputDevice();

    if (device == paNoDevice) throw std::runtime_error("No default output device");

    const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(device);
    PaStreamParameters outParams;
    outParams.device = device;
    outParams.channelCount = 2;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = devInfo->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&stream_,
                        nullptr,
                        &outParams,
                        cfg::OUTPUT_SAMPLE_RATE,
                        cfg::PA_FRAMES,
                        paNoFlag,
                        &Audio::paCallback,
                        this);
    if (err != paNoError) {
        Pa_Terminate();
        throw std::runtime_error("Pa_OpenStream failed");
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        Pa_CloseStream(stream_);
        Pa_Terminate();
        throw std::runtime_error("Pa_StartStream failed");
    }
}

Audio::~Audio() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    Pa_Terminate();
}

void Audio::initHannWindow() {
    for (int i = 0; i < cfg::FFT_SIZE; ++i) {
        hannWindow_[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (cfg::FFT_SIZE - 1)));
    }
}

float Audio::frequencyFromMidi(int key) const {
    return 440.f * std::pow(2.f, (key - 69) / 12.f);
}

float Audio::pitchBendFactor() const {
    int bend = static_cast<int>(pitch_.load()) - 64;
    float semitoneRange = 5.f;
    return std::pow(2.f, bend * semitoneRange / 12.f / 63.f);
}

void Audio::noteOn(uint8_t key, uint8_t velocity) {
    if (key >= cfg::NUM_KEYS) return;
    
    std::lock_guard<std::mutex> lockVoice(voiceMutex_);
    float inc = static_cast<float>(wavSampleRate_) / cfg::OUTPUT_SAMPLE_RATE *
                (frequencyFromMidi(key) / frequencyFromMidi(60));
    Voice v;
    v.key = static_cast<int>(key);
    v.pos = 0.f;
    v.increment = inc;
    v.velocity = (velocity / 127.f);
    v.alive = true;
    activeVoices_.push_back(v);

    {
        std::lock_guard<std::mutex> lockKeys(sampleMutex_);
    }
    keys_[key] = velocity;
}

void Audio::pitchBend(uint8_t value) {
    pitch_.store(value);
}

bool Audio::loadSample(const char* path) {
    SF_INFO sfinfo{};
    SNDFILE* sndfile = sf_open(path, SFM_READ, &sfinfo);
    if (!sndfile) {
        std::cerr << "Failed to open sample: " << path << "\n";
        return false;
    }

    wavSampleRate_ = sfinfo.samplerate;
    wavChannels_ = sfinfo.channels;
    size_t samples = static_cast<size_t>(sfinfo.frames) * sfinfo.channels;
    std::vector<float> data(samples);
    sf_read_float(sndfile, data.data(), static_cast<sf_count_t>(samples));
    sf_close(sndfile);

    if (wavChannels_ == 2) {
        std::vector<float> mono(sfinfo.frames);
        for (size_t i = 0; i < static_cast<size_t>(sfinfo.frames); ++i) {
            mono[i] = 0.5f * (data[i * 2] + data[i * 2 + 1]);
        }
        data = std::move(mono);
        wavChannels_ = 1;
    }

    {
        std::lock_guard<std::mutex> lock(sampleMutex_);
        sampleData_ = std::move(data);
        sampleLoaded_.store(true);
    }
    
    {
        std::lock_guard<std::mutex> lock(voiceMutex_);
        activeVoices_.clear();
    }
    return true;
}

int Audio::paCallback(const void* input, void* output,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void* userData)
{
    (void) input;
    (void) timeInfo;
    (void) statusFlags;

    Audio* self = static_cast<Audio*>(userData);
    return self->processAudio(output, framesPerBuffer);
}

int Audio::processAudio(void* outputBuffer, unsigned long framesPerBuffer) {
    float* out = reinterpret_cast<float*>(outputBuffer);
    std::vector<float> localSample;
    {
        std::lock_guard<std::mutex> lock(sampleMutex_);
        if (sampleLoaded_ && !sampleData_.empty()) {
            localSample = sampleData_;
        }
    }

    if (!sampleLoaded_ || localSample.empty()) {
        for (unsigned long i = 0; i < framesPerBuffer; ++i) {
            *out++ = 0.f;
            *out++ = 0.f;
        }
        return paContinue;
    }

    std::scoped_lock lock(voiceMutex_);

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float left = 0.f, right = 0.f;
        for (auto &v : activeVoices_) {
            if (!v.alive) continue;
            if (v.pos + 1.f < static_cast<float>(localSample.size())) {
                size_t ipos = static_cast<size_t>(v.pos);
                float frac = v.pos - ipos;
                float smp = localSample[ipos] + (localSample[ipos + 1] - localSample[ipos]) * frac;
                left += smp * v.velocity;
                right += smp * v.velocity;
                v.pos += v.increment * pitchBendFactor();
            } else {
                v.alive = false;
            }
        }

        activeVoices_.erase(std::remove_if(activeVoices_.begin(), activeVoices_.end(),
                                           [](const Voice& vv){ return !vv.alive; }),
                            activeVoices_.end());

        *out++ = left * 0.2f;
        *out++ = right * 0.2f;
    }

    {
        std::lock_guard<std::mutex> snapLock(audioSnapshotMutex_);
        audioSnapshot_.resize(framesPerBuffer * 2);
        std::memcpy(audioSnapshot_.data(), outputBuffer, sizeof(float) * framesPerBuffer * 2);
    }

    return paContinue;
}

void Audio::computeSpectrum() {
    std::vector<float> buffer(cfg::FFT_SIZE, 0.f);

    {
        std::lock_guard<std::mutex> snapLock(audioSnapshotMutex_);
        size_t n = std::min(audioSnapshot_.size() / 2, static_cast<size_t>(cfg::FFT_SIZE));
        for (size_t i = 0; i < n; ++i) buffer[i] = audioSnapshot_[i*2];
        for (size_t i = n; i < cfg::FFT_SIZE; ++i) buffer[i] = 0.f;
    }

    kiss_fft_cfg cfgk = kiss_fft_alloc(cfg::FFT_SIZE, 0, nullptr, nullptr);
    std::vector<kiss_fft_cpx> in(cfg::FFT_SIZE), out(cfg::FFT_SIZE);

    for (int i = 0; i < cfg::FFT_SIZE; ++i) {
        in[i].r = buffer[i] * hannWindow_[i];
        in[i].i = 0.f;
    }

    kiss_fft(cfgk, in.data(), out.data());
    free(cfgk);

    for (int i = 0; i < cfg::FFT_SIZE / 2; ++i) {
        float magnitude = std::sqrt(out[i].r*out[i].r + out[i].i*out[i].i);
        fftSmoothed_[i] = cfg::SMOOTHING_FACTOR * magnitude + (1.f - cfg::SMOOTHING_FACTOR) * fftSmoothed_[i];
    }
}

std::vector<float> Audio::getSpectrumCopy() const {
    std::lock_guard<std::mutex> lock(sampleMutex_);
    return fftSmoothed_;
}

std::vector<uint8_t> Audio::getKeyVelocitiesCopy() const {
    std::lock_guard<std::mutex> lock(sampleMutex_);
    return keys_;
}

void Audio::decayKeysOnce(uint32_t ms) {
    (void) ms;
    
    for (size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i]) --keys_[i];
    }
}
