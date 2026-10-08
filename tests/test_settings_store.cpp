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
    settings.graphicsQuality = gdp::gameplay::GraphicsQuality::Low;
    settings.windowResolution
        = gdp::gameplay::WindowResolution::R1600x900;
    settings.fullscreen = true;
    settings.verticalSync = false;
    settings.language = gdp::gameplay::Language::PortugueseBrazil;
    settings.gamepadVibration = false;
    settings.gamepadVibrationPercent = 80U;
    settings.gamepadSouthpaw = true;
    settings.gamepadLookHorizontalPercent = 130U;
    settings.gamepadLookVerticalPercent = 90U;
    settings.gamepadAimSensitivityPercent = 70U;
    settings.gamepadMoveDeadzonePercent = 10U;
    settings.gamepadLookDeadzonePercent = 22U;
    settings.invertGamepadY = true;
    expect(gdp::gameplay::assignGamepadBinding(settings.gamepadControls,
            gdp::gameplay::ControlAction::Fire, wkc::platform::GamepadButton::West),
        "custom controller binding is accepted before persistence");
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
        "version = 10\n"
        "master_volume = 250\n"
        "effects_volume = loud\n"
        "ambient_volume = 45\n"
        "mouse_sensitivity = 400\n"
        "invert_mouse_y = maybe\n"
        "field_of_view = 20\n"
        "graphics_quality = impossible\n"
        "window_width = 1111\n"
        "window_height = 777\n"
        "fullscreen = maybe\n"
        "vertical_sync = maybe\n"
        "language = unavailable\n"
        "gamepad_look_horizontal = 999\n"
        "gamepad_look_deadzone = 999\n"
        "gamepad_southpaw = maybe\n"
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
            && recovered.settings.graphicsQuality
                == gdp::gameplay::GraphicsQuality::Medium
            && recovered.settings.windowResolution
                == gdp::gameplay::WindowResolution::R1280x720
            && !recovered.settings.fullscreen
            && recovered.settings.verticalSync
            && recovered.settings.language == gdp::gameplay::Language::English
            && recovered.settings.gamepadLookHorizontalPercent == 200U
            && recovered.settings.gamepadLookDeadzonePercent == 40U
            && !recovered.settings.gamepadSouthpaw,
        "recovery clamps ranges and defaults malformed or missing values");

    writeText(path,
        "version = 10\n"
        "master_volume = 35\n"
        "graphics_quality = HIGH\n"
        "future_option = any future representation\n");
    const SettingsLoadResult forwardCompatible = loadRuntimeSettings(path);
    expect(forwardCompatible.status == SettingsLoadStatus::Loaded
            && forwardCompatible.settings.masterVolumePercent == 35U
            && forwardCompatible.settings.effectsVolumePercent == 100U
            && forwardCompatible.settings.graphicsQuality
                == gdp::gameplay::GraphicsQuality::High
            && forwardCompatible.settings.verticalSync,
        "unknown and missing fields remain forward compatible");

    writeText(path, "version = 8\nmouse_sensitivity = 140\nbinding_0 = I\n");
    const auto previousV8 = loadRuntimeSettings(path);
    expect(previousV8.status == SettingsLoadStatus::Migrated
            && previousV8.settings.mouseSensitivityPercent == 140U
            && previousV8.settings.controls.values[0] == gdp::gameplay::bindingForName("I")
            && previousV8.settings.gamepadControls == gdp::gameplay::GamepadBindings{},
        "v8 migration preserves keyboard bindings and adds controller defaults");

    writeText(path, "version = 9\ngamepad_vibration_strength = 70\n");
    const auto previousV9 = loadRuntimeSettings(path);
    expect(previousV9.status == SettingsLoadStatus::Migrated
            && previousV9.settings.gamepadVibrationPercent == 70U
            && !previousV9.settings.gamepadSouthpaw
            && previousV9.settings.gamepadLookHorizontalPercent == 100U
            && previousV9.settings.gamepadMoveDeadzonePercent == 18U,
        "v9 migration preserves controller preferences and adds default camera controls");

    writeText(path, "version = 10\ngamepad_binding_7 = X / SQUARE\n"
        "gamepad_binding_9 = X / SQUARE\ngamepad_vibration_strength = 250\n");
    const auto brokenGamepad = loadRuntimeSettings(path);
    expect(brokenGamepad.status == SettingsLoadStatus::LoadedWithRecovery
            && brokenGamepad.settings.gamepadControls == gdp::gameplay::GamepadBindings{}
            && brokenGamepad.settings.gamepadVibrationPercent == 100U,
        "duplicate controller bindings and excessive vibration recover safely");

    writeText(path, "version = 7\nmaster_volume = 65\nbinding_0 = I\n");
    const auto previous = loadRuntimeSettings(path);
    expect(previous.status == SettingsLoadStatus::Migrated
        && previous.settings.language == gdp::gameplay::Language::English
        && previous.settings.masterVolumePercent == 65U
        && previous.settings.controls.values[0] == gdp::gameplay::bindingForName("I"),
        "v7 migration preserves existing settings and bindings without forcing a new language");

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

    writeText(path,
        "version = 3\n"
        "master_volume = 75\n"
        "fullscreen = true\n"
        "window_width = 960\n"
        "window_height = 540\n");
    const SettingsLoadResult migratedV3 = loadRuntimeSettings(path);
    expect(migratedV3.status == SettingsLoadStatus::Migrated
            && migratedV3.settings.masterVolumePercent == 75U
            && migratedV3.settings.fullscreen
            && migratedV3.settings.windowResolution
                == gdp::gameplay::WindowResolution::R1280x720,
        "schema version 3 preserves fullscreen and defaults window resolution");

    writeText(path,
        "version = 4\n"
        "window_width = 960\n"
        "window_height = 540\n"
        "fullscreen = true\n"
        "vertical_sync = false\n");
    const SettingsLoadResult migratedV4 = loadRuntimeSettings(path);
    expect(migratedV4.status == SettingsLoadStatus::Migrated
            && migratedV4.settings.windowResolution
                == gdp::gameplay::WindowResolution::R960x540
            && migratedV4.settings.fullscreen
            && migratedV4.settings.verticalSync
            && migratedV4.settings.graphicsQuality
                == gdp::gameplay::GraphicsQuality::Ultra,
        "schema version 4 preserves video settings and migrates legacy visuals");

    writeText(path,
        "version = 5\n"
        "window_width = 1280\n"
        "window_height = 720\n"
        "vertical_sync = false\n");
    const SettingsLoadResult migratedV5 = loadRuntimeSettings(path);
    expect(migratedV5.status == SettingsLoadStatus::Migrated
            && !migratedV5.settings.verticalSync
            && migratedV5.settings.graphicsQuality
                == gdp::gameplay::GraphicsQuality::Ultra,
        "schema version 5 preserves the former renderer as Ultra");
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
    invalidSource.windowResolution
        = static_cast<gdp::gameplay::WindowResolution>(99U);
    invalidSource.graphicsQuality
        = static_cast<gdp::gameplay::GraphicsQuality>(99U);
    expect(saveRuntimeSettings(path, invalidSource),
        "save accepts runtime values that require canonicalization");
    const SettingsLoadResult canonical = loadRuntimeSettings(path);
    expect(canonical.status == SettingsLoadStatus::Loaded
            && canonical.settings.masterVolumePercent == 100U
            && canonical.settings.mouseSensitivityPercent == 30U
            && canonical.settings.fieldOfViewDegrees == 100U
            && canonical.settings.windowResolution
                == gdp::gameplay::WindowResolution::R1280x720
            && canonical.settings.graphicsQuality
                == gdp::gameplay::GraphicsQuality::Medium,
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
