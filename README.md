# Grain Field

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)

A generative ambient instrument for macOS: granular textures, evolving drones, and layered harmony, shaped into pieces over time. No MIDI keyboard required.

**Version 0.7.0 — pre-release** · Audio Unit · VST3 · Standalone · Apple silicon + Intel

Experimental build for evaluation: Developer ID signing/notarization and real DAW integration remain unverified. Back up projects before testing; this is not a production-ready release.

![Grain Field interface](Screenshots/ui_wall.png)

## What it does

- Blend three granular sound layers with drones and a background bed.
- Shape harmony with chord slots, layer tuning, or a MIDI keyboard.
- Keep sounds as scenes, blend between them, and automate changes in your DAW; shape intensity with **Swell**.
- Add tape, drive, echo, and reverb, or route four stereo outputs to your DAW.
- Import recordings, use side-chain input, resample, and record 24-bit WAV takes.

## Get started

Requires macOS 11+, Apple's command line tools, and internet access for the first build.

1. Clone this repository or download the source.
2. From the project folder, run `bash build-mac.sh` to build and install the plugins.
3. Load the AU in Logic Pro, the VST3 in Ableton Live, or open `build/GrainField_artefacts/Release/Standalone/Grain Field.app`.
4. Choose a preset with **‹ ›**, then press **play**. New instances start empty and stopped.

Prefer a download? Use the [v0.7.0 pre-release](https://github.com/wallstrom/grain-field/releases/tag/v0.7.0), or download **Grain Field macOS** from a successful [Actions run](https://github.com/wallstrom/grain-field/actions). Follow the [downloaded-build installation and security guidance](docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional) before opening it. Downloaded builds are not verified Developer ID-signed or notarized, so macOS may block them.

## Status

Build/package verification for **v0.7.0** on **2026-11-11**:

- [Main build](https://github.com/wallstrom/grain-field/actions/runs/43075448018) and [tagged release build](https://github.com/wallstrom/grain-field/actions/runs/43075448159) passed for cleaned initial commit `47181b6`, including packaging and artifact upload.
- The tagged ZIP passed integrity checks; all three bundles report version 0.7.0 and contain Apple silicon and Intel executables. The project license, both font notices, and sound credits matched the source files.
- **9 local build-tool regression tests** passed before the cleaned commit was published.
- **Historical audio/UI testing:** 40 audio checks in full mode and 6 synthetic UI checks passed before the history cleanup, including scene persistence, DAW scene automation, and transport sync. Those suites were not rerun for the refreshed release.
- Manual listening, native window interaction, and real DAW integration remain unverified, as do distribution signing and notarization.

Scenes replace timed parts: there is no internal timeline or automatic play-through. Existing v0.6 parts load as scenes; automate the **Scene** parameter in your DAW to arrange transitions.

CI currently builds and packages; neither the audio/UI suites nor the Python build-tool tests run in CI. See the [recorded test results](Tests/FunctionalTestResults.txt) for historical metrics, coverage, limitations, and reproduction instructions.

## Documentation

- [User guide](docs/USER_GUIDE.md): installation, controls, composition, sounds, routing, and troubleshooting.
- [Developer reference](docs/USER_GUIDE.md#for-developers): source layout and architecture.
- [Running tests](docs/USER_GUIDE.md#running-the-functional-tests) and [recorded results](Tests/FunctionalTestResults.txt).
- [Security policy](SECURITY.md): private vulnerability reporting, version scope, and distribution limitations.
- [Contributing](CONTRIBUTING.md): development setup, validation, and pull request guidelines.

Built with **C++17, JUCE 8.0.15, and CMake**. Build outputs and downloaded tools are ignored by Git.

## Licensing

Grain Field's original code is licensed under the GNU Affero General Public License, version 3 only ([LICENSE](LICENSE)). JUCE is separately dual-licensed under AGPLv3 or its [commercial licence](https://juce.com/get-juce/); a commercial JUCE licence does not override Grain Field's own AGPL terms. Fonts remain under the SIL Open Font License, and bundled sounds retain their separate terms in [Resources/Sounds/CREDITS.txt](Resources/Sounds/CREDITS.txt). Release archives include the project license, font notices, and sound credits. See the [licensing notes](docs/USER_GUIDE.md#11-licence).

## Pre-release status and known limitations

Grain Field **v0.7.0 is an experimental pre-release** intended for evaluation and feedback, not production use. Back up your projects and existing plugin installations before testing.

### What has been verified

- GitHub Actions successfully built and packaged the Audio Unit, VST3, and standalone app.
- All three bundles report version 0.7.0 and include Apple silicon and Intel executables.
- The release ZIP passed integrity checks and includes the project license, font notices, and sound credits.
- Nine local build-tool regression tests passed.

Earlier audio/UI testing recorded **40 audio checks and six synthetic UI checks passing**. These results predate the repository cleanup; those suites were not rerun for the refreshed release.

### Known limitations

- **Signing and notarization:** Developer ID signing and Apple notarization have not been verified. macOS may block downloaded bundles.
- **DAW compatibility:** Real-world integration with Logic Pro, Ableton Live, and other hosts remains unverified.
- **Manual testing:** Listening checks, native window interaction, hardware audio routing, and latency remain unverified.
- **Test coverage:** CI builds and packages the plugin but does not run the Python regression tests or audio/UI functional suites. Synthetic checks do not establish real-world reliability.

### Before installing

Download from the [official release](https://github.com/wallstrom/grain-field/releases/tag/v0.7.0), compare the ZIP's SHA-256 with the release notes, and read the [installation and security guidance](docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional). A matching checksum confirms file integrity, not software safety.

Do not disable Gatekeeper or lower your Mac's startup security to load the plugin.

Report ordinary bugs through [GitHub Issues](https://github.com/wallstrom/grain-field/issues), including your macOS version, architecture, plugin format, DAW/version, and reproduction steps. Report vulnerabilities privately through the [security reporting channel](https://github.com/wallstrom/grain-field/security/advisories/new).
