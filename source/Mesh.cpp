#include <utility>
#include <cstring>

#include "Mesh.hpp"

Mesh::Mesh() = default;

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, const std::vector<Vertex> &vertices,
           VkQueue transferQueue, VkCommandPool transferCommandPool)
        : vertexCount_(vertices.size()), physicalDevice_(physicalDevice), device_(device) {
    createVertexbuffer(vertices, transferQueue, transferCommandPool);
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

void Mesh::createVertexbuffer(const std::vector<Vertex> &vertices, VkQueue transferQueue,
                              VkCommandPool transferCommandPool) {
    // Get size of buffer needed for vertices
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    // Temporary buffer to "stage" vertex data before transferring to GPU
    VkBuffer stagingBuffer{};
    VkDeviceMemory statingBufferMemory{};

    // Create Staging Buffer and Allocate Memory to it
    createBuffer(physicalDevice_, device_, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer,
                 &statingBufferMemory);

    // MAP MEMORY TO VERTEX BUFFER
    void* data; // Create a pointer to a point in normal memory
    vkMapMemory(device_, statingBufferMemory, 0, bufferSize, 0, &data); // "Map" the vertex buffer memory to that point
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize)); // Copy memory from vertices to the point
    vkUnmapMemory(device_, statingBufferMemory); // Unmap the vertex memory buffer  

    // Create buffer with TRANSFER_DST_BIT to mark as recipient of transform data (also VERTEX_BUFFER)
    // Buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU(host)
    createBuffer(physicalDevice_, device_, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexbuffer_, &vertexBufferMemory);

    // Copy staging buffer to vertex buffer on GPU
    copyBuffer(device_, transferQueue, transferCommandPool, stagingBuffer, vertexbuffer_, bufferSize);

    // Clean up staging buffer parts
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, statingBufferMemory, nullptr);
}


