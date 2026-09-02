#include "persistence/SettingsStore.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
            / ("gdp_settings_store_" + std::to_string(suffix));
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

void testMissingAndRoundTrip()
{
    using namespace gdp::persistence;
    TemporaryDirectory directory;
    const std::filesystem::path path = directory.path() / "settings.toml";

    const SettingsLoadResult missing = loadRuntimeSettings(path);
    expect(missing.status == SettingsLoadStatus::Missing
            && missing.settings == gdp::gameplay::RuntimeSettings{},
        "missing file returns defaults without becoming an error");

    gdp::gameplay::RuntimeSettings settings;
    settings.masterVolumePercent = 70U;
    settings.effectsVolumePercent = 80U;
    settings.ambientVolumePercent = 40U;
    settings.uiVolumePercent = 60U;
    settings.mouseSensitivityPercent = 130U;
    settings.invertMouseY = true;
    settings.fieldOfViewDegrees = 82U;
    settings.fullscreen = true;
    expect(saveRuntimeSettings(path, settings),
        "settings are written through the validated temporary file");
    const SettingsLoadResult loaded = loadRuntimeSettings(path);
    expect(loaded.status == SettingsLoadStatus::Loaded
            && loaded.settings == settings,
        "all settings survive a disk round trip");
    expect(!std::filesystem::exists(path.string() + ".tmp"),
        "successful atomic replacement leaves no temporary file");

    settings.masterVolumePercent = 20U;
    expect(saveRuntimeSettings(path, settings)
            && loadRuntimeSettings(path).settings == settings,
        "atomic replacement updates an existing settings file");
}

void testRecoveryAndForwardCompatibility()
{
    using namespace gdp::persistence;
    TemporaryDirectory directory;
    const std::filesystem::path path = directory.path() / "settings.toml";
    writeText(path,
        "# recover individual invalid fields\n"
        "version = 3\n"
        "master_volume = 250\n"
        "effects_volume = loud\n"
        "ambient_volume = 45\n"
        "mouse_sensitivity = 400\n"
        "invert_mouse_y = maybe\n"
        "field_of_view = 20\n"
        "fullscreen = maybe\n"
        "future_option = enabled\n");

    const SettingsLoadResult recovered = loadRuntimeSettings(path);
    expect(recovered.status == SettingsLoadStatus::LoadedWithRecovery,
        "invalid known fields are reported as recovered");
    expect(recovered.settings.masterVolumePercent == 100U
            && recovered.settings.effectsVolumePercent == 100U
            && recovered.settings.ambientVolumePercent == 45U
            && recovered.settings.uiVolumePercent == 100U
            && recovered.settings.mouseSensitivityPercent == 200U
            && !recovered.settings.invertMouseY
            && recovered.settings.fieldOfViewDegrees == 60U
            && !recovered.settings.fullscreen,
        "recovery clamps ranges and defaults malformed or missing values");

    writeText(path,
        "version = 3\n"
        "master_volume = 35\n"
        "future_option = any future representation\n");
    const SettingsLoadResult forwardCompatible = loadRuntimeSettings(path);
    expect(forwardCompatible.status == SettingsLoadStatus::Loaded
            && forwardCompatible.settings.masterVolumePercent == 35U
            && forwardCompatible.settings.effectsVolumePercent == 100U,
        "unknown and missing fields remain forward compatible");

    writeText(path,
        "version = 1\n"
        "master_volume = 55\n"
        "mouse_sensitivity = 120\n"
        "invert_mouse_y = true\n"
        "field_of_view = 76\n");
    const SettingsLoadResult migrated = loadRuntimeSettings(path);
    expect(migrated.status == SettingsLoadStatus::Migrated
            && migrated.settings.masterVolumePercent == 55U
            && migrated.settings.mouseSensitivityPercent == 120U
            && !migrated.settings.invertMouseY
            && migrated.settings.fieldOfViewDegrees == 76U,
        "schema version 1 migrates with invert Y safely disabled");

    writeText(path,
        "version = 2\n"
        "master_volume = 65\n"
        "invert_mouse_y = true\n"
        "field_of_view = 84\n"
        "fullscreen = true\n");
    const SettingsLoadResult migratedV2 = loadRuntimeSettings(path);
    expect(migratedV2.status == SettingsLoadStatus::Migrated
            && migratedV2.settings.masterVolumePercent == 65U
            && migratedV2.settings.invertMouseY
            && migratedV2.settings.fieldOfViewDegrees == 84U
            && !migratedV2.settings.fullscreen,
        "schema version 2 preserves invert Y and defaults fullscreen safely");
}

void testInvalidAndUnsupportedFiles()
{
    using namespace gdp::persistence;
    TemporaryDirectory directory;
    const std::filesystem::path path = directory.path() / "settings.toml";

    writeText(path, "master_volume = 10\n");
    const SettingsLoadResult invalid = loadRuntimeSettings(path);
    expect(invalid.status == SettingsLoadStatus::Invalid
            && invalid.settings == gdp::gameplay::RuntimeSettings{},
        "a file without a valid version is rejected safely");

    writeText(path, "version = 99\nmaster_volume = 10\n");
    const SettingsLoadResult unsupported = loadRuntimeSettings(path);
    expect(unsupported.status == SettingsLoadStatus::UnsupportedVersion
            && unsupported.settings == gdp::gameplay::RuntimeSettings{},
        "unknown schema versions cannot be interpreted as the current format");

    gdp::gameplay::RuntimeSettings invalidSource;
    invalidSource.masterVolumePercent = 500U;
    invalidSource.mouseSensitivityPercent = 1U;
    invalidSource.fieldOfViewDegrees = 500U;
    expect(saveRuntimeSettings(path, invalidSource),
        "save accepts runtime values that require canonicalization");
    const SettingsLoadResult canonical = loadRuntimeSettings(path);
    expect(canonical.status == SettingsLoadStatus::Loaded
            && canonical.settings.masterVolumePercent == 100U
            && canonical.settings.mouseSensitivityPercent == 30U
            && canonical.settings.fieldOfViewDegrees == 100U,
        "save persists only canonical settings ranges");
}

} // namespace

int main()
{
    testMissingAndRoundTrip();
    testRecoveryAndForwardCompatibility();
    testInvalidAndUnsupportedFiles();
    if (failures != 0) {
        std::cerr << failures << " settings persistence test(s) failed\n";
        return 1;
    }
    std::cout << "All GDP settings persistence tests passed\n";
    return 0;
}
