#include "core/Log.hpp"

#include <iostream>

namespace wkc::core {
namespace {

constexpr std::string_view label(const LogLevel level)
{
    switch (level) {
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }

    return "UNKNOWN";
}

} // namespace

void log(const LogLevel level, const std::string_view message)
{
    std::clog << '[' << label(level) << "] " << message << '\n';
}

} // namespace wkc::core
