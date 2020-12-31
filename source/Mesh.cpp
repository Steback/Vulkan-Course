#include <cstring>

#include "Mesh.hpp"

Mesh::Mesh() = default;

Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, const std::vector<Vertex> &vertices,
           VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<uint32_t>& indices)
        : vertexCount_(vertices.size()), physicalDevice_(physicalDevice), device_(device), indexCount_(indices.size()) {
    createVertexBuffer(vertices, transferQueue, transferCommandPool);
    createIndexBuffer(indices, transferQueue, transferCommandPool);

    uboModel_ = {glm::mat4(1.0f)};
}

Mesh::~Mesh() = default;

int Mesh::getVertexCount() const {
    return vertexCount_;
}

VkBuffer Mesh::getVertexBuffer() {
    return vertexbuffer_;
}

int Mesh::getIndexCount() const {
    return indexCount_;
}

VkBuffer Mesh::getIndexBuffer() {
    return indexBuffer_;
}

void Mesh::clean() {
    vkDestroyBuffer(device_, vertexbuffer_, nullptr);
    vkFreeMemory(device_, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device_, indexBuffer_, nullptr);
    vkFreeMemory(device_, indexBufferMemory_, nullptr);
}

const UboModel &Mesh::getUboModel() const {
    return uboModel_;
}

void Mesh::setUboModel(const UboModel &uboModel) {
    uboModel_ = uboModel;
}

void Mesh::createVertexBuffer(const std::vector<Vertex> &vertices, VkQueue transferQueue,
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
    std::memcpy(data, vertices.data(), static_cast<size_t>(bufferSize)); // Copy memory from vertices to the point
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

void Mesh::createIndexBuffer(const std::vector<uint32_t> &indices, VkQueue transferQueue,
                             VkCommandPool transferCommandPool) {
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

    // Temporary buffer to "stage" index data before transferring to GPU
    VkBuffer stagingBuffer{};
    VkDeviceMemory statingBufferMemory{};

    createBuffer(physicalDevice_, device_, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &statingBufferMemory);

    // MAP MEMORY TO INDEX BUFFER
    void* data;
    vkMapMemory(device_, statingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device_, statingBufferMemory);

    // Create buffer for INDEX data on GPU access only area
    createBuffer(physicalDevice_, device_, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer_, &indexBufferMemory_);

    // Copy from staging buffer to GPU access buffer
    copyBuffer(device_, transferQueue, transferCommandPool, stagingBuffer, indexBuffer_, bufferSize);

    // Destroy and Release Staging Buffer resources
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, statingBufferMemory, nullptr);
}
