#include "vrmod_log.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <cstring>

static FILE* g_logFile = nullptr;
static std::mutex g_logMutex;
static VrmodPrintFunc g_printFunc = nullptr;

// Dedup tracker: avoid spamming identical messages
static char  g_lastMsg[512] = {0};
static char  g_lastLevel[16] = {0};
static int   g_repeatCount = 0;

void vrmod_log_set_print(VrmodPrintFunc fn) {
    g_printFunc = fn;
}

void vrmod_log_init(const char* filepath) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) fclose(g_logFile);
    g_logFile = fopen(filepath, "a");
    if (g_logFile) {
        fprintf(g_logFile, "--- VRMOD log session started ---\n");
        fflush(g_logFile);
    }
    g_lastMsg[0] = '\0';
    g_lastLevel[0] = '\0';
    g_repeatCount = 0;
}

void vrmod_log_close() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        if (g_repeatCount > 0) {
            fprintf(g_logFile, "  (previous message repeated %d times)\n", g_repeatCount);
        }
        fprintf(g_logFile, "--- VRMOD log session ended ---\n");
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    g_repeatCount = 0;
}

void vrmod_log_write(const char* level, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    std::lock_guard<std::mutex> lock(g_logMutex);

    // Dedup: skip if same message and level as last time
    if (strcmp(buf, g_lastMsg) == 0 && strcmp(level, g_lastLevel) == 0) {
        g_repeatCount++;
        return;
    }

    // Flush any repeated message count
    if (g_repeatCount > 0 && g_logFile) {
        fprintf(g_logFile, "  (previous message repeated %d times)\n", g_repeatCount);
        fflush(g_logFile);
    }
    g_repeatCount = 0;
    strncpy(g_lastMsg, buf, sizeof(g_lastMsg) - 1);
    g_lastMsg[sizeof(g_lastMsg) - 1] = '\0';
    strncpy(g_lastLevel, level, sizeof(g_lastLevel) - 1);
    g_lastLevel[sizeof(g_lastLevel) - 1] = '\0';

    // Write to file
    if (g_logFile) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        fprintf(g_logFile, "[%ld.%03ld] [%s] %s\n", (long)ts.tv_sec, ts.tv_nsec / 1000000, level, buf);
        fflush(g_logFile);
    }

    // Forward to client-side print (if set)
    if (g_printFunc) {
        char printBuf[600];
        snprintf(printBuf, sizeof(printBuf), "[VRMOD][%s] %s", level, buf);
        g_printFunc(printBuf);
    }
}
