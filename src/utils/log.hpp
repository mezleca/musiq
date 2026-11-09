#pragma once

#include <format>
#include <string_view>
#include <utility>

enum class LogLevel {
    Default,
    Info,
    Warn,
    Error,
};

void write_log(LogLevel level, std::string_view message);

inline void log(LogLevel level, std::string_view message) {
    write_log(level, message);
}

template <typename... Args>
void log(LogLevel level, std::format_string<Args...> format, Args&&... args) {
    write_log(level, std::format(format, std::forward<Args>(args)...));
}

#define LOG(message, ...) ::log(::LogLevel::Default, message __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(message, ...) ::log(::LogLevel::Info, message __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARN(message, ...) ::log(::LogLevel::Warn, message __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERROR(message, ...) ::log(::LogLevel::Error, message __VA_OPT__(, ) __VA_ARGS__)
