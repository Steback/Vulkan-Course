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
struct SwapChainDetails;
struct SwapChainImage;

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
        void createSurface();
        void createSwapChain();

        // - Get functions
        void getPhysicalDevice();

        // - Support functions
        // -- Checker functions
        bool checkInstanceSupport(std::vector<const char*>* extensions);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        bool checkDeviceSuitable(VkPhysicalDevice device);

        // -- Getter functions
        QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
        SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

        // -- Choose functions
        VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
        VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

        // -- Create functions
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    private:
        std::unique_ptr<Window>& window_;
        std::unique_ptr<ValidationLayers> validationLayers;

        // Vulkan components
        // - Main
        VkInstance instance_{};
        Device device_{};
        VkQueue graphicsQueues_{};
        VkQueue presentationQueue_{};
        VkSurfaceKHR surface_{};
        VkSwapchainKHR swapChain_{};
        std::vector<SwapChainImage> swapChainImages_;

        // - Utility
        VkFormat swapChainImageFormat_{};
        VkExtent2D swapChainExtent_{};
};


#endif
