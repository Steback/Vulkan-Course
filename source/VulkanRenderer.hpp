#ifndef VULKAN_COURSE_VULKANRENDERER_HPP
#define VULKAN_COURSE_VULKANRENDERER_HPP


#include <memory>
#include <stdexcept>
#include <vector>

#include "vulkan//vulkan.h"
#include "GLFW/glfw3.h"

#include "Window.hpp"


class ValidationLayers;

struct QueueFamilyIndices;

struct Device {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice;
};

class VulkanRenderer {
    public:
        explicit VulkanRenderer(std::unique_ptr<Window>& window);
        ~VulkanRenderer();
        int init();
        void clean();

    private:
        // Vulkan function
        // - Create functions
        void createInstance();
        void createLogicalDevice();

        // - Get functions
        void getPhysicalDevice();

        // - Support functions
        // -- Checker functions
        static bool checkInstanceSupport(std::vector<const char*>* extensions);
        static bool checkDeviceSuitable(VkPhysicalDevice device);

        // -- Getter functions
        static QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);

    private:
        std::unique_ptr<Window>& window_;
        std::unique_ptr<ValidationLayers> validationLayers;

        // Vulkan components
        VkInstance instance_{};
        Device device_{};
        VkQueue graphicsQueues_{};
};


#endif
