#ifndef VULKAN_COURSE_UTILITIES_HPP
#define VULKAN_COURSE_UTILITIES_HPP


#include <fstream>
#include <optional>

#include "glm/glm.hpp"
#include "vulkan/vulkan.h"


const int MAX_FRAME_DRAWS = 3;
const int MAX_OBJECTS = 2;

const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Vertex {
    glm::vec3 pos; // Vertex Position (x, y, z)
    glm::vec3 col; // Vertex Colour (r, g, b)
    glm::vec2 tex; // Texture Coords (u, v)
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

struct UboViewProjection {
    glm::mat4 projection;
    glm::mat4 view;
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

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties) {
    // Get properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        // Index of memory flags types must be match corresponding bit in allowTypes
        // Desired property bit flags are part of memory type's property flags
        if ((allowedTypes & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            // this memory type is valid, so return its index
            return i;
        }
    }

    return -1;
}

static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize,
                         VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer* buffer,
                         VkDeviceMemory* bufferMemory) {
    // CREATE VERTEX BUFFER
    // Information to create a buffer (doesn't include assigment memory)
    VkBufferCreateInfo bufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = bufferSize, // Size of buffer (size of 1 vertex * number of vertices)
            .usage = usageFlags, // Multiple types of buffer possible, we want Vertex Buffer
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE // Similar to Swap Chain images, can share vertex buffers
    };

    VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a Vertex Buffer");
    }

    // GET BUFFET MEMORY REQUIREMENTS
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

    // ALLOCATE MEMORY TO BUFFER
    // Index of memory type on Physical Device that has required bif flags
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : GPU can interact with memory
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : Allows placement of data straight
    VkMemoryAllocateInfo memoryAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, propertyFlags)
    };

    // Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Vertex Buffer Memory");
    }

    // Allocate memory to given vertex buffer
    vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static VkCommandBuffer beginCmdBuffer(VkDevice device, VkCommandPool commandPool) {
    // Command buffer to hold transfer commands
    VkCommandBuffer commandBuffer{};

    // Command buffer details
    VkCommandBufferAllocateInfo bufferAllocateInfo{};
    bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferAllocateInfo.commandPool = commandPool;
    bufferAllocateInfo.commandBufferCount = 1;

    // Allocate command buffer from pool
    vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer);

    // Information to begin the command buffer record
    VkCommandBufferBeginInfo bufferBeginInfo{};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // We're only using the command buffer once, so set up for one time submit

    // Begin recording transfer commands
    vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo);

    return commandBuffer;
}

static void endAndSubmitCmdBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                                  VkCommandBuffer commandBuffer) {
    // End Commands
    vkEndCommandBuffer(commandBuffer);

    // Queue submission information
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit transfer command to transfer queue and wait until it finishes
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // Free temporary command buffer back to pool
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer,
                     VkBuffer dstBuffer, VkDeviceSize bufferSize) {
    // Create Buffer
    VkCommandBuffer transferCmdBuffer = beginCmdBuffer(device, transferCommandPool);

    // Region of data to copy form and to
    VkBufferCopy bufferCopyRegion{};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    // Command to copy src buffer to dst buffer
    vkCmdCopyBuffer(transferCmdBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    endAndSubmitCmdBuffer(device, transferCommandPool, transferQueue, transferCmdBuffer);
}

static void copyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                            VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height) {
    // Create Buffer
    VkCommandBuffer transferCmdBuffer = beginCmdBuffer(device, transferCommandPool);

    VkBufferImageCopy imageRegion{};
    imageRegion.bufferOffset = 0; // Offset into data
    imageRegion.bufferRowLength = 0; // Row length of data to calculate data spacing
    imageRegion.bufferImageHeight = 0; // Image height to calculate data spacing
    imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Witch aspect of image to copy
    imageRegion.imageSubresource.mipLevel = 0; // Mipmap level to copy
    imageRegion.imageSubresource.baseArrayLayer = 0; // Starting array layer (if array)
    imageRegion.imageSubresource.layerCount = 1; // Number of layer to copy starting at baseArrayLayer
    imageRegion.imageOffset = { 0, 0, 0 }; // Offset into image (as opposed to raw data in bufferOffset)
    imageRegion.imageExtent = { width, height, 1 }; // Size of region to copy as (x, y, z) values

    // Copy buffer to given image
    vkCmdCopyBufferToImage(transferCmdBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

    endAndSubmitCmdBuffer(device, transferCommandPool, transferQueue, transferCmdBuffer);
}

static void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout) {
    // Create Buffer
    VkCommandBuffer cmdBuffer = beginCmdBuffer(device, commandPool);

    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = oldLayout; // Layout to transition from
    imageMemoryBarrier.newLayout = newLayout; // Layout to transition to
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Queue family to transition from
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Queue family to transition to
    imageMemoryBarrier.image = image; // Image being accessed and modified as part of barrier
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Aspect of image being altered
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0; // First mip level to start altering on
    imageMemoryBarrier.subresourceRange.levelCount = 1; // Number of mip levels to later starting from baseMipLevel
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0; // First layer to start alterations on
    imageMemoryBarrier.subresourceRange.layerCount = 1; // Number of layers to alter starting from baseArrayLayer

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    // If transitioning from new image to image ready to receive data...
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        imageMemoryBarrier.srcAccessMask = 0;								// Memory access stage transition must after...
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;		// Memory access stage transition must before...

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
        // If transitioning from transfer destination to shader readable...
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(
            cmdBuffer,
            srcStage, dstStage, // Pipeline Stages (match to src and dst AccessMask)
            0,  // Dependency flags
            0, nullptr, // Memory Barrier count + data
            0, nullptr, // Buffer Memory Barrier count + data
            1, &imageMemoryBarrier // Image Memory Barrier count + data
            );

    endAndSubmitCmdBuffer(device, commandPool, queue, cmdBuffer);
}

#endif
