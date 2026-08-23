#pragma once

namespace bb {

enum class LogLevel : int {
    Debug = 0,
    Info,
    Warn,
    Error,
};

bool log_init(const char* path);
void log_shutdown();
void log_write(LogLevel lvl, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

} // namespace bb

#define LOG_DEBUG(...) ::bb::log_write(::bb::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::bb::log_write(::bb::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) ::bb::log_write(::bb::LogLevel::Warn, __VA_ARGS__)
#define LOG_ERROR(...) ::bb::log_write(::bb::LogLevel::Error, __VA_ARGS__)
