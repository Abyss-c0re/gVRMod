#pragma once

// Logging is always available. In non-dev builds it only writes if initialized.
// The VRMOD_LOG_* macros are always active so the PoC is fully debuggable.

void vrmod_log_init(const char* filepath);
void vrmod_log_close();
void vrmod_log_write(const char* level, const char* fmt, ...);

// Lua-side print callback (set by lua_interface to forward messages to client console)
typedef void (*VrmodPrintFunc)(const char* msg);
void vrmod_log_set_print(VrmodPrintFunc fn);

#define VRMOD_LOG_INIT(path)    vrmod_log_init(path)
#define VRMOD_LOG_CLOSE()       vrmod_log_close()
#define VRMOD_LOG_INFO(...)     vrmod_log_write("INFO",  __VA_ARGS__)
#define VRMOD_LOG_WARN(...)     vrmod_log_write("WARN",  __VA_ARGS__)
#define VRMOD_LOG_ERROR(...)    vrmod_log_write("ERROR", __VA_ARGS__)
#define VRMOD_LOG_DEBUG(...)    vrmod_log_write("DEBUG", __VA_ARGS__)
