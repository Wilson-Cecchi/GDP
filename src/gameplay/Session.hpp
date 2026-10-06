#pragma once

#include "core/Seed.hpp"
#include "entities/EntityManager.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

inline constexpr std::uint64_t runTimerTicksPerSecond = 60U;
inline constexpr std::uint32_t campaignFloorCount = 10U;
enum class Difficulty : std::uint8_t { Easy, Normal, Hard, Count };
[[nodiscard]] const char* difficultyName(Difficulty difficulty) noexcept;
[[nodiscard]] float baseDifficultyMultiplier(Difficulty difficulty) noexcept;

enum class SessionPhase : std::uint8_t {
    Playing,
    Victory,
    Defeat,
};

enum class RunMode : std::uint8_t {
    Standard,
    Campaign,
    Speedrun,
    TestMode,
    Count,
};

enum class RunRestartMode : std::uint8_t {
    NextSeed,
    SameSeed,
    NextCampaignFloor,
    Count,
};

struct SessionState {
    SessionPhase phase{SessionPhase::Playing};
    RunMode mode{RunMode::Standard};
    wkc::core::Seed initialSeed{0};
    wkc::core::Seed currentSeed{0};
    std::uint32_t runIndex{0U};
    std::uint32_t seedIndex{0U};
    std::uint32_t campaignFloor{1U};
    bool bossEngaged{false};
    std::uint64_t elapsedRunTicks{0U};
    Difficulty difficulty{Difficulty::Normal};
    bool campaignExitReady{false};
};

struct SessionUpdate {
    SessionPhase previousPhase{SessionPhase::Playing};
    SessionPhase phase{SessionPhase::Playing};
    bool changed{false};
};

struct BossStatus {
    bool exists{false};
    bool alive{false};

    constexpr bool operator==(const BossStatus&) const noexcept = default;
};

class SessionSystem {
public:
    explicit SessionSystem(wkc::core::Seed initialSeed) noexcept;

    [[nodiscard]] SessionUpdate update(
        bool playerAlive,
        gdp::world::RoomRole currentRoomRole,
        bool bossExists,
        bool bossAlive,
        bool bossAccessible = false,
        bool exitTerminalUsed = false,
        bool campaignMissionComplete = false) noexcept;
    [[nodiscard]] std::optional<wkc::core::Seed> restart(
        RunRestartMode mode = RunRestartMode::NextSeed) noexcept;
    [[nodiscard]] bool selectRunMode(RunMode mode) noexcept;
    [[nodiscard]] bool selectDifficulty(Difficulty difficulty) noexcept;
    [[nodiscard]] bool selectRunSeed(wkc::core::Seed seed) noexcept;
    [[nodiscard]] bool restoreCampaignCheckpoint(
        wkc::core::Seed initialSeed,
        wkc::core::Seed currentSeed,
        std::uint32_t runIndex,
        std::uint32_t seedIndex,
        std::uint32_t floor,
        Difficulty difficulty = Difficulty::Normal) noexcept;
    void advanceRunTimer(bool gameplayActive = true) noexcept;
    [[nodiscard]] const SessionState& state() const noexcept;

private:
    SessionState state_{};
};

[[nodiscard]] wkc::core::Seed seedForRun(
    wkc::core::Seed initialSeed,
    std::uint32_t runIndex) noexcept;
[[nodiscard]] const char* sessionPhaseName(SessionPhase phase) noexcept;
[[nodiscard]] const char* runModeName(RunMode mode) noexcept;
[[nodiscard]] bool canAdvanceCampaign(const SessionState& state) noexcept;
[[nodiscard]] BossStatus bossStatus(
    const gdp::entities::EntityManager& entities,
    gdp::world::RoomId roomId) noexcept;

} // namespace gdp::gameplay
