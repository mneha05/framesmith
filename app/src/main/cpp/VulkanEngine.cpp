#include "VulkanEngine.hpp"
#include <android/log.h>
#include <vulkan/vulkan_android.h>
#include <chrono>
#include <sstream>
#include <vector>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"FrameSmith",__VA_ARGS__)

bool VulkanEngine::start(ANativeWindow* w){ if(running_) return true; window_=w; ANativeWindow_acquire(window_); running_=true; thread_=std::thread(&VulkanEngine::loop,this); return true; }
void VulkanEngine::stop(){ running_=false; if(thread_.joinable())thread_.join(); if(window_){ANativeWindow_release(window_);window_=nullptr;} }
std::string VulkanEngine::stats() const { std::ostringstream o; o<<"FrameSmith · Vulkan\nGPU: "<<gpu_name_<<"\navg "<<pacer_.average()<<" ms · p95 "<<pacer_.p95()<<" ms"<<(pacer_.janky()?" · JANK":""); return o.str(); }
void VulkanEngine::loop(){ if(!initVulkan()){LOGI("Vulkan unavailable"); running_=false; return;} using clock=std::chrono::steady_clock; while(running_){auto a=clock::now(); if(!drawFrame()) recreateSwapchain(); auto b=clock::now(); pacer_.push(std::chrono::duration<double,std::milli>(b-a).count()); std::this_thread::sleep_for(std::chrono::milliseconds(8));} destroyVulkan(); }
bool VulkanEngine::initVulkan(){
  VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO}; ai.pApplicationName="FrameSmith"; ai.apiVersion=VK_API_VERSION_1_1;
  const char* exts[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_ANDROID_SURFACE_EXTENSION_NAME}; VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&ai; ci.enabledExtensionCount=2; ci.ppEnabledExtensionNames=exts; if(vkCreateInstance(&ci,nullptr,&instance_)!=VK_SUCCESS)return false;
  uint32_t n=0; vkEnumeratePhysicalDevices(instance_,&n,nullptr); if(!n)return false; std::vector<VkPhysicalDevice> p(n); vkEnumeratePhysicalDevices(instance_,&n,p.data()); physical_=p[0]; VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(physical_,&props); gpu_name_=props.deviceName;
  uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(physical_,&qn,nullptr); std::vector<VkQueueFamilyProperties> qp(qn); vkGetPhysicalDeviceQueueFamilyProperties(physical_,&qn,qp.data()); uint32_t q=0; for(uint32_t i=0;i<qn;i++)if(qp[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){q=i;break;} float pr=1; VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=q; qi.queueCount=1; qi.pQueuePriorities=&pr; const char* dext[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME}; VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi; di.enabledExtensionCount=1; di.ppEnabledExtensionNames=dext; if(vkCreateDevice(physical_,&di,nullptr,&device_)!=VK_SUCCESS)return false; vkGetDeviceQueue(device_,q,0,&graphics_);
  VkAndroidSurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR}; si.window=window_; if(vkCreateAndroidSurfaceKHR(instance_,&si,nullptr,&surface_)!=VK_SUCCESS)return false; return recreateSwapchain();
}
bool VulkanEngine::recreateSwapchain(){ if(!device_||!surface_)return false; if(swapchain_){vkDeviceWaitIdle(device_);vkDestroySwapchainKHR(device_,swapchain_,nullptr);swapchain_=VK_NULL_HANDLE;} /* Full image-view/render-pass path intentionally isolated for extension. */ return true; }
bool VulkanEngine::drawFrame(){ /* Skeleton keeps lifecycle + device/surface ownership real; rendering path is extended in renderer milestone. */ return true; }
void VulkanEngine::destroyVulkan(){ if(device_)vkDeviceWaitIdle(device_); if(swapchain_)vkDestroySwapchainKHR(device_,swapchain_,nullptr); if(surface_)vkDestroySurfaceKHR(instance_,surface_,nullptr); if(device_)vkDestroyDevice(device_,nullptr); if(instance_)vkDestroyInstance(instance_,nullptr); swapchain_=VK_NULL_HANDLE;surface_=VK_NULL_HANDLE;device_=VK_NULL_HANDLE;instance_=VK_NULL_HANDLE; }
