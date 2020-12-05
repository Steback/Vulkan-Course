#ifndef VULKAN_COURSE_UTILITIES_HPP
#define VULKAN_COURSE_UTILITIES_HPP

const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Indices (locations) of Queue Families (if they exists at all)
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; // Location of Graphics Queue Family
    std::optional<uint32_t> presentationFamily; // Location of Presentation Queue Family

    [[nodiscard]] bool isValid() const {
        return graphicsFamily.has_value() && presentationFamily.has_value();
    }
};

struct SwapChainDetails {
    VkSurfaceCapabilitiesKHR surfaceCapabilities; // Surface properties, e.g. image size/extent
    std::vector<VkSurfaceFormatKHR> formats; // Surface image formats, e.g. RGBA and size of each colour
    std::vector<VkPresentModeKHR> presentationModes; // How images should be presented to screen
};

struct SwapChainImage {
    VkImage image;
    VkImageView imageView;
};

#endif
