#include "Graphics.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <vector>

Graphics* Graphics::s_instance_ = nullptr;

Graphics::Graphics(Audio& audio)
    : audio_(audio), window_(nullptr)
{
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window_ = glfwCreateWindow(cfg::WINDOW_WIDTH, cfg::WINDOW_HEIGHT, cfg::WINDOW_TITLE, nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(window_);
    glfwSetDropCallback(window_, &Graphics::dropCallbackStatic);

    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("glewInit failed");
    }

    s_instance_ = this;
}

Graphics::~Graphics() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
    s_instance_ = nullptr;
}

void Graphics::dropCallbackStatic(GLFWwindow* window, int count, const char** paths) {
    (void) window;
    
    if (s_instance_ && count > 0) {
        s_instance_->audio_.loadSample(paths[0]);
    }
}

void Graphics::fillRect(float x, float y, float w, float h, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void Graphics::drawKey(float x, float y, float w, float h, float r, float g, float b, bool border) {
    fillRect(x, y, w, h, r, g, b);
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

void Graphics::run() {
    while (!glfwWindowShouldClose(window_)) {
        audio_.computeSpectrum();

        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, 0, height, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        float pianoHeight = height / 3.f;
        float whiteKeyWidth = width / 52.f;
        float whiteKeyHeight = pianoHeight;
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        float blackKeyHeight = pianoHeight * 0.6f;

        auto keys = audio_.getKeyVelocitiesCopy();
        auto spectrum = audio_.getSpectrumCopy();

        int whiteIndex = 0;
        for (int i = 0; i < cfg::NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = (mod12==0||mod12==2||mod12==4||mod12==5||mod12==7||mod12==9||mod12==11);
            if (isWhite) {
                float x = whiteIndex * whiteKeyWidth;
                float t = keys[i]/127.f;
                drawKey(x, 0, whiteKeyWidth, whiteKeyHeight, 1, 1-t, 1-t);
                whiteIndex++;
            }
        }

        whiteIndex = 0;
        for (int i = 0; i < cfg::NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = (mod12==0||mod12==2||mod12==4||mod12==5||mod12==7||mod12==9||mod12==11);
            bool isBlack = (mod12==1||mod12==3||mod12==6||mod12==8||mod12==10);
            if (isBlack) {
                float x = whiteIndex * whiteKeyWidth - blackKeyWidth*0.5f;
                float t = keys[i]/127.f;
                drawKey(x, whiteKeyHeight - blackKeyHeight, blackKeyWidth, blackKeyHeight, t, 0, 0, false);
            }
            if (isWhite) whiteIndex++;
        }

        int numPixels = width;
        float minFreq = 20.f;
        float maxFreq = 20000.f;
        float logMin = std::log10(minFreq);
        float logMax = std::log10(maxFreq);

        for (int x = 0; x < numPixels; ++x) {
            float frac = float(x) / numPixels;
            float logFreq = logMin + frac * (logMax - logMin);
            float freq = std::pow(10.f, logFreq);

            float bin = freq * cfg::FFT_SIZE / cfg::OUTPUT_SAMPLE_RATE;
            int bin0 = static_cast<int>(std::floor(bin));
            int bin1 = bin0 + 1;
            float ffrac = bin - bin0;

            float mag0 = (bin0 < static_cast<int>(spectrum.size())) ? spectrum[bin0] : 0.f;
            float mag1 = (bin1 < static_cast<int>(spectrum.size())) ? spectrum[bin1] : 0.f;
            float mag = mag0 * (1 - ffrac) + mag1 * ffrac;

            mag = std::sqrt(mag);

            float y = mag * (height - pianoHeight) * 5.f;
            fillRect(x, pianoHeight, 1.f, y, 1.f - frac, 0.f, frac);
        }

        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
}
