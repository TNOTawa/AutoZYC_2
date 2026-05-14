// AutoZYC — Development-only logging system
// Define AUTOZYC_DEV_LOG=1 to enable logging (release = disabled by default)
#pragma once

#include <windows.h>
#include <cstdio>
#include <ctime>
#include <cstdarg>

// ---------------------------------------------------------------------------
// Log level
// ---------------------------------------------------------------------------
enum AutoZycLogLevel {
    AUTOZYC_LOG_DEBUG = 0,
    AUTOZYC_LOG_INFO  = 1,
    AUTOZYC_LOG_WARN  = 2,
    AUTOZYC_LOG_ERROR = 3,
};

// ---------------------------------------------------------------------------
// Internal: write a formatted log entry. Expand to nothing in release.
// ---------------------------------------------------------------------------
inline void AutoZycLogWrite(const char* file, int line, AutoZycLogLevel level, const char* fmt, ...)
{
#if AUTOZYC_DEV_LOG
    static bool initialized = false;
    static FILE* logFile = nullptr;
    static CRITICAL_SECTION cs;
    static bool csInitialized = false;

    if (!csInitialized) {
        InitializeCriticalSection(&cs);
        csInitialized = true;
    }

    EnterCriticalSection(&cs);

    if (!initialized) {
        wchar_t logPath[MAX_PATH];
        GetTempPathW(MAX_PATH, logPath);
        wcscat(logPath, L"AutoZYC_dev.log");
        logFile = _wfopen(logPath, L"a");
        if (logFile) setvbuf(logFile, nullptr, _IONBF, 0);
        initialized = true;
    }

    if (logFile) {
        SYSTEMTIME st;
        GetLocalTime(&st);

        const char* levelStr = "DEBUG";
        if (level == AUTOZYC_LOG_INFO)  levelStr = "INFO";
        if (level == AUTOZYC_LOG_WARN)  levelStr = "WARN";
        if (level == AUTOZYC_LOG_ERROR) levelStr = "ERROR";

        fprintf(logFile, "[%02d:%02d:%02d.%03d][%s] %s:%d - ",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                levelStr, file, line);

        va_list args;
        va_start(args, fmt);
        vfprintf(logFile, fmt, args);
        va_end(args);

        fprintf(logFile, "\n");
        fflush(logFile);
    }

    LeaveCriticalSection(&cs);
#else
    (void)file; (void)line; (void)level; (void)fmt;
#endif
}

#define AUTOZYC_LOG_DEBUG(...) AutoZycLogWrite(__FILE__, __LINE__, AUTOZYC_LOG_DEBUG, __VA_ARGS__)
#define AUTOZYC_LOG_INFO(...)  AutoZycLogWrite(__FILE__, __LINE__, AUTOZYC_LOG_INFO,  __VA_ARGS__)
#define AUTOZYC_LOG_WARN(...)  AutoZycLogWrite(__FILE__, __LINE__, AUTOZYC_LOG_WARN,  __VA_ARGS__)
#define AUTOZYC_LOG_ERROR(...) AutoZycLogWrite(__FILE__, __LINE__, AUTOZYC_LOG_ERROR, __VA_ARGS__)
