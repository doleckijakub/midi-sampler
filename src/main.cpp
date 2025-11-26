#include "USB.hpp"

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <vector>
#include <array>
#include <cmath>
#include <thread>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define NUM_KEYS 121

static uint8_t keys[NUM_KEYS] = { 0 };

void setKeyVelocity(uint8_t key, uint8_t velocity) {
    assert(key < NUM_KEYS);
    keys[key] = velocity;
}

void fillRect(float x1, float y1, float x2, float y2,
              float r, float g, float b) {
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s /dev/bus/usb/BBB/DDD\n", argv[0]);
        return 1;
    }

    USB usb(argv[1]);

    std::thread([&]() {
        usb.start([&](uint8_t address, uint8_t *data, uint8_t count) {
            (void) address;

            std::printf("%d/%02x %d/%02x %d/%02x %d/%02x\n", data[0], data[0], data[1], data[1], data[2], data[2], data[3], data[3]);

            assert(count > 0);
            switch (data[0]) {
                case 0x08:
                case 0x09: {
                    assert(count == 4);
                    setKeyVelocity(data[2], data[3]);
                } break;
            }
        });
    }).detach();

    std::thread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            for (int i = 0; i < NUM_KEYS; i++) {
                if (keys[i]) keys[i]--;
            }
        }
    }).detach();

    if (!glfwInit()) {
        std::fprintf(stderr, "GLFW initialization failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1800, 300, "M-Audio Oxygen Pro Mini Sampler", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "GLFW window creation failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "GLEW init failed\n");
        return 1;
    }

    glViewport(0, 0, 1800, 300);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(0.0f, 1.0f, 1.0f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        float whiteKeyWidth = 1.0f / 52.0f;
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
            bool isWhite = (mod12 == 0 || mod12 == 2 || mod12 == 4 || mod12 == 5 ||
                            mod12 == 7 || mod12 == 9 || mod12 == 11);
            if (isWhite) {
                float x0 = whiteIndex * whiteKeyWidth;
                float x1 = x0 + whiteKeyWidth;
                float y0 = 0.0f;
                float y1 = whiteKeyHeight;

                float t = keys[i] / 127.f;
                float r = 1.f;
                float g = 1.0f - t;
                float b = 1.0f - t;

                fillRect(x0 * xScale + xOffset, y1 * yScale + yOffset,
                        x1 * xScale + xOffset, y0 * yScale + yOffset,
                        r, g, b);
                drawRect(x0 * xScale + xOffset, y1 * yScale + yOffset,
                        x1 * xScale + xOffset, y0 * yScale + yOffset,
                        0.0f, 0.0f, 0.0f);

                whiteIndex++;
            }
        }

        whiteIndex = 0;
        for (int i = 0; i < NUM_KEYS; ++i) {
            int mod12 = i % 12;
            bool isWhite = (mod12 == 0 || mod12 == 2 || mod12 == 4 || mod12 == 5 || mod12 == 7 || mod12 == 9 || mod12 == 11);
            bool isBlack = (mod12 == 1 || mod12 == 3 || mod12 == 6 || mod12 == 8 || mod12 == 10);

            if (isBlack) {
                float x0 = whiteIndex * whiteKeyWidth - blackKeyWidth * 0.5f;
                float x1 = x0 + blackKeyWidth;
                float y0 = 0.0f;
                float y1 = blackKeyHeight;

                float t = keys[i] / 127.f;
                float r = t;
                float g = 0.0f;
                float b = 0.0f;

                fillRect(x0 * xScale + xOffset, y1 * yScale + yOffset,
                        x1 * xScale + xOffset, y0 * yScale + yOffset,
                        r, g, b);
                drawRect(x0 * xScale + xOffset, y1 * yScale + yOffset,
                        x1 * xScale + xOffset, y0 * yScale + yOffset,
                        0.0f, 0.0f, 0.0f);
            }

            if (isWhite) whiteIndex++;
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
