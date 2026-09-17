#include "cssvrmod/hook_api.hpp"

__attribute__((constructor)) static void cssvr_ctor() { cssvr::HookOnLoad(); }
__attribute__((destructor)) static void cssvr_dtor() { cssvr::HookOnUnload(); }
