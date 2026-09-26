#include "VulkanEngine.hpp"

#include <android/asset_manager.h>
#include <android/log.h>
#include <vulkan/vulkan_android.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FrameSmith", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FrameSmith", __VA_ARGS__)

namespace {

bool has_swapchain_extension(VkPhysicalDevice device) {
  std::uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
  for (const auto& extension : extensions) {
    if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool VulkanEngine::start(ANativeWindow* window, AAssetManager* assets) {
  if (running_ || window == nullptr || assets == nullptr) return false;
  window_ = window;
  assets_ = assets;
  ANativeWindow_acquire(window_);
  running_ = true;
  thread_ = std::thread(&VulkanEngine::loop, this);
  return true;
}

void VulkanEngine::stop() {
  running_ = false;
  if (thread_.joinable()) thread_.join();
  if (window_) {
    ANativeWindow_release(window_);
    window_ = nullptr;
  }
  assets_ = nullptr;
}

std::string VulkanEngine::stats() const {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(2);
  out << "FrameSmith · Vulkan\n"
      << "GPU: " << gpu_name_ << "\n"
      << "CPU frame avg " << pacer_.average() << " ms · p95 " << pacer_.p95()
      << " ms" << (pacer_.janky() ? " · JANK" : "") << "\n"
      << "GPU timestamp " << gpu_ms_.load() << " ms\n"
      << "presented " << presented_frames_.load() << " frames";
  if (!timestamps_supported_) out << "\nGPU timestamps unavailable on this device/API";
  return out.str();
}

void VulkanEngine::loop() {
  if (!initVulkan()) {
    LOGE("Vulkan initialization failed");
    running_ = false;
    return;
  }

  using clock = std::chrono::steady_clock;
  while (running_) {
    const auto begin = clock::now();
    if (!drawFrame()) {
      if (!recreateSwapchain()) {
        LOGE("swapchain recreation failed");
        break;
      }
    }
    const auto end = clock::now();
    pacer_.push(std::chrono::duration<double, std::milli>(end - begin).count());
  }

  if (device_) vkDeviceWaitIdle(device_);
  destroyVulkan();
  running_ = false;
}

bool VulkanEngine::initVulkan() {
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "FrameSmith";
  app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app.pEngineName = "FrameSmith Native";
  app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app.apiVersion = VK_API_VERSION_1_1;

  const char* instance_extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
  };
  VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  instance_info.pApplicationInfo = &app;
  instance_info.enabledExtensionCount = 2;
  instance_info.ppEnabledExtensionNames = instance_extensions;

  if (vkCreateInstance(&instance_info, nullptr, &instance_) != VK_SUCCESS) {
    LOGE("vkCreateInstance failed");
    return false;
  }

  VkAndroidSurfaceCreateInfoKHR surface_info{
      VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
  surface_info.window = window_;
  if (vkCreateAndroidSurfaceKHR(instance_, &surface_info, nullptr, &surface_) !=
      VK_SUCCESS) {
    LOGE("vkCreateAndroidSurfaceKHR failed");
    return false;
  }

  if (!pickPhysicalDevice() || !createLogicalDevice()) return false;
  if (!createSwapchain() || !createRenderPass() || !createGraphicsPipeline() ||
      !createFramebuffers() || !createCommandResources() ||
      !createSyncObjects() || !createTimestampQueries()) {
    return false;
  }

  LOGI("initialized GPU=%s swapchain=%ux%u images=%zu",
       gpu_name_.c_str(), swapchain_extent_.width, swapchain_extent_.height,
       swapchain_images_.size());
  return true;
}

bool VulkanEngine::pickPhysicalDevice() {
  std::uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance_, &count, nullptr);
  if (count == 0) {
    LOGE("no Vulkan physical devices");
    return false;
  }

  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance_, &count, devices.data());

  for (auto candidate : devices) {
    if (!has_swapchain_extension(candidate)) continue;

    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count,
                                             families.data());

    std::uint32_t graphics = UINT32_MAX;
    std::uint32_t present = UINT32_MAX;
    for (std::uint32_t i = 0; i < family_count; ++i) {
      if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = i;
      VkBool32 supports_present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_,
                                           &supports_present);
      if (supports_present) present = i;
      if (graphics != UINT32_MAX && present != UINT32_MAX) break;
    }

    if (graphics == UINT32_MAX || present == UINT32_MAX) continue;

    std::uint32_t format_count = 0;
    std::uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface_, &format_count,
                                         nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface_, &mode_count,
                                              nullptr);
    if (format_count == 0 || mode_count == 0) continue;

    physical_ = candidate;
    graphics_family_ = graphics;
    present_family_ = present;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    gpu_name_ = properties.deviceName;
    timestamp_period_ns_ = properties.limits.timestampPeriod;
    timestamps_supported_ =
        properties.limits.timestampComputeAndGraphics == VK_TRUE &&
        VK_VERSION_MAJOR(properties.apiVersion) >= 1 &&
        VK_VERSION_MINOR(properties.apiVersion) >= 2;
    return true;
  }

  LOGE("no device supports graphics + Android presentation + swapchain");
  return false;
}

bool VulkanEngine::createLogicalDevice() {
  const float priority = 1.0f;
  std::set<std::uint32_t> unique_families = {graphics_family_, present_family_};
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (auto family : unique_families) {
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex = family;
    q.queueCount = 1;
    q.pQueuePriorities = &priority;
    queue_infos.push_back(q);
  }

  const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  info.queueCreateInfoCount =
      static_cast<std::uint32_t>(queue_infos.size());
  info.pQueueCreateInfos = queue_infos.data();
  info.enabledExtensionCount = 1;
  info.ppEnabledExtensionNames = extensions;

  if (vkCreateDevice(physical_, &info, nullptr, &device_) != VK_SUCCESS) {
    LOGE("vkCreateDevice failed");
    return false;
  }

  vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
  vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
  return true;
}

bool VulkanEngine::createSwapchain() {
  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &capabilities);

  std::uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &format_count,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &format_count,
                                       formats.data());

  VkSurfaceFormatKHR chosen = formats.front();
  for (const auto& format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = format;
      break;
    }
  }

  if (capabilities.currentExtent.width != UINT32_MAX) {
    swapchain_extent_ = capabilities.currentExtent;
  } else {
    swapchain_extent_.width = std::clamp(
        static_cast<std::uint32_t>(ANativeWindow_getWidth(window_)),
        capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    swapchain_extent_.height = std::clamp(
        static_cast<std::uint32_t>(ANativeWindow_getHeight(window_)),
        capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  }

  std::uint32_t image_count = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0) {
    image_count = std::min(image_count, capabilities.maxImageCount);
  }

  VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  info.surface = surface_;
  info.minImageCount = image_count;
  info.imageFormat = chosen.format;
  info.imageColorSpace = chosen.colorSpace;
  info.imageExtent = swapchain_extent_;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  const std::uint32_t indices[] = {graphics_family_, present_family_};
  if (graphics_family_ != present_family_) {
    info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount = 2;
    info.pQueueFamilyIndices = indices;
  } else {
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  info.preTransform = capabilities.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  info.clipped = VK_TRUE;

  if (vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_) != VK_SUCCESS) {
    LOGE("vkCreateSwapchainKHR failed");
    return false;
  }

  swapchain_format_ = chosen.format;
  vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
  swapchain_images_.resize(image_count);
  vkGetSwapchainImagesKHR(device_, swapchain_, &image_count,
                          swapchain_images_.data());

  swapchain_views_.resize(image_count);
  for (std::size_t i = 0; i < swapchain_images_.size(); ++i) {
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = swapchain_images_[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = swapchain_format_;
    view.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY};
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &view, nullptr, &swapchain_views_[i]) !=
        VK_SUCCESS) {
      return false;
    }
  }
  return true;
}

bool VulkanEngine::createRenderPass() {
  VkAttachmentDescription color{};
  color.format = swapchain_format_;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference reference{};
  reference.attachment = 0;
  reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &reference;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  info.attachmentCount = 1;
  info.pAttachments = &color;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;
  return vkCreateRenderPass(device_, &info, nullptr, &render_pass_) == VK_SUCCESS;
}

std::vector<std::uint32_t> VulkanEngine::loadSpirv(const char* asset_path) const {
  AAsset* asset = AAssetManager_open(assets_, asset_path, AASSET_MODE_BUFFER);
  if (!asset) {
    LOGE("missing shader asset %s", asset_path);
    return {};
  }
  const auto length = static_cast<std::size_t>(AAsset_getLength(asset));
  if (length == 0 || length % sizeof(std::uint32_t) != 0) {
    AAsset_close(asset);
    return {};
  }
  std::vector<std::uint32_t> words(length / sizeof(std::uint32_t));
  const int read = AAsset_read(asset, words.data(), length);
  AAsset_close(asset);
  if (read != static_cast<int>(length)) return {};
  return words;
}

bool VulkanEngine::createGraphicsPipeline() {
  const auto vert = loadSpirv("shaders/scene.vert.spv");
  const auto frag = loadSpirv("shaders/scene.frag.spv");
  if (vert.empty() || frag.empty()) return false;

  auto make_module = [&](const std::vector<std::uint32_t>& code,
                         VkShaderModule* module) {
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(std::uint32_t);
    info.pCode = code.data();
    return vkCreateShaderModule(device_, &info, nullptr, module) == VK_SUCCESS;
  };

  VkShaderModule vert_module = VK_NULL_HANDLE;
  VkShaderModule frag_module = VK_NULL_HANDLE;
  if (!make_module(vert, &vert_module) || !make_module(frag, &frag_module)) {
    if (vert_module) vkDestroyShaderModule(device_, vert_module, nullptr);
    if (frag_module) vkDestroyShaderModule(device_, frag_module, nullptr);
    return false;
  }

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert_module;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag_module;
  stages[1].pName = "main";

  VkPipelineVertexInputStateCreateInfo vertex{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo assembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo raster{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.lineWidth = 1.0f;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisample{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState attachment{};
  attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo blend{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  blend.attachmentCount = 1;
  blend.pAttachments = &attachment;

  const VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic.dynamicStateCount = 2;
  dynamic.pDynamicStates = dynamics;

  VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  if (vkCreatePipelineLayout(device_, &layout, nullptr, &pipeline_layout_) !=
      VK_SUCCESS) {
    vkDestroyShaderModule(device_, vert_module, nullptr);
    vkDestroyShaderModule(device_, frag_module, nullptr);
    return false;
  }

  VkGraphicsPipelineCreateInfo pipeline{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipeline.stageCount = 2;
  pipeline.pStages = stages;
  pipeline.pVertexInputState = &vertex;
  pipeline.pInputAssemblyState = &assembly;
  pipeline.pViewportState = &viewport;
  pipeline.pRasterizationState = &raster;
  pipeline.pMultisampleState = &multisample;
  pipeline.pColorBlendState = &blend;
  pipeline.pDynamicState = &dynamic;
  pipeline.layout = pipeline_layout_;
  pipeline.renderPass = render_pass_;
  pipeline.subpass = 0;

  const VkResult result = vkCreateGraphicsPipelines(
      device_, VK_NULL_HANDLE, 1, &pipeline, nullptr, &pipeline_);

  vkDestroyShaderModule(device_, vert_module, nullptr);
  vkDestroyShaderModule(device_, frag_module, nullptr);
  return result == VK_SUCCESS;
}

bool VulkanEngine::createFramebuffers() {
  framebuffers_.resize(swapchain_views_.size());
  for (std::size_t i = 0; i < swapchain_views_.size(); ++i) {
    VkImageView attachment = swapchain_views_[i];
    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass = render_pass_;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.width = swapchain_extent_.width;
    info.height = swapchain_extent_.height;
    info.layers = 1;
    if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffers_[i]) !=
        VK_SUCCESS) {
      return false;
    }
  }
  return true;
}

bool VulkanEngine::createCommandResources() {
  VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool.queueFamilyIndex = graphics_family_;
  if (vkCreateCommandPool(device_, &pool, nullptr, &command_pool_) !=
      VK_SUCCESS) {
    return false;
  }

  command_buffers_.resize(kFramesInFlight);
  VkCommandBufferAllocateInfo allocate{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocate.commandPool = command_pool_;
  allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate.commandBufferCount = kFramesInFlight;
  return vkAllocateCommandBuffers(device_, &allocate, command_buffers_.data()) ==
         VK_SUCCESS;
}

bool VulkanEngine::createSyncObjects() {
  VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (std::uint32_t i = 0; i < kFramesInFlight; ++i) {
    if (vkCreateSemaphore(device_, &semaphore, nullptr, &image_available_[i]) !=
            VK_SUCCESS ||
        vkCreateSemaphore(device_, &semaphore, nullptr, &render_finished_[i]) !=
            VK_SUCCESS ||
        vkCreateFence(device_, &fence, nullptr, &in_flight_[i]) != VK_SUCCESS) {
      return false;
    }
  }
  return true;
}

bool VulkanEngine::createTimestampQueries() {
  if (!timestamps_supported_) return true;
  VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = kFramesInFlight * 2;
  return vkCreateQueryPool(device_, &info, nullptr, &timestamp_pool_) ==
         VK_SUCCESS;
}

bool VulkanEngine::recordCommandBuffer(VkCommandBuffer cmd,
                                       std::uint32_t image_index,
                                       std::uint32_t frame_slot) {
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) return false;

  const std::uint32_t q = frame_slot * 2;
  if (timestamps_supported_) {
    vkCmdResetQueryPool(cmd, timestamp_pool_, q, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamp_pool_,
                        q);
  }

  const VkClearValue clear = {{{0.025f, 0.035f, 0.07f, 1.0f}}};
  VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  pass.renderPass = render_pass_;
  pass.framebuffer = framebuffers_[image_index];
  pass.renderArea.extent = swapchain_extent_;
  pass.clearValueCount = 1;
  pass.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

  VkViewport viewport{};
  viewport.width = static_cast<float>(swapchain_extent_.width);
  viewport.height = static_cast<float>(swapchain_extent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = swapchain_extent_;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);

  if (timestamps_supported_) {
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        timestamp_pool_, q + 1);
  }

  return vkEndCommandBuffer(cmd) == VK_SUCCESS;
}

bool VulkanEngine::drawFrame() {
  const std::uint32_t slot = frame_slot_;
  vkWaitForFences(device_, 1, &in_flight_[slot], VK_TRUE,
                  std::numeric_limits<std::uint64_t>::max());

  if (timestamps_supported_ && presented_frames_.load() >= kFramesInFlight) {
    std::uint64_t timestamps[2]{};
    const std::uint32_t q = slot * 2;
    if (vkGetQueryPoolResults(device_, timestamp_pool_, q, 2,
                              sizeof(timestamps), timestamps,
                              sizeof(std::uint64_t),
                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS &&
        timestamps[1] >= timestamps[0]) {
      gpu_ms_.store((timestamps[1] - timestamps[0]) * timestamp_period_ns_ /
                    1e6);
    }
  }

  std::uint32_t image_index = 0;
  VkResult acquire = vkAcquireNextImageKHR(
      device_, swapchain_, std::numeric_limits<std::uint64_t>::max(),
      image_available_[slot], VK_NULL_HANDLE, &image_index);
  if (acquire == VK_ERROR_OUT_OF_DATE_KHR) return false;
  if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) return false;

  vkResetFences(device_, 1, &in_flight_[slot]);
  vkResetCommandBuffer(command_buffers_[slot], 0);
  if (!recordCommandBuffer(command_buffers_[slot], image_index, slot)) {
    return false;
  }

  const VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &image_available_[slot];
  submit.pWaitDstStageMask = &wait_stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &command_buffers_[slot];
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &render_finished_[slot];

  if (vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_[slot]) !=
      VK_SUCCESS) {
    return false;
  }

  VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &render_finished_[slot];
  present.swapchainCount = 1;
  present.pSwapchains = &swapchain_;
  present.pImageIndices = &image_index;

  const VkResult result = vkQueuePresentKHR(present_queue_, &present);
  presented_frames_.fetch_add(1);
  frame_slot_ = (frame_slot_ + 1) % kFramesInFlight;

  return result == VK_SUCCESS;
}

void VulkanEngine::destroySwapchain() {
  for (auto framebuffer : framebuffers_) {
    if (framebuffer) vkDestroyFramebuffer(device_, framebuffer, nullptr);
  }
  framebuffers_.clear();

  if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
  if (pipeline_layout_)
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);
  pipeline_ = VK_NULL_HANDLE;
  pipeline_layout_ = VK_NULL_HANDLE;
  render_pass_ = VK_NULL_HANDLE;

  for (auto view : swapchain_views_) {
    if (view) vkDestroyImageView(device_, view, nullptr);
  }
  swapchain_views_.clear();
  swapchain_images_.clear();

  if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  swapchain_ = VK_NULL_HANDLE;
}

bool VulkanEngine::recreateSwapchain() {
  if (!device_) return false;
  vkDeviceWaitIdle(device_);
  destroySwapchain();

  while (running_ &&
         (ANativeWindow_getWidth(window_) == 0 ||
          ANativeWindow_getHeight(window_) == 0)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  if (!running_) return false;

  return createSwapchain() && createRenderPass() && createGraphicsPipeline() &&
         createFramebuffers();
}

void VulkanEngine::destroyVulkan() {
  if (!device_) return;

  if (timestamp_pool_)
    vkDestroyQueryPool(device_, timestamp_pool_, nullptr);
  timestamp_pool_ = VK_NULL_HANDLE;

  for (std::uint32_t i = 0; i < kFramesInFlight; ++i) {
    if (image_available_[i])
      vkDestroySemaphore(device_, image_available_[i], nullptr);
    if (render_finished_[i])
      vkDestroySemaphore(device_, render_finished_[i], nullptr);
    if (in_flight_[i]) vkDestroyFence(device_, in_flight_[i], nullptr);
  }

  if (command_pool_)
    vkDestroyCommandPool(device_, command_pool_, nullptr);
  command_pool_ = VK_NULL_HANDLE;

  destroySwapchain();

  vkDestroyDevice(device_, nullptr);
  device_ = VK_NULL_HANDLE;

  if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
  surface_ = VK_NULL_HANDLE;
  if (instance_) vkDestroyInstance(instance_, nullptr);
  instance_ = VK_NULL_HANDLE;
}
