#pragma once

#include <android/asset_manager.h>
#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "FramePacer.hpp"

class VulkanEngine {
 public:
  bool start(ANativeWindow* window, AAssetManager* assets);
  void stop();
  std::string stats() const;

 private:
  void loop();
  bool initVulkan();
  void destroyVulkan();

  bool pickPhysicalDevice();
  bool createLogicalDevice();
  bool createSwapchain();
  void destroySwapchain();
  bool recreateSwapchain();

  bool createRenderPass();
  bool createGraphicsPipeline();
  bool createFramebuffers();
  bool createCommandResources();
  bool createSyncObjects();
  bool createTimestampQueries();

  bool drawFrame();
  bool recordCommandBuffer(VkCommandBuffer cmd, std::uint32_t image_index,
                           std::uint32_t frame_slot);
  std::vector<std::uint32_t> loadSpirv(const char* asset_path) const;

  ANativeWindow* window_{nullptr};
  AAssetManager* assets_{nullptr};
  std::thread thread_;
  std::atomic<bool> running_{false};

  VkInstance instance_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  VkQueue present_queue_{VK_NULL_HANDLE};

  std::uint32_t graphics_family_{UINT32_MAX};
  std::uint32_t present_family_{UINT32_MAX};

  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat swapchain_format_{VK_FORMAT_UNDEFINED};
  VkExtent2D swapchain_extent_{};
  std::vector<VkImage> swapchain_images_;
  std::vector<VkImageView> swapchain_views_;
  std::vector<VkFramebuffer> framebuffers_;

  VkRenderPass render_pass_{VK_NULL_HANDLE};
  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkPipeline pipeline_{VK_NULL_HANDLE};

  VkCommandPool command_pool_{VK_NULL_HANDLE};
  std::vector<VkCommandBuffer> command_buffers_;

  static constexpr std::uint32_t kFramesInFlight = 2;
  VkSemaphore image_available_[kFramesInFlight]{};
  VkSemaphore render_finished_[kFramesInFlight]{};
  VkFence in_flight_[kFramesInFlight]{};
  VkQueryPool timestamp_pool_{VK_NULL_HANDLE};

  std::uint32_t frame_slot_{0};
  float timestamp_period_ns_{0.0f};
  bool timestamps_supported_{false};

  mutable FramePacer pacer_;
  std::atomic<double> gpu_ms_{0.0};
  std::atomic<std::uint64_t> presented_frames_{0};
  std::string gpu_name_{"uninitialized"};
};
