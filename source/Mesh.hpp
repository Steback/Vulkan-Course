#ifndef VULKAN_COURSE_MESH_HPP
#define VULKAN_COURSE_MESH_HPP


#include <vector>

#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"

#include "Utilities.hpp"


class Mesh {
    public:
        Mesh();
        Mesh(VkPhysicalDevice physicalDevice, VkDevice device, const std::vector<Vertex>& vertices);
        ~Mesh();
        [[nodiscard]] int getVertexCount() const;
        VkBuffer getVertexBuffer();
        void clean();

    private:
        void createVertexbuffer(const std::vector<Vertex>& vertices);
        uint32_t findMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties);

    private:
        int vertexCount_{};
        VkBuffer vertexbuffer_{};
        VkPhysicalDevice physicalDevice_{};
        VkDevice device_{};
        VkDeviceMemory vertexBufferMemory{};
};


#endif
