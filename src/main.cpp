#include "USB.hpp"
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <vector>
#include <thread>
#include <cmath>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <portaudio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <sndfile.h>
#include <alsa/asoundlib.h>

constexpr int NUM_KEYS = 121;
int wavSampleRate = 44100;
int wavChannels = 1;
static uint8_t keys[NUM_KEYS] = {0};
static std::vector<float> sampleData;
static std::atomic<bool> sampleLoaded = false;
std::mutex sampleMutex;

void setKeyVelocity(uint8_t key, uint8_t velocity) {
    assert(key < NUM_KEYS);
    keys[key] = velocity;
}

void fillRect(float x1, float y1, float x2, float y2, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
}

void drawRect(float x1, float y1, float x2, float y2, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_LINES);
    glVertex2f(x1, y1); glVertex2f(x2, y1);
    glVertex2f(x2, y1); glVertex2f(x2, y2);
    glVertex2f(x2, y2); glVertex2f(x1, y2);
    glVertex2f(x1, y2); glVertex2f(x1, y1);
    glVertex2f(x1, y1); glVertex2f(x2, y1);
    glEnd();
}

float frequencyFromMidi(int key) {
    return 440.0f * std::pow(2.0f, (key - 69) / 12.0f);
}

struct Voice {
    int key;
    float pos;
    float increment;
    float velocity;
    bool alive;
};

std::vector<Voice> activeVoices;
std::mutex voiceMutex;
constexpr int baseMidiKey = 60;
constexpr float outputSampleRate = 44100.0f;

static int paCallback(const void*, void* outputBuffer, unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*) {
    float* out = reinterpret_cast<float*>(outputBuffer);
    std::scoped_lock lock(sampleMutex, voiceMutex);

    if (!sampleLoaded || sampleData.empty()) {
        for (unsigned long i = 0; i < framesPerBuffer; ++i) {
            *out++ = 0.0f;
            *out++ = 0.0f;
        }
        return paContinue;
    }

    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float left = 0.0f;
        float right = 0.0f;

        for (auto& v : activeVoices) {
            if (!v.alive) continue;
            if (v.pos + 1.0f < static_cast<float>(sampleData.size())) {
                size_t ipos = static_cast<size_t>(v.pos);
                float frac = v.pos - ipos;
                float smp = sampleData[ipos] + (sampleData[ipos + 1] - sampleData[ipos]) * frac;
                left += smp * v.velocity;
                right += smp * v.velocity;
                v.pos += v.increment;
            } else {
                v.alive = false;
            }
        }

        activeVoices.erase(std::remove_if(activeVoices.begin(), activeVoices.end(),
                                          [](const Voice& vv) { return !vv.alive; }),
                           activeVoices.end());

        *out++ = left * 0.2f;
        *out++ = right * 0.2f;
    }
    return paContinue;
}

void loadWavFile(const char* path) {
    SF_INFO sfinfo{};
    SNDFILE* sndfile = sf_open(path, SFM_READ, &sfinfo);
    if (!sndfile) return;

    wavSampleRate = sfinfo.samplerate;
    wavChannels = sfinfo.channels;

    size_t samples = static_cast<size_t>(sfinfo.frames) * sfinfo.channels;
    std::vector<float> data(samples);
    sf_read_float(sndfile, data.data(), samples);
    sf_close(sndfile);

    if (wavChannels == 2) {
        std::vector<float> mono(sfinfo.frames);
        for (size_t i = 0; i < static_cast<size_t>(sfinfo.frames); i++)
            mono[i] = 0.5f * (data[i * 2] + data[i * 2 + 1]);
        data = std::move(mono);
        wavChannels = 1;
    }

    std::lock_guard<std::mutex> lock(sampleMutex);
    sampleData = std::move(data);
    sampleLoaded = true;
}

void dropCallback(GLFWwindow*, int count, const char** paths) {
    if (count > 0) loadWavFile(paths[0]);
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;

    USB usb(argv[1]);
    std::thread([&]() {
        usb.start([&](uint8_t, uint8_t* data, uint8_t count) {
            if (count < 4) return;
            if (data[0] == 0x09) {
                int key = data[2];
                int vel = data[3];
                if (vel > 0) {
                    std::lock_guard<std::mutex> lock(voiceMutex);
                    Voice v{key, 0.0f, static_cast<float>(wavSampleRate) / outputSampleRate * (frequencyFromMidi(key) / frequencyFromMidi(baseMidiKey)), vel / 127.0f, true};
                    activeVoices.push_back(v);
                }
                setKeyVelocity(static_cast<uint8_t>(key), static_cast<uint8_t>(vel));
            }
        });
    }).detach();

    std::thread([&]() {
        while (true) {
            for (int i = 0; i < NUM_KEYS; ++i) if (keys[i]) keys[i]--;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }).detach();

    Pa_Initialize();
    PaStream* stream;
    int numDevices = Pa_GetDeviceCount();
    int outputDevice = -1;

    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (std::string("pipewire") == info->name) { outputDevice = i; break; }
    }

    if (outputDevice < 0) {
        for (int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            const PaHostApiInfo* api = Pa_GetHostApiInfo(info->hostApi);
            if (info->maxOutputChannels) std::printf("%d: %s (%s)\n", i, info->name, api->name);
        }
    }

    while (outputDevice == -1) {
        std::printf("Select the device index: ");
        int n = -1;
        std::scanf("%d", &n);
        const PaDeviceInfo* info = Pa_GetDeviceInfo(n);
        if (!info || info->maxOutputChannels <= 0) continue;
        PaStreamParameters testParams{n, 2, paFloat32, info->defaultLowOutputLatency, nullptr};
        if (Pa_IsFormatSupported(nullptr, &testParams, outputSampleRate) != paNoError) continue;
        outputDevice = n;
    }

    PaStreamParameters outParams{outputDevice, 2, paFloat32, Pa_GetDeviceInfo(outputDevice)->defaultLowOutputLatency, nullptr};
    if (Pa_OpenStream(&stream, nullptr, &outParams, outputSampleRate, 256, paNoFlag, paCallback, nullptr) != paNoError) return 1;
    Pa_StartStream(stream);

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1800, 300, "M-Audio Oxygen Pro Mini Sampler", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, dropCallback);
    if (glewInit() != GLEW_OK) return 1;
    glViewport(0, 0, 1800, 300);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0,1,1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        float whiteKeyWidth = 1.0f/52.0f;
        float whiteKeyHeight = 0.8f;
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        float blackKeyHeight = whiteKeyHeight * 0.6f;
        float xScale = 2.0f;
        float xOffset = -1.0f;
        float yScale = -2.0f;
        float yOffset = 1.0f;

        int whiteIndex = 0;
        for (int i = 0; i < NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = mod12==0||mod12==2||mod12==4||mod12==5||mod12==7||mod12==9||mod12==11;
            if (isWhite) {
                float x0 = whiteIndex * whiteKeyWidth;
                float x1 = x0 + whiteKeyWidth;
                float t = keys[i] / 127.0f;
                fillRect(x0*xScale + xOffset, whiteKeyHeight*yScale + yOffset, x1*xScale + xOffset, 0*yScale + yOffset, 1,1-t,1-t);
                drawRect(x0*xScale + xOffset, whiteKeyHeight*yScale + yOffset, x1*xScale + xOffset, 0*yScale + yOffset, 0,0,0);
                whiteIndex++;
            }
        }

        whiteIndex = 0;
        for (int i = 0; i < NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = mod12==0||mod12==2||mod12==4||mod12==5||mod12==7||mod12==9||mod12==11;
            bool isBlack = mod12==1||mod12==3||mod12==6||mod12==8||mod12==10;
            if (isBlack) {
                float x0 = whiteIndex * whiteKeyWidth - blackKeyWidth*0.5f;
                float x1 = x0 + blackKeyWidth;
                float t = keys[i] / 127.0f;
                fillRect(x0*xScale + xOffset, blackKeyHeight*yScale + yOffset, x1*xScale + xOffset, 0*yScale + yOffset, t,0,0);
                drawRect(x0*xScale + xOffset, blackKeyHeight*yScale + yOffset, x1*xScale + xOffset, 0*yScale + yOffset, 0,0,0);
            }
            if (isWhite) whiteIndex++;
        }

        glfwSwapBuffers(window);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    glfwTerminate();
    return 0;
}
