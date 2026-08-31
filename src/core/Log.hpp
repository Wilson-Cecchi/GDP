#pragma once

#include <string_view>

namespace wkc::core {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

void log(LogLevel level, std::string_view message);

} // namespace wkc::core
