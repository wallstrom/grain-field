# Grain Field v0.8.1

## What's new

- **Release and swell between chords:** old notes ring out while new notes swell in, rather than sliding in pitch. Shared notes keep sounding, and Dissolve controls the transition time.
- **Arrive:** an automatable control that brings the piece in from silence, introducing the bed, texture, drones, sub, and room in sequence. At 100%, everything plays as configured. Arrive is not stored in scenes.
- **Voice:** blend between tuned resonating strings and the drone source's own sound, held across the notes of the chord.
- **Wear:** progressively erode the sounds where they play, losing highs, adding hiss, rounding peaks, and developing dropouts. Worn copies are kept in `Music/Grain Field/Worn`; original source files are not modified. Pick the original sound again to start fresh.

## Compatibility

**Existing projects will sound different. Voice defaults to 70%.** Back up projects and existing plugin bundles before upgrading, and compare important sessions with the previous version before saving over them.

## macOS download

The release asset is `Grain-Field-v0.8.1-macOS-universal.zip`, containing AU, VST3, and standalone bundles for Apple silicon and Intel Macs running macOS 11 or later, together with the license and notices.

Developer ID signing and notarization are not verified. A successful CI build does not establish DAW compatibility. See the [installation and security guidance](https://github.com/wallstrom/grain-field/blob/main/docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional) before installing.

## Validation

- Linux testing was reported for the functional changes.
- The [macOS build](https://github.com/wallstrom/grain-field/actions/runs/43411865836) passed for commit `04490eeb2516aba8e7029a245739d1bf2c55bfae`.
- The release ZIP passed its integrity check, and the AU, VST3, and standalone bundles all report version 0.8.1.
- The downloaded 0.8.1 build passed smoke tests in both Logic Pro and Ableton Live, as reported by the maintainer. These checks do not establish comprehensive DAW compatibility.

## SHA-256

Asset: `Grain-Field-v0.8.1-macOS-universal.zip`

SHA-256: `c1be4c765bf9a34f9dbe11198a7b074051d919bf972049ec86693a860237bde6`
