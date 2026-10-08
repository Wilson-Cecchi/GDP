#include "persistence/CampaignCheckpointStore.hpp"

#include "gameplay/Player.hpp"
#include "gameplay/Progression.hpp"
#include "gameplay/Session.hpp"
#include "gameplay/Weapon.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now()
                                .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("gdp_campaign_checkpoint_" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void writeText(const std::filesystem::path& path, const std::string_view text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

std::string readText(const std::filesystem::path& path)
{
    // End the handle's lifetime before replacing the save (Windows sharing rules).
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

gdp::gameplay::CampaignCheckpoint makeCheckpoint()
{
    using namespace gdp::gameplay;
    Player player;
    player.applyDamage(18.0F);
    player.addKeys(2U);
    Weapon weapon;
    (void)weapon.equip(WeaponArchetype::Carbine);
    weapon.fixedUpdate(1.0F);
    (void)weapon.tryFire();
    ProgressionSystem progression;
    const wkc::core::Seed initialSeed = 0x434845434B504F49ULL;
    constexpr std::uint32_t seedIndex = 1U;
    return {
        currentCampaignRulesVersion,
        initialSeed,
        seedForRun(initialSeed, seedIndex),
        2U,
        seedIndex,
        2U,
        captureCampaignCarryover(player, weapon, progression),
    };
}

gdp::gameplay::CampaignCheckpoint legacyRoundReserves(gdp::gameplay::CampaignCheckpoint checkpoint)
{
    const auto convert = [](auto& slot) {
        if (gdp::gameplay::weaponUsesSpareMagazines(slot.definition.archetype)) {
            slot.reserveAmmo *= slot.definition.magazineSize;
            slot.definition.reserveMaximum *= slot.definition.magazineSize;
        }
    };
    convert(checkpoint.floorEntry.weapon);
    if (checkpoint.floorEntry.weapon.stowed) convert(*checkpoint.floorEntry.weapon.stowed);
    return checkpoint;
}

void testMissingRoundTripReplacementAndRemoval()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "campaign_checkpoint.toml";

    const persistence::CampaignCheckpointLoadResult missing
        = persistence::loadCampaignCheckpoint(path);
    expect(missing.status == persistence::CampaignCheckpointLoadStatus::Missing
            && !missing.checkpoint,
        "a missing Campaign checkpoint is a valid empty state");

    gameplay::CampaignCheckpoint checkpoint = makeCheckpoint();
    expect(persistence::saveCampaignCheckpoint(path, checkpoint),
        "a valid Campaign floor entry is saved atomically");
    const persistence::CampaignCheckpointLoadResult loaded
        = persistence::loadCampaignCheckpoint(path);
    expect(loaded.status == persistence::CampaignCheckpointLoadStatus::Loaded
            && loaded.checkpoint == checkpoint,
        "all deterministic session and carry-over data survive disk round trip");
    expect(!std::filesystem::exists(path.string() + ".tmp"),
        "successful checkpoint save leaves no temporary file");

    checkpoint.floorEntry.player.health = 60.0F;
    expect(persistence::saveCampaignCheckpoint(path, checkpoint)
            && persistence::loadCampaignCheckpoint(path).checkpoint == checkpoint,
        "saving again atomically replaces the previous floor entry");

    expect(persistence::removeCampaignCheckpoint(path)
            && persistence::loadCampaignCheckpoint(path).status
                == persistence::CampaignCheckpointLoadStatus::Missing
            && persistence::removeCampaignCheckpoint(path),
        "checkpoint removal is idempotent and returns to the empty state");
}

void testMalformedDecimalFields()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const auto path = directory.path() / "campaign_checkpoint.toml";
    expect(persistence::saveCampaignCheckpoint(path, makeCheckpoint()),
        "a valid checkpoint supplies the malformed decimal fixture");
    const std::string validFile = readText(path);
    const std::string field = "player_health = ";
    const auto valueStart = validFile.find(field);
    expect(valueStart != std::string::npos,
        "the checkpoint fixture contains player health");
    if (valueStart == std::string::npos) return;
    const auto valueEnd = validFile.find('\n', valueStart);
    for (const std::string_view invalidValue : {
             "12.5junk", "nan", "+12.5", "12,5"}) {
        std::string malformed = validFile;
        malformed.replace(valueStart, valueEnd - valueStart,
            field + std::string{invalidValue});
        writeText(path, malformed);
        expect(persistence::loadCampaignCheckpoint(path).status
                == persistence::CampaignCheckpointLoadStatus::Invalid,
            "malformed or locale-dependent decimal fields are rejected");
    }
}

void testInvalidAndVersionedData()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "campaign_checkpoint.toml";

    writeText(path, "version = 99\nrules_version = 1\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::UnsupportedVersion,
        "unknown checkpoint schemas are not interpreted as current data");

    writeText(path, "version = 2\nrules_version = 25\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::UnsupportedVersion,
        "single-slot checkpoint schema is not reinterpreted as a two-slot loadout");

    writeText(path, "version = 3\nrules_version = 99\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from another Campaign ruleset remain isolated");

    writeText(path, "version = 3\nrules_version = 1\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from the pre-progression balance are rejected safely");

    writeText(path, "version = 3\nrules_version = 2\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before procedural Armor remain isolated");

    writeText(path, "version = 3\nrules_version = 3\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints without weapon identity remain isolated by their rules");

    writeText(path, "version = 3\nrules_version = 4\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before Arena Overdrive remain isolated by their rules");

    writeText(path, "version = 3\nrules_version = 5\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before deterministic obstacle avoidance remain isolated");

    writeText(path, "version = 3\nrules_version = 6\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before deterministic enemy separation remain isolated");

    writeText(path, "version = 3\nrules_version = 7\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before hard enemy contact volumes remain isolated");

    writeText(path, "version = 3\nrules_version = 8\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before deterministic patrol remain isolated");

    writeText(path, "version = 3\nrules_version = 9\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before enemy investigation remain isolated");

    writeText(path, "version = 3\nrules_version = 10\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before the START validation cover remain isolated");

    writeText(path, "version = 3\nrules_version = 11\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before combat-noise perception remain isolated");

    writeText(path, "version = 3\nrules_version = 12\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before the dedicated Alert state remain isolated");

    writeText(path, "version = 3\nrules_version = 13\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before the Recover state remain isolated");

    writeText(path, "version = 3\nrules_version = 14\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before global enemy pathfinding remain isolated");

    writeText(path, "version = 3\nrules_version = 15\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before directional enemy vision remain isolated");

    writeText(path, "version = 3\nrules_version = 16\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before group route diversity remain isolated");

    writeText(path, "version = 3\nrules_version = 17\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before material impact feedback remain isolated");

    writeText(path, "version = 3\nrules_version = 18\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before synchronized door motion remain isolated");

    writeText(path, "version = 3\nrules_version = 20\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before weak points remain isolated");

    writeText(path, "version = 3\nrules_version = 21\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before deterministic weapon spread remain isolated");

    writeText(path, "version = 3\nrules_version = 22\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before automatic Carbine fire remain isolated");

    writeText(path, "version = 3\nrules_version = 23\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before timed reloads remain isolated");

    writeText(path, "version = 3\nrules_version = 24\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before two-slot weapon loadouts remain isolated");

    writeText(path, "version = 3\nrules_version = 25\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before authoritative weapon switching remain isolated");

    writeText(path, "version = 3\nrules_version = 26\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before responsive weapon selection remain isolated");

    writeText(path, "version = 3\nrules_version = 27\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before the permanent Knife fallback remain isolated");

    writeText(path, "version = 3\nrules_version = 28\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before crouch and jump movement remain isolated");

    writeText(path, "version = 3\nrules_version = 29\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before encounter variation remain isolated");

    writeText(path, "version = 3\nrules_version = 30\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before route layout variation remain isolated");

    writeText(path, "version = 3\nrules_version = 31\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before moving enemy searches remain isolated");

    writeText(path, "version = 3\nrules_version = 32\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before neural Worker memory remain isolated");

    writeText(path, "version = 3\nrules_version = 33\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::IncompatibleRules,
        "checkpoints from before distance-aware investigation remain isolated");

    writeText(path, "version = 1\nrules_version = 3\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::UnsupportedVersion,
        "checkpoint schema without weapon identity is not reinterpreted");

    writeText(path,
        "version = 3\n"
        "rules_version = 19\n"
        "initial_seed = broken\n");
    expect(persistence::loadCampaignCheckpoint(path).status
            == persistence::CampaignCheckpointLoadStatus::Invalid,
        "malformed checkpoint values are rejected safely");

    gameplay::CampaignCheckpoint invalid = makeCheckpoint();
    invalid.currentSeed += 1U;
    expect(!persistence::saveCampaignCheckpoint(path, invalid),
        "an inconsistent deterministic seed is never persisted");
    invalid = makeCheckpoint();
    invalid.floorEntry.player.health
        = std::numeric_limits<float>::quiet_NaN();
    expect(!persistence::saveCampaignCheckpoint(path, invalid),
        "non-finite gameplay state is never persisted");
    invalid = makeCheckpoint();
    invalid.floorEntry.weapon.stowed
        = gameplay::WeaponCampaignProgress::StowedWeapon{
            invalid.floorEntry.weapon.definition, 0U, 0U};
    expect(!persistence::saveCampaignCheckpoint(path, invalid),
        "duplicate active and stowed weapon identities are never persisted");
}

void testCommentsAndFutureFields()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "campaign_checkpoint.toml";
    const gameplay::CampaignCheckpoint checkpoint = makeCheckpoint();
    expect(persistence::saveCampaignCheckpoint(path, checkpoint),
        "forward compatibility fixture starts from a valid checkpoint");
    {
        std::ofstream output(path, std::ios::binary | std::ios::app);
        output << "# reserved for later versions\n"
               << "future_field = ignored_by_version_one\n";
    }
    const persistence::CampaignCheckpointLoadResult loaded
        = persistence::loadCampaignCheckpoint(path);
    expect(loaded.status == persistence::CampaignCheckpointLoadStatus::Loaded
            && loaded.checkpoint == checkpoint,
        "comments and unknown future fields remain forward compatible");
}

void testKnifeOnlyAndLegacyRoundTrip()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const std::filesystem::path path
        = directory.path() / "campaign_checkpoint.toml";
    gameplay::CampaignCheckpoint knifeOnly = makeCheckpoint();
    gameplay::Weapon weapon;
    expect(weapon.dropSelectedFirearm().has_value(),
        "Knife-only checkpoint fixture drops its last gun");
    knifeOnly.floorEntry.weapon = weapon.campaignProgress();
    expect(persistence::saveCampaignCheckpoint(path, knifeOnly)
            && persistence::loadCampaignCheckpoint(path).checkpoint == knifeOnly,
        "schema 4 preserves a checkpoint with no firearm");

    const gameplay::CampaignCheckpoint legacy = makeCheckpoint();
    expect(persistence::saveCampaignCheckpoint(path, legacyRoundReserves(legacy)),
        "legacy migration fixture saves a valid armed checkpoint");
    std::ifstream input(path, std::ios::binary);
    std::string serialized{
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const std::size_t ownedLine = serialized.find("weapon_owned = 1\n");
    expect(ownedLine != std::string::npos,
        "new checkpoint explicitly stores firearm ownership");
    if (ownedLine != std::string::npos) {
        serialized.erase(ownedLine, std::string{"weapon_owned = 1\n"}.size());
        writeText(path, serialized);
        expect(persistence::loadCampaignCheckpoint(path).status
                == persistence::CampaignCheckpointLoadStatus::Invalid,
            "schema 4 rejects missing ownership instead of inventing a gun");
        serialized.replace(0U, std::string{"version = 7"}.size(), "version = 3");
        serialized.replace(serialized.find("rules_version = 40"), std::string{"rules_version = 40"}.size(), "rules_version = 34");
        writeText(path, serialized);
        expect(persistence::loadCampaignCheckpoint(path).checkpoint == legacy,
            "schema 3 checkpoints migrate as an armed loadout");
    }
}

void testNewPrimaryRoundTripsAndPreviousRulesMigration()
{
    using namespace gdp;
    using namespace gameplay;
    TemporaryDirectory directory;
    const auto path = directory.path() / "new-primary.toml";
    for (const auto type : {WeaponArchetype::Sniper, WeaponArchetype::Shotgun, WeaponArchetype::SubmachineGun}) {
        auto checkpoint = makeCheckpoint();
        Weapon weapon;
        (void)weapon.equip(type); weapon.fixedUpdate(1.0F);
        (void)weapon.tryFire(); (void)weapon.increaseDamage(12.0F);
        checkpoint.floorEntry.weapon = weapon.campaignProgress();
        expect(persistence::saveCampaignCheckpoint(path, checkpoint)
            && persistence::loadCampaignCheckpoint(path).checkpoint == checkpoint,
            "schema six round-trips each new primary, partial ammo, upgrades and pistol");
        (void)weapon.switchTo(WeaponArchetype::Pistol);
        checkpoint.floorEntry.weapon = weapon.campaignProgress();
        expect(persistence::saveCampaignCheckpoint(path, checkpoint)
            && persistence::loadCampaignCheckpoint(path).checkpoint == checkpoint,
            "schema six also preserves each new primary when stowed");
    }
    const auto old = makeCheckpoint();
    expect(persistence::saveCampaignCheckpoint(path, legacyRoundReserves(old)), "previous-rule fixture is serializable");
    std::string serialized = readText(path);
    serialized.replace(serialized.find("version = 7"), std::string{"version = 7"}.size(), "version = 5");
    serialized.replace(serialized.find("rules_version = 40"), std::string{"rules_version = 40"}.size(), "rules_version = 34");
    writeText(path, serialized);
    const auto migrated = persistence::loadCampaignCheckpoint(path);
    expect(migrated.migratedRules && migrated.checkpoint == old,
        "rules 34 round reserves migrate to complete magazines preserving floor-entry progress");
    const std::string after = readText(path);
    expect(after == serialized, "reading a migratable checkpoint does not overwrite its original file");
    for (const auto type : {WeaponArchetype::Sniper, WeaponArchetype::Shotgun, WeaponArchetype::SubmachineGun}) {
        auto expected = makeCheckpoint();
        Weapon weapon(baseWeaponDefinition(type));
        (void)weapon.equip(WeaponArchetype::Pistol);
        weapon.fixedUpdate(1.0F);
        expected.floorEntry.weapon = weapon.campaignProgress();
        auto rounds = legacyRoundReserves(expected);
        // A partially stocked old reserve rounds up once to one full magazine.
        if (weaponUsesSpareMagazines(type)) --rounds.floorEntry.weapon.stowed->reserveAmmo;
        const bool saved = persistence::saveCampaignCheckpoint(path, rounds);
        expect(saved, "schema six legacy fixture saves");
        if (!saved) continue; // Report the failure, not an out_of_range from missing text.
        std::string text = readText(path);
        text.replace(text.find("version = 7"), 11U, "version = 6");
        text.replace(text.find("rules_version = 40"), 18U, "rules_version = 35");
        writeText(path, text);
        const auto loaded = persistence::loadCampaignCheckpoint(path);
        expect(loaded.migratedRules && loaded.checkpoint == expected,
            "schema six migrates active pistol and stowed new primary, preserving shotgun shells");
        expect(loaded.checkpoint && persistence::saveCampaignCheckpoint(path, *loaded.checkpoint)
            && !persistence::loadCampaignCheckpoint(path).migratedRules
            && persistence::loadCampaignCheckpoint(path).checkpoint == expected,
            "saving migrated schema seven does not convert reserves a second time");
    }
}

void testPreviousCampaignFloorMigration()
{
    using namespace gdp;
    TemporaryDirectory directory;
    const auto path = directory.path() / "previous-floor.toml";
    const auto expected = makeCheckpoint();
    expect(persistence::saveCampaignCheckpoint(path, expected),
        "current checkpoint can supply the prior-floor migration fixture");
    std::string oldFile = readText(path);
    const auto version = oldFile.find("rules_version = 40");
    expect(version != std::string::npos, "fixture has the current rules marker");
    if (version == std::string::npos) return;
    for (const auto priorRules : {37U, 38U, 39U}) {
        std::string priorFile = oldFile;
        priorFile.replace(version, std::string{"rules_version = 40"}.size(),
            "rules_version = " + std::to_string(priorRules));
        writeText(path, priorFile);
        const auto migrated = persistence::loadCampaignCheckpoint(path);
        expect(migrated.migratedRules && migrated.checkpoint == expected
                && readText(path) == priorFile,
            "prior floor checkpoint preserves its entry state and original file on load");
        expect(migrated.checkpoint
                && persistence::saveCampaignCheckpoint(path, *migrated.checkpoint)
                && !persistence::loadCampaignCheckpoint(path).migratedRules,
            "saving a resumed floor upgrades only the rules marker to 40");
    }
}

} // namespace

int main()
{
    testMissingRoundTripReplacementAndRemoval();
    testMalformedDecimalFields();
    testInvalidAndVersionedData();
    testCommentsAndFutureFields();
    testKnifeOnlyAndLegacyRoundTrip();
    testNewPrimaryRoundTripsAndPreviousRulesMigration();
    testPreviousCampaignFloorMigration();
    if (failures != 0) {
        std::cerr << failures << " Campaign checkpoint persistence test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP Campaign checkpoint persistence tests passed\n";
    return 0;
}
