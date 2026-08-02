#pragma once
#include <openxr/openxr.h>

// Instance extension probe + environment blend selection.
bool XrExtensionAvailable(const char* name);
XrEnvironmentBlendMode XrPickBlendMode(XrInstance instance, XrSystemId systemId, bool wantPassthrough);
