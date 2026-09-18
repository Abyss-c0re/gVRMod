// CSS 64-bit defaults to shaderapivk / DXVK. GL swap never runs.
// Intercept vkQueuePresentKHR and dump the presented swapchain image.
#include "xr_host.hpp"
#include "cssvrmod/input.hpp"
#include "cssvrmod/source_if.hpp"
#include "cssvrmod/weapons.hpp"
#include <vulkan/vulkan.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <pthread.h>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

namespace {
using namespace cssvr;

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
  VkBuffer stage = VK_NULL_HANDLE;
  VkDeviceMemory stage_mem = VK_NULL_HANDLE;
  VkDeviceSize stage_bytes = 0;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;
  bool copy_inflight = false;
  uint32_t copy_w = 0, copy_h = 0;
  VkFormat copy_fmt = VK_FORMAT_UNDEFINED;
};

std::mutex g_mu;
std::unordered_map<VkDevice, DeviceState> g_devs;
std::unordered_map<VkSwapchainKHR, SwapState> g_scs;
int g_presents = 0;
int g_dumps = 0;
int g_xr_ok = 0;
int g_xr_fail = 0;
EngineIf g_eng{};
UserCmdOverlay g_prev_cmd{};

bool XrWanted() {
  const char* e = std::getenv("CSSVR_XR");
  if (!e || !e[0]) return true;
  return !(e[0] == '0' && e[1] == 0);
}

struct XrMailbox {
  std::mutex mu;
  std::condition_variable cv;
  std::vector<unsigned char> rgba;
  int w = 0, h = 0;
  bool bgra = false;
  bool have = false;
  bool stop = false;
};
XrMailbox g_mb;
std::atomic<bool> g_xr_thread{false};

void PushXrFrame(const unsigned char* rgba, int w, int h, bool bgra) {
  if (!rgba || w < 2 || h < 2) return;
  std::lock_guard<std::mutex> lk(g_mb.mu);
  // Latest-wins. Never queue — a backlog is what made the game hitch.
  g_mb.rgba.assign(rgba, rgba + (size_t)w * (size_t)h * 4);
  g_mb.w = w;
  g_mb.h = h;
  g_mb.bgra = bgra;
  g_mb.have = true;
  g_mb.cv.notify_one();
}

bool MailboxFull() {
  std::lock_guard<std::mutex> lk(g_mb.mu);
  return g_mb.have;
}

void* XrWorker(void*) {
  Log("xr worker start");
  while (true) {
    std::vector<unsigned char> frame;
    int w = 0, h = 0;
    bool bgra = false;
    {
      std::unique_lock<std::mutex> lk(g_mb.mu);
      g_mb.cv.wait_for(lk, std::chrono::milliseconds(50),
                       [] { return g_mb.have || g_mb.stop; });
      if (g_mb.stop) break;
      if (!g_mb.have) continue;
      frame.swap(g_mb.rgba);
      w = g_mb.w;
      h = g_mb.h;
      bgra = g_mb.bgra;
      g_mb.have = false;
    }
    if (XrHostSubmitPixels(frame.data(), w, h, bgra)) {
      g_xr_ok++;
      if (g_xr_ok == 1 || (g_xr_ok % 300) == 0)
        Log("xr submit #%d %ux%u %s", g_xr_ok, w, h, XrHostStatus().reason);
      XrSample xr{};
      if (XrHostPollInput(&xr)) {
        GunPose gun = GunFromHand(xr.right, WeaponOffset{});
        UserCmdOverlay cmd = InputMap(xr, gun, InputConfig{}, 0.011f);
        if (g_eng.screen_ok || ProbeLiveEngine(g_eng)) {
          auto edge = [&](int bit, const char* plus, const char* minus) {
            const bool now = (cmd.buttons & bit) != 0;
            const bool was = (g_prev_cmd.buttons & bit) != 0;
            if (now && !was) EngineClientCmd(g_eng, plus);
            if (!now && was) EngineClientCmd(g_eng, minus);
          };
          edge(kInAttack, "+attack", "-attack");
          edge(kInJump, "+jump", "-jump");
          edge(kInForward, "+forward", "-forward");
          edge(kInBack, "+back", "-back");
          edge(kInMoveLeft, "+moveleft", "-moveleft");
          edge(kInMoveRight, "+moveright", "-moveright");
          edge(kInReload, "+reload", "-reload");
        }
        g_prev_cmd = cmd;
      }
    } else {
      g_xr_fail++;
      if (g_xr_fail == 1 || (g_xr_fail % 120) == 0)
        Log("xr submit fail #%d %s", g_xr_fail, XrHostStatus().reason);
    }
  }
  Log("xr worker stop");
  return nullptr;
}

void EnsureXrWorker() {
  bool expected = false;
  if (!g_xr_thread.compare_exchange_strong(expected, true)) return;
  pthread_t th;
  if (pthread_create(&th, nullptr, XrWorker, nullptr) != 0) {
    g_xr_thread = false;
    Log("xr worker create failed");
    return;
  }
  pthread_detach(th);
}

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

bool EnsureStage(DeviceState* ds, VkDevice dev, VkDeviceSize bytes) {
  if (ds->stage && ds->stage_bytes >= bytes && ds->cmd && ds->fence) return true;
  if (!ds->fn.createFence) return false;
  if (ds->stage) { ds->fn.destroyBuf(dev, ds->stage, nullptr); ds->stage = VK_NULL_HANDLE; }
  if (ds->stage_mem) { ds->fn.freeMem(dev, ds->stage_mem, nullptr); ds->stage_mem = VK_NULL_HANDLE; }
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bi.size = bytes;
  bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (ds->fn.createBuf(dev, &bi, nullptr, &ds->stage) != VK_SUCCESS) return false;
  VkMemoryRequirements req{};
  ds->fn.bufReq(dev, ds->stage, &req);
  uint32_t memIdx = FindHostMem(ds->phys, ds->fn.memProps, req.memoryTypeBits);
  if (memIdx == UINT32_MAX) return false;
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = memIdx;
  if (ds->fn.allocMem(dev, &ai, nullptr, &ds->stage_mem) != VK_SUCCESS) return false;
  ds->fn.bindBuf(dev, ds->stage, ds->stage_mem, 0);
  ds->stage_bytes = bytes;
  if (!ds->pool) {
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = ds->gfx_family;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (ds->fn.createPool(dev, &pci, nullptr, &ds->pool) != VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = ds->pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    ds->fn.allocCmd(dev, &cai, &ds->cmd);
  }
  if (!ds->fence) {
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    ds->fn.createFence(dev, &fi, nullptr, &ds->fence);
  }
  return ds->stage && ds->cmd && ds->fence;
}

bool FormatIsBgra(VkFormat fmt) {
  return fmt == VK_FORMAT_B8G8R8A8_UNORM || fmt == VK_FORMAT_B8G8R8A8_SRGB;
}

void HarvestCopy(DeviceState* ds, VkDevice dev, bool want_ppm) {
  if (!ds->copy_inflight || !ds->fn.waitFences) return;
  if (ds->fn.waitFences(dev, 1, &ds->fence, VK_TRUE, 0) != VK_SUCCESS) return;
  ds->copy_inflight = false;
  ds->fn.resetFences(dev, 1, &ds->fence);
  const uint32_t w = ds->copy_w, h = ds->copy_h;
  if (w < 2 || h < 2 || !ds->stage_mem) return;
  const VkDeviceSize bytes = (VkDeviceSize)w * h * 4;
  const bool take_xr = XrWanted() && !MailboxFull();
  if (!take_xr && !want_ppm) return;
  void* mapped = nullptr;
  if (ds->fn.map(dev, ds->stage_mem, 0, bytes, 0, &mapped) != VK_SUCCESS || !mapped) return;
  const auto* srcp = static_cast<const unsigned char*>(mapped);
  if (take_xr) {
    EnsureXrWorker();
    PushXrFrame(srcp, (int)w, (int)h, FormatIsBgra(ds->copy_fmt));
  }
  if (want_ppm && g_dumps < 2) {
    std::vector<unsigned char> rgba((size_t)bytes);
    if (FormatIsBgra(ds->copy_fmt)) {
      for (size_t i = 0; i < (size_t)w * h; ++i) {
        rgba[i * 4 + 0] = srcp[i * 4 + 2];
        rgba[i * 4 + 1] = srcp[i * 4 + 1];
        rgba[i * 4 + 2] = srcp[i * 4 + 0];
        rgba[i * 4 + 3] = srcp[i * 4 + 3];
      }
    } else {
      std::memcpy(rgba.data(), srcp, (size_t)bytes);
    }
    mkdir(DumpDir().c_str(), 0755);
    char path[512];
    std::snprintf(path, sizeof(path), "%s/vk_%03d_%ux%u_scene.ppm", DumpDir().c_str(), g_dumps, w, h);
    WritePpm(path, (int)w, (int)h, rgba.data());
    g_dumps++;
    Log("dump %s", path);
  }
  ds->fn.unmap(dev, ds->stage_mem);
}

void DumpSwapchain(VkQueue queue, VkSwapchainKHR sc, uint32_t idx, bool want_ppm) {
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
  if (!ds->fn.copy || idx >= ss->images.size() || ss->w == 0 || ss->h == 0 || !ds->fn.memProps) return;
  VkDevice dev = ss->device;
  HarvestCopy(ds, dev, want_ppm);
  const bool want_xr = XrWanted() && !MailboxFull();
  if (!want_xr && !want_ppm) return;
  if (ds->copy_inflight) return; // previous GPU copy still running — never stall present
  const VkDeviceSize bytes = (VkDeviceSize)ss->w * ss->h * 4;
  if (!EnsureStage(ds, dev, bytes)) return;
  VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  ds->fn.beginCmd(ds->cmd, &cbi);
  VkImageMemoryBarrier bar{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  bar.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  bar.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bar.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  bar.image = ss->images[idx];
  bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  ds->fn.barrier(ds->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                 nullptr, 0, nullptr, 1, &bar);
  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {ss->w, ss->h, 1};
  ds->fn.copy(ds->cmd, ss->images[idx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ds->stage, 1, &copy);
  bar.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  bar.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  bar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  ds->fn.barrier(ds->cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                 nullptr, 0, nullptr, 1, &bar);
  ds->fn.endCmd(ds->cmd);
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &ds->cmd;
  if (ds->fn.submit(queue, 1, &si, ds->fence) != VK_SUCCESS) return;
  ds->copy_inflight = true;
  ds->copy_w = ss->w;
  ds->copy_h = ss->h;
  ds->copy_fmt = ss->format;
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
  if (!real) return VK_ERROR_UNKNOWN;
  const VkResult pr = real(queue, info);
  // After present: never wait. Harvest a finished GPU copy, kick the next if XR is hungry.
  if (info && info->swapchainCount) {
    const bool want_ppm = g_dumps < 2;
    DumpSwapchain(queue, info->pSwapchains[0], info->pImageIndices[0], want_ppm);
  }
  if ((g_presents % 300) == 0)
    Log("present=%d xr_ok=%d xr_fail=%d dumps=%d", g_presents, g_xr_ok, g_xr_fail, g_dumps);
  return pr;
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
