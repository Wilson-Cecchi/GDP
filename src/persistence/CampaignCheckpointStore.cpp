#include "persistence/CampaignCheckpointStore.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
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

std::optional<float> parseFloat(std::string_view value)
{
    value = trim(value);
    if (value.empty() || value.front() == '+') return std::nullopt;
    float parsedValue = 0.0F;
    std::istringstream input{std::string{value}};
    input.imbue(std::locale::classic());
    input >> std::noskipws >> parsedValue;
    if (!input || input.rdbuf()->sgetc() != std::char_traits<char>::eof()
        || !std::isfinite(parsedValue)) {
        return std::nullopt;
    }
    return parsedValue;
}

std::optional<std::uint32_t> narrowUnsigned(
    const std::optional<std::uint64_t>& value) noexcept
{
    if (!value || *value > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

struct Fields {
    std::optional<std::uint64_t> version;
    std::optional<std::uint64_t> rulesVersion;
    std::optional<std::uint64_t> initialSeed;
    std::optional<std::uint64_t> currentSeed;
    std::optional<std::uint64_t> runIndex;
    std::optional<std::uint64_t> seedIndex;
    std::optional<std::uint64_t> floor;
    std::optional<std::uint64_t> difficulty;
    std::optional<float> health;
    std::optional<float> maximumHealth;
    std::optional<float> armor;
    std::optional<float> maximumArmor;
    std::optional<std::uint64_t> keys;
    std::optional<std::uint64_t> grenades;
    std::optional<std::uint64_t> weaponArchetype;
    std::optional<std::uint64_t> firearmOwned;
    std::optional<float> weaponDamage;
    std::optional<float> fireInterval;
    std::optional<float> range;
    std::optional<std::uint64_t> magazineSize;
    std::optional<std::uint64_t> reserveMaximum;
    std::optional<std::uint64_t> magazineAmmo;
    std::optional<std::uint64_t> reserveAmmo;
    std::optional<std::uint64_t> stowedOwned;
    std::optional<std::uint64_t> stowedArchetype;
    std::optional<float> stowedDamage;
    std::optional<float> stowedFireInterval;
    std::optional<float> stowedRange;
    std::optional<std::uint64_t> stowedMagazineSize;
    std::optional<std::uint64_t> stowedReserveMaximum;
    std::optional<std::uint64_t> stowedMagazineAmmo;
    std::optional<std::uint64_t> stowedReserveAmmo;
    std::optional<std::uint64_t> level;
    std::optional<std::uint64_t> experienceIntoLevel;
    std::optional<std::uint64_t> totalExperience;
    std::optional<std::uint64_t> score;
    std::optional<std::uint64_t> enemiesKilled;
    std::optional<std::uint64_t> propsDestroyed;
    std::optional<std::uint64_t> lootCollected;
    std::optional<std::uint64_t> eventsCompleted;
    std::optional<std::uint64_t> vitalityRank;
    std::optional<std::uint64_t> firepowerRank;
    std::optional<std::uint64_t> capacityRank;
};

bool assignField(
    Fields& fields,
    const std::string_view key,
    const std::string_view value)
{
    std::optional<float>* floatTarget = nullptr;
    if (key == "player_health") {
        floatTarget = &fields.health;
    } else if (key == "player_maximum_health") {
        floatTarget = &fields.maximumHealth;
    } else if (key == "player_armor") {
        floatTarget = &fields.armor;
    } else if (key == "player_maximum_armor") {
        floatTarget = &fields.maximumArmor;
    } else if (key == "weapon_damage") {
        floatTarget = &fields.weaponDamage;
    } else if (key == "weapon_fire_interval") {
        floatTarget = &fields.fireInterval;
    } else if (key == "weapon_range") {
        floatTarget = &fields.range;
    } else if (key == "weapon_stowed_damage") {
        floatTarget = &fields.stowedDamage;
    } else if (key == "weapon_stowed_fire_interval") {
        floatTarget = &fields.stowedFireInterval;
    } else if (key == "weapon_stowed_range") {
        floatTarget = &fields.stowedRange;
    }
    if (floatTarget != nullptr) {
        *floatTarget = parseFloat(value);
        return floatTarget->has_value();
    }

    std::optional<std::uint64_t>* unsignedTarget = nullptr;
    if (key == "version") {
        unsignedTarget = &fields.version;
    } else if (key == "difficulty") {
        unsignedTarget = &fields.difficulty;
    } else if (key == "rules_version") {
        unsignedTarget = &fields.rulesVersion;
    } else if (key == "initial_seed") {
        unsignedTarget = &fields.initialSeed;
    } else if (key == "current_seed") {
        unsignedTarget = &fields.currentSeed;
    } else if (key == "run_index") {
        unsignedTarget = &fields.runIndex;
    } else if (key == "seed_index") {
        unsignedTarget = &fields.seedIndex;
    } else if (key == "floor") {
        unsignedTarget = &fields.floor;
    } else if (key == "player_keys") {
        unsignedTarget = &fields.keys;
    } else if (key == "player_grenades") {
        unsignedTarget = &fields.grenades;
    } else if (key == "weapon_archetype") {
        unsignedTarget = &fields.weaponArchetype;
    } else if (key == "weapon_owned") {
        unsignedTarget = &fields.firearmOwned;
    } else if (key == "weapon_magazine_size") {
        unsignedTarget = &fields.magazineSize;
    } else if (key == "weapon_reserve_maximum") {
        unsignedTarget = &fields.reserveMaximum;
    } else if (key == "weapon_magazine_ammo") {
        unsignedTarget = &fields.magazineAmmo;
    } else if (key == "weapon_reserve_ammo") {
        unsignedTarget = &fields.reserveAmmo;
    } else if (key == "weapon_stowed_owned") {
        unsignedTarget = &fields.stowedOwned;
    } else if (key == "weapon_stowed_archetype") {
        unsignedTarget = &fields.stowedArchetype;
    } else if (key == "weapon_stowed_magazine_size") {
        unsignedTarget = &fields.stowedMagazineSize;
    } else if (key == "weapon_stowed_reserve_maximum") {
        unsignedTarget = &fields.stowedReserveMaximum;
    } else if (key == "weapon_stowed_magazine_ammo") {
        unsignedTarget = &fields.stowedMagazineAmmo;
    } else if (key == "weapon_stowed_reserve_ammo") {
        unsignedTarget = &fields.stowedReserveAmmo;
    } else if (key == "progression_level") {
        unsignedTarget = &fields.level;
    } else if (key == "progression_experience") {
        unsignedTarget = &fields.experienceIntoLevel;
    } else if (key == "progression_total_experience") {
        unsignedTarget = &fields.totalExperience;
    } else if (key == "progression_score") {
        unsignedTarget = &fields.score;
    } else if (key == "progression_enemies_killed") {
        unsignedTarget = &fields.enemiesKilled;
    } else if (key == "progression_props_destroyed") {
        unsignedTarget = &fields.propsDestroyed;
    } else if (key == "progression_loot_collected") {
        unsignedTarget = &fields.lootCollected;
    } else if (key == "progression_events_completed") {
        unsignedTarget = &fields.eventsCompleted;
    } else if (key == "progression_vitality_rank") {
        unsignedTarget = &fields.vitalityRank;
    } else if (key == "progression_firepower_rank") {
        unsignedTarget = &fields.firepowerRank;
    } else if (key == "progression_capacity_rank") {
        unsignedTarget = &fields.capacityRank;
    }
    if (unsignedTarget != nullptr) {
        *unsignedTarget = parseUnsigned(value);
        return unsignedTarget->has_value();
    }
    return true;
}

std::string serialize(const gameplay::CampaignCheckpoint& checkpoint)
{
    const gameplay::CampaignCarryover& state = checkpoint.floorEntry;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "version = " << currentCampaignCheckpointFileVersion << '\n'
           << "rules_version = " << checkpoint.rulesVersion << '\n'
           << "difficulty = " << static_cast<unsigned>(checkpoint.difficulty) << '\n'
           << "initial_seed = " << checkpoint.initialSeed << '\n'
           << "current_seed = " << checkpoint.currentSeed << '\n'
           << "run_index = " << checkpoint.runIndex << '\n'
           << "seed_index = " << checkpoint.seedIndex << '\n'
           << "floor = " << checkpoint.floor << '\n'
           << "player_health = " << state.player.health << '\n'
           << "player_maximum_health = " << state.player.maximumHealth << '\n'
           << "player_armor = " << state.player.armor << '\n'
           << "player_maximum_armor = " << state.player.maximumArmor << '\n'
           << "player_keys = " << state.player.keys << '\n'
           << "player_grenades = " << state.player.grenades << '\n'
           << "weapon_archetype = "
           << static_cast<std::uint32_t>(state.weapon.definition.archetype) << '\n'
           << "weapon_owned = " << (state.weapon.firearmOwned ? 1U : 0U) << '\n'
           << "weapon_damage = " << state.weapon.definition.damage << '\n'
           << "weapon_fire_interval = " << state.weapon.definition.fireInterval << '\n'
           << "weapon_range = " << state.weapon.definition.range << '\n'
           << "weapon_magazine_size = " << state.weapon.definition.magazineSize << '\n'
           << "weapon_reserve_maximum = " << state.weapon.definition.reserveMaximum << '\n'
           << "weapon_magazine_ammo = " << state.weapon.magazineAmmo << '\n'
           << "weapon_reserve_ammo = " << state.weapon.reserveAmmo << '\n'
           << "weapon_stowed_owned = " << (state.weapon.stowed ? 1U : 0U) << '\n';
    if (state.weapon.stowed) {
        output << "weapon_stowed_archetype = "
               << static_cast<std::uint32_t>(
                    state.weapon.stowed->definition.archetype) << '\n'
               << "weapon_stowed_damage = "
               << state.weapon.stowed->definition.damage << '\n'
               << "weapon_stowed_fire_interval = "
               << state.weapon.stowed->definition.fireInterval << '\n'
               << "weapon_stowed_range = "
               << state.weapon.stowed->definition.range << '\n'
               << "weapon_stowed_magazine_size = "
               << state.weapon.stowed->definition.magazineSize << '\n'
               << "weapon_stowed_reserve_maximum = "
               << state.weapon.stowed->definition.reserveMaximum << '\n'
               << "weapon_stowed_magazine_ammo = "
               << state.weapon.stowed->magazineAmmo << '\n'
               << "weapon_stowed_reserve_ammo = "
               << state.weapon.stowed->reserveAmmo << '\n';
    }
    output
           << "progression_level = " << state.progression.level << '\n'
           << "progression_experience = " << state.progression.experienceIntoLevel << '\n'
           << "progression_total_experience = " << state.progression.totalExperience << '\n'
           << "progression_score = " << state.progression.score << '\n'
           << "progression_enemies_killed = " << state.progression.enemiesKilled << '\n'
           << "progression_props_destroyed = " << state.progression.propsDestroyed << '\n'
           << "progression_loot_collected = " << state.progression.lootCollected << '\n'
           << "progression_events_completed = " << state.progression.eventsCompleted << '\n'
           << "progression_vitality_rank = " << state.progression.vitalityRank << '\n'
           << "progression_firepower_rank = " << state.progression.firepowerRank << '\n'
           << "progression_capacity_rank = " << state.progression.capacityRank << '\n';
    return output.str();
}

} // namespace

CampaignCheckpointLoadResult loadCampaignCheckpoint(
    const std::filesystem::path& path) noexcept
{
    CampaignCheckpointLoadResult result;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            result.status = !error && !exists
                ? CampaignCheckpointLoadStatus::Missing
                : CampaignCheckpointLoadStatus::IoError;
            return result;
        }
        Fields fields;
        std::string line;
        while (std::getline(input, line)) {
            std::string_view content = trim(line);
            const std::size_t comment = content.find('#');
            if (comment != std::string_view::npos) {
                content = trim(content.substr(0U, comment));
            }
            if (content.empty()) continue;
            const std::size_t separator = content.find('=');
            if (separator == std::string_view::npos
                || !assignField(fields, trim(content.substr(0U, separator)),
                    trim(content.substr(separator + 1U)))) {
                result.status = CampaignCheckpointLoadStatus::Invalid;
                return result;
            }
        }
        if (input.bad()) {
            result.status = CampaignCheckpointLoadStatus::IoError;
            return result;
        }
        if (!fields.version) {
            result.status = CampaignCheckpointLoadStatus::Invalid;
            return result;
        }
        if (*fields.version != 3U
            && *fields.version != 4U
            && *fields.version != 5U
            && *fields.version != 6U
            && *fields.version != 7U
            && *fields.version != currentCampaignCheckpointFileVersion) {
            result.status = CampaignCheckpointLoadStatus::UnsupportedVersion;
            return result;
        }
        const auto rulesVersion = narrowUnsigned(fields.rulesVersion);
        if (!rulesVersion) {
            result.status = CampaignCheckpointLoadStatus::Invalid;
            return result;
        }
        const bool migratePreviousRules = (*rulesVersion == 34U && *fields.version <= 5U)
            || (*rulesVersion == 35U && *fields.version <= 6U);
        const bool migratePreviousFloor = (*rulesVersion == 37U || *rulesVersion == 38U
                || *rulesVersion == 39U || *rulesVersion == 40U
                || *rulesVersion == 41U) && *fields.version == 7U;
        const bool migratePreviousCampaign = (*rulesVersion == 42U
                || *rulesVersion == 43U)
            && *fields.version == currentCampaignCheckpointFileVersion;
        // A current-rules file must not pretend to use the legacy round schema.
        if (!migratePreviousRules && !migratePreviousFloor && !migratePreviousCampaign
            && *fields.version != currentCampaignCheckpointFileVersion) {
            result.status = CampaignCheckpointLoadStatus::IncompatibleRules;
            return result;
        }
        if (*rulesVersion != gameplay::currentCampaignRulesVersion
            && !migratePreviousRules && !migratePreviousFloor
            && !migratePreviousCampaign) {
            result.status = CampaignCheckpointLoadStatus::IncompatibleRules;
            return result;
        }
        const auto runIndex = narrowUnsigned(fields.runIndex);
        const auto seedIndex = narrowUnsigned(fields.seedIndex);
        const auto floor = narrowUnsigned(fields.floor);
        const auto keys = narrowUnsigned(fields.keys);
        const auto grenades = narrowUnsigned(fields.grenades);
        const auto weaponArchetype = narrowUnsigned(fields.weaponArchetype);
        const auto firearmOwned = narrowUnsigned(fields.firearmOwned);
        const auto magazineSize = narrowUnsigned(fields.magazineSize);
        const auto reserveMaximum = narrowUnsigned(fields.reserveMaximum);
        const auto magazineAmmo = narrowUnsigned(fields.magazineAmmo);
        const auto reserveAmmo = narrowUnsigned(fields.reserveAmmo);
        const auto stowedOwned = narrowUnsigned(fields.stowedOwned);
        const auto level = narrowUnsigned(fields.level);
        const auto experience = narrowUnsigned(fields.experienceIntoLevel);
        const auto enemiesKilled = narrowUnsigned(fields.enemiesKilled);
        const auto propsDestroyed = narrowUnsigned(fields.propsDestroyed);
        const auto lootCollected = narrowUnsigned(fields.lootCollected);
        const auto eventsCompleted = narrowUnsigned(fields.eventsCompleted);
        const auto vitalityRank = narrowUnsigned(fields.vitalityRank);
        const auto firepowerRank = narrowUnsigned(fields.firepowerRank);
        const auto capacityRank = narrowUnsigned(fields.capacityRank);
        if (!fields.initialSeed || !fields.currentSeed || !runIndex || !seedIndex
            || !floor || !fields.health || !fields.maximumHealth || !fields.armor
            || !fields.maximumArmor || !keys
            || (*fields.version >= 8U && (!grenades || *grenades > 2U))
            || !weaponArchetype
            || *weaponArchetype > (*fields.version >= 6U ? 5U : 1U)
            || *weaponArchetype == static_cast<std::uint32_t>(gameplay::WeaponArchetype::Knife)
            || (*fields.version >= 4U
                && (!firearmOwned || *firearmOwned > 1U))
            || !fields.weaponDamage
            || !fields.fireInterval || !fields.range || !magazineSize
            || !reserveMaximum || !magazineAmmo || !reserveAmmo || !stowedOwned
            || *stowedOwned > 1U || !level
            || !experience || !fields.totalExperience || !fields.score
            || !enemiesKilled || !propsDestroyed || !lootCollected
            || !eventsCompleted || !vitalityRank || !firepowerRank
            || !capacityRank) {
            result.status = CampaignCheckpointLoadStatus::Invalid;
            return result;
        }
        std::optional<gameplay::WeaponCampaignProgress::StowedWeapon> stowed;
        if (*stowedOwned == 1U) {
            const auto stowedArchetype = narrowUnsigned(fields.stowedArchetype);
            const auto stowedMagazineSize
                = narrowUnsigned(fields.stowedMagazineSize);
            const auto stowedReserveMaximum
                = narrowUnsigned(fields.stowedReserveMaximum);
            const auto stowedMagazineAmmo
                = narrowUnsigned(fields.stowedMagazineAmmo);
            const auto stowedReserveAmmo
                = narrowUnsigned(fields.stowedReserveAmmo);
            if (!stowedArchetype || !fields.stowedDamage
                || !fields.stowedFireInterval || !fields.stowedRange
                || !stowedMagazineSize || !stowedReserveMaximum
                || !stowedMagazineAmmo || !stowedReserveAmmo
                || *stowedArchetype > (*fields.version >= 6U ? 5U : 1U)
                || *stowedArchetype == static_cast<std::uint32_t>(gameplay::WeaponArchetype::Knife)) {
                result.status = CampaignCheckpointLoadStatus::Invalid;
                return result;
            }
            stowed = gameplay::WeaponCampaignProgress::StowedWeapon{
                {*fields.stowedDamage, *fields.stowedFireInterval,
                    *fields.stowedRange, *stowedMagazineSize,
                    *stowedReserveMaximum,
                    static_cast<gameplay::WeaponArchetype>(*stowedArchetype)},
                *stowedMagazineAmmo,
                *stowedReserveAmmo,
            };
        }
        gameplay::CampaignCheckpoint checkpoint{
            gameplay::currentCampaignRulesVersion,
            *fields.initialSeed,
            *fields.currentSeed,
            *runIndex,
            *seedIndex,
            *floor,
            {
                {*fields.health, *fields.maximumHealth, *fields.armor,
                    *fields.maximumArmor, *keys,
                    *fields.version >= 8U ? *grenades : 2U},
                {{*fields.weaponDamage, *fields.fireInterval, *fields.range,
                    *magazineSize, *reserveMaximum,
                    static_cast<gameplay::WeaponArchetype>(*weaponArchetype)},
                    *magazineAmmo, *reserveAmmo, stowed,
                    *fields.version == 3U || *firearmOwned == 1U},
                {*level, *experience, *fields.totalExperience, *fields.score,
                    *enemiesKilled, *propsDestroyed, *lootCollected,
                    *eventsCompleted, *vitalityRank, *firepowerRank, *capacityRank},
            },
        };
        if (*fields.version >= 5U) {
            if (!fields.difficulty || *fields.difficulty >= static_cast<unsigned>(gameplay::Difficulty::Count)) {
                result.status = CampaignCheckpointLoadStatus::Invalid;
                return result;
            }
            checkpoint.difficulty = static_cast<gameplay::Difficulty>(*fields.difficulty);
        }
        if (!gameplay::validateCampaignCheckpoint(checkpoint)) {
            result.status = CampaignCheckpointLoadStatus::Invalid;
            return result;
        }
        if (migratePreviousRules) {
            const auto migrateSlot = [](auto& slot) {
                slot.reserveAmmo = gameplay::reserveUnitsFromRounds(slot.definition, slot.reserveAmmo);
                slot.definition.reserveMaximum = gameplay::reserveUnitsFromRounds(
                    slot.definition, slot.definition.reserveMaximum);
            };
            migrateSlot(checkpoint.floorEntry.weapon);
            if (checkpoint.floorEntry.weapon.stowed) migrateSlot(*checkpoint.floorEntry.weapon.stowed);
        }
        result.checkpoint = std::move(checkpoint);
        result.status = CampaignCheckpointLoadStatus::Loaded;
        result.migratedRules = migratePreviousRules || migratePreviousFloor
            || migratePreviousCampaign;
        return result;
    } catch (...) {
        result.status = CampaignCheckpointLoadStatus::IoError;
        return result;
    }
}

bool saveCampaignCheckpoint(
    const std::filesystem::path& path,
    const gameplay::CampaignCheckpoint& checkpoint) noexcept
{
    if (!gameplay::validateCampaignCheckpoint(checkpoint)) return false;
    std::filesystem::path temporary;
    try {
        std::error_code error;
        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, error);
            if (error) return false;
        }
        temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) return false;
            output << serialize(checkpoint);
            output.flush();
            if (!output.good()) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }
        const CampaignCheckpointLoadResult validation
            = loadCampaignCheckpoint(temporary);
        if (validation.status != CampaignCheckpointLoadStatus::Loaded
            || validation.checkpoint != checkpoint) {
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
        if (!temporary.empty()) std::filesystem::remove(temporary, error);
        return false;
    }
}

bool removeCampaignCheckpoint(const std::filesystem::path& path) noexcept
{
    try {
        std::error_code error;
        (void)std::filesystem::remove(path, error);
        if (error) return false;
        std::filesystem::path temporary = path;
        temporary += ".tmp";
        (void)std::filesystem::remove(temporary, error);
        return !error;
    } catch (...) {
        return false;
    }
}

} // namespace gdp::persistence
