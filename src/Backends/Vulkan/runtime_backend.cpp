#include "Backends/Vulkan/runtime_backend.hpp"

#include "Backends/Vulkan/Render/object_buffer_pool.hpp"
#include "Engine/Scene/game_object.hpp"

namespace lve::backend {

  VulkanRuntimeBackend::VulkanRuntimeBackend(int width, int height, std::string title)
    : windowImpl{width, height, std::move(title), WindowClientApi::Vulkan}
    , inputProvider{windowImpl}
    , windowBackend{windowImpl, inputProvider}
    , device{windowImpl}
    , assetFactory{device}
    , sceneSystemImpl{
        assetFactory,
        std::make_unique<VulkanObjectBufferPool>(
          device,
          LveGameObjectManager::MAX_GAME_OBJECTS,
          sizeof(GameObjectBufferData))}
    , renderBackendImpl{windowImpl, device} {}

} // namespace lve::backend
