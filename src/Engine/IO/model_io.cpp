#include "Engine/IO/model_io.hpp"

#include "Engine/Core/runtime_paths.hpp"
#include "Engine/Core/hash.hpp"

// libs
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <unordered_map>
#include <utility>

namespace std {
  template <>
  struct hash<lve::backend::ModelVertex> {
    size_t operator()(lve::backend::ModelVertex const &vertex) const {
      size_t seed = 0;
      lve::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv, vertex.tangent);
      return seed;
    }
  };
}  // namespace std

namespace lve {

  namespace {
    glm::mat4 toGlmMat4(const aiMatrix4x4 &m) {
      glm::mat4 out{1.0f};
      out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
      out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
      out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
      out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
      return out;
    }

    std::string resolveTexturePath(const std::filesystem::path &baseDir, const std::string &relative) {
      if (relative.empty()) {
        return {};
      }
      std::filesystem::path path{relative};
      if (path.is_relative()) {
        path = baseDir / path;
      }
      return path.generic_string();
    }

    backend::ModelTextureSource loadEmbeddedTexture(const aiScene *scene, int index) {
      backend::ModelTextureSource source{};
      if (!scene || index < 0 || index >= static_cast<int>(scene->mNumTextures)) {
        return source;
      }

      const aiTexture *tex = scene->mTextures[index];
      if (!tex) {
        return source;
      }

      if (tex->mHeight == 0) {
        source.kind = backend::ModelTextureSource::Kind::EmbeddedCompressed;
        const auto *dataPtr = reinterpret_cast<const unsigned char *>(tex->pcData);
        source.data.assign(dataPtr, dataPtr + tex->mWidth);
      } else {
        source.kind = backend::ModelTextureSource::Kind::EmbeddedRaw;
        source.width = static_cast<int>(tex->mWidth);
        source.height = static_cast<int>(tex->mHeight);
        source.data.resize(static_cast<std::size_t>(source.width) * source.height * 4);
        for (int y = 0; y < source.height; ++y) {
          for (int x = 0; x < source.width; ++x) {
            const aiTexel &texel = tex->pcData[y * source.width + x];
            const std::size_t idx = (static_cast<std::size_t>(y) * source.width + x) * 4;
            source.data[idx + 0] = texel.r;
            source.data[idx + 1] = texel.g;
            source.data[idx + 2] = texel.b;
            source.data[idx + 3] = texel.a;
          }
        }
      }
      return source;
    }

    backend::ModelTextureSource loadTextureSource(
      const aiScene *scene,
      const aiMaterial *material,
      aiTextureType textureType,
      const std::filesystem::path &baseDir) {
      backend::ModelTextureSource source{};
      if (!material || material->GetTextureCount(textureType) == 0) {
        return source;
      }

      aiString path;
      if (material->GetTexture(textureType, 0, &path) != AI_SUCCESS) {
        return source;
      }

      const std::string texPath = path.C_Str();
      if (texPath.empty()) {
        return source;
      }

      if (texPath[0] == '*') {
        const int texIndex = std::atoi(texPath.c_str() + 1);
        return loadEmbeddedTexture(scene, texIndex);
      }

      source.kind = backend::ModelTextureSource::Kind::File;
      source.path = resolveTexturePath(baseDir, texPath);
      return source;
    }

    void processMesh(
      const aiMesh *mesh,
      const aiMaterial *material,
      backend::ModelData &data,
      std::unordered_map<backend::ModelVertex, uint32_t> &uniqueVertices,
      backend::ModelSubMesh &outSubMesh,
      const backend::ModelLoadOptions &options) {
      const uint32_t indexStart = static_cast<uint32_t>(data.indices.size());
      glm::vec3 boundsMin(std::numeric_limits<float>::max());
      glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
      bool hasBounds = false;
      for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        const aiFace &face = mesh->mFaces[faceIndex];
        if (face.mNumIndices != 3) {
          continue;
        }
        for (unsigned int i = 0; i < face.mNumIndices; ++i) {
          unsigned int index = face.mIndices[i];
          backend::ModelVertex vertex{};

          if (mesh->HasPositions()) {
            const aiVector3D &pos = mesh->mVertices[index];
            vertex.position = glm::vec3{pos.x, pos.y, pos.z} * options.scale;
            boundsMin = glm::min(boundsMin, vertex.position);
            boundsMax = glm::max(boundsMax, vertex.position);
            hasBounds = true;
          }

          if (mesh->HasNormals()) {
            const aiVector3D &n = mesh->mNormals[index];
            vertex.normal = {n.x, n.y, n.z};
          }

          if (mesh->HasTextureCoords(0)) {
            const aiVector3D &uv = mesh->mTextureCoords[0][index];
            vertex.uv = {uv.x, uv.y};
          }

          if (mesh->HasTangentsAndBitangents()) {
            const aiVector3D &t = mesh->mTangents[index];
            const aiVector3D &b = mesh->mBitangents[index];
            const glm::vec3 tangent{t.x, t.y, t.z};
            const glm::vec3 bitangent{b.x, b.y, b.z};
            const float handedness =
                glm::dot(glm::cross(vertex.normal, tangent), bitangent) < 0.0f ? -1.0f : 1.0f;
            vertex.tangent = {tangent.x, tangent.y, tangent.z, handedness};
          }

          if (mesh->HasVertexColors(0)) {
            const aiColor4D &c = mesh->mColors[0][index];
            vertex.color = {c.r, c.g, c.b};
          } else if (material) {
            aiColor4D diffuse;
            if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS) {
              vertex.color = {diffuse.r, diffuse.g, diffuse.b};
            } else {
              vertex.color = {1.0f, 1.0f, 1.0f};
            }
          } else {
            vertex.color = {1.0f, 1.0f, 1.0f};
          }

          if (uniqueVertices.count(vertex) == 0) {
            uniqueVertices[vertex] = static_cast<uint32_t>(data.vertices.size());
            data.vertices.push_back(vertex);
          }
          data.indices.push_back(uniqueVertices[vertex]);
        }
      }
      outSubMesh.firstIndex = indexStart;
      outSubMesh.indexCount = static_cast<uint32_t>(data.indices.size()) - indexStart;
      outSubMesh.materialIndex = static_cast<int>(mesh->mMaterialIndex);
      outSubMesh.hasBounds = hasBounds;
      if (hasBounds) {
        outSubMesh.boundsMin = boundsMin;
        outSubMesh.boundsMax = boundsMax;
      }
    }

    void processNode(
      const aiNode *node,
      int parentIndex,
      const std::vector<int> &meshIndexToSubmesh,
      backend::ModelData &data) {
      backend::ModelNode newNode{};
      newNode.name = node->mName.C_Str();
      newNode.parent = parentIndex;
      newNode.localTransform = toGlmMat4(node->mTransformation);
      newNode.meshes.reserve(node->mNumMeshes);
      for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
        const unsigned int sceneMeshIndex = node->mMeshes[meshIndex];
        if (sceneMeshIndex < meshIndexToSubmesh.size()) {
          const int subMeshIndex = meshIndexToSubmesh[sceneMeshIndex];
          if (subMeshIndex >= 0) {
            newNode.meshes.push_back(subMeshIndex);
          }
        }
      }

      const int nodeIndex = static_cast<int>(data.nodes.size());
      data.nodes.push_back(std::move(newNode));
      if (parentIndex >= 0) {
        data.nodes[static_cast<std::size_t>(parentIndex)].children.push_back(nodeIndex);
      }

      for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        processNode(node->mChildren[childIndex], nodeIndex, meshIndexToSubmesh, data);
      }
    }
  } // namespace

  bool loadModelDataFromFile(
    const std::string &path,
    backend::ModelData &outData,
    std::string *outError,
    const backend::ModelLoadOptions &options) {
    const std::string resolvedPath = RuntimePaths::resolveResourcePath(path);

    unsigned int postProcessFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices;
    if (options.generateNormals) {
      postProcessFlags |= aiProcess_GenNormals;
    }
    if (options.generateTangents) {
      postProcessFlags |= aiProcess_CalcTangentSpace;
    }
    if (options.flipUV) {
      postProcessFlags |= aiProcess_FlipUVs;
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(resolvedPath, postProcessFlags);
    if (!scene || !scene->mRootNode) {
      if (outError) {
        *outError = importer.GetErrorString();
      }
      return false;
    }

    outData.vertices.clear();
    outData.indices.clear();
    outData.subMeshes.clear();
    outData.nodes.clear();
    outData.materials.clear();

    const std::filesystem::path baseDir = std::filesystem::path(resolvedPath).parent_path();
    outData.materials.resize(scene->mNumMaterials);
    for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
      const aiMaterial *material = scene->mMaterials[materialIndex];
      backend::ModelMaterialSource materialSource{};
      materialSource.diffuse = loadTextureSource(scene, material, aiTextureType_BASE_COLOR, baseDir);
      if (materialSource.diffuse.kind == backend::ModelTextureSource::Kind::None) {
        materialSource.diffuse = loadTextureSource(scene, material, aiTextureType_DIFFUSE, baseDir);
      }
      outData.materials[materialIndex] = std::move(materialSource);
    }

    std::unordered_map<backend::ModelVertex, uint32_t> uniqueVertices{};
    std::vector<int> meshIndexToSubmesh(scene->mNumMeshes, -1);
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
      const aiMesh *mesh = scene->mMeshes[meshIndex];
      const aiMaterial *material = nullptr;
      if (mesh->mMaterialIndex < scene->mNumMaterials) {
        material = scene->mMaterials[mesh->mMaterialIndex];
      }
      backend::ModelSubMesh subMesh{};
      processMesh(mesh, material, outData, uniqueVertices, subMesh, options);
      meshIndexToSubmesh[meshIndex] = static_cast<int>(outData.subMeshes.size());
      outData.subMeshes.push_back(subMesh);
    }

    processNode(scene->mRootNode, -1, meshIndexToSubmesh, outData);
    return true;
  }

}  // namespace lve
