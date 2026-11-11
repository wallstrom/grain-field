# Contributing to Grain Field

Bug reports, documentation improvements, tests, and focused fixes are welcome. Grain Field is an experimental macOS instrument; real DAW integration and Developer ID signing/notarization remain unverified.

## Before you start

- Search [existing issues](https://github.com/wallstrom/grain-field/issues) and pull requests for related work.
- For substantial features, dependency changes, or architecture changes, open an issue to discuss the approach before implementation.
- Report vulnerabilities privately using [SECURITY.md](SECURITY.md), not a public issue or pull request.
- Keep discussions respectful and focused on the technical problem.

## Report a bug

Include the release or commit, macOS version, Apple silicon or Intel architecture, plugin format (AU, VST3, or standalone), DAW/version if applicable, minimal reproduction steps, and expected versus actual behavior.

Use synthetic or otherwise shareable audio when a reproduction requires a sample. Remove credentials, personal paths, private recordings, and other sensitive information from logs and attachments. Distinguish a build failure, host validation problem, crash, or audio issue rather than assuming they have the same cause.

## Development setup

1. Fork the repository, clone your fork, and create a topic branch from the latest `main`.
2. Use macOS 11 or later with Apple's command line tools, CMake 3.22 or later, and a C++17 compiler. CMake downloads JUCE 8.0.15 during the first configure; internet access is required unless you supply a local checkout with `-DJUCE_DIR=/path/to/JUCE`.
3. Review the [build instructions](docs/USER_GUIDE.md#1-build-it-on-your-mac) and [security guidance](docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional) before running scripts or loading a build.

From the repository root, configure and build with:

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
```

The default configuration builds universal Apple silicon/Intel AU, VST3, and standalone bundles. Building plugin targets copies them into your user plugin folders and can replace installed copies; back up existing plugins and projects before testing. `bash build-mac.sh` is an alternative that can download a local CMake installation and also installs the plugins.

The checked-in Ogg files are sufficient for ordinary builds. Only regenerate assets when intentionally changing them; see [rebuilding bundled sounds](docs/USER_GUIDE.md#rebuilding-the-bundled-sounds). Do not commit downloaded tools, build outputs, scratch files, or personal captures.

## Code and compatibility

- Follow the surrounding C++ or Python formatting and naming conventions. Avoid unrelated reformatting, dependency upgrades, and generated-file churn.
- Keep changes small and explain non-obvious DSP behavior, units, assumptions, and thread ownership.
- Avoid allocations, blocking locks, file/network I/O, and other unbounded work in real-time audio processing.
- Preserve parameter IDs, plugin identifiers, saved-state formats, and preset compatibility unless a change and migration strategy have been discussed.
- Add a regression test for a bug fix where practical. Update user-facing documentation when behavior or installation changes.
- Use repository-relative defaults and explicit overrides in tooling; do not add machine-specific absolute paths.
- Do not include third-party code, fonts, or audio without compatible licensing and provenance. Preserve existing notices and update asset credits and distribution packaging when needed.

## Validation

Run the checks relevant to your change and describe what you did not test.

For sound-tool or packaging changes, use Python 3 with NumPy and SciPy installed, preferably in a virtual environment under the ignored `.tools` directory:

```bash
python3 -m venv .tools/contrib-venv
source .tools/contrib-venv/bin/activate
python -m pip install numpy scipy
python -m unittest discover -s Tests -p 'test_sound_tools.py' -v
```

The Python tests do not require the VCSL sample library or render audio. Without NumPy and SciPy, the builder-specific checks are skipped; report skips rather than presenting them as passing tests.

For C++ audio or UI changes, build and run the [functional test suites](docs/USER_GUIDE.md#running-the-functional-tests). They require `GF_BUILD_TESTS=ON`; quick audio mode skips the full demo/listening-guide renders. Tests can write WAV/PNG files and captures under `~/Music/Grain Field/Captures`, so use fresh output directories and do not delete unrelated recordings.

For changes affecting sound or host behavior, describe any manual listening and AU/VST3/standalone checks, including the host/version and architecture. Synthetic UI tests and screenshots are not substitutes for real DAW testing. Keep test volume conservative and avoid testing first in a valuable project.

The current GitHub workflow runs on pushes and manual dispatch, not pull requests. It builds and packages but does not run the Python or audio/UI test suites. Include local validation results even when a workflow build passes. Run `git diff --check` before submitting; there is no dedicated repository-wide formatter or lint command currently configured.

## Submit a pull request

Open a focused pull request from your topic branch to `main`. Include:

- The problem, approach, and any related issue.
- The affected behavior and any compatibility or migration considerations.
- Exact validation commands, results, skipped checks, and relevant environment details.
- For interface or audio changes, suitable screenshots or short shareable audio examples where helpful.
- Any new dependency or asset provenance and licensing information.

Do not change release versions or move release tags as part of an unrelated contribution. Respond to review feedback and keep follow-up changes within the agreed scope. Review and acceptance are best-effort; there is no guaranteed response time.

## Licensing

By submitting a contribution, you agree to license your original contributions under the project's GNU Affero General Public License, version 3 only, as described in [LICENSE](LICENSE). Only submit work you have the right to contribute.

JUCE, fonts, sounds, and other third-party material retain their separate terms; see the [licensing notes](README.md#licensing). A commercial JUCE license does not remove Grain Field's own AGPL obligations.
