# Grain Field User Guide

Version 0.7.1. [Back to the README](../README.md).

This experimental build is for evaluation, not production use. Developer ID signing/notarization and real DAW integration remain unverified. Back up projects and existing plugin copies before testing.

Paths and commands below are relative to the project root unless noted otherwise.

## 1. Build it on your Mac

You need a Mac with macOS 11 or later and an internet connection. You don't need Xcode.

1. Unzip this folder somewhere easy, such as your Desktop.
2. Open **Terminal** (press Cmd+Space, type `Terminal`, press Return).
3. Type `bash ` (with a space after it), drag the file **build-mac.sh** from this folder into the Terminal window, and press Return.

The script does the rest:

- If Apple's command line tools are missing, a window asks to install them. Click **Install**, wait, then run step 3 again.
- It downloads CMake and JUCE the first time it runs.
- It compiles the plugin. **The first build takes 5 to 15 minutes**; later builds are much quicker.
- It installs the plugin, replacing the earlier version.

Local builds are not automatically trusted or verified Developer ID-signed and notarized. Only build source you trust, and review `build-mac.sh` before running it: the script downloads build tools and dependencies, installs plugins, and replaces existing copies. If macOS or your DAW blocks a bundle, stop and read the exact warning. Do not lower system-wide security settings, disable Gatekeeper, or change your Mac's startup security policy to load Grain Field. See the downloaded-build guidance in section 10.

If the build reports an **Xcode license** error, review and accept Apple's applicable developer-tools terms through Xcode or Apple's documented command-line process, then retry. Do not run administrator commands or delete build files solely because a generic troubleshooting instruction says to.

When it says **Done**, the plugin is installed here:

| Format | Location | Used by |
| --- | --- | --- |
| Audio Unit | `~/Library/Audio/Plug-Ins/Components/Grain Field.component` | Logic Pro, GarageBand |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/Grain Field.vst3` | Ableton Live |
| Standalone app | `build/GrainField_artefacts/Release/Standalone/Grain Field.app` | No DAW needed |

**Coming from 0.6:** the parts are now **scenes** with no timeline: their lengths and **play through** are gone, so nothing moves on by itself. Your projects open with every part as a scene, in the same order. If you relied on play through, automate the **Scene** parameter in your DAW instead (section 4). **Coming from 0.5:** projects open exactly as they were. Every layer starts on the root, which is the old key, so nothing moves until you ask it to (section 4, *Notes in layers*). **Coming from 0.4:** projects open with their parts, chords and sounds. Tape and space are now set per module, so those start at their defaults. **Coming from 0.3 or earlier:** old projects open as a new, empty piece.

## 2. Open it

**Logic Pro:** create a new Software Instrument track, click the instrument slot, then choose **AU Instruments > Grain Field Audio > Grain Field**. Pick **Stereo** for now; **Multi Output** is for section 7.

**Ableton Live:** open Settings > Plug-ins, turn on **Use VST3 Plug-in System Folders**, and click Rescan. Then find **Grain Field** under Plug-ins > VST3 and drag it onto a MIDI track.

It opens **empty and stopped**: no sounds, nothing playing. Pick your sounds in the texture and drones modules (or a whole piece with **‹ ›** at the top), then press the round **play** button. Stop fades everything out to silence over a second and a half; play carries on from where it was. Turn on **sync** (right of the transport bar) to play only while your song plays, in its tempo. A saved project also opens stopped, unless sync is on.

## 3. Reading the screen

The screen is a rack of modules that fill the window. Under the header:

- **The transport bar:** play, the clock, record and loop on the left; the **chords** across the middle; then **line** and **dissolve**, the **root**, the **tempo** and **sync**.
- **The scenes:** whole sounds you've kept, as a row you click to blend between, with the **blend** time and your last **take** on the right.
- **Three modules side by side:** **texture → drones → output**.

Each module has its **picture** on top (what it's doing right now), its **sounds** under that (with their waveforms and notes), and its **knobs** along the bottom. The texture melts into the drones as far as **merge** says (the arrow between their pictures gets brighter with it), and both go on to the output.

### Texture

Clouds of grains, cut from up to three sounds.

- **Where it plays.** Beside **texture**, at the top of the module, is the note the texture plays on: the root, or a step away from it (a fifth or a fourth above, an octave up or down). Each sound has its own note too, in the small column after its name: it **follows** the texture, takes a step of its own, or keeps its **own** pitch. Click either to change it; see *Notes in layers* in section 4.
- **The sounds.** Click a name to change it. Beside each name is its **waveform**, and on the waveform a lit **window**: the stretch of the sound the grains are being cut from. Every grain flashes at the spot it was taken from. **Drag the window** to cut from somewhere else; hold Shift and drag to move all three together; double-click to put it back. **smear** makes the window wider, so the grains roam further. The slider at the end of the row is that sound's **amount** in the cloud.
- **The picture.** Each grain is a soft light: left to right is where you hear it (so **spread** widens the cloud), height is its note, bigger lights are longer grains, and reversed grains swell in instead of fading out.
- **tape** and **space** are the texture's own. Tape is one knob from a gentle waver to a worn machine (the cloud sways with it, and dims when the tape drops out). Space is how far back in the room the texture sits (the lights get wider).
- **resample** (top of the module) records the last 8 seconds of everything you hear and adds it to the texture as a sound of its own, so the piece can feed on itself. It goes into the first empty sound, or replaces the one you use least, and shows up in orange.

### Drones

A slow body holding the chord.

- **from** is the sound the drones grow from; **under** is the bed, a sound that plays underneath everything. Their waveforms show an orange line where each is being read right now. **bed** on the third row is the bed's level, with its own solo and mute.
- **Where they play.** The note after **from** is where the drones play, and the note after **under** is where the bed plays, each counted from the root. The bed can also keep its **own** pitch.
- **The picture.** Each line is a voice at its pitch, brighter when louder, with its note on the right. The **orange line** is the melody. When the chord or root changes you see the old notes fade while the new ones come up beside them. The glow along the bottom is the bed.
- **merge** is how far the texture and the bed melt into the drones. Low, you hear layers side by side. High, the texture rings at the chord's notes and is taken up into the drones, so you hear one sound being reshaped.
- **tape** and **space** are the drones' own, separate from the texture's. Because each module has its own tape, the two never wobble in step, which is a large part of why they now sit inside each other instead of one lying on top of the other.

### Output

What happens to everything on its way out: **drive** (saturation, up to distortion), **echo** (a tape echo on the texture, in time), **room** (how big the room is that both modules are sent into, from small to vast) and **volume**. In the picture, the core is how loud it is and turns warmer with drive, the rings are the room, and the dots trailing off are the echo. **inside / outside** is for using your own reverb (section 7).

### Everywhere

**solo** and **mute** sit next to texture, drones and the bed. Solo one to hear only what it makes. Grainy movement comes from the texture, the slow wall from the drones, the room tone from the bed. Crackle comes from **detail** and from **tape**.

**root sound** (top left, beside the name) loads one recording into every module at once; see section 6. Right after it is the **root note**, the note the whole piece is counted from. The arrows step it a semitone; click the note for the list.

**Window size.** Drag the bottom-right corner to any shape: a wider window stretches the modules sideways, a taller one gives the pictures more room. **size** (top right) makes everything bigger or smaller, from 70% to 200%. The size is kept with your project.

Hover over anything to see what it does in the footer. **Double-click a knob to put it back where it starts.** Hold Shift while dragging for fine changes. Knobs in **orange** are the ones Swell moves. When the sound is somewhere other than where a knob points (because of Swell, or a blend between scenes) a small hollow mark on the ring shows where it really is.

## 4. Composing a piece

### Scenes

A scene is a whole sound kept as it is: its sounds, notes, chords and every knob. The row under the transport bar holds them; the one you're in is orange.

- **Click a scene to blend into it.** It fills from the left as it comes in, over the **blend** time on the right (click it: from a cut to three minutes). Nothing new is made, and nothing moves on by itself.
- **What you change is kept with the scene you're in.** Leave it and come back, and it's as you left it.
- **+ keep this as a scene** keeps everything you hear as a new scene and puts you in it, so you can carry on changing it while the one you left stays as it was.
- **Double-click** a scene to rename it. **Right-click** to rename or remove it.
- The small dots on the Swell line show where Swell sits in your other scenes.

**There is no timeline.** Grain Field doesn't play a piece through by itself; your DAW does that. Its **Scene** parameter is automatable: draw it on an automation lane (the parameter is called *Scene*; 1 is the first scene) and Grain Field blends into that scene at that point of the song, over the blend time. The same goes for **Swell**, the **root** and every **note**. A bounce of your song then comes out the same every time.

### Playing

- **play** starts and stops. Stop fades everything out over a second and a half; play carries on where it was.
- The **clock** beside it is how long Grain Field has been playing. Click it to go back to the first chord, with the clock at zero (the scene you're in stays).
- **loop** (the two arrows): lit, the chords go round; dim, each chord stays until you queue the next (see *Harmony* below).
- **tempo** is your song's when Grain Field is in a DAW. In the standalone app, click it to set it.
- **sync** plays only while your song plays.

### Recording a session

The ring beside the clock is **record**. Click it and everything you hear is written to a file, your knob moves and chord changes included, until you click it again. If the piece was stopped, recording starts it. With **sync** on, the recording waits and starts when your song does.

When you stop, the take appears at the right end of the scenes row as **take 2:31**. **Drag it onto an audio track** in Logic or Live and it's a region like any other. Click it instead to show the file, or to make the take the **root sound** of a new layer. Takes are 24-bit WAV files kept in `Music/Grain Field/Recordings`.

Each take is stamped with the place in your song where it began. In Logic, drop it anywhere, select the region and choose **Edit > Move > To Recorded Position** to put it back exactly there.

(Your DAW's own bounce also works, and with sync on and your scene changes drawn as automation it comes out the same every time. Record is for capturing a performance as you play it.)

### Swell

Swell is the long orange line at the bottom: **quiet** on the left, **wall** on the right. It moves density, body, sub, drive and both spaces together, so one gesture takes a piece from a few grains in a room to a dense, saturated mass.

- **Drag the handle** to move it yourself.
- **Click a point on the line** and Swell travels there by itself, over the time shown as **fade** on the right (click it: 5 seconds to 5 minutes). You see where it's heading and how long is left. Click **quiet** or **wall** under the line to go all the way. Take the handle at any moment and it's yours again.
- The small dots on the line show where Swell sits in your other scenes.

**freeze** holds the texture where it is while the drones keep breathing.

### Harmony: chord slots

Eight slots, across the middle of the transport bar. The texture and drones are tuned to whichever is playing (orange frame, with a line along its bottom showing how far into it you are). Each slot shows its chord, the notes as they will sound (the lowest first, with its octave, such as **e2**), and how many bars it lasts.

- **Set up a chord:** click the empty slot with **+**. Pick its **note**, its **kind** (fifth, open, minor, major, sus, cluster), how many **bars** it lasts, and its **octave**.
- **Octave** is how high the chord sits: **-1** is darker, **0** is where it would sit by itself, **+1** and **+2** are brighter. A chord moved from 0 shows it beside its name, so you can tell before you get there whether the next chord will be dark or bright.
- **Queue a chord:** click it. It's marked **next** and comes in when the playing chord's bars are done. Click it again and it's marked **at bar**: it comes in at the next bar line instead. A third click un-queues it.
- **now** (on hover, bottom of the slot): go to that chord right away.
- **Change a chord:** hover a slot and click the small pencil (or click the one that's playing). **remove** is in the same pop-up.
- **Reorder:** drag a slot left or right.
- **loop** (the two arrows left of the chords): what happens when a chord's bars are done and nothing is queued. Dim, the chord **stays** until you queue another. Lit, it moves on to the next chord, round and round. The bars follow your song's tempo.
- **In or out of the loop:** with loop on, each slot has a small dot at its top right. Click it to take the chord out of the loop. Chords that are out only play when you queue them.
- The chords are counted from the **root** (shown as **on d** above them, and set with **root ‹ d ›** to their right). Moving the root moves every chord together; in its list, the two roots a fifth away are marked **close**, because they share most of their notes and are the gentlest places to go.
- **dissolve** is how long one chord or root takes to melt into the next, from a third of a second to twenty. **line** sets how present the melody is.

**With a MIDI keyboard.** You never need one, but if one is connected (and Grain Field's track is selected, so your DAW sends it the keys), playing sets the harmony:

- **One note** moves the chord to that root and keeps its kind. So with a minor chord playing, an A gives you A minor.
- **A chord** is recognised: E G B is e minor, D G A is d sus, two notes a fifth apart is a fifth, notes right beside each other a cluster. It doesn't matter which note is at the bottom.
- **Where you play sets how high it sits:** low on the keyboard is dark, high is bright, the same as a slot's octave.
- The chord you played shows in the harmony header as **keys**, and holds after you lift your hands. **keep** makes it a slot of its own, so playing the keyboard is also a way to find a progression. **let go** returns to the lit slot. It also lets go when the loop moves on, when you pick a slot, or when you change scene.
- Record the MIDI on Grain Field's track and your DAW plays the harmony back for you.

Nothing ever slides in pitch. When a chord or the root changes, each voice's new note comes up beside its old one, which keeps ringing as it fades: a true crossfade, one voice after another, and the texture's tuning crosses over the same way. Notes two chords share simply keep sounding.

**Whatever sound you add follows where its layer plays.** A sound with a clear note of its own (it's shown in orange by the name) is moved by the smallest step to fit; this includes the bed. A sound with no clear note is tuned by the chord's filter. A sound set to its **own** pitch is left exactly as recorded.

### Notes in layers

The notes of a piece come in three levels, each counted from the one above it:

| Level | Where you set it | What it moves |
| --- | --- | --- |
| **Root** | **root ‹ d ›** in the transport bar | everything that follows it, the chords included |
| **Texture, drones, bed** | beside **texture**; after **from** and **under** | that module and its sounds |
| **Each texture sound** | the small column after its name | just that sound |

Each level can sit **on** the one above (for a texture sound: **follows**), a **fifth** or a **fourth above** it, or an **octave up** or **down**. The texture sounds and the bed can also keep their **own pitch**: left as recorded, untuned, and kept out of the texture's tuned resonators.

Because each level is counted from the one above, moving the root moves the whole shape of the piece and keeps every interval. With the root on d, the texture a fifth above (a) and the drones an octave down, moving the root to f puts the texture on c and the drones on a low f. Sounds at their own pitch stay where they are.

A change at a higher level is a bigger event. Moving the root, or the drones' note, melts every voice over the **dissolve** time, the same way a chord change does. The texture's note crossfades its resonators the same way. A single sound's note is quickest: its next grains simply arrive on the new note.

The notes are saved in each scene, so a scene can have its own layering, and your DAW can automate them (they're listed as *Texture note*, *Sound 1 note* and so on).

A texture sound a fifth above plays the chord as if it were built a fifth higher, so over a rich chord the layers can rub against each other. Over **fifth** and **open** chords they blend most easily.

## 5. Pieces (presets)

The name at the top is the whole piece: its sounds, chords and all its scenes. Use **‹ ›** to step through them, or click the name to pick one, **save this piece**, or open the presets folder.

| Piece | What it does |
| --- | --- |
| arc | Five scenes in D: opening, root, build, peak, wind down |
| haze | Long, smeared glass over wind, in A, slowly thawing |
| night rain | Rain under a low hum, in E minor, then a flood |
| wall | One dense, driven mass in G |

Your pieces are saved in `Music/Grain Field/Presets` and appear under **yours**.

## 6. Sounds

Click any sound's name in the texture or drones module to choose another. The built-in sounds are made to blend: instruments stretched until they're more texture than instrument.

- **textures:** glass haze, tape choir, dust halo
- **sustained** (best for the drones): bowed air, low hum, glass drone
- **places** (best under all): low room, ocean drum, rain, wind

**Your own sounds.** Put audio files (WAV, AIFF, FLAC, MP3, Ogg) in `Music/Grain Field/Sounds` and they appear under **yours**. **open the sounds folder** takes you there. **load a sound…** picks a file from anywhere. Field recordings make good beds; anything bowed, sung or blown makes good drones. Grain Field uses up to the first two minutes of a file.

**record the last 8 seconds** (in a sound's menu) puts what you just heard into that slot; **resample** in the texture module does the same and picks the slot for you. Recordings are saved as WAV files in `Music/Grain Field/Captures`.

### A field recording as the root of everything

Click **root sound** at the top left, beside the name, and load a recording. It becomes every source at once (all three textures, the drones and the bed), and the root moves to its **core note**: the pitch it keeps coming back to (a hum, wind in wires, a fridge, a voice). From there everything is that one recording, granulated, merged into the drones and tuned to the chords; swap any single slot afterwards if you like.

When you load any recording of your own, Grain Field listens for its core note and shows it in orange next to the name. Its grains are moved only by the smallest step to fit the chord, so it keeps its character.

**Changing a sound's note.** The note beside the name is the note the sound is tuned *from*, like the root key of a sample in a sampler. If listening got it wrong, or found nothing (then it says **note** in grey), click it and pick the note the sound really is. From then on the sound is shifted from that note to fit each chord. **as found by listening** goes back; **no note** leaves the sound at its own pitch, with only the chord's filter tuning it. An underline shows a note you set yourself. The note belongs to the sound, so each of your sounds has its own, the same sound has the same note in every slot, and it's saved with the piece. The built-in sounds are already mapped note by note, so they have nothing to set. Click the name for **tune to this sound** (the root becomes its note) and **make a piece from this sound** (five scenes, from mostly the raw recording to a peak and back, all grown from it).

### Another track as the source (side chain)

Grain Field can listen to another track while it plays. Pick **the track sent in (side chain)** for a slot, or as the root sound. Its grains are cut from the last few seconds of what comes in, and the drones resonate it, so a second piece can be built out of the first one as it plays.

The sound's name tells you what is happening:

- **no track connected**: your DAW isn't sending anything. Choose the track as described below.
- **connected, silent**: a track is chosen but nothing is coming from it. Audio only arrives while your song is playing and that track has sound.
- **the track sent in**, with a light that follows its level: it's working.

- **Logic Pro:** at the top right of Grain Field's own window, open the **Side Chain** menu and pick the track to listen to. If the track you want isn't in the list (some versions only list audio tracks and buses), send that track to a bus and pick the bus.
- **Ableton Live:** open the device's **Sidechain** section (the triangle at the device's top left, if your version shows it for instruments) and choose the track under **Audio From**. If Live doesn't offer a sidechain for instruments, record or bounce the first track and load the file instead.

## 7. Using your own reverbs and delays

Grain Field has four stereo outputs:

| Output | What comes out |
| --- | --- |
| **Main** | Everything, mixed |
| **Texture** | The texture alone, after its tape, without the room |
| **Drones** | The drones and the bed alone, after their tape, without the room |
| **Send** | What would go into the room and the echo |

To use your own effects, switch **inside** to **outside** (in the output module). The built-in space and echo come off the main output, and **Send** carries what would have gone into them. Put your reverb or delay on that output at 100% wet.

**Ableton Live:** create a new audio track. In its **Audio From** menu choose the Grain Field track, then in the menu below it choose **Send** (or Texture, or Drones). Set Monitor to **In** and put your effect on that track.

**Logic Pro:** load Grain Field as **Multi Output (Stereo)**. In the Mixer, click the small **+** at the bottom of Grain Field's channel strip to add channels for the other outputs. Put your effect on the Send channel.

## 8. Where things are saved

- The whole piece (scenes, chords, which sound each slot uses, which scene you're in) is saved inside your Logic or Ableton project.
- Sounds you load are referenced by their file location. If you move or delete the file, the slot falls back to a built-in sound.

## 9. If something goes wrong

- **Logic doesn't list it, or shows the old version:** open Logic > Settings > Plug-in Manager, select Grain Field and click **Reset & Rescan Selection**.
- **Ableton doesn't list it:** check that VST3 system folders are on, then click Rescan.
- **It's silent:** check that the round button shows two bars. If sync is on, Grain Field only plays while your song plays.
- **macOS or your DAW blocks a downloaded bundle:** follow the source and checksum checks in section 10. Do not bypass a malware, damaged-file, or revoked-signature warning; get a fresh copy from the official release or ask the maintainer for help.
- **The build stops with an error:** read the error before retrying. If it still fails, share the relevant final lines from Terminal after removing tokens, personal paths, and other private information.

## 10. Build in the cloud instead (optional)

The included workflow (`.github/workflows/build.yml`) builds the Mac plugins on GitHub's computers each time you push. Download the ZIP from the [official v0.7.1 release](https://github.com/wallstrom/grain-field/releases/tag/v0.7.1), or **Grain Field macOS** from a successful [Actions run](https://github.com/wallstrom/grain-field/actions). Access to source and downloads depends on repository visibility and permissions; a GitHub sign-in may be required for Actions artifacts.

### Verify and install a download

1. Confirm that the download came from `wallstrom/grain-field` on GitHub, not an unknown mirror.
2. For a release ZIP, compare its SHA-256 with the value in that release's notes. Run `shasum -a 256 "Grain-Field-v0.7.1-macOS-universal.zip"` from the download directory. If it differs, do not install the ZIP. A matching checksum confirms the published file's integrity, not that the software is malware-free or signed.
3. Extract the ZIP, quit your DAW, and copy `Grain Field.component` and `Grain Field.vst3` into the user Plug-Ins folders listed in section 1. Copy `Grain Field.app` to your preferred applications folder. Back up existing copies if you need to retain an older build, and keep the included license and notices with redistributed copies.

### macOS security warnings

These downloads are not verified Developer ID-signed or notarized. macOS may block the app or plugins, and successful CI builds do not establish that they are safe to run.

For the **standalone app**, if you trust the source and have verified the download, follow [Apple's guidance for safely opening apps](https://support.apple.com/en-us/102445). Where available, **Open Anyway** in System Settings > Privacy & Security approves that specific app after an attempted launch. The settings and wording vary by macOS version; managed Macs may not permit an exception.

That app-specific process is not a guaranteed fix for **AU or VST3 plugins**. If your DAW blocks a plugin, consult the DAW's documentation or ask the maintainer for a signed/notarized build. Building reviewed source locally is another option, but does not guarantee security approval or working DAW integration.

Do not disable Gatekeeper, lower startup security, or recursively remove quarantine attributes as a routine installation step. If macOS reports malware, a damaged file, or revoked authorization, do not override the warning; stop and contact the maintainer or your IT administrator.

## 11. Licence

Grain Field's original code is licensed under the GNU Affero General Public License, version 3 only (see `LICENSE` at the project root). When distributing binaries, comply with its corresponding-source and notice requirements. JUCE 8 is separately dual-licensed under AGPLv3 or a commercial JUCE licence; see juce.com/get-juce. A commercial JUCE licence does not remove Grain Field's own AGPL obligations.

The interface uses DM Mono and Instrument Serif, both under the SIL Open Font License (see `Resources/Fonts`). The built-in sounds start from the Versilian Community Sample Library, released as CC0 (see `Resources/Sounds/CREDITS.txt`).

To change the company name shown in your DAW, edit `COMPANY_NAME` in `CMakeLists.txt` and rebuild.

---

## For developers

| Path | What it is |
| --- | --- |
| `Source/Params.*` | Every parameter: IDs, ranges, defaults, hints, what Swell moves, chords and progressions |
| `Source/SourceLibrary.*` | Built-in sounds (Ogg, decoded once and shared), the Sounds folder, file loading, captures |
| `Source/BuiltinSounds.inc`, `Tools/` | The sound catalogue and note maps, and the scripts that build them |
| `Source/Engine.*` | The real-time engine: tuned grain clouds read from a window per sound, spectral smear into comb resonator voices that crossfade between notes, the melody line, sediment, Swell, a tape per module, drive, echo, convolution reverb, dynamics, blends between scenes, the four outputs |
| `Source/Scenes.*` | Part storage and the factory pieces |
| `Source/PluginProcessor.*` | Host glue, buses, MIDI in, scenes (blending into one, keeping a new one, automation of the Scene parameter), resample, Swell fades, the recorder, presets, saved state (version 4) |
| `Source/PluginEditor.*`, `Source/UI/*` | The interface: theme and fonts, controls, the piece, harmony, the sounds with their waveforms, and each module's picture |
| `Tests/RenderTest.cpp` | Offline checks: levels, CPU, moving between scenes, going back, scenes staying put (with a song playing too), key changes as crossfades, chord octaves, grain windows, the tuned bed, notes in layers (texture, each sound, drones, bed), Swell fades, state restore, outputs, and a demo render |
| `Tests/Snapshot.cpp` | Saves PNGs of the interface while audio runs, and tries the controls with hand-made mouse events |

The audio thread never allocates or locks. Grains come from a fixed pool of 448. Parts are blended on the audio thread so automation and offline bounces are sample-accurate; the parameters catch up afterwards on the message thread.

### Rebuilding the bundled sounds

Normal plugin builds use the checked-in Ogg files; these steps are only for regenerating them. Install Python 3 with NumPy and SciPy, plus `ffmpeg` on your PATH. Clone `https://github.com/sgossner/VCSL` into `.tools/VCSL`, or pass `--vcsl /path/to/VCSL` to either tool.

Run `python3 Tools/select_samples.py` to regenerate `Tools/selection.json`, then `python3 Tools/build_textures.py` to rebuild the current textures and `Source/BuiltinSounds.inc`. The older `Tools/build_sounds.py` builds the legacy instrument catalogue instead; it is not a prerequisite for building textures.

Default paths are resolved relative to the scripts' repository, so the tools also work when called from another directory. Explicit relative CLI paths are relative to your current directory. Both builders accept `--selection`, `--output`, `--include`, and `--work-dir`; scratch audio and `catalogue.json` default to the ignored `build/soundbuild` directory. Rebuilding overwrites generated files, but does not delete unrelated Ogg files. Run `python3 -m unittest discover -s Tests -p 'test_sound_tools.py'` for the lightweight path and packaging checks.

## Running the functional tests

With CMake available, run these commands from the project root:

```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DGF_BUILD_TESTS=ON
cmake --build build --config Release --target GrainFieldRenderTest GrainFieldSnapshot -j 4
mkdir -p build/functional-test/audio build/functional-test/ui
build/GrainFieldRenderTest_artefacts/Release/GrainFieldRenderTest build/functional-test/audio quick
build/GrainFieldSnapshot_artefacts/Release/GrainFieldSnapshot.app/Contents/MacOS/GrainFieldSnapshot build/functional-test/ui
```

If the build script downloaded CMake, use `.tools/cmake-3.30.5-macos-universal/CMake.app/Contents/bin/cmake` instead of `cmake`. Omit `quick` for the longer audio run, including demo and listening-guide renders. Use a fresh output directory to retain previous results. The tests write media files and may also create a resample capture under `~/Music/Grain Field/Captures`.
