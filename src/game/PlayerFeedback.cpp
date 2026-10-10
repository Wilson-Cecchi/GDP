#include "game/PlayerFeedback.hpp"

#include "game/Localization.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace gdp::game {
namespace {

void appendJsonString(std::string& result, const std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    result.push_back('"');
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (byte < 0x20U) {
                result += "\\u00";
                result.push_back(hex[byte >> 4U]);
                result.push_back(hex[byte & 0x0fU]);
            } else {
                result.push_back(static_cast<char>(byte));
            }
        }
    }
    result.push_back('"');
}

} // namespace

std::string_view playerFeedbackTopicName(const PlayerFeedbackTopic topic) noexcept
{
    switch (topic) {
    case PlayerFeedbackTopic::Bug: return "BUG";
    case PlayerFeedbackTopic::Gameplay: return "GAMEPLAY";
    case PlayerFeedbackTopic::Update: return "UPDATE IDEA";
    }
    return "BUG";
}

bool validPlayerFeedback(const std::string_view message) noexcept
{
    if (message.empty() || message.size() > 2048U
        || uiTextLength(message) > maximumFeedbackCharacters) return false;
    return std::any_of(message.begin(), message.end(), [](const unsigned char byte) {
        return !std::isspace(byte);
    });
}

std::string playerFeedbackJson(const PlayerFeedbackTopic topic,
    const std::string_view message, const std::string_view version,
    const std::string_view platform)
{
    if (!validPlayerFeedback(message)) return {};
    std::string result{"{\"subject\":"};
    appendJsonString(result, "Iron Seed feedback - "
        + std::string{playerFeedbackTopicName(topic)});
    result += ",\"category\":";
    appendJsonString(result, playerFeedbackTopicName(topic));
    result += ",\"message\":";
    appendJsonString(result, message);
    result += ",\"version\":";
    appendJsonString(result, version);
    result += ",\"platform\":";
    appendJsonString(result, platform);
    result += '}';
    return result;
}

PlayerFeedbackTransport::~PlayerFeedbackTransport()
{
    if (process_) {
        SDL_KillProcess(process_, true);
        SDL_DestroyProcess(process_);
    }
}

bool PlayerFeedbackTransport::start(const std::string_view json)
{
    if (process_ || json.empty() || json.size() > 4096U) return false;
    // Do not pass player text in argv, invoke a shell, or read a user's .curlrc.
    // curl is supplied by the operating system and reports an error if absent.
    const char* const args[]{
        "curl", "--disable", "--silent", "--fail", "--max-time", "10",
        "--connect-timeout", "4", "--proto", "=https",
        "--request", "POST", "--header", "Accept: application/json",
        "--header", "Content-Type: application/json",
        "--data-binary", "@-", "--output",
#if defined(_WIN32)
        "NUL",
#else
        "/dev/null",
#endif
        "--write-out", "%{http_code}",
        playerFeedbackEndpoint.data(), nullptr,
    };
    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (!properties) return false;
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
        const_cast<char**>(args));
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
        SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,
        SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,
        SDL_PROCESS_STDIO_NULL);
    SDL_Process* const process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (!process) return false;
    SDL_IOStream* const input = SDL_GetProcessInput(process);
    const bool wrote = input && SDL_WriteIO(input, json.data(), json.size())
        == json.size() && SDL_FlushIO(input);
    if (input) {
        SDL_CloseIO(input);
        SDL_ClearProperty(SDL_GetProcessProperties(process),
            SDL_PROP_PROCESS_STDIN_POINTER);
    }
    if (!wrote) {
        SDL_KillProcess(process, true);
        SDL_DestroyProcess(process);
        return false;
    }
    process_ = process;
    return true;
}

PlayerFeedbackResult PlayerFeedbackTransport::poll()
{
    if (!process_) return PlayerFeedbackResult::Failed;
    int exitCode = -1;
    if (!SDL_WaitProcess(process_, false, &exitCode))
        return PlayerFeedbackResult::Pending;
    std::size_t outputSize = 0U;
    void* const output = SDL_ReadProcess(process_, &outputSize, nullptr);
    const bool sent = exitCode == 0 && output != nullptr && outputSize == 3U
        && std::string_view{static_cast<const char*>(output), outputSize}
            .starts_with('2');
    SDL_free(output);
    SDL_DestroyProcess(process_);
    process_ = nullptr;
    return sent ? PlayerFeedbackResult::Sent : PlayerFeedbackResult::Failed;
}

} // namespace gdp::game
