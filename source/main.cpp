#include <stdexcept>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "spdlog/spdlog.h"

#include "Window.hpp"
#include "VulkanRenderer.hpp"

int main() {
    std::unique_ptr<Window> window = std::make_unique<Window>("Vulkan course", 800, 600);
    std::unique_ptr<VulkanRenderer> renderer = std::make_unique<VulkanRenderer>(window);

    if (renderer->init() == EXIT_FAILURE) return EXIT_FAILURE;

    while (window->isOpen()) {
        glfwPollEvents();
    }

    renderer->clean();
    window->clean();
    glfwTerminate();

    return 0;
}
