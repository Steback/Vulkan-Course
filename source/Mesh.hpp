#ifndef VULKAN_COURSE_MESH_HPP
#define VULKAN_COURSE_MESH_HPP


#include <vector>

#include "vulkan/vulkan.h"
#include "GLFW/glfw3.h"

#include "Utilities.hpp"


class Mesh {
    public:
        Mesh();
        Mesh(VkPhysicalDevice physicalDevice, VkDevice device, const std::vector<Vertex>& vertices,
             VkQueue transferQueue, VkCommandPool transferCommandPool, const std::vector<uint32_t>& indices);
        ~Mesh();
        [[nodiscard]] int getVertexCount() const;
        VkBuffer getVertexBuffer();
        void clean();
        [[nodiscard]] int getIndexCount() const;
        VkBuffer getIndexBuffer();

    private:
        void createVertexBuffer(const std::vector<Vertex>& vertices, VkQueue transferQueue,
                                VkCommandPool transferCommandPool);
        void createIndexBuffer(const std::vector<uint32_t>& indices, VkQueue transferQueue,
                               VkCommandPool transferCommandPool);

    private:
        int vertexCount_{};
        VkBuffer vertexbuffer_{};
        VkPhysicalDevice physicalDevice_{};
        VkDevice device_{};
        VkDeviceMemory vertexBufferMemory{};
        int indexCount_{};
        VkBuffer indexBuffer_{};
        VkDeviceMemory indexBufferMemory_{};
};


#endif
