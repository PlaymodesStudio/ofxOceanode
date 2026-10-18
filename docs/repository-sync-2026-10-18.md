# Addon repository cleanup — 2026-10-18

## Result and scope

The Oct 16–18 mixer and Macro UI work has been committed by feature. The older
Body Repulsion and MetaballsV3 refinements were included at the user’s request.
The active implementation is MacroMixer. The six PresetMixer archive branches
remain unchanged; no PresetMixer implementation remains in the active source.

The local development branches are checked out. In the six migration repositories,
`codex/macro-mixer` points to the same commit as the corresponding development
branch. The redundant local `feature_PRESET_MIXER` branches in Oceanode and
Collection were removed after verifying that their tips are ancestors of the
archive snapshots. No remote branches were deleted.

Nothing was pushed. The user will publish the prepared commits.

## Repository overview

| Repository | Local development branch | Feature work | Pending commits |
| --- | --- | --- | --- |
| ofxOceanode | `megabranch26_A` | MacroMixer, native Macro dialogs/titles, Noise smoothing, initialization fixes and regressions | 11 including this report |
| ofxOceanodeCollection | `megabranch26_A` | No remaining mixer code changes; baseline already current | 0 |
| ofxOceanodeTextures | `megabranch26_A` | Texture Receiver lifecycle; RGBA, vector upload and metadata fixes | 2 |
| ofxOceanodeSuperCollider | `megabranch26_A` | ScBus mixing, sync fences, stable buses and synth teardown | 3 |
| ofxOceanodeVectorGraphics | `master` | Body Repulsion; Fatline mixer policies | 2 |
| ofxOceanodeLaser | `2026` | Explicit MacroMixer output/simulator exclusions | 1 |
| ofxEloiNodes | `main` | MetaballsV3 mode controls and contour refinements | 2 against cached origin/main; remote unverified |

EloiNodes’ pending list includes the existing `9cc50f2` MetaballsV3 feature commit
and the new refinements. Collection requires no new feature commit.

## Remote integration

Fetched `origin` in all six migration repositories. Oceanode integrated four
remote development commits through `102732ba`: named model refresh, timeline
Play on preset load, Save As layout preservation, and Help/Window menus with the
assistant. SuperCollider integrated eleven development commits through `4ff2d49`,
including server/graph fixes, SC Code changes, MetroBox repeat, AnalogBox,
EuclideanBox, ArpeggiatorBox, modulation, analysis/recorder fixes and plugin binaries.
The other four development branches had no new remote commits.

Oceanode’s registry merge preserves both Help descriptions and mixer capabilities.
Named factory refresh avoids constructing ordinary expensive models; declared
mixer blockers are still cached. Tests cover retaining and clearing those blockers.

EloiNodes fetch failed because this shell lacks GitHub credentials; SSH access
also failed. Its source was committed locally, but its current remote state remains
unverified until authenticated access is restored. No authentication settings or remote URLs were changed.

## Feature commits

### ofxOceanode

- `a39a4d91` — Smooth Noise Scale changes while keeping Seed stable
- `9d48865d` — Initialize generator and router state before setup callbacks
- `1d7b9033` — Rework Macro load and save dialogs with independent local imports
- `30b8bfc8` — Show the current Macro name in the node title
- `9a3899f4` — Add native MacroMixer with typed transitions and addon adapters
- `8e229fd8` — Cover Macro dialogs and local import persistence
- `915a82ec` — Merge current development menus and timeline with MacroMixer capabilities
- `b19ffaa3` — Verify Help registry and MacroMixer integration after merging development
- `34e1b368` — Add regressions for canvas navigation, node selection and scope docking
- `917fa4a4` — Add clipboard and Macro snapshot vector persistence regressions

### ofxOceanodeCollection

No new commits required.

### ofxOceanodeTextures

- `c00bb34` — Make texture receiver lifecycle safe for independent Macro instances
- `fa588cf` — Preserve texture RGBA and guard vector uploads and metadata

### ofxOceanodeSuperCollider

- `ba1a361` — Release synth resources when retiring Macro instances
- `836a1bd` — Add MacroMixer ScBus bridges with stable allocation and sync fences
- `10cab98` — Merge remote-tracking branch 'origin/megabranch26_A' into codex/macro-mixer

### ofxOceanodeVectorGraphics

- `80a6997` — Add Body Repulsion with per-point spring and force controls
- `17e4a41` — Register Fatline opacity and switch policies for MacroMixer

### ofxOceanodeLaser

- `7afc315` — Declare laser output and simulator exclusions for MacroMixer

### ofxEloiNodes

- `9cc50f2` — Add MetaballsV3 with signed fields, smooth unions and deformation
- `5d1cc5f` — Refine MetaballsV3 mode controls and contour extraction

The cleanup documentation is recorded in the commit containing this report.

## Preserved PresetMixer snapshots

| Repository | Archive tip |
| --- | --- |
| ofxOceanode | `71ec24fe` |
| ofxOceanodeCollection | `48a8488` |
| ofxOceanodeTextures | `2fa9b36` |
| ofxOceanodeSuperCollider | `300245b` |
| ofxOceanodeVectorGraphics | `2c4088c` |
| ofxOceanodeLaser | `7e8c69d` |

All six snapshots retain the name `codex/archive-preset-mixer-2026-10-17`.
They are local until pushed; the commands below publish them explicitly.

## Validation

Completed during this cleanup:

- MacroMixer lifecycle, selected/extra routers, nested Macros, feedback, exclusions,
  transitions, router group files, removal cleanup, persistence and independent copies.
- Numeric/Boolean policies, Fatline opacity/Switch, real CGL texture pixels and
  captured SuperCollider server graph traversal/routing.
- Actual offline SuperCollider audio render of the MacroMixer SynthDefs.
- Macro dialogs, independent local import/save, library loading and snapshots.
- Noise smoothing; clipboard values; snapshot vectors; canvas navigation; node
  selection; scope docking; Help catalog and named-registration compatibility.
- Body Repulsion physics and real OF parameter/events adapter.
- MetaballsV3 mode controls and contour extraction under ASan/UBSan.
- All 69 Oceanode C++ translation units with MIDI and OSC enabled.
- Addon registration syntax for Collection, Textures, VectorGraphics, SuperCollider
  and Laser; production SC bridge, node, Output, SynthDef and server manager syntax.

The core MacroMixer, ScBus and Macro dialog regressions were rerun successfully
after merging the remote changes. The other policy/backend checks passed before
the merge; the corresponding production files were unchanged by it. Some older
test fixtures needed updates for current parameter forwarding and Help APIs.

The host application has not been rebuilt or run. Regenerate its project to add
the new MacroMixer `.cpp` files and remove old PresetMixer entries; copy the updated
SuperCollider data. Windowed UI, live Syphon, physical DAC/MIDI and live audio
remain host acceptance checks.

## Local files left outside feature commits

Unrelated working files remain in place: Oceanode `output/`; Textures Finder
metadata; Collection/VectorGraphics Finder metadata; Laser `.roo/`, `AGENTS.md`,
`plans/`, the prompt document and Finder metadata; EloiNodes `.roo/`, `examples/`,
the editor workspace and the skull-illusion document. These were not included in
feature commits or removed.

Older changes in other addons (including ofxCv, ofxGStreamer, ofxImGui, ofxMidi,
ofxOceanodeGraaf, ofxOceanodeLights, ofxOceanodeNDI, ofxOceanodePonk,
ofxSonyCameraRemote, ofxSunCalc and ofxTensorFlow2) fall outside the reviewed
recent mixer work and were left untouched. The modified ofxLaser submodule was
also left untouched. Source timestamps and recent chat/history evidence were
used to distinguish those older changes.

## Push commands

Run from the addons directory after restoring GitHub authentication. Each command
publishes only the explicitly named branches. No force push is required. If a
remote advanced after the fetch, reconcile that update before retrying.

```sh
git -C ofxOceanode push --atomic -u origin megabranch26_A codex/macro-mixer codex/archive-preset-mixer-2026-10-17
git -C ofxOceanodeCollection push --atomic -u origin megabranch26_A codex/macro-mixer codex/archive-preset-mixer-2026-10-17
git -C ofxOceanodeTextures push --atomic -u origin megabranch26_A codex/macro-mixer codex/archive-preset-mixer-2026-10-17
git -C ofxOceanodeSuperCollider push --atomic -u origin megabranch26_A codex/macro-mixer codex/archive-preset-mixer-2026-10-17
git -C ofxOceanodeVectorGraphics push --atomic -u origin master codex/macro-mixer codex/archive-preset-mixer-2026-10-17
git -C ofxOceanodeLaser push --atomic -u origin 2026 codex/macro-mixer codex/archive-preset-mixer-2026-10-17
```

EloiNodes needs a fresh authenticated fetch and remote comparison first:

```sh
git -C ofxEloiNodes fetch origin
git -C ofxEloiNodes log --left-right --oneline origin/main...main
```

If that comparison contains remote-only commits, integrate them before publishing.
Then:

```sh
git -C ofxEloiNodes push origin main
```
