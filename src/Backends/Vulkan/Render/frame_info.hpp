#pragma once

#include "Engine/Rendering/render_types.hpp"
#include "Engine/Core/camera.hpp"
#include "Backends/Vulkan/Core/descriptors.hpp"
#include "Engine/Scene/game_object.hpp"

// lib
#include <vulkan/vulkan.h>
#include <array>
#include <unordered_map>
#include <vector>

namespace lve{

    #define MAX_LIGHTS 10
    constexpr int RENDER_VIEW_COUNT = 2;

    struct PointLight{
        glm::vec4 position{}; // ignore w
        glm::vec4 color{};    // w is intensity
    };

    struct GlobalUbo{
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};  // w is intensity
        PointLight pointLights[MAX_LIGHTS];
        int numLights;
    };

    struct DescriptorSetCacheEntry {
        std::array<VkDescriptorSet, backend::kMaxFramesInFlight> sets{};
        std::array<MaterialTextureBindings, backend::kMaxFramesInFlight> textures{};

        void clearFrame(int frameIndex) {
            sets[frameIndex] = VK_NULL_HANDLE;
            textures[frameIndex] = MaterialTextureBindings{};
        }
    };

    class FrameDescriptorCache {
    public:
        void clear() {
            simpleObjectCaches.clear();
            spriteObjectCaches.clear();
            subMeshObjectCaches.clear();
        }

        void clearFrame(int frameIndex) {
            for (auto &kv : simpleObjectCaches) {
                kv.second.clearFrame(frameIndex);
            }
            for (auto &kv : spriteObjectCaches) {
                kv.second.clearFrame(frameIndex);
            }
            for (auto &kv : subMeshObjectCaches) {
                for (auto &cache : kv.second) {
                    cache.clearFrame(frameIndex);
                }
            }
        }

        DescriptorSetCacheEntry &simpleObjectCacheFor(LveGameObject::id_t objectId) {
            return simpleObjectCaches[objectId];
        }

        DescriptorSetCacheEntry &spriteObjectCacheFor(LveGameObject::id_t objectId) {
            return spriteObjectCaches[objectId];
        }

        std::vector<DescriptorSetCacheEntry> &subMeshCachesFor(
            LveGameObject::id_t objectId,
            std::size_t subMeshCount) {
            auto &caches = subMeshObjectCaches[objectId];
            if (caches.size() < subMeshCount) {
                caches.resize(subMeshCount);
            }
            return caches;
        }

        std::size_t simpleObjectCacheCount() const { return simpleObjectCaches.size(); }
        std::size_t spriteObjectCacheCount() const { return spriteObjectCaches.size(); }
        std::size_t subMeshObjectCacheCount() const { return subMeshObjectCaches.size(); }

        std::size_t subMeshCacheCount() const {
            std::size_t count = 0;
            for (const auto &kv : subMeshObjectCaches) {
                count += kv.second.size();
            }
            return count;
        }

    private:
        std::unordered_map<LveGameObject::id_t, DescriptorSetCacheEntry> simpleObjectCaches{};
        std::unordered_map<LveGameObject::id_t, DescriptorSetCacheEntry> spriteObjectCaches{};
        std::unordered_map<LveGameObject::id_t, std::vector<DescriptorSetCacheEntry>> subMeshObjectCaches{};
    };

    struct FrameInfo{
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        LveCamera &camera;
        VkDescriptorSet globalDescriptorSet;
        LveDescriptorPool &frameDescriptorPool;  // pool for cached per-object descriptors
        FrameDescriptorCache &descriptorCache;
        std::vector<LveGameObject*> &gameObjects;
    };
} // namespace lve

