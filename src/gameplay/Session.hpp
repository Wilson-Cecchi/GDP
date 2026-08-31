#pragma once

#include "core/Seed.hpp"
#include "entities/EntityManager.hpp"
#include "world/RoomModule.hpp"

#include <cstdint>
#include <optional>

namespace gdp::gameplay {

enum class SessionPhase : std::uint8_t {
    Playing,
    Victory,
    Defeat,
};

struct SessionState {
    SessionPhase phase{SessionPhase::Playing};
    wkc::core::Seed initialSeed{0};
    wkc::core::Seed currentSeed{0};
    std::uint32_t runIndex{0U};
    bool bossEngaged{false};
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
        bool bossAlive) noexcept;
    [[nodiscard]] std::optional<wkc::core::Seed> restart() noexcept;
    [[nodiscard]] const SessionState& state() const noexcept;

private:
    SessionState state_{};
};

[[nodiscard]] wkc::core::Seed seedForRun(
    wkc::core::Seed initialSeed,
    std::uint32_t runIndex) noexcept;
[[nodiscard]] const char* sessionPhaseName(SessionPhase phase) noexcept;
[[nodiscard]] BossStatus bossStatus(
    const gdp::entities::EntityManager& entities,
    gdp::world::RoomId roomId) noexcept;

} // namespace gdp::gameplay
