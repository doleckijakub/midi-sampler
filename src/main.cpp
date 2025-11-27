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

constexpr int NUM_KEYS = 121;
int wavSampleRate = 44100;
int wavChannels = 1;
static uint8_t keys[NUM_KEYS] = {0};
static uint8_t pitch = 64;
static std::vector<float> sampleData;
static std::atomic<bool> sampleLoaded = false;
std::mutex sampleMutex;

void setKeyVelocity(uint8_t key, uint8_t velocity) {
    assert(key < NUM_KEYS);
    keys[key] = velocity;
}

float pitchBendFactor() {
    int bend = static_cast<int>(pitch) - 64;
    float semitoneRange = 5.0f;
    return std::pow(2.0f, bend * semitoneRange / 12.0f / 63.0f);
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
                v.pos += v.increment * pitchBendFactor();
            } else {
                v.alive = false;
            }
        }

        activeVoices.erase(std::remove_if(activeVoices.begin(), activeVoices.end(),
                                          [](const Voice& vv){ return !vv.alive; }),
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
    if (count > 0) {
        loadWavFile(paths[0]);
        
        std::lock_guard<std::mutex> lock(voiceMutex);
        activeVoices.clear();
    }
}

float frequencyFromMidi(int key) {
    return 440.0f * std::pow(2.0f, (key - 69) / 12.0f);
}

void drawKey(float x, float y, float w, float h, float r, float g, float b, bool border = true) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();

    if (border) {
        glColor3f(0, 0, 0);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    USB usb(argv[1]);

    std::thread([&]() {
        usb.start([&](uint8_t, uint8_t* data, uint8_t count){
            if (count < 4) return;

            switch (data[0]) {
                case 0x08: break; // Note off
                case 0x09: { // Note on
                    int key = data[2];
                    int vel = data[3];
                    
                    if (vel > 0) {
                        std::lock_guard<std::mutex> lock(voiceMutex);
                        Voice v{key, 0.0f, static_cast<float>(wavSampleRate) / outputSampleRate *
                                        (frequencyFromMidi(key)/frequencyFromMidi(baseMidiKey)),
                                vel / 127.0f, true};
                        activeVoices.push_back(v);
                    }
                    
                    setKeyVelocity(static_cast<uint8_t>(key), static_cast<uint8_t>(vel));
                } break;
                case 0x0E: { // Pitch controll
                    pitch = data[3];
                } break;
                default: {
                    std::printf("%02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);
                }
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1800, 400, "M-Audio Oxygen Pro Mini Sampler", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, dropCallback);
    if (glewInit() != GLEW_OK) return 1;

    while (!glfwWindowShouldClose(window)) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, 0, height, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        float pianoHeight = height / 3.0f;
        float whiteKeyWidth = width / 52.0f;
        float whiteKeyHeight = pianoHeight;
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        float blackKeyHeight = pianoHeight * 0.6f;

        int whiteIndex = 0;
        for (int i = 0; i < NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = mod12==0||mod12==2||mod12==4||mod12==5||mod12==7||mod12==9||mod12==11;
            if (isWhite) {
                float x = whiteIndex * whiteKeyWidth;
                float t = keys[i]/127.f;
                drawKey(x, 0, whiteKeyWidth, whiteKeyHeight, 1,1-t,1-t);
                whiteIndex++;
            }
        }

        whiteIndex = 0;
        for (int i = 0; i < NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = mod12==0||mod12==2||mod12==4||mod12==5||mod12==7||mod12==9||mod12==11;
            bool isBlack = mod12==1||mod12==3||mod12==6||mod12==8||mod12==10;
            if (isBlack) {
                float x = whiteIndex * whiteKeyWidth - blackKeyWidth*0.5f;
                float t = keys[i]/127.f;
                drawKey(x, whiteKeyHeight - blackKeyHeight, blackKeyWidth, blackKeyHeight, t,0,0);
            }
            if (isWhite) whiteIndex++;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    glfwTerminate();
    return 0;
}