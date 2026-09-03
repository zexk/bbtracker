#pragma once

#include <cstddef>
#include <cstdint>

namespace bb {

enum class LogLevel : int {
    Debug = 0,
    Info,
    Warn,
    Error,
};

bool log_init(const char* path);
void log_shutdown();
void log_hex_dump(const uint8_t* data, size_t len);
void log_write(LogLevel lvl, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

} // namespace bb

#define LOG_DEBUG(...) ::bb::log_write(::bb::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::bb::log_write(::bb::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) ::bb::log_write(::bb::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) ::bb::log_write(::bb::LogLevel::Error, __VA_ARGS__)
