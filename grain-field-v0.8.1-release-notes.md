# Grain Field v0.8.1

## What's new

- **Release and swell between chords:** old notes ring out while new notes swell in, rather than sliding in pitch. Shared notes keep sounding, and Dissolve controls the transition time.
- **Arrive:** an automatable control that brings the piece in from silence, introducing the bed, texture, drones, sub, and room in sequence. At 100%, everything plays as configured. Arrive is not stored in scenes.
- **Voice:** blend between tuned resonating strings and the drone source's own sound, held across the notes of the chord.
- **Wear:** progressively erode the sounds where they play, losing highs, adding hiss, rounding peaks, and developing dropouts. Worn copies are kept in `Music/Grain Field/Worn`; original source files are not modified. Pick the original sound again to start fresh.

## Compatibility

**Existing projects will sound different. Voice defaults to 70%.** Back up projects and existing plugin bundles before upgrading, and compare important sessions with the previous version before saving over them.

## macOS download

The intended release asset is `Grain-Field-v0.8.1-macOS-universal.zip`, containing AU, VST3, and standalone bundles for Apple silicon and Intel Macs running macOS 11 or later, together with the license and notices.

Developer ID signing and notarization are not verified. A successful CI build does not establish DAW compatibility. See the [installation and security guidance](https://github.com/wallstrom/grain-field/blob/main/docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional) before installing.

## Validation status — draft, not ready to publish

- Linux testing was reported for the functional changes.
- The earlier successful macOS build at commit `a9c38619e1ec4a57ea497f71b4afa36f8f6bfc12` embeds version 0.8.0 and must not be used as the v0.8.1 release asset.
- Build a fresh macOS artifact after the version bump and verify that its bundles report 0.8.1.
- Test that artifact in Logic Pro or Ableton Live before publishing. No macOS host smoke test has been completed in this session.
- Replace the pending checksum below with the SHA-256 of the final release ZIP and update this validation section with actual results before publishing.

## SHA-256

Asset: `Grain-Field-v0.8.1-macOS-universal.zip`

SHA-256: **Pending — compute from the final 0.8.1 release ZIP before publishing.**
