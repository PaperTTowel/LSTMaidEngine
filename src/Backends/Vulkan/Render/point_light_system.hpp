#pragma once

#include "Engine/Core/camera.hpp"
#include "Engine/Scene/game_object.hpp"
#include "Backends/Vulkan/Core/pipeline.hpp"
#include "Backends/Vulkan/Render/frame_info.hpp"
#include "Backends/Vulkan/Core/device.hpp"

// std
#include <memory>
#include <vector>

namespace lve{
    class PointLightSystem{
    public:

        PointLightSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~PointLightSystem();

        PointLightSystem(const LveWindow &) = delete;
        PointLightSystem &operator=(const LveWindow &) = delete;

        void update(FrameInfo &frameInfo, GlobalUbo &ubo);
        void render(FrameInfo &frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;
        std::unique_ptr<LvePipeline> lvePipeline;
        VkPipelineLayout pipelineLayout;
    };
} // namespace lve

