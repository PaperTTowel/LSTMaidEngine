#include "vulkan_debug.hpp"

#include <iostream>
#include <string>

namespace lve::vulkan_debug {
namespace {

const char *severityName(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) return "ERROR";
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) return "WARN";
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) return "INFO";
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) return "VERBOSE";
  return "UNKNOWN";
}

std::string messageTypeName(VkDebugUtilsMessageTypeFlagsEXT type) {
  std::string out;
  if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) out += "general|";
  if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) out += "validation|";
  if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) out += "performance|";
  if (!out.empty()) out.pop_back();
  return out.empty() ? "unknown" : out;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
    void *userData) {
  (void)userData;

  std::cerr << "validation layer [" << severityName(messageSeverity) << "/"
            << messageTypeName(messageType) << "]";
  if (callbackData->pMessageIdName) {
    std::cerr << " " << callbackData->pMessageIdName;
  }
  std::cerr << ": " << callbackData->pMessage << std::endl;

  for (uint32_t i = 0; i < callbackData->objectCount; ++i) {
    const auto &object = callbackData->pObjects[i];
    std::cerr << "  object[" << i << "] type=" << object.objectType
              << " handle=0x" << std::hex << object.objectHandle << std::dec;
    if (object.pObjectName) {
      std::cerr << " name=" << object.pObjectName;
    }
    std::cerr << std::endl;
  }

  return VK_FALSE;
}

} // namespace

void populateMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  createInfo.pUserData = nullptr;
}

VkResult createDebugUtilsMessenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *debugMessenger) {
  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  if (!func) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return func(instance, createInfo, allocator, debugMessenger);
}

void destroyDebugUtilsMessenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *allocator) {
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (func) {
    func(instance, debugMessenger, allocator);
  }
}

void setObjectName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char *name) {
  if (!name || objectHandle == 0) {
    return;
  }

  auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
      vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
  if (!func) {
    return;
  }

  VkDebugUtilsObjectNameInfoEXT nameInfo{};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = objectType;
  nameInfo.objectHandle = objectHandle;
  nameInfo.pObjectName = name;
  func(device, &nameInfo);
}

void beginLabel(
    VkDevice device,
    VkCommandBuffer commandBuffer,
    const char *name,
    float r,
    float g,
    float b,
    float a) {
  if (!name || commandBuffer == VK_NULL_HANDLE) {
    return;
  }

  auto func = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
  if (!func) {
    return;
  }

  VkDebugUtilsLabelEXT label{};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = name;
  label.color[0] = r;
  label.color[1] = g;
  label.color[2] = b;
  label.color[3] = a;
  func(commandBuffer, &label);
}

void endLabel(VkDevice device, VkCommandBuffer commandBuffer) {
  if (commandBuffer == VK_NULL_HANDLE) {
    return;
  }

  auto func = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
  if (func) {
    func(commandBuffer);
  }
}

void insertLabel(
    VkDevice device,
    VkCommandBuffer commandBuffer,
    const char *name,
    float r,
    float g,
    float b,
    float a) {
  if (!name || commandBuffer == VK_NULL_HANDLE) {
    return;
  }

  auto func = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));
  if (!func) {
    return;
  }

  VkDebugUtilsLabelEXT label{};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = name;
  label.color[0] = r;
  label.color[1] = g;
  label.color[2] = b;
  label.color[3] = a;
  func(commandBuffer, &label);
}

} // namespace lve::vulkan_debug
