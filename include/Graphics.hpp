#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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

private:
    Audio& audio_;
    GLFWwindow* window_;

    void drawFrame();
    void fillRect(float x, float y, float w, float h, float r, float g, float b);
    void drawKey(float x, float y, float w, float h, float r, float g, float b, bool border = true);

    static Graphics* s_instance_;
};
