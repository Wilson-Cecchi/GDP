#include "persistence/SettingsStore.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace gdp::persistence {
namespace {

std::string_view trim(std::string_view value) noexcept
{
    constexpr std::string_view whitespace{" \t\r\n"};
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, (last - first) + 1U);
}

std::optional<std::uint64_t> parseUnsigned(std::string_view value) noexcept
{
    value = trim(value);
    std::uint64_t parsedValue = 0U;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, parsedValue);
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != end) {
        return std::nullopt;
    }
    return parsedValue;
}

std::optional<bool> parseBoolean(std::string_view value) noexcept
{
    value = trim(value);
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    return std::nullopt;
}

bool assignClamped(
    std::uint16_t& target,
    const std::string_view value,
    const std::uint16_t minimum,
    const std::uint16_t maximum) noexcept
{
    const std::optional<std::uint64_t> parsed = parseUnsigned(value);
    if (!parsed) {
        return false;
    }
    const std::uint64_t clamped = std::clamp<std::uint64_t>(
        *parsed, static_cast<std::uint64_t>(minimum),
        static_cast<std::uint64_t>(maximum));
    target = static_cast<std::uint16_t>(clamped);
    return clamped == *parsed;
}

std::string serialize(const gameplay::RuntimeSettings& source)
{
    const gameplay::RuntimeSettings settings
        = gameplay::sanitizeRuntimeSettings(source);
    std::ostringstream output;
    output << "version = " << currentSettingsVersion << '\n'
           << "master_volume = " << settings.masterVolumePercent << '\n'
           << "effects_volume = " << settings.effectsVolumePercent << '\n'
           << "ambient_volume = " << settings.ambientVolumePercent << '\n'
           << "ui_volume = " << settings.uiVolumePercent << '\n'
           << "mouse_sensitivity = " << settings.mouseSensitivityPercent << '\n'
           << "invert_mouse_y = "
           << (settings.invertMouseY ? "true" : "false") << '\n'
           << "field_of_view = " << settings.fieldOfViewDegrees << '\n'
           << "window_width = "
           << gameplay::windowResolutionSize(settings.windowResolution).width << '\n'
           << "window_height = "
           << gameplay::windowResolutionSize(settings.windowResolution).height << '\n'
           << "fullscreen = "
           << (settings.fullscreen ? "true" : "false") << '\n';
    return output.str();
}

} // namespace

SettingsLoadResult loadRuntimeSettings(
    const std::filesystem::path& path) noexcept
{
    SettingsLoadResult result;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            result.status = !error && !exists
                ? SettingsLoadStatus::Missing
                : SettingsLoadStatus::IoError;
            return result;
        }

        bool recovered = false;
        bool foundVersion = false;
        std::optional<std::uint64_t> version;
        std::optional<std::uint64_t> windowWidth;
        std::optional<std::uint64_t> windowHeight;
        std::string line;
        while (std::getline(input, line)) {
            std::string_view content = trim(line);
            const std::size_t comment = content.find('#');
            if (comment != std::string_view::npos) {
                content = trim(content.substr(0U, comment));
            }
            if (content.empty()) {
                continue;
            }
            const std::size_t separator = content.find('=');
            if (separator == std::string_view::npos) {
                recovered = true;
                continue;
            }
            const std::string_view key = trim(content.substr(0U, separator));
            const std::string_view value = trim(content.substr(separator + 1U));
            if (key == "version") {
                foundVersion = true;
                version = parseUnsigned(value);
                continue;
            }
            if (key == "master_volume") {
                recovered = !assignClamped(
                    result.settings.masterVolumePercent, value, 0U, 100U)
                    || recovered;
            } else if (key == "effects_volume") {
                recovered = !assignClamped(
                    result.settings.effectsVolumePercent, value, 0U, 100U)
                    || recovered;
            } else if (key == "ambient_volume") {
                recovered = !assignClamped(
                    result.settings.ambientVolumePercent, value, 0U, 100U)
                    || recovered;
            } else if (key == "ui_volume") {
                recovered = !assignClamped(
                    result.settings.uiVolumePercent, value, 0U, 100U)
                    || recovered;
            } else if (key == "mouse_sensitivity") {
                recovered = !assignClamped(
                    result.settings.mouseSensitivityPercent, value, 30U, 200U)
                    || recovered;
            } else if (key == "invert_mouse_y") {
                const std::optional<bool> inverted = parseBoolean(value);
                if (inverted) {
                    result.settings.invertMouseY = *inverted;
                } else {
                    recovered = true;
                }
            } else if (key == "field_of_view") {
                recovered = !assignClamped(
                    result.settings.fieldOfViewDegrees, value, 60U, 100U)
                    || recovered;
            } else if (key == "window_width") {
                windowWidth = parseUnsigned(value);
                recovered = !windowWidth || recovered;
            } else if (key == "window_height") {
                windowHeight = parseUnsigned(value);
                recovered = !windowHeight || recovered;
            } else if (key == "fullscreen") {
                const std::optional<bool> fullscreen = parseBoolean(value);
                if (fullscreen) {
                    result.settings.fullscreen = *fullscreen;
                } else {
                    recovered = true;
                }
            }
        }
        if (input.bad()) {
            result.settings = {};
            result.status = SettingsLoadStatus::IoError;
            return result;
        }
        if (!foundVersion || !version) {
            result.settings = {};
            result.status = SettingsLoadStatus::Invalid;
            return result;
        }
        constexpr std::uint64_t oldestSupportedVersion = 1U;
        if (*version < oldestSupportedVersion
            || *version > currentSettingsVersion) {
            result.settings = {};
            result.status = SettingsLoadStatus::UnsupportedVersion;
            return result;
        }
        if (*version == 1U) {
            result.settings.invertMouseY = false;
        }
        if (*version <= 2U) {
            result.settings.fullscreen = false;
        }
        if (*version <= 3U) {
            result.settings.windowResolution
                = gameplay::WindowResolution::R1280x720;
        } else if (windowWidth && windowHeight
            && *windowWidth <= 65'535U && *windowHeight <= 65'535U) {
            const std::optional<gameplay::WindowResolution> resolution
                = gameplay::windowResolutionForSize(
                    static_cast<std::uint16_t>(*windowWidth),
                    static_cast<std::uint16_t>(*windowHeight));
            if (resolution) {
                result.settings.windowResolution = *resolution;
            } else {
                recovered = true;
            }
        } else if (windowWidth || windowHeight) {
            recovered = true;
        }
        result.settings = gameplay::sanitizeRuntimeSettings(result.settings);
        if (*version < currentSettingsVersion) {
            result.status = SettingsLoadStatus::Migrated;
        } else {
            result.status = recovered
                ? SettingsLoadStatus::LoadedWithRecovery
                : SettingsLoadStatus::Loaded;
        }
        return result;
    } catch (...) {
        result.settings = {};
        result.status = SettingsLoadStatus::IoError;
        return result;
    }
}

bool saveRuntimeSettings(
    const std::filesystem::path& path,
    const gameplay::RuntimeSettings& settings) noexcept
{
    std::filesystem::path temporary;
    try {
        std::error_code error;
        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
            if (error) {
                return false;
            }
        }

        std::filesystem::path temporaryCandidate = path;
        temporaryCandidate += ".tmp";
        temporary = std::move(temporaryCandidate);
        {
            std::ofstream output(
                temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return false;
            }
            output << serialize(settings);
            output.flush();
            if (!output.good()) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

        const SettingsLoadResult validation = loadRuntimeSettings(temporary);
        const gameplay::RuntimeSettings expected
            = gameplay::sanitizeRuntimeSettings(settings);
        if (validation.status != SettingsLoadStatus::Loaded
            || validation.settings != expected) {
            std::filesystem::remove(temporary, error);
            return false;
        }

        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    } catch (...) {
        std::error_code error;
        if (!temporary.empty()) {
            std::filesystem::remove(temporary, error);
        }
        return false;
    }
}

} // namespace gdp::persistence
