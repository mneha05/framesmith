#pragma once
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <atomic>
#include <thread>
#include <string>
#include "FramePacer.hpp"
class VulkanEngine {
public:
  bool start(ANativeWindow* window); void stop(); std::string stats() const;
private:
  void loop(); bool initVulkan(); void destroyVulkan(); bool recreateSwapchain(); bool drawFrame();
  ANativeWindow* window_{nullptr}; std::thread thread_; std::atomic<bool> running_{false};
  VkInstance instance_{VK_NULL_HANDLE}; VkPhysicalDevice physical_{VK_NULL_HANDLE}; VkDevice device_{VK_NULL_HANDLE}; VkQueue graphics_{VK_NULL_HANDLE}; VkSurfaceKHR surface_{VK_NULL_HANDLE}; VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  mutable FramePacer pacer_; std::string gpu_name_{"uninitialized"};
};
