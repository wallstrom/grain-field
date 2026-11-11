# Grain Field

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE) [![Build](https://github.com/wallstrom/grain-field/actions/workflows/build.yml/badge.svg)](https://github.com/wallstrom/grain-field/actions/workflows/build.yml)

A generative ambient instrument for macOS: granular textures, evolving drones, and layered harmony, shaped into pieces over time. No MIDI keyboard required.

**Version 0.8.0** · Audio Unit · VST3 · Standalone · Apple silicon + Intel

Download the [v0.8.0 release](https://github.com/wallstrom/grain-field/releases/tag/v0.8.0) for the OLED interface redesign and usability fixes. The macOS ZIP includes the Audio Unit, VST3, and standalone app.

Experimental build for evaluation: Developer ID signing/notarization and real DAW integration remain unverified. Back up projects before testing; this is not a production-ready release.

![Grain Field 0.8.0 OLED interface showing the arc preset's peak scene](Screenshots/ui_wall.png)

## What it does

- Blend three granular sound layers with drones and a background bed.
- Shape harmony with chord slots, layer tuning, or a MIDI keyboard.
- Keep sounds as scenes, blend between them, and automate changes in your DAW; shape intensity with **Swell**.
- Add tape, drive, echo, and reverb, or route four stereo outputs to your DAW.
- Import recordings, use side-chain input, resample, and record 24-bit WAV takes.

## Get started

Requires **macOS 11 or later** on Apple silicon or Intel. Developer tools are only needed if you build from source.

### Install a downloaded build

1. Download `Grain-Field-v0.8.0-macOS-universal.zip` from the [v0.8.0 release](https://github.com/wallstrom/grain-field/releases/tag/v0.8.0). Choose the plugin ZIP under **Assets**, not GitHub's source-code archives.
2. From the download folder, run `shasum -a 256 "Grain-Field-v0.8.0-macOS-universal.zip"` and compare the result with the SHA-256 in the release notes. If it matches, extract the ZIP. Quit your DAW and back up existing plugin copies before replacing them.
3. In Finder, use **Go > Go to Folder** to open the destinations below; create the user folders if they do not exist. Copy each complete bundle, without renaming it:
   - **Audio Unit:** `Grain Field.component` → `~/Library/Audio/Plug-Ins/Components/`
   - **VST3:** `Grain Field.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - **Standalone:** `Grain Field.app` → your preferred Applications folder.
4. Reopen your DAW and rescan plugins if necessary. You only need the format used by your host; the standalone app does not require a DAW.

Downloaded builds are not verified Developer ID-signed or notarized, so macOS may block them. Read the [checksum and macOS security guidance](docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional) before opening a download. Do not disable Gatekeeper, lower startup security, or remove quarantine attributes as a routine installation step.

### Build from source instead

Clone this repository or download the source, review `build-mac.sh`, then run `bash build-mac.sh` from the project folder. Apple's command line tools and internet access are required; the script can download CMake and fetches JUCE during the first build. It builds and installs the AU/VST3 plugins, replacing existing copies. Open the standalone build at `build/GrainField_artefacts/Release/Standalone/Grain Field.app`.

See the [source-build instructions](docs/USER_GUIDE.md#1-build-it-on-your-mac) for details. Alternatively, successful [Actions runs](https://github.com/wallstrom/grain-field/actions) provide a **Grain Field macOS** artifact containing a build ZIP.

### Open the instrument

- **Logic Pro:** create a Software Instrument track and choose **AU Instruments > Grain Field Audio > Grain Field > Stereo** in its instrument slot. If it is missing, try **Settings > Plug-in Manager > Reset & Rescan Selection**.
- **Ableton Live:** enable **Use VST3 Plug-in System Folders** in Settings > Plug-ins, rescan, then drag **Grain Field** from Plug-ins > VST3 onto a MIDI track.
- **Standalone:** open `Grain Field.app` from wherever you installed or built it.

Host integration remains unverified for this release. If a host or macOS blocks a plugin, consult the security guidance rather than weakening system-wide protections.

### Make your first sound

1. Start with a low monitoring volume. New instances open **empty and stopped**; no MIDI keyboard is required.
2. Use **‹ ›** at the top to select a factory piece such as **arc**, **haze**, **night rain**, or **wall**, then press **play**.
3. Move **Swell** from quiet toward wall to explore intensity. Change individual sounds in the **texture** and **drones** modules, and use **volume** to control the output.
4. With **sync** enabled, start your DAW's transport too; the plugin waits while the host is stopped. With sync off, its own play button runs it independently.
5. Click a **scene** to blend into another sound. Scenes do not advance automatically; arrange changes with the DAW's **Scene** automation parameter.

Click the piece name to **save this piece**, or click **record** to capture a performance and drag the resulting take to an audio track. Your presets and recordings are stored under `~/Music/Grain Field/`. To use your own audio, choose **load a sound…** from a sound's menu; imported files are referenced by location, so keep them available.

For all controls, MIDI harmony, side-chain input, recording, and multi-output routing, see the [user guide](docs/USER_GUIDE.md).

## Status

Release validation for **v0.8.0** on **2026-11-11**:

- **40 audio checks in full mode**, **22 synthetic UI checks**, and **10 Python build-tool regression tests** passed on Apple silicon.
- UI regressions cover WALL edge clicks, visible fade countdowns across six window sizes, scene-name contrast during blending, 16-bar hover actions, and RMS meter calibration.
- Screenshots were regenerated at 2× resolution from the fixed OLED source; representative normal, transition, chord-hover, and scaled states were visually inspected.
- [Tagged macOS CI passed](https://github.com/wallstrom/grain-field/actions/runs/43122411639), including packaging and artifact upload. The published ZIP passed integrity checks; all three bundles report version 0.8.0 and contain arm64 and x86_64 executables, with the project license, both font notices, and sound credits verified against the tagged source.
- Manual listening, native window interaction, and real DAW integration remain unverified, as do distribution signing and notarization.

Scenes replace timed parts: there is no internal timeline or automatic play-through. Existing v0.6 parts load as scenes; automate the **Scene** parameter in your DAW to arrange transitions.

CI currently builds and packages; neither the audio/UI suites nor the Python build-tool tests run in CI. See the [recorded test results](Tests/FunctionalTestResults.txt) for current validation, historical metrics, limitations, and reproduction instructions.

## Documentation

- [User guide](docs/USER_GUIDE.md): installation, controls, composition, sounds, routing, and troubleshooting.
- [Developer reference](docs/USER_GUIDE.md#for-developers): source layout and architecture.
- [Running tests](docs/USER_GUIDE.md#running-the-functional-tests) and [recorded results](Tests/FunctionalTestResults.txt).
- [Security policy](SECURITY.md): private vulnerability reporting, version scope, and distribution limitations.
- [Contributing](CONTRIBUTING.md): development setup, validation, and pull request guidelines.

Built with **C++17, JUCE 8.0.15, and CMake**. Build outputs and downloaded tools are ignored by Git.

## Licensing

Grain Field's original code is licensed under the GNU Affero General Public License, version 3 only ([LICENSE](LICENSE)). JUCE is separately dual-licensed under AGPLv3 or its [commercial licence](https://juce.com/get-juce/); a commercial JUCE licence does not override Grain Field's own AGPL terms. Fonts remain under the SIL Open Font License, and bundled sounds retain their separate terms in [Resources/Sounds/CREDITS.txt](Resources/Sounds/CREDITS.txt). Release archives include the project license, font notices, and sound credits. See the [licensing notes](docs/USER_GUIDE.md#11-licence).

## Release status and known limitations

Grain Field **v0.8.0 is published**, but remains experimental software intended for evaluation and feedback, not production use. Publishing a release does not imply Apple signing/notarization or verified DAW compatibility. Back up your projects and existing plugin installations before testing.

### What has been verified

- Native Apple silicon render and snapshot targets compiled for version 0.8.0 with JUCE 8.0.15.
- The full audio suite passed 40 checks, including scene persistence, DAW scene automation, and transport sync in the offline harness.
- Twenty-two synthetic UI checks and ten Python build-tool regression tests passed.
- The refreshed README screenshot was generated from this source and visually inspected.
- The tagged universal build and published ZIP passed the package checks described above; the release notes include its SHA-256.

Local test execution was on Apple silicon. Intel runtime behavior and real DAW integration remain unverified, even though the release bundles contain both architectures. See the [v0.8.0 release notes](https://github.com/wallstrom/grain-field/releases/tag/v0.8.0) for verification details and the download checksum.

### Known limitations

- **Signing and notarization:** Developer ID signing and Apple notarization have not been verified. macOS may block downloaded bundles.
- **DAW compatibility:** Real-world integration with Logic Pro, Ableton Live, and other hosts remains unverified.
- **Manual testing:** Listening checks, native window interaction, hardware audio routing, and latency remain unverified.
- **Test coverage:** CI builds and packages the plugin but does not run the Python regression tests or audio/UI functional suites. Synthetic checks do not establish real-world reliability.

### Before installing

Download from the [official v0.8.0 release](https://github.com/wallstrom/grain-field/releases/tag/v0.8.0), compare the ZIP's SHA-256 with the release notes, and read the [installation instructions above](#install-a-downloaded-build) and [macOS security guidance](docs/USER_GUIDE.md#macos-security-warnings). A matching checksum confirms file integrity, not software safety.

Do not disable Gatekeeper or lower your Mac's startup security to load the plugin.

Report ordinary bugs through [GitHub Issues](https://github.com/wallstrom/grain-field/issues), including your macOS version, architecture, plugin format, DAW/version, and reproduction steps. Report vulnerabilities privately through the [security reporting channel](https://github.com/wallstrom/grain-field/security/advisories/new).
