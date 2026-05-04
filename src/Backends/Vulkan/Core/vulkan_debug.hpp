#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace lve::vulkan_debug {

void populateMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

VkResult createDebugUtilsMessenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *debugMessenger);

void destroyDebugUtilsMessenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *allocator);

void setObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char *name);

void beginLabel(
    VkDevice device,
    VkCommandBuffer commandBuffer,
    const char *name,
    float r,
    float g,
    float b,
    float a = 1.0f);

void endLabel(VkDevice device, VkCommandBuffer commandBuffer);

void insertLabel(
    VkDevice device,
    VkCommandBuffer commandBuffer,
    const char *name,
    float r,
    float g,
    float b,
    float a = 1.0f);

} // namespace lve::vulkan_debug
