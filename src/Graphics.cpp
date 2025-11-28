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
    glfwSetErrorCallback([](int error, const char *desc) {
        std::fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
    });

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
    glfwSetCursorPosCallback(window_, &Graphics::cursorPosCallbackStatic);

    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("glewInit failed");
    }

    percColors_[0] = { 0.f, 1.f, 0.f };
    percColors_[1] = { 0.f, 1.f, 1.f };
    percColors_[2] = { 1.f, 1.f, 0.f };
    percColors_[3] = { 1.f, 0.f, .5f };
    percColors_[4] = { 1.f, .5f, 0.f };
    percColors_[5] = { 0.f, 1.f, 0.f };
    percColors_[6] = { 0.f, .8f, 0.f };
    percColors_[7] = { 1.f, 0.f, 0.f };

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

    if (!s_instance_ || count <= 0) return;
    
    double x = s_instance_->mouseX_;
    double y = s_instance_->mouseY_;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    y = height - y;

    float pianoHeight = height / 3.f;

    bool isPerc = (x > width / 2 && y > pianoHeight);
    if (isPerc) {
        int x0 = int(8 * x / width - 4);
        int y0 = 2 - int(3 * y / height);
        int idx = x0 + 4  * y0;

        s_instance_->audio_.loadPercSample(idx, paths[0]);
    } else {
        s_instance_->audio_.loadSample(paths[0]);
    }
}

void Graphics::cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos) {
    (void) window;
    
    if (s_instance_) {
        s_instance_->mouseX_ = xpos;
        s_instance_->mouseY_ = ypos;
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
        glLineWidth(1.f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }
}

void Graphics::drawPerc(float x, float y, float w, float h, float r, float g, float b, float v) {
    const float BW = 4.f;
    
    fillRect(x, y, w, h, r, g, b);
    fillRect(x + BW, y + BW, w - 2 * BW, h - 2 * BW, v, v, v);
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

        // Piano

        float pianoHeight = height / 3.f;
        float whiteKeyWidth = width / 52.f;
        float whiteKeyHeight = pianoHeight;
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        float blackKeyHeight = pianoHeight * 0.6f;

        auto keys = audio_.getKeyVelocitiesCopy();

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

        // Percussion

        auto perc = audio_.getPercVelocitiesCopy();

        float percStartX = width / 2;

        float percKeyWidth = width / 2 / 4;
        float percKeyHeight = (height - pianoHeight) / 2;

        for (int py = 0; py < 2; py++) {
            for (int px = 0; px < 4; px++) {
                int idx = px + py * 4;

                auto color = percColors_[idx];
                
                drawPerc(
                    percStartX + px * percKeyWidth,
                    pianoHeight + (1 - py) * percKeyHeight,
                    percKeyWidth,
                    percKeyHeight,
                    color[0],
                    color[1],
                    color[2],
                    perc[idx] / 127.f
                );
            }
        }

        // Spectrum

        auto spectrum = audio_.getSpectrumCopy();

        int spectrumWidth = width / 2;
        float minFreq = 20.f;
        float maxFreq = 20000.f;
        float logMin = std::log10(minFreq);
        float logMax = std::log10(maxFreq);

        for (int x = 0; x < spectrumWidth; ++x) {
            float frac = float(x) / spectrumWidth;
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
