#include "MeshModel.hpp"

#include <utility>
#include "Mesh.hpp"

MeshModel::MeshModel() = default;

MeshModel::MeshModel(std::vector<Mesh> meshList) : meshList_(std::move(meshList)), model_(1.0f) {

}

MeshModel::~MeshModel() = default;

size_t MeshModel::getMeshCount() const {
    return meshList_.size();
}

Mesh *MeshModel::getMesh(size_t index) {
    if (index >= meshList_.size()) {
        throw std::runtime_error("Attempted to access invalid Mesh Index");
    }

    return &meshList_[index];
}

const glm::mat4 &MeshModel::getModel() const {
    return model_;
}

void MeshModel::setModel(const glm::mat4 &model) {
    MeshModel::model_ = model;
}

void MeshModel::clean() {
    for (auto& mesh : meshList_) {
        mesh.clean();
    }
}

std::vector<std::string> MeshModel::loadMaterials(const aiScene *scene) {
    // Create 1:1 sized list of textures
    std::vector<std::string> textureList(scene->mNumMaterials);

    // Go through each material and copy its texture file name (if it exists)
    for (size_t i = 0; i < scene->mNumMaterials; ++i) {
        // Get the material
        aiMaterial * material = scene->mMaterials[i];

        // Initialise the texture to empty string (will be replaced if texture exists)
        textureList[i] = "";

        // Check for a Diffuse Texture (standard detail texture)
        if (material->GetTextureCount(aiTextureType_DIFFUSE)) {
            // Get the path of the texture file
            aiString path;

            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
                // Cut off any directory information aleady present
                int idx = std::string(path.data).rfind("\\");
                std::string fileName = std::string(path.data).substr(idx + 1);

                textureList[i] = fileName;
            }
        }
    }

    return textureList;
}

std::vector<Mesh>MeshModel::LoadNode(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool commandPool,
                    aiNode *node, const aiScene *scene, const std::vector<int>& matToTex) {
    std::vector<Mesh> meshList;

    // Go through each mesh at this node and create it, then add it to our meshList
    for (size_t i = 0; i < node->mNumMeshes; ++i) {
        meshList.push_back(
                LoadMesh(physicalDevice, device, queue, commandPool, scene->mMeshes[node->mMeshes[i]], scene, matToTex)
        );
    }

    // Go through each node attached to this node and load it, then append their meshes to this node's mesh list
    for (size_t i = 0; i < node->mNumChildren; ++i) {
        std::vector<Mesh> newList = LoadNode(physicalDevice, device, queue, commandPool, node->mChildren[i], scene, matToTex);
        meshList.insert(meshList.end(), newList.begin(), newList.end());
    }

    return meshList;
}

Mesh MeshModel::LoadMesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool commandPool,
                         aiMesh *mesh, const aiScene *scene, const std::vector<int>& matToTex) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Resize vertex list to hold all vertices for mesh
    vertices.resize(mesh->mNumVertices);

    // Go through each vertex and copy it across to our vertices
    for (size_t i = 0; i < mesh->mNumVertices; ++i) {
        // Set position
        vertices[i].pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

        // Set tex coords (if they exist)
        if (mesh->mTextureCoords[0]) {
            vertices[i].tex = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        } else {
            vertices[i].tex = { 0.0f, 0.0f };
        }

        // Set colour (just use white for now)
        vertices[i].col = { 1.0f, 1.0f, 1.0f };
    }

    // Iterate over indices through faces and copy across
    for (size_t i = 0; i < mesh->mNumFaces; ++i) {
        // Get a face
        aiFace face = mesh->mFaces[i];

        // Go through face's indices and add to list
        for (size_t j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Create new mesh with details and return it
    Mesh newMesh = Mesh(physicalDevice, device, vertices, queue, commandPool, indices, matToTex[mesh->mMaterialIndex]);

    return newMesh;
}
