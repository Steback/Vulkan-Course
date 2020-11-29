#include "spdlog/spdlog.h"

#include "Window.hpp"


Window::Window(const std::string& name, const int width, const int height) : width_(width), height_(height) {
    if (!glfwInit()) {
        spdlog::error("[GLFW] Failed init");
    }

    // Set GLFW to NOT work with OpenGL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    spdlog::info("[GLFW] Create window");
    window_ = glfwCreateWindow(width_, height_, name.c_str(), nullptr, nullptr);
}

Window::~Window() = default;

bool Window::isOpen() {
    return !glfwWindowShouldClose(window_);
}

void Window::clean() {
    spdlog::info("[GLFW] Clean window");
    glfwDestroyWindow(window_);
}
