#include "persistence/SpeedrunRecordStore.hpp"

#include <charconv>
#include <fstream>
#include <limits>
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

bool validRecord(const gameplay::SpeedrunRecord& record) noexcept
{
    return record.rulesVersion == gameplay::currentSpeedrunRulesVersion
        && record.bestTicks > 0U;
}

std::string serialize(const gameplay::SpeedrunRecord& record)
{
    std::ostringstream output;
    output << "version = " << currentSpeedrunRecordFileVersion << '\n'
           << "rules_version = " << record.rulesVersion << '\n'
           << "best_ticks = " << record.bestTicks << '\n'
           << "seed = " << record.seed << '\n';
    return output.str();
}

} // namespace

SpeedrunRecordLoadResult loadSpeedrunRecord(
    const std::filesystem::path& path) noexcept
{
    SpeedrunRecordLoadResult result;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            result.status = !error && !exists
                ? SpeedrunRecordLoadStatus::Missing
                : SpeedrunRecordLoadStatus::IoError;
            return result;
        }

        std::optional<std::uint64_t> version;
        std::optional<std::uint64_t> rulesVersion;
        std::optional<std::uint64_t> bestTicks;
        std::optional<std::uint64_t> seed;
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
                result.status = SpeedrunRecordLoadStatus::Invalid;
                return result;
            }
            const std::string_view key = trim(content.substr(0U, separator));
            const std::string_view value = trim(content.substr(separator + 1U));
            if (key == "version") {
                version = parseUnsigned(value);
            } else if (key == "rules_version") {
                rulesVersion = parseUnsigned(value);
            } else if (key == "best_ticks") {
                bestTicks = parseUnsigned(value);
            } else if (key == "seed") {
                seed = parseUnsigned(value);
            }
        }
        if (input.bad()) {
            result.status = SpeedrunRecordLoadStatus::IoError;
            return result;
        }
        if (!version || !rulesVersion || !bestTicks || !seed
            || *bestTicks == 0U
            || *rulesVersion > static_cast<std::uint64_t>(
                std::numeric_limits<std::uint32_t>::max())) {
            result.status = SpeedrunRecordLoadStatus::Invalid;
            return result;
        }
        if (*version != currentSpeedrunRecordFileVersion) {
            result.status = SpeedrunRecordLoadStatus::UnsupportedVersion;
            return result;
        }
        if (*rulesVersion != gameplay::currentSpeedrunRulesVersion) {
            result.status = SpeedrunRecordLoadStatus::IncompatibleRules;
            return result;
        }
        result.record = gameplay::SpeedrunRecord{
            static_cast<std::uint32_t>(*rulesVersion), *bestTicks, *seed};
        result.status = SpeedrunRecordLoadStatus::Loaded;
        return result;
    } catch (...) {
        result.status = SpeedrunRecordLoadStatus::IoError;
        return result;
    }
}

bool saveSpeedrunRecord(
    const std::filesystem::path& path,
    const gameplay::SpeedrunRecord& record) noexcept
{
    if (!validRecord(record)) {
        return false;
    }
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

        temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return false;
            }
            output << serialize(record);
            output.flush();
            if (!output.good()) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

        const SpeedrunRecordLoadResult validation
            = loadSpeedrunRecord(temporary);
        if (validation.status != SpeedrunRecordLoadStatus::Loaded
            || validation.record != record) {
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
