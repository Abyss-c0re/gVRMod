#include "xr_util.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

bool XrExtensionAvailable(const char* name) {
  uint32_t n = 0;
  xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr);
  std::vector<XrExtensionProperties> props(n, {XR_TYPE_EXTENSION_PROPERTIES});
  if (n) xrEnumerateInstanceExtensionProperties(nullptr, n, &n, props.data());
  for (auto& p : props)
    if (std::strcmp(p.extensionName, name) == 0) return true;
  return false;
}

XrEnvironmentBlendMode XrPickBlendMode(XrInstance instance, XrSystemId systemId, bool wantPassthrough) {
  uint32_t n = 0;
  xrEnumerateEnvironmentBlendModes(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                   0, &n, nullptr);
  std::vector<XrEnvironmentBlendMode> modes(n);
  if (n)
    xrEnumerateEnvironmentBlendModes(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                     n, &n, modes.data());
  auto has = [&](XrEnvironmentBlendMode m) {
    for (auto x : modes) if (x == m) return true;
    return false;
  };
  if (wantPassthrough && has(XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND)) {
    fprintf(stderr, "[cube_webui] blend=ALPHA_BLEND (passthrough)\n");
    return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
  }
  if (wantPassthrough && has(XR_ENVIRONMENT_BLEND_MODE_ADDITIVE)) {
    fprintf(stderr, "[cube_webui] blend=ADDITIVE\n");
    return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
  }
  fprintf(stderr, "[cube_webui] blend=OPAQUE%s\n",
          wantPassthrough ? " (no alpha/additive)" : "");
  return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
}
