// CSS 64-bit defaults to shaderapivk / DXVK. GL swap never runs.
// Intercept vkQueuePresentKHR and dump the presented swapchain image.
#include <vulkan/vulkan.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

namespace {

void Log(const char* fmt, ...) {
  FILE* f = std::fopen("/tmp/cssvrmod.log", "a");
  if (!f) return;
  std::fputs("vk: ", f);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);
  std::fputc('\n', f);
  std::fclose(f);
}

std::string DumpDir() {
  if (const char* e = std::getenv("CSSVR_DUMP_DIR")) return e;
  return "/home/voldemar/Dev/GMod/gVRMod/.scratch/cssvrmod";
}

void WritePpm(const std::string& path, int w, int h, const unsigned char* rgba) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  std::vector<unsigned char> row((size_t)w * 3);
  for (int y = 0; y < h; ++y) {
    const unsigned char* src = rgba + (size_t)y * (size_t)w * 4;
    for (int x = 0; x < w; ++x) {
      row[(size_t)x * 3 + 0] = src[(size_t)x * 4 + 0];
      row[(size_t)x * 3 + 1] = src[(size_t)x * 4 + 1];
      row[(size_t)x * 3 + 2] = src[(size_t)x * 4 + 2];
    }
    std::fwrite(row.data(), 1, row.size(), f);
  }
  std::fclose(f);
}

struct DevFns {
  PFN_vkGetDeviceProcAddr gdpa = nullptr;
  PFN_vkGetDeviceQueue getQueue = nullptr;
  PFN_vkCreateSwapchainKHR createSc = nullptr;
  PFN_vkGetSwapchainImagesKHR getScImgs = nullptr;
  PFN_vkQueuePresentKHR present = nullptr;
  PFN_vkCreateCommandPool createPool = nullptr;
  PFN_vkDestroyCommandPool destroyPool = nullptr;
  PFN_vkAllocateCommandBuffers allocCmd = nullptr;
  PFN_vkBeginCommandBuffer beginCmd = nullptr;
  PFN_vkEndCommandBuffer endCmd = nullptr;
  PFN_vkCmdPipelineBarrier barrier = nullptr;
  PFN_vkCmdCopyImageToBuffer copy = nullptr;
  PFN_vkQueueSubmit submit = nullptr;
  PFN_vkQueueWaitIdle waitIdle = nullptr;
  PFN_vkCreateBuffer createBuf = nullptr;
  PFN_vkDestroyBuffer destroyBuf = nullptr;
  PFN_vkGetBufferMemoryRequirements bufReq = nullptr;
  PFN_vkAllocateMemory allocMem = nullptr;
  PFN_vkFreeMemory freeMem = nullptr;
  PFN_vkBindBufferMemory bindBuf = nullptr;
  PFN_vkMapMemory map = nullptr;
  PFN_vkUnmapMemory unmap = nullptr;
  PFN_vkCreateFence createFence = nullptr;
  PFN_vkDestroyFence destroyFence = nullptr;
  PFN_vkWaitForFences waitFences = nullptr;
  PFN_vkResetFences resetFences = nullptr;
  PFN_vkGetPhysicalDeviceMemoryProperties memProps = nullptr;
};

struct SwapState {
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  uint32_t w = 0, h = 0;
  VkFormat format = VK_FORMAT_UNDEFINED;
  std::vector<VkImage> images;
};

struct DeviceState {
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  uint32_t gfx_family = 0;
  VkQueue queue = VK_NULL_HANDLE;
  DevFns fn;
};

std::mutex g_mu;
std::unordered_map<VkDevice, DeviceState> g_devs;
std::unordered_map<VkSwapchainKHR, SwapState> g_scs;
int g_presents = 0;
int g_dumps = 0;

PFN_vkGetInstanceProcAddr g_gipa = nullptr;
PFN_vkGetDeviceProcAddr g_gdpa = nullptr;
PFN_vkCreateDevice g_createDevice = nullptr;
PFN_vkQueuePresentKHR g_present = nullptr;
PFN_vkCreateSwapchainKHR g_createSc = nullptr;

void* VulkanSym(const char* name) {
  static void* lib = nullptr;
  if (!lib) lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!lib) lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  void* p = lib ? dlsym(lib, name) : nullptr;
  if (!p) p = dlsym(RTLD_NEXT, name);
  return p;
}

uint32_t FindHostMem(VkPhysicalDevice phys, PFN_vkGetPhysicalDeviceMemoryProperties getProps,
                     uint32_t typeBits) {
  VkPhysicalDeviceMemoryProperties p{};
  getProps(phys, &p);
  for (uint32_t i = 0; i < p.memoryTypeCount; ++i) {
    if ((typeBits & (1u << i)) &&
        (p.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        (p.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
      return i;
  }
  return UINT32_MAX;
}

void FillDevFns(VkDevice dev, DeviceState& ds) {
  auto gdpa = ds.fn.gdpa ? ds.fn.gdpa : g_gdpa;
  if (!gdpa) return;
#define L(name, field) ds.fn.field = (PFN_##name)gdpa(dev, #name)
  L(vkGetDeviceQueue, getQueue);
  L(vkCreateSwapchainKHR, createSc);
  L(vkGetSwapchainImagesKHR, getScImgs);
  L(vkQueuePresentKHR, present);
  L(vkCreateCommandPool, createPool);
  L(vkDestroyCommandPool, destroyPool);
  L(vkAllocateCommandBuffers, allocCmd);
  L(vkBeginCommandBuffer, beginCmd);
  L(vkEndCommandBuffer, endCmd);
  L(vkCmdPipelineBarrier, barrier);
  L(vkCmdCopyImageToBuffer, copy);
  L(vkQueueSubmit, submit);
  L(vkQueueWaitIdle, waitIdle);
  L(vkCreateBuffer, createBuf);
  L(vkDestroyBuffer, destroyBuf);
  L(vkGetBufferMemoryRequirements, bufReq);
  L(vkAllocateMemory, allocMem);
  L(vkFreeMemory, freeMem);
  L(vkBindBufferMemory, bindBuf);
  L(vkMapMemory, map);
  L(vkUnmapMemory, unmap);
  L(vkCreateFence, createFence);
  L(vkDestroyFence, destroyFence);
  L(vkWaitForFences, waitFences);
  L(vkResetFences, resetFences);
#undef L
  if (!ds.fn.memProps && g_gipa) {
    // instance-level; try device first then instance
    ds.fn.memProps =
        (PFN_vkGetPhysicalDeviceMemoryProperties)g_gipa(VK_NULL_HANDLE,
                                                        "vkGetPhysicalDeviceMemoryProperties");
  }
}

void DumpSwapchain(VkQueue queue, VkSwapchainKHR sc, uint32_t idx) {
  DeviceState* ds = nullptr;
  SwapState* ss = nullptr;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    auto sit = g_scs.find(sc);
    if (sit == g_scs.end()) return;
    ss = &sit->second;
    auto dit = g_devs.find(ss->device);
    if (dit == g_devs.end()) return;
    ds = &dit->second;
  }
  if (!ds->fn.copy || !ds->fn.createBuf || idx >= ss->images.size() || ss->w == 0 || ss->h == 0)
    return;
  if (!ds->fn.memProps) return;

  VkDevice dev = ss->device;
  const VkDeviceSize bytes = (VkDeviceSize)ss->w * ss->h * 4;
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = bytes;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkBuffer buf = VK_NULL_HANDLE;
  if (ds->fn.createBuf(dev, &bi, nullptr, &buf) != VK_SUCCESS) return;
  VkMemoryRequirements req{};
  ds->fn.bufReq(dev, buf, &req);
  uint32_t memIdx = FindHostMem(ss->phys, ds->fn.memProps, req.memoryTypeBits);
  if (memIdx == UINT32_MAX) {
    ds->fn.destroyBuf(dev, buf, nullptr);
    return;
  }
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = memIdx;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  if (ds->fn.allocMem(dev, &ai, nullptr, &mem) != VK_SUCCESS) {
    ds->fn.destroyBuf(dev, buf, nullptr);
    return;
  }
  ds->fn.bindBuf(dev, buf, mem, 0);

  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = ds->gfx_family;
  pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  VkCommandPool pool = VK_NULL_HANDLE;
  if (ds->fn.createPool(dev, &pci, nullptr, &pool) != VK_SUCCESS) {
    ds->fn.freeMem(dev, mem, nullptr);
    ds->fn.destroyBuf(dev, buf, nullptr);
    return;
  }
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cai.commandPool = pool;
  cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cai.commandBufferCount = 1;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  ds->fn.allocCmd(dev, &cai, &cmd);
  VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  ds->fn.beginCmd(cmd, &cbi);

  VkImageMemoryBarrier bar{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  bar.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  bar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bar.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  bar.image = ss->images[idx];
  bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  ds->fn.barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                 0, nullptr, 1, &bar);

  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {ss->w, ss->h, 1};
  ds->fn.copy(cmd, ss->images[idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &copy);

  bar.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bar.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  bar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  ds->fn.barrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                 nullptr, 0, nullptr, 1, &bar);
  ds->fn.endCmd(cmd);

  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  if (ds->fn.submit(queue, 1, &si, VK_NULL_HANDLE) == VK_SUCCESS)
    ds->fn.waitIdle(queue);

  void* mapped = nullptr;
  if (ds->fn.map(dev, mem, 0, bytes, 0, &mapped) == VK_SUCCESS && mapped) {
    std::vector<unsigned char> rgba((size_t)bytes);
    // Convert common present formats to RGBA8.
    const auto* src = static_cast<const unsigned char*>(mapped);
    if (ss->format == VK_FORMAT_B8G8R8A8_UNORM || ss->format == VK_FORMAT_B8G8R8A8_SRGB) {
      for (size_t i = 0; i < (size_t)ss->w * ss->h; ++i) {
        rgba[i * 4 + 0] = src[i * 4 + 2];
        rgba[i * 4 + 1] = src[i * 4 + 1];
        rgba[i * 4 + 2] = src[i * 4 + 0];
        rgba[i * 4 + 3] = src[i * 4 + 3];
      }
    } else {
      std::memcpy(rgba.data(), src, (size_t)bytes);
    }
    ds->fn.unmap(dev, mem);
    int nz = 0, uniq = 0;
    const int n = (int)ss->w * (int)ss->h;
    const int step = n > 4000 ? n / 4000 : 1;
    unsigned seen[32] = {};
    for (int i = 0; i < n; i += step) {
      if (rgba[(size_t)i * 4] | rgba[(size_t)i * 4 + 1] | rgba[(size_t)i * 4 + 2]) nz++;
      unsigned k = ((unsigned)rgba[(size_t)i * 4] << 16) | ((unsigned)rgba[(size_t)i * 4 + 1] << 8) |
                   rgba[(size_t)i * 4 + 2];
      seen[k % 32] = k ? k : 1;
    }
    for (unsigned s : seen)
      if (s) uniq++;
    const bool scene = uniq >= 8 && nz > 30;
    if (scene || g_dumps < 3) {
      mkdir(DumpDir().c_str(), 0755);
      char path[512];
      std::snprintf(path, sizeof(path), "%s/vk_%03d_%ux%u_%s.ppm", DumpDir().c_str(), g_dumps,
                    ss->w, ss->h, scene ? "scene" : "raw");
      WritePpm(path, (int)ss->w, (int)ss->h, rgba.data());
      g_dumps++;
      Log("dump %s nz=%d uniq=%d fmt=%d", path, nz, uniq, (int)ss->format);
    }
  }

  ds->fn.destroyPool(dev, pool, nullptr);
  ds->fn.freeMem(dev, mem, nullptr);
  ds->fn.destroyBuf(dev, buf, nullptr);
}

VKAPI_ATTR VkResult VKAPI_CALL WrapPresent(VkQueue queue, const VkPresentInfoKHR* info) {
  PFN_vkQueuePresentKHR real = g_present;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    g_presents++;
    if (!real && info && info->swapchainCount) {
      auto it = g_scs.find(info->pSwapchains[0]);
      if (it != g_scs.end()) {
        auto dit = g_devs.find(it->second.device);
        if (dit != g_devs.end()) real = dit->second.fn.present;
      }
    }
  }
  // Capture the image about to be shown (still in presentable layout).
  if (info && g_dumps < 8) {
    const int p = g_presents;
    if (p == 8 || p == 40 || p == 120 || (p % 180) == 0) {
      for (uint32_t i = 0; i < info->swapchainCount; ++i)
        DumpSwapchain(queue, info->pSwapchains[i], info->pImageIndices[i]);
    }
  }
  if (!real) return VK_ERROR_UNKNOWN;
  return real(queue, info);
}

VKAPI_ATTR VkResult VKAPI_CALL WrapCreateSwapchain(VkDevice device,
                                                   const VkSwapchainCreateInfoKHR* ci,
                                                   const VkAllocationCallbacks* a,
                                                   VkSwapchainKHR* out) {
  PFN_vkCreateSwapchainKHR real = g_createSc;
  DeviceState* ds = nullptr;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_devs.find(device);
    if (it != g_devs.end()) {
      ds = &it->second;
      if (ds->fn.createSc) real = ds->fn.createSc;
    }
  }
  if (!real) return VK_ERROR_UNKNOWN;
  VkResult rc = real(device, ci, a, out);
  if (rc != VK_SUCCESS || !out || !ci) return rc;
  SwapState ss;
  ss.device = device;
  ss.w = ci->imageExtent.width;
  ss.h = ci->imageExtent.height;
  ss.format = ci->imageFormat;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_devs.find(device);
    if (it != g_devs.end()) {
      ss.phys = it->second.phys;
      if (it->second.fn.getScImgs) {
        uint32_t n = 0;
        it->second.fn.getScImgs(device, *out, &n, nullptr);
        ss.images.resize(n);
        if (n) it->second.fn.getScImgs(device, *out, &n, ss.images.data());
      }
    }
    g_scs[*out] = std::move(ss);
  }
  Log("swapchain %ux%u fmt=%d imgs=%zu", ci->imageExtent.width, ci->imageExtent.height,
      (int)ci->imageFormat, g_scs[*out].images.size());
  (void)ds;
  return rc;
}

VKAPI_ATTR VkResult VKAPI_CALL WrapCreateDevice(VkPhysicalDevice phys,
                                                const VkDeviceCreateInfo* ci,
                                                const VkAllocationCallbacks* a, VkDevice* out) {
  if (!g_createDevice) g_createDevice = (PFN_vkCreateDevice)VulkanSym("vkCreateDevice");
  if (!g_createDevice) return VK_ERROR_UNKNOWN;
  VkResult rc = g_createDevice(phys, ci, a, out);
  if (rc != VK_SUCCESS || !out) return rc;
  DeviceState ds;
  ds.phys = phys;
  ds.fn.gdpa = g_gdpa ? g_gdpa : (PFN_vkGetDeviceProcAddr)VulkanSym("vkGetDeviceProcAddr");
  if (ci && ci->queueCreateInfoCount)
    ds.gfx_family = ci->pQueueCreateInfos[0].queueFamilyIndex;
  FillDevFns(*out, ds);
  if (ds.fn.getQueue) ds.fn.getQueue(*out, ds.gfx_family, 0, &ds.queue);
  if (!ds.fn.memProps) {
    auto instFn = (PFN_vkGetPhysicalDeviceMemoryProperties)VulkanSym(
        "vkGetPhysicalDeviceMemoryProperties");
    ds.fn.memProps = instFn;
  }
  {
    std::lock_guard<std::mutex> lk(g_mu);
    g_devs[*out] = ds;
    if (ds.fn.present) g_present = ds.fn.present;
    if (ds.fn.createSc) g_createSc = ds.fn.createSc;
  }
  Log("device ok family=%u present=%p", ds.gfx_family, (void*)ds.fn.present);
  return rc;
}

PFN_vkVoidFunction WrapName(const char* name, PFN_vkVoidFunction real) {
  if (!name) return real;
  if (std::strcmp(name, "vkQueuePresentKHR") == 0) {
    if (real) g_present = (PFN_vkQueuePresentKHR)real;
    return (PFN_vkVoidFunction)WrapPresent;
  }
  if (std::strcmp(name, "vkCreateSwapchainKHR") == 0) {
    if (real) g_createSc = (PFN_vkCreateSwapchainKHR)real;
    return (PFN_vkVoidFunction)WrapCreateSwapchain;
  }
  if (std::strcmp(name, "vkCreateDevice") == 0) {
    if (real) g_createDevice = (PFN_vkCreateDevice)real;
    return (PFN_vkVoidFunction)WrapCreateDevice;
  }
  return real;
}

} // namespace

extern "C" {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance inst, const char* name) {
  if (!g_gipa) g_gipa = (PFN_vkGetInstanceProcAddr)VulkanSym("vkGetInstanceProcAddr");
  PFN_vkVoidFunction real = g_gipa ? g_gipa(inst, name) : nullptr;
  return WrapName(name, real);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice dev, const char* name) {
  if (!g_gdpa) g_gdpa = (PFN_vkGetDeviceProcAddr)VulkanSym("vkGetDeviceProcAddr");
  PFN_vkVoidFunction real = g_gdpa ? g_gdpa(dev, name) : nullptr;
  return WrapName(name, real);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice phys, const VkDeviceCreateInfo* ci,
                                              const VkAllocationCallbacks* a, VkDevice* out) {
  return WrapCreateDevice(phys, ci, a, out);
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* info) {
  return WrapPresent(queue, info);
}

} // extern "C"

__attribute__((constructor)) static void vk_ctor() {
  g_gipa = (PFN_vkGetInstanceProcAddr)VulkanSym("vkGetInstanceProcAddr");
  g_gdpa = (PFN_vkGetDeviceProcAddr)VulkanSym("vkGetDeviceProcAddr");
  g_createDevice = (PFN_vkCreateDevice)VulkanSym("vkCreateDevice");
  Log("vulkan intercept ready gipa=%p", (void*)g_gipa);
}
