#ifndef VULKAN_COURSE_WINDOW_HPP
#define VULKAN_COURSE_WINDOW_HPP


#include <string>

#include "GLFW/glfw3.h"


class Window {
    public:
        explicit Window(const std::string& name = "Vulkan course", int width = 800, int height = 600);
        ~Window();
        bool isOpen();
        void clean();

    private:
        GLFWwindow* window_;
        int width_, height_;
};


#endif
