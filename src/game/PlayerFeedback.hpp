#pragma once

#include <cstddef>
#include <string>
#include <string_view>

struct SDL_Process;

namespace gdp::game {

inline constexpr std::size_t maximumFeedbackCharacters = 500U;
inline constexpr std::string_view playerFeedbackEndpoint =
    "https://formspree.io/f/xoejwlaw";

enum class PlayerFeedbackTopic {
    Bug,
    Gameplay,
    Update,
};

enum class PlayerFeedbackResult {
    Pending,
    Sent,
    Failed,
};

[[nodiscard]] std::string_view playerFeedbackTopicName(PlayerFeedbackTopic topic) noexcept;
[[nodiscard]] bool validPlayerFeedback(std::string_view message) noexcept;
[[nodiscard]] std::string playerFeedbackJson(PlayerFeedbackTopic topic,
    std::string_view message, std::string_view version,
    std::string_view platform);

class PlayerFeedbackTransport {
public:
    PlayerFeedbackTransport() = default;
    ~PlayerFeedbackTransport();
    PlayerFeedbackTransport(const PlayerFeedbackTransport&) = delete;
    PlayerFeedbackTransport& operator=(const PlayerFeedbackTransport&) = delete;

    [[nodiscard]] bool start(std::string_view json);
    [[nodiscard]] PlayerFeedbackResult poll();
    [[nodiscard]] bool sending() const noexcept { return process_ != nullptr; }

private:
    SDL_Process* process_{nullptr};
};

} // namespace gdp::game
