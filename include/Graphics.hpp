#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>

#include "Config.hpp"
#include "Audio.hpp"

#include <memory>

class Graphics {
public:
    explicit Graphics(Audio& audio);
    ~Graphics();

    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;

    void run();

    static void dropCallbackStatic(GLFWwindow* window, int count, const char** paths);
    static void cursorPosCallbackStatic(GLFWwindow* window, double xpos, double ypos);

private:
    Audio& audio_;
    GLFWwindow* window_;

    std::array<std::array<float,3>, cfg::NUM_PERC> percColors_;
    double mouseX_, mouseY_;

    void drawFrame();
    void fillRect(float x, float y, float w, float h, float r, float g, float b);
    void drawKey(float x, float y, float w, float h, float r, float g, float b, bool border = true);
    void drawPerc(float x, float y, float w, float h, float r, float g, float b, float v);

    static Graphics* s_instance_;
};
