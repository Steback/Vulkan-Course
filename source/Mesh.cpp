#include <utility>
#include <cstring>

#include "Mesh.hpp"

Mesh::Mesh() = default;

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, const std::vector<Vertex> &vertices)
        : vertexCount_(vertices.size()), physicalDevice_(physicalDevice), device_(device) {
    createVertexbuffer(vertices);
}

Mesh::~Mesh() = default;

int Mesh::getVertexCount() const {
    return vertexCount_;
}

VkBuffer Mesh::getVertexBuffer() {
    return vertexbuffer_;
}

void Mesh::clean() {
    vkDestroyBuffer(device_, vertexbuffer_, nullptr);
    vkFreeMemory(device_, vertexBufferMemory, nullptr);
}

void Mesh::createVertexbuffer(const std::vector<Vertex> &vertices) {
    // CREATE VERTEX BUFFER
    // Information to create a buffer (doesn't include assigment memory)
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * vertices.size(), // Size of buffer (size of 1 vertex * number of vertices)
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, // Multiple types of buffer possible, we want Vertex Buffer
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE // Similar to Swap Chain images, can share vertex buffers
    };

    VkResult result = vkCreateBuffer(device_, &bufferCreateInfo, nullptr, &vertexbuffer_);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a Vertex Buffer");
    }

    // GET BUFFET MEMORY REQUIREMENTS
    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device_, vertexbuffer_, &memoryRequirements);

    // ALLOCATE MEMORY TO BUFFER
    // Index of memory type on Physical Device that has required bif flags
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : GPU can interact with memory
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : Allows placement of data straight
    VkMemoryAllocateInfo memoryAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = findMemoryTypeIndex(memoryRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    // Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(device_, &memoryAllocateInfo, nullptr, &vertexBufferMemory);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Vertex Buffer Memory");
    }

    // Allocate memory to given vertex buffer
    vkBindBufferMemory(device_, vertexbuffer_, vertexBufferMemory, 0);

    // MAP MEMORY TO VERTEX BUFFER
    void* data; // Create a pointer to a point in normal memory
    vkMapMemory(device_, vertexBufferMemory, 0, bufferCreateInfo.size, 0, &data); // "Map" the vertex buffer memory to that point
    memcpy(data, vertices.data(), static_cast<size_t>(bufferCreateInfo.size)); // Copy memory from vertices to the point
    vkUnmapMemory(device_, vertexBufferMemory); // Unmap the vertex memory buffer
}

uint32_t Mesh::findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties) {
    // Get properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);

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
