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

        angle += 10.0f * deltaTime;
        if (angle > 350.0f) angle -= 360.0f;

        glm::mat4 model1(1.0f);
        glm::mat4 model2(1.0f);

        model1 = glm::translate(model1, glm::vec3(-2.0f, 0.0f, -5.0f));
        model1 = glm::rotate(model1, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

        model2 = glm::translate(model2, glm::vec3(2.0f, 0.0f, -5.0f));
        model2 = glm::rotate(model2, glm::radians(-angle * 100), glm::vec3(0.0f, 0.0f, 1.0f));

        renderer->updateModel(0, model1);
        renderer->updateModel(1, model2);

        renderer->draw();
    }

    renderer->clean();
    window->clean();

    return 0;
}
