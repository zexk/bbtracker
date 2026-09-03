#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace bb {
namespace {

FILE* g_file = nullptr;

const char* level_tag(LogLevel lvl)
{
    switch (lvl) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

} // namespace

bool log_init(const char* path)
{
    if (g_file) {
        return true;
    }
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") != 0 || !f) {
        return false;
    }
    g_file = f;
    return true;
}

void log_shutdown()
{
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
}

void log_write(LogLevel lvl, const char* fmt, ...)
{
    if (!g_file) {
        return;
    }
    SYSTEMTIME st{};
    GetLocalTime(&st);

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    fprintf(g_file, "[%02u:%02u:%02u.%03u] [%s] %s\n", st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds, level_tag(lvl), msg);
    fflush(g_file);
}

void log_hex_dump(const uint8_t* data, size_t len)
{
    for (size_t row = 0; row < len; row += 16) {
        char line[128];
        size_t pos = 0;
        pos += snprintf(line + pos, sizeof(line) - pos, "  %04zx:", row);
        for (size_t i = 0; i < 16 && row + i < len; ++i) {
            pos += snprintf(line + pos, sizeof(line) - pos, " %02X", data[row + i]);
        }
        LOG_INFO("%s", line);
    }
}

} // namespace bb
