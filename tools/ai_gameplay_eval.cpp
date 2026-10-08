#include "game/ai/ActiveEnemyBrains.hpp"

#include "entities/EntityManager.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/Weapon.hpp"
#include "core/Random.hpp"
#include "world/WorldGenerator.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using gdp::entities::EnemyArchetype;
using gdp::entities::EntityId;
using gdp::gameplay::CollisionWorld;
using wkc::core::Vec3;

constexpr std::array<std::string_view, 5> archetypeNames{
    "worker", "guard", "officer", "engineer", "robot"};
constexpr std::array<std::string_view, 5> scenarioNames{
    "open", "center_cover", "elevated", "group", "boss_cover"};
constexpr std::array<std::string_view, 2> duelScenarioNames{
    "ground_strafe", "bhop"};
constexpr std::array<std::string_view, 3> teamScenarioNames{
    "firing_lane", "officer_guard", "engineer_guard"};
constexpr std::array<std::array<EnemyArchetype, 2>, 3> teamPairs{{
    {EnemyArchetype::Guard, EnemyArchetype::Worker},
    {EnemyArchetype::Officer, EnemyArchetype::Guard},
    {EnemyArchetype::Engineer, EnemyArchetype::Guard},
}};
constexpr float fixedStep = 1.0F / 60.0F;

struct EvaluationOptions {
    std::filesystem::path brainDirectory{"assets/ai"};
    std::array<std::optional<std::filesystem::path>, 5> overrides{};
    std::uint64_t seed{12'345U};
    std::uint32_t frames{360U};
    bool duel{false};
    bool team{false};
};

struct ScenarioResult {
    std::uint32_t elapsedFrames{0U};
    std::uint32_t defeatFrame{0U};
    std::size_t enemyCount{0U};
    std::size_t hits{0U};
    std::size_t enemyShots{0U};
    std::size_t blockedShots{0U};
    std::size_t dodgedShots{0U};
    std::size_t reloads{0U};
    std::size_t supportActions{0U};
    std::uint64_t neuralTicks{0U};
    float playerDamage{0.0F};
    float enemyTravel{0.0F};
    float closestDistance{0.0F};
    float finalDistance{0.0F};
};

struct DuelResult {
    std::uint32_t elapsedFrames{0U};
    std::string_view outcome{"timeout"};
    std::size_t playerShots{0U};
    std::size_t playerHits{0U};
    std::size_t enemyHits{0U};
    std::size_t enemyShots{0U};
    std::size_t blockedShots{0U};
    std::size_t dodgedShots{0U};
    std::size_t reloads{0U};
    std::size_t jumps{0U};
    std::uint64_t neuralTicks{0U};
    float damageToEnemy{0.0F};
    float damageToPlayer{0.0F};
    float peakPlayerSpeed{0.0F};
    float playerTravel{0.0F};
    float enemyTravel{0.0F};
};

struct TeamResult {
    std::string_view outcome{"timeout"};
    std::uint32_t elapsedFrames{0U};
    std::size_t playerShots{0U};
    std::size_t playerHits{0U};
    std::size_t enemyShots{0U};
    std::size_t enemyHits{0U};
    std::size_t friendlyHits{0U};
    std::size_t friendlyKills{0U};
    std::size_t fireHolds{0U};
    std::size_t supportActions{0U};
    std::uint64_t neuralTicks{0U};
    float playerDamage{0.0F};
    float enemyDamage{0.0F};
    float friendlyDamage{0.0F};
};

void printUsage()
{
    std::cout << "Usage: gdp_ai_gameplay_eval [--brain-dir DIR]"
                 " [--brain TYPE=FILE] [--seed N] [--frames N] [--duel|--team]\n"
                 "Compares scripted and trained enemy AI in deterministic combat"
                 " fixtures built inside generated Start/Boss rooms.\n"
                 "--duel adds player hitscan and physics-based ground/Bhop movement.\n"
                 "--team evaluates mixed-role fights and friendly-fire telemetry.\n";
}

std::optional<std::uint64_t> parseUnsigned(const std::string_view text)
{
    std::uint64_t value = 0U;
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::size_t> archetypeIndex(const std::string_view name)
{
    for (std::size_t index = 0U; index < archetypeNames.size(); ++index) {
        if (name == archetypeNames[index]) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<EvaluationOptions> parseOptions(const int argc, char* argv[])
{
    EvaluationOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help") {
            printUsage();
            return std::nullopt;
        }
        if (argument == "--duel") {
            options.duel = true;
            continue;
        }
        if (argument == "--team") {
            options.team = true;
            continue;
        }
        if (index + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n';
            return std::nullopt;
        }
        const std::string_view value{argv[++index]};
        if (argument == "--brain-dir") {
            options.brainDirectory = value;
        } else if (argument == "--brain") {
            const std::size_t separator = value.find('=');
            const auto type = archetypeIndex(value.substr(0U, separator));
            if (separator == std::string_view::npos || !type
                || separator + 1U == value.size() || options.overrides[*type]) {
                std::cerr << "Expected a unique --brain TYPE=FILE\n";
                return std::nullopt;
            }
            options.overrides[*type] = value.substr(separator + 1U);
        } else if (argument == "--seed") {
            const auto parsed = parseUnsigned(value);
            if (!parsed) {
                std::cerr << "Invalid --seed\n";
                return std::nullopt;
            }
            options.seed = *parsed;
        } else if (argument == "--frames") {
            const auto parsed = parseUnsigned(value);
            if (!parsed || *parsed == 0U || *parsed > 3600U) {
                std::cerr << "--frames must be 1..3600\n";
                return std::nullopt;
            }
            options.frames = static_cast<std::uint32_t>(*parsed);
        } else {
            std::cerr << "Unknown option: " << argument << '\n';
            return std::nullopt;
        }
    }
    if (options.duel && options.team) {
        std::cerr << "Choose --duel or --team, not both\n";
        return std::nullopt;
    }
    return options;
}

const gdp::world::RoomInstance* roomFor(
    const gdp::world::World& world, const EnemyArchetype archetype)
{
    const auto role = archetype == EnemyArchetype::IndustrialRobot
        ? gdp::world::RoomRole::Boss : gdp::world::RoomRole::Start;
    const auto room = std::find_if(world.rooms.begin(), world.rooms.end(),
        [role](const gdp::world::RoomInstance& candidate) {
            return candidate.role == role;
        });
    return room == world.rooms.end() ? nullptr : &*room;
}

float planarDistance(const Vec3 left, const Vec3 right)
{
    return std::hypot(left.x - right.x, left.z - right.z);
}

float enemyHealth(const EnemyArchetype archetype)
{
    switch (archetype) {
    case EnemyArchetype::Worker: return 50.0F;
    case EnemyArchetype::Guard: return 75.0F;
    case EnemyArchetype::Officer: return 70.0F;
    case EnemyArchetype::Engineer: return 70.0F;
    case EnemyArchetype::IndustrialRobot: return 220.0F;
    }
    return 50.0F;
}

gdp::gameplay::EnemyUpdateSummary updateEnemy(
    const EnemyArchetype archetype,
    gdp::entities::EntityManager& manager,
    gdp::gameplay::Player& player,
    const gdp::world::RoomId roomId,
    const CollisionWorld& collision,
    gdp::gameplay::NeuralEnemyExperiment* const brain,
    std::vector<gdp::gameplay::EnemyProjectile>& shots)
{
    using namespace gdp::gameplay;
    const EnemyPerception perception{.projectiles = &shots};
    switch (archetype) {
    case EnemyArchetype::Worker:
        return updateWorkers(manager, player, roomId, collision,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::Guard:
        return updateGuards(manager, player, roomId, collision,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::Officer:
        return updateOfficers(manager, player, roomId, collision,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::Engineer:
        return updateEngineers(manager, player, roomId, collision,
            fixedStep, {}, perception, brain);
    case EnemyArchetype::IndustrialRobot:
        return updateIndustrialRobots(manager, player, roomId, collision,
            fixedStep, {}, perception, brain);
    }
    return {};
}

std::optional<ScenarioResult> runScenario(
    const gdp::world::World& world,
    const EnemyArchetype archetype,
    const std::size_t scenario,
    const bool neural,
    const gdp::persistence::EnemyBrainRecord& record,
    const std::uint32_t frames)
{
    using namespace gdp;
    const world::RoomInstance* const room = roomFor(world, archetype);
    if (room == nullptr) {
        return std::nullopt;
    }
    const Vec3 center = (room->worldBounds.minimum
        + room->worldBounds.maximum) * 0.5F;
    const float floor = room->worldBounds.minimum.y;
    // Keep the two policies on exactly the same fixture, but make different
    // seeds exercise genuinely different relative positions and obstacle sizes.
    // Previously only the generated room changed while these combat layouts
    // stayed fixed, so a multi-seed comparison repeated nearly one encounter.
    wkc::core::RandomStream fixtureRandom{wkc::core::deriveSeed(
        world.masterSeed, 0x434f4d4241544658ULL,
        static_cast<std::uint64_t>(archetype) * scenarioNames.size()
            + scenario)};
    const float lateral = fixtureRandom.rangeFloat(-1.0F, 1.0F);
    const float playerX = center.x - lateral * 0.5F;
    const float enemyX = center.x + lateral * 0.5F;
    const float playerZ = center.z - fixtureRandom.rangeFloat(2.1F, 2.8F);
    const float enemyZ = center.z + fixtureRandom.rangeFloat(2.2F, 3.0F);
    const float coverX = center.x + fixtureRandom.rangeFloat(-0.15F, 0.15F);
    const float platformHalfWidth = fixtureRandom.rangeFloat(0.9F, 1.25F);
    const float platformHeight = fixtureRandom.rangeFloat(0.95F, 1.2F);
    const float movement = fixtureRandom.rangeFloat(
        scenario == 3U ? 0.6F : 0.9F, scenario == 3U ? 1.0F : 1.3F);
    const float movementPhase = fixtureRandom.rangeFloat(-0.8F, 0.8F);
    const bool elevated = scenario == 2U;
    const bool grouped = scenario == 3U;
    const bool covered = scenario == 1U || scenario == 4U;
    const Vec3 initialPlayer{playerX,
        floor + (elevated ? platformHeight + 0.1F : 0.0F), playerZ};

    std::vector<wkc::core::Aabb> solids;
    if (covered) {
        const float halfWidth = scenario == 4U
            ? fixtureRandom.rangeFloat(1.0F, 1.3F)
            : fixtureRandom.rangeFloat(0.7F, 0.95F);
        solids.push_back({
            {coverX - halfWidth, floor, center.z - 0.55F},
            {coverX + halfWidth, floor + 2.4F, center.z + 0.55F}});
    }
    if (elevated) {
        solids.push_back({
            {playerX - platformHalfWidth, floor, playerZ - 1.1F},
            {playerX + platformHalfWidth, floor + platformHeight,
                playerZ + 1.1F}});
    }
    const CollisionWorld collision{room->worldBounds, std::move(solids)};
    if (!collision.canOccupy(initialPlayer)) {
        return std::nullopt;
    }

    gameplay::Player player;
    player.spawn(initialPlayer);
    entities::EntityManager manager;
    std::vector<EntityId> enemies;
    const std::size_t enemyCount = grouped ? 3U : 1U;
    for (std::size_t index = 0U; index < enemyCount; ++index) {
        const float xOffset = grouped
            ? (static_cast<float>(index) - 1.0F) * 1.6F : 0.0F;
        const Vec3 position{enemyX + xOffset, floor,
            enemyZ + (grouped && index == 1U ? 0.4F : 0.0F)};
        if (!collision.canOccupy(position)) {
            return std::nullopt;
        }
        enemies.push_back(manager.createEnemy({
            .transform = {.position = position,
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = room->id,
            .archetype = archetype,
            .health = enemyHealth(archetype),
            .visualSeed = world.masterSeed + index + 1U,
        }));
        if (scenario == 4U && archetype == EnemyArchetype::IndustrialRobot) {
            auto* const boss = manager.findData<entities::EnemyData>(enemies.back());
            boss->health = boss->maximumHealth * 0.28F;
        }
    }
    if (covered || elevated) {
        (void)gameplay::alertEnemiesToNoise(manager, room->id,
            {.position = initialPlayer, .radius = 20.0F});
    }

    std::optional<gameplay::NeuralEnemyExperiment> brain;
    if (neural) {
        brain.emplace(world.masterSeed, archetype);
        brain->genome = record.genome;
        brain->contract = record.contract;
        brain->controlAll = true;
        brain->logSelection = false;
    }
    ScenarioResult result;
    gameplay::EnemyProjectileSystem projectiles;
    result.enemyCount = enemyCount;
    result.closestDistance = 1000.0F;
    for (std::uint32_t frame = 1U; frame <= frames; ++frame) {
        if (scenario == 0U || grouped) {
            const float time = static_cast<float>(frame) * fixedStep;
            const Vec3 next{playerX
                    + movement * std::sin(time * 0.85F + movementPhase),
                floor, playerZ};
            if (collision.canOccupy(next)) {
                (void)player.relocate(next, -90.0F);
            }
        }
        std::vector<Vec3> before;
        before.reserve(enemies.size());
        for (const EntityId id : enemies) {
            before.push_back(manager.find(id)->transform.position);
        }
        std::vector<gameplay::EnemyProjectile> shots;
        const auto summary = updateEnemy(archetype, manager, player,
            room->id, collision, brain ? &*brain : nullptr, shots);
        projectiles.spawn(shots);
        result.hits += summary.attacksLanded;
        result.enemyShots += summary.shotsFired;
        result.reloads += summary.reloadsStarted;
        for (const auto& event : projectiles.update(fixedStep, player, collision, manager)) {
            result.hits += event.hitPlayer ? 1U : 0U;
            result.blockedShots += event.blocked ? 1U : 0U;
            result.dodgedShots += event.dodged ? 1U : 0U;
        }
        result.supportActions += summary.commandsIssued + summary.repairsPerformed;
        for (std::size_t index = 0U; index < enemies.size(); ++index) {
            const Vec3 after = manager.find(enemies[index])->transform.position;
            result.enemyTravel += planarDistance(before[index], after);
            result.closestDistance = std::min(result.closestDistance,
                planarDistance(after, player.state().position));
        }
        result.elapsedFrames = frame;
        if (!player.state().alive) {
            result.defeatFrame = frame;
            break;
        }
    }
    result.playerDamage = 100.0F - player.state().health;
    result.finalDistance = planarDistance(
        manager.find(enemies.front())->transform.position,
        player.state().position);
    result.neuralTicks = brain ? brain->controlledTicks : 0U;
    return result;
}

std::optional<DuelResult> runDuelScenario(
    const gdp::world::World& world,
    const EnemyArchetype archetype,
    const bool bunnyHop,
    const bool neural,
    const gdp::persistence::EnemyBrainRecord& record,
    const std::uint32_t frames)
{
    using namespace gdp;
    const world::RoomInstance* const room = roomFor(world, archetype);
    if (room == nullptr) {
        return std::nullopt;
    }
    const Vec3 center = (room->worldBounds.minimum
        + room->worldBounds.maximum) * 0.5F;
    const float floor = room->worldBounds.minimum.y;
    wkc::core::RandomStream fixtureRandom{wkc::core::deriveSeed(
        world.masterSeed, 0x4455454c46585452ULL,
        static_cast<std::uint64_t>(archetype) * 2U
            + static_cast<std::uint64_t>(bunnyHop))};
    const float lateral = fixtureRandom.rangeFloat(-1.2F, 1.2F);
    const float separation = fixtureRandom.rangeFloat(4.2F, 6.2F);
    const Vec3 playerStart{center.x - lateral * 0.5F,
        floor, center.z - separation * 0.5F};
    const Vec3 enemyStart{center.x + lateral * 0.5F,
        floor, center.z + separation * 0.5F};
    const CollisionWorld collision{room->worldBounds, {}};
    if (!collision.canOccupy(playerStart)
        || !collision.canOccupy(enemyStart)) {
        return std::nullopt;
    }

    gameplay::Player player;
    player.spawn(playerStart, 90.0F);
    entities::EntityManager manager;
    constexpr float degreesPerRadian = 57.29577951308232F;
    const Vec3 towardPlayer = playerStart - enemyStart;
    const float enemyYaw = std::atan2(towardPlayer.x, towardPlayer.z)
        * degreesPerRadian + fixtureRandom.rangeFloat(-25.0F, 25.0F);
    const EntityId enemyId = manager.createEnemy({
        .transform = {.position = enemyStart,
            .rotationDegrees = {0.0F, enemyYaw, 0.0F}},
        .roomId = room->id,
        .archetype = archetype,
        .health = enemyHealth(archetype),
        .visualSeed = world.masterSeed + 1U,
    });
    // Fixed evaluation weapon: deliberately weaker than the campaign pistol,
    // so both sides have time to act. This is not a gameplay balance change.
    gameplay::Weapon weapon{{10.0F, 0.65F, 30.0F, 8U, 32U,
        gameplay::WeaponArchetype::Pistol}};
    std::optional<gameplay::NeuralEnemyExperiment> brain;
    if (neural) {
        brain.emplace(world.masterSeed, archetype);
        brain->genome = record.genome;
        brain->contract = record.contract;
        brain->controlAll = true;
        brain->logSelection = false;
    }

    DuelResult result;
    gameplay::EnemyProjectileSystem projectiles;
    for (std::uint32_t frame = 1U; frame <= frames; ++frame) {
        const Vec3 previousPlayer = player.state().position;
        const Vec3 previousEnemy = manager.find(enemyId)->transform.position;
        const float previousVerticalSpeed = player.state().velocity.y;
        gameplay::PlayerCommand command;
        command.moveRight = (frame / 45U) % 2U == 0U ? 1.0F : -1.0F;
        if (bunnyHop) {
            // One press at the first takeoff and one per landing: holding jump
            // continuously is not required, and Player applies the actual
            // air-strafe, momentum cap, gravity and collision rules.
            command.jump = frame == 1U || player.state().grounded;
            command.moveForward = frame <= 10U ? 1.0F : 0.0F;
        }
        player.fixedUpdate(command, fixedStep, collision);
        const Vec3 velocity = player.state().velocity;
        result.playerTravel += planarDistance(previousPlayer,
            player.state().position);
        result.peakPlayerSpeed = std::max(result.peakPlayerSpeed,
            std::hypot(velocity.x, velocity.z));
        if (previousVerticalSpeed <= 0.0F && velocity.y > 0.0F) {
            ++result.jumps;
        }

        weapon.fixedUpdate(fixedStep);
        if (weapon.magazineAmmo() == 0U) {
            (void)weapon.reload();
        } else if (!weapon.reloading() && weapon.cooldownRemaining() <= 0.0F) {
            const Vec3 target = manager.find(enemyId)->transform.position
                + Vec3{0.0F, 1.0F, 0.0F};
            const Vec3 aim = wkc::core::normalized(
                target - player.cameraPosition());
            const auto shot = gameplay::fireHitscan(weapon,
                player.cameraPosition(), aim, room->id, collision, manager,
                0.0F);
            if (shot.outcome != gameplay::ShotOutcome::Cooldown
                && shot.outcome != gameplay::ShotOutcome::Reloading
                && shot.outcome != gameplay::ShotOutcome::Switching
                && shot.outcome != gameplay::ShotOutcome::Empty) {
                ++result.playerShots;
            }
            if ((shot.outcome == gameplay::ShotOutcome::Hit
                    || shot.outcome == gameplay::ShotOutcome::Killed)
                && shot.hit && shot.hit->target == enemyId) {
                ++result.playerHits;
            }
        }

        const auto* const enemy = manager.findData<entities::EnemyData>(enemyId);
        result.damageToEnemy = enemyHealth(archetype) - enemy->health;
        result.elapsedFrames = frame;
        if (enemy->behavior == entities::EnemyBehaviorState::Dead) {
            result.outcome = "player_win";
            break;
        }
        std::vector<gameplay::EnemyProjectile> shots;
        const auto summary = updateEnemy(archetype, manager, player,
            room->id, collision, brain ? &*brain : nullptr, shots);
        projectiles.spawn(shots);
        result.enemyHits += summary.attacksLanded;
        result.enemyShots += summary.shotsFired;
        result.reloads += summary.reloadsStarted;
        for (const auto& event : projectiles.update(fixedStep, player, collision, manager)) {
            result.enemyHits += event.hitPlayer ? 1U : 0U;
            result.blockedShots += event.blocked ? 1U : 0U;
            result.dodgedShots += event.dodged ? 1U : 0U;
        }
        result.enemyTravel += planarDistance(previousEnemy,
            manager.find(enemyId)->transform.position);
        if (!player.state().alive) {
            result.outcome = "enemy_win";
            break;
        }
    }
    result.damageToPlayer = 100.0F - player.state().health;
    result.neuralTicks = brain ? brain->controlledTicks : 0U;
    return result;
}

std::optional<TeamResult> runTeamScenario(
    const gdp::world::World& world,
    const std::size_t scenario,
    const bool neural,
    const gdp::game::ai::ActiveEnemyBrainCatalog& records,
    const std::uint32_t frames)
{
    using namespace gdp;
    if (scenario >= teamPairs.size()) {
        return std::nullopt;
    }
    const world::RoomInstance* room = roomFor(world, EnemyArchetype::Guard);
    if (room == nullptr) {
        return std::nullopt;
    }
    const Vec3 center = (room->worldBounds.minimum
        + room->worldBounds.maximum) * 0.5F;
    const float floor = room->worldBounds.minimum.y;
    wkc::core::RandomStream random{wkc::core::deriveSeed(
        world.masterSeed, 0x5445414d46585452ULL, scenario)};
    const float playerLateral = random.rangeFloat(-0.35F, 0.35F);
    const float enemyLateral = playerLateral
        + random.rangeFloat(scenario == 0U ? -0.12F : -0.65F,
            scenario == 0U ? 0.12F : 0.65F);
    const float separation = random.rangeFloat(2.2F, 2.8F);
    const std::uint32_t strafePeriod = static_cast<std::uint32_t>(
        random.rangeInt(32, 58));
    const float strafeSpeed = random.rangeFloat(0.45F, 0.75F);
    const Vec3 playerStart{center.x + playerLateral, floor, center.z - separation};
    const Vec3 primaryStart{center.x + enemyLateral
            + (scenario == 0U ? 0.0F : -1.1F),
        floor, center.z + 2.4F};
    const Vec3 allyStart{center.x + enemyLateral
            + (scenario == 0U ? 0.0F : 1.1F),
        floor, center.z + (scenario == 0U ? 0.7F : 1.6F)};
    const CollisionWorld collision{room->worldBounds, {}};
    if (!collision.canOccupy(playerStart)
        || !collision.canOccupy(primaryStart)
        || !collision.canOccupy(allyStart)) {
        return std::nullopt;
    }
    gameplay::Player player;
    player.spawn(playerStart);
    entities::EntityManager manager;
    const auto types = teamPairs[scenario];
    const std::array<Vec3, 2> starts{primaryStart, allyStart};
    std::array<EntityId, 2> ids{};
    std::array<float, 2> startingHealth{};
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        startingHealth[index] = scenario == 2U && index == 1U
            ? 35.0F : enemyHealth(types[index]);
        ids[index] = manager.createEnemy({
            .transform = {.position = starts[index],
                .rotationDegrees = {0.0F, 180.0F, 0.0F}},
            .roomId = room->id,
            .archetype = types[index],
            .health = enemyHealth(types[index]),
            .visualSeed = world.masterSeed + index + 1U,
        });
        manager.findData<entities::EnemyData>(ids[index])->health
            = startingHealth[index];
    }
    gameplay::Weapon weapon{{10.0F, 0.65F, 30.0F, 8U, 32U,
        gameplay::WeaponArchetype::Pistol}};
    std::array<std::optional<gameplay::NeuralEnemyExperiment>, 5> brains;
    if (neural) {
        for (const EnemyArchetype type : types) {
            const std::size_t index = static_cast<std::size_t>(type);
            if (!records[index]) {
                return std::nullopt;
            }
            brains[index].emplace(world.masterSeed, type, records[index]->contract);
            brains[index]->genome = records[index]->genome;
            brains[index]->controlAll = true;
            brains[index]->logSelection = false;
        }
    }

    TeamResult result;
    gameplay::EnemyProjectileSystem projectiles;
    for (std::uint32_t frame = 1U; frame <= frames; ++frame) {
        gameplay::PlayerCommand command;
        command.moveRight = (frame / strafePeriod) % 2U == 0U
            ? strafeSpeed : -strafeSpeed;
        player.fixedUpdate(command, fixedStep, collision);
        weapon.fixedUpdate(fixedStep);
        EntityId targetId;
        float nearest = std::numeric_limits<float>::infinity();
        for (const EntityId id : ids) {
            const auto* enemy = manager.findData<entities::EnemyData>(id);
            if (enemy == nullptr
                || enemy->behavior == entities::EnemyBehaviorState::Dead) {
                continue;
            }
            const float distance = planarDistance(
                manager.find(id)->transform.position, player.state().position);
            if (distance < nearest) {
                nearest = distance;
                targetId = id;
            }
        }
        if (weapon.magazineAmmo() == 0U) {
            (void)weapon.reload();
        } else if (targetId.isValid() && !weapon.reloading()
            && weapon.cooldownRemaining() <= 0.0F) {
            const Vec3 target = manager.find(targetId)->transform.position
                + Vec3{0.0F, 1.0F, 0.0F};
            const auto shot = gameplay::fireHitscan(weapon,
                player.cameraPosition(),
                wkc::core::normalized(target - player.cameraPosition()),
                room->id, collision, manager, 0.0F);
            if (shot.outcome != gameplay::ShotOutcome::Cooldown
                && shot.outcome != gameplay::ShotOutcome::Reloading
                && shot.outcome != gameplay::ShotOutcome::Switching
                && shot.outcome != gameplay::ShotOutcome::Empty) {
                ++result.playerShots;
            }
            if (shot.hit && manager.findData<entities::EnemyData>(shot.hit->target)
                && (shot.outcome == gameplay::ShotOutcome::Hit
                    || shot.outcome == gameplay::ShotOutcome::Killed)) {
                ++result.playerHits;
            }
        }
        result.elapsedFrames = frame;
        const bool allEnemiesDead = std::all_of(ids.begin(), ids.end(),
            [&manager](const EntityId id) {
                return manager.findData<entities::EnemyData>(id)->behavior
                    == entities::EnemyBehaviorState::Dead;
            });
        if (allEnemiesDead) {
            result.outcome = "player_win";
            break;
        }

        std::vector<gameplay::EnemyProjectile> shots;
        for (const EnemyArchetype type : types) {
            const std::size_t index = static_cast<std::size_t>(type);
            auto* brain = neural ? &*brains[index] : nullptr;
            const auto summary = updateEnemy(type, manager, player,
                room->id, collision, brain, shots);
            result.enemyHits += summary.attacksLanded;
            result.enemyShots += summary.shotsFired;
            result.fireHolds += summary.friendlyFireHolds;
            result.supportActions += summary.commandsIssued
                + summary.repairsPerformed;
        }
        projectiles.spawn(shots);
        for (const auto& event
            : projectiles.update(fixedStep, player, collision, manager)) {
            result.enemyHits += event.hitPlayer ? 1U : 0U;
            result.friendlyHits += event.hitEnemy.isValid() ? 1U : 0U;
            result.friendlyKills += event.killedEnemy ? 1U : 0U;
            if (event.hitEnemy.isValid()) {
                result.friendlyDamage += event.damageApplied;
            }
        }
        if (!player.state().alive) {
            result.outcome = "enemy_win";
            break;
        }
    }
    result.playerDamage = 100.0F - player.state().health;
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        const auto* enemy = manager.findData<entities::EnemyData>(ids[index]);
        result.enemyDamage += std::max(0.0F,
            startingHealth[index] - enemy->health);
    }
    for (const auto& brain : brains) {
        if (brain) {
            result.neuralTicks += brain->controlledTicks;
        }
    }
    return result;
}

void printTeamRow(const std::string_view policy,
    const std::string_view scenario,
    const std::uint64_t seed,
    const EnemyArchetype primary,
    const EnemyArchetype ally,
    const gdp::game::ai::ActiveEnemyBrainCatalog& records,
    const TeamResult& result)
{
    const auto generation = [&](const EnemyArchetype type) {
        const auto& record = records[static_cast<std::size_t>(type)];
        return policy == "neural" && record ? record->genome.generation : 0U;
    };
    std::cout << policy << ',' << scenario << ',' << seed << ','
              << archetypeNames[static_cast<std::size_t>(primary)] << ','
              << archetypeNames[static_cast<std::size_t>(ally)] << ','
              << generation(primary) << ',' << generation(ally) << ','
              << result.outcome << ',' << result.elapsedFrames << ','
              << result.playerDamage << ',' << result.enemyDamage << ','
              << result.playerShots << ',' << result.playerHits << ','
              << result.enemyShots << ',' << result.enemyHits << ','
              << result.friendlyHits << ',' << result.friendlyKills << ','
              << result.friendlyDamage << ',' << result.fireHolds << ','
              << result.supportActions << ',' << result.neuralTicks << '\n';
}

void printDuelRow(const std::string_view policy,
    const std::string_view scenario,
    const std::string_view archetype,
    const std::uint64_t seed,
    const std::uint32_t brainGeneration,
    const DuelResult& result)
{
    std::cout << policy << ',' << scenario << ',' << archetype << ','
              << seed << ',' << brainGeneration << ','
              << result.outcome << ',' << result.elapsedFrames << ','
              << result.playerShots << ',' << result.playerHits << ','
              << result.enemyHits << ',' << result.damageToEnemy << ','
              << result.damageToPlayer << ',' << result.jumps << ','
              << result.peakPlayerSpeed << ',' << result.playerTravel << ','
              << result.enemyTravel << ',' << result.neuralTicks << ','
              << result.enemyShots << ',' << result.blockedShots << ','
              << result.dodgedShots << ',' << result.reloads << '\n';
}

void printRow(const std::string_view policy,
    const std::string_view scenario,
    const std::string_view archetype,
    const std::uint64_t seed,
    const std::uint32_t brainGeneration,
    const ScenarioResult& result)
{
    std::cout << policy << ',' << scenario << ',' << archetype << ','
              << seed << ',' << brainGeneration << ','
              << result.enemyCount << ',' << result.elapsedFrames << ','
              << result.defeatFrame << ',' << result.playerDamage << ','
              << result.hits << ',' << result.supportActions << ','
              << result.enemyTravel << ',' << result.closestDistance << ','
              << result.finalDistance << ',' << result.neuralTicks << ','
              << result.enemyShots << ',' << result.blockedShots << ','
              << result.dodgedShots << ',' << result.reloads << '\n';
}

} // namespace

int main(const int argc, char* argv[])
{
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        printUsage();
        return 0;
    }
    const auto options = parseOptions(argc, argv);
    if (!options) {
        return 2;
    }
    auto brains = gdp::game::ai::loadActiveEnemyBrains(options->brainDirectory);
    for (std::size_t index = 0U; index < brains.size(); ++index) {
        if (options->overrides[index]) {
            brains[index] = gdp::persistence::loadEnemyBrain(
                *options->overrides[index]);
        }
        if (!brains[index]
            || static_cast<std::size_t>(brains[index]->archetype) != index) {
            std::cerr << "Missing or mismatched brain for "
                      << archetypeNames[index] << '\n';
            return 2;
        }
    }
    const auto generated = gdp::world::generateWorld(options->seed);
    if (!generated.succeeded()) {
        std::cerr << "Could not generate evaluation world for seed "
                  << options->seed << '\n';
        return 1;
    }
    std::cout << std::fixed << std::setprecision(3);
    if (options->team) {
        std::cout << "policy,scenario,seed,primary,ally,primary_generation,"
                     "ally_generation,outcome,elapsed_frames,player_damage,"
                     "enemy_damage,player_shots,player_hits,enemy_shots,"
                     "enemy_hits,friendly_hits,friendly_kills,friendly_damage,"
                     "fire_holds,support_actions,neural_ticks\n";
        for (std::size_t scenario = 0U; scenario < teamScenarioNames.size(); ++scenario) {
            for (const bool neural : {false, true}) {
                const auto result = runTeamScenario(*generated.world,
                    scenario, neural, brains, options->frames);
                if (!result) {
                    std::cerr << "Invalid team fixture: "
                              << teamScenarioNames[scenario] << '\n';
                    return 1;
                }
                printTeamRow(neural ? "neural" : "scripted",
                    teamScenarioNames[scenario], options->seed,
                    teamPairs[scenario][0], teamPairs[scenario][1], brains, *result);
            }
        }
        return 0;
    }
    if (options->duel) {
        std::cout << "policy,scenario,archetype,seed,brain_generation,"
                     "outcome,elapsed_frames,player_shots,player_hits,"
                     "enemy_hits,damage_to_enemy,damage_to_player,jumps,"
                     "peak_player_speed,player_travel,enemy_travel,neural_ticks,"
                     "enemy_shots,enemy_shots_blocked,enemy_shots_dodged,"
                     "enemy_reloads\n";
        for (std::size_t type = 0U; type < archetypeNames.size(); ++type) {
            const auto archetype = static_cast<EnemyArchetype>(type);
            for (std::size_t scenario = 0U;
                scenario < duelScenarioNames.size(); ++scenario) {
                for (const bool neural : {false, true}) {
                    const auto result = runDuelScenario(*generated.world,
                        archetype, scenario == 1U, neural,
                        *brains[type], options->frames);
                    if (!result) {
                        std::cerr << "Invalid duel fixture: "
                                  << archetypeNames[type] << '/'
                                  << duelScenarioNames[scenario] << '\n';
                        return 1;
                    }
                    printDuelRow(neural ? "neural" : "scripted",
                        duelScenarioNames[scenario], archetypeNames[type],
                        options->seed,
                        neural ? brains[type]->genome.generation : 0U,
                        *result);
                }
            }
        }
        return 0;
    }
    std::cout << "policy,scenario,archetype,seed,brain_generation,enemies,"
                 "elapsed_frames,defeat_frame,player_damage,hits,support_actions,"
                 "enemy_travel,closest_distance,final_distance,neural_ticks,"
                 "enemy_shots,enemy_shots_blocked,enemy_shots_dodged,"
                 "enemy_reloads\n";
    for (std::size_t type = 0U; type < archetypeNames.size(); ++type) {
        const auto archetype = static_cast<EnemyArchetype>(type);
        for (std::size_t scenario = 0U; scenario < scenarioNames.size(); ++scenario) {
            if (scenario == 4U && archetype != EnemyArchetype::IndustrialRobot) {
                continue;
            }
            for (const bool neural : {false, true}) {
                const auto result = runScenario(*generated.world, archetype,
                    scenario, neural, *brains[type], options->frames);
                if (!result) {
                    std::cerr << "Invalid fixture: " << archetypeNames[type]
                              << '/' << scenarioNames[scenario] << '\n';
                    return 1;
                }
                printRow(neural ? "neural" : "scripted",
                    scenarioNames[scenario], archetypeNames[type],
                    options->seed, neural ? brains[type]->genome.generation : 0U,
                    *result);
            }
        }
    }
    return 0;
}
