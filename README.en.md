# Iron Seed

[Português (BR)](README.md) · [English](README.en.md)

Iron Seed is a procedural FPS in development, built with C++20, SDL3, and
OpenGL on the project's WKC Forge engine. Its campaign spans ten floors with
varying missions and encounters; there is also a Speedrun mode and a local
arena for testing gameplay mechanics. The game supports Brazilian Portuguese
and English, keyboard and mouse, and SDL3-compatible game controllers.

## Play

Get a development build from the
[official Iron Seed download page](https://wkcdev.dev/en/projects/gdp/).
It suggests a platform and lets you choose another version; files are hosted
on GitHub Releases. Check the guide for your platform before launching:

| Platform | Guide |
| --- | --- |
| Windows x64 | [Install and run](README_WINDOWS.md) |
| Linux x64 | [Install and run](README_LINUX.md) |
| macOS Intel and Apple Silicon | [Install and run](README_MACOS.md) |

The platform guides are currently written in Brazilian Portuguese.

These are development builds, not a final release. You can switch languages
in-game under `Settings → Language` / `Configurações → Idioma`.

To report a bug or suggest an improvement, select `Send feedback` from the
main menu or Settings during a run. Nothing is sent until you confirm. Only
the category, your message, game version, and platform are submitted; saves
and logs are not attached. The form service also receives your IP address and
may log connection metadata. An internet connection and the system `curl`
program are required. If submission fails, the draft remains available to retry.

## Build on Linux

Requirements: CMake 3.24+, Ninja, a C++20 compiler, and an OpenGL 3.3 Core
driver. On the first configuration, CMake may download SDL3 if it is not
installed locally.

```bash
cmake --preset release
cmake --build --preset release --target gdp
build/release/gdp
```

To run the test suite after building all targets:

```bash
cmake --build --preset release --parallel
ctest --preset release
```

To open the test arena, configure a separate build with the arena enabled:

```bash
cmake -S . -B build/playtest -G Ninja -DCMAKE_BUILD_TYPE=Release -DGDP_ENABLE_TEST_MODE=ON -DGDP_BUILD_TESTS=OFF
cmake --build build/playtest --target gdp --parallel 4
build/playtest/gdp --test-mode
```

Distribution packages can be built without that arena. The executable still
uses the technical name `gdp`.

## Find the documentation

| Looking for… | Start here |
| --- | --- |
| Architecture and module layout | [`architecture.md`](architecture.md) and the [technical index](00_INDEX.md) |
| Gameplay, campaign, weapons, and mechanics tests | [`04_gameplay/`](04_gameplay/) |
| Xbox and PlayStation controls | [Controller guide](README_GAMEPAD.md) |
| Menus, HUD, and languages | [`07_interface/menus_and_settings.md`](07_interface/menus_and_settings.md) |
| Texture review and visual approval | [`visual/TEXTURE_APPROVALS.md`](visual/TEXTURE_APPROVALS.md) |
| Optional AI training on ZimaOS | [`tools/zimaos/README.md`](tools/zimaos/README.md) |
| Download build and publishing process | [Release guide](README_RELEASES.md) |
| Earlier project phases and decisions | [Technical history](README_HISTORY.md) |

Folders `01_core/` through `10_testing/` contain subsystem specifications.
`architecture.md` is the master document; the history records earlier steps
and does not replace current contracts. Most detailed technical documents are
currently written in Brazilian Portuguese.
