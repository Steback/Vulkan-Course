#ifndef VULKAN_COURSE_UTILITIES_HPP
#define VULKAN_COURSE_UTILITIES_HPP


#include <fstream>


const int MAX_FRAME_DRAWS = 2;

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

static std::vector<char> readFile(const std::string& fileName) {
    // Open stream from given file
    // std::ios::binary tells stream to read file as binary
    // std::ios::ate tells stream to start reading from end of file
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);

    // Check if file stream successfully opened
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open " + fileName + " file");
    }

    // Get current read position and use to resize file buffer
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> fileBuffer(fileSize);

    // Move read position (seek to) the start of the file
    file.seekg(0);


    // Read the file data into the buffer (stream "fileSize" in total)
    file.read(fileBuffer.data(), fileSize);

    // Close the stream
    file.close();

    return fileBuffer;
}


#endif
