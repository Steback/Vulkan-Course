#include <stdexcept>
#include <memory>

#include "GLFW/glfw3.h"
#include "glm/gtc/matrix_transform.hpp"

#include "Window.hpp"
#include "VulkanRenderer.hpp"

int main() {
    std::unique_ptr<Window> window = std::make_unique<Window>("Vulkan course", 800, 600);
    std::unique_ptr<VulkanRenderer> renderer = std::make_unique<VulkanRenderer>(window);

    if (renderer->init() == EXIT_FAILURE) return EXIT_FAILURE;

    float angle = 0.0f;
    float deltaTime = 0.0f;
    float lastTime = 0.0f;

    while (window->isOpen()) {
        glfwPollEvents();

        auto now = static_cast<float>(glfwGetTime());
        deltaTime = now - lastTime;
        lastTime = now;

        renderer->draw();
    }

    renderer->clean();
    window->clean();

    return 0;
}
