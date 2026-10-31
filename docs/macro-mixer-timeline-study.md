# MacroMixer and nested timelines: feasibility study

Status: study, no code changed. Written against `megabranch26_A` (core) and
ofxOceanodeTimeline at `3c73eb7` on 2026-10-30.

Question: with per-macro timelines (a macro owns a `sequence.json`, the parent
canvas places it through macro tracks), what happens when MacroMixer mixes or
morphs two sources that each carry a timeline? Is it a problem, and what would
it take to support it?

## Short answer

- **It is not an incompatibility today, it is a silent no-op.** A source's
  timeline is ignored when the mixer loads it. Nothing crashes and nothing
  half-plays: the nodes run, the routers mix or morph, and the automation that
  the source relied on simply does not happen inside the mixer. Both Mix and
  Morph behave this way.
- **Mix mode and the Morph fallback (two graphs, output crossfade) are
  straightforward to support.** Each graph is a complete, independent macro
  instance with its own container. Giving each one its own timeline is a
  resolution/placement problem, not a conceptual one.
- **In-place Morph (one graph, controls interpolated) is the hard case.** Two
  timelines that drive the same live nodes have to be blended, and the
  timeline currently has no notion of a weighted blend between two sequences.
  It is feasible, but it needs a timeline-side feature plus a node-key remap on
  the mixer side.
- **A parent timeline driving the mixer already works for numeric controls**
  (Target, Mode, Duration_ms, Bi Pow), but **not for Start!/Stop!** because
  they are `ofParameter<void>` and the timeline has no void lane type.

## How the pieces work today

### Nested timelines (ofxOceanodeTimeline)

- Every macro can own one `ofxOceanodeTimelineSequence`
  (`Timeline/ofxOceanodeTimelineMacroSequences.cpp`). Binding paths are
  relative to the macro's container (`Node_1/Param`, or
  `Macro_2::Node_1/Param` one level further in). It is saved as
  `sequence.json` in the macro's folder through the core hooks
  `loadMacroPreset` / `saveMacroPreset` on `ofxOceanodeAutomationExtension`,
  called from `ofxOceanodeContainer::loadPreset/savePreset` when the container
  has a `timelineHost` (`ofxOceanodeContainer.cpp:570-572`, `1423-1425`).
- The hook resolves the macro with `owningMacro(*container, macroContainer)`,
  a recursive walk of the **root** container's nodes. A container that is not
  reachable from the root through `ofxOceanodeNodeMacro` nodes gets no
  sequence. The code says so explicitly:
  `if(macro == nullptr) return; // not a macro's container (e.g. a mixer graph)`
  (`ofxOceanodeTimelineMacroSequences.cpp:84`).
- Playback: a macro timeline only plays when a **macro track** in the parent
  sequence places it with macro clips (start, length, repeat, stretch, offset,
  reversed). `collectMacroSources()` walks macro tracks breadth-first from the
  main sequence, `rebuildPlayedMacros()` makes a **copy** of each reached
  sequence with paths rewritten to **absolute** canvas ids, and
  `playedSequences()` maps each clip repeat onto main-timeline beats.
  Evaluation and `applyAutomation()` then resolve every path from the root with
  `container->findTimelineParameter(path)` (`ofxOceanodeTimeline.cpp:4376`).
- Per frame, `updateMacroGating()` writes the macro node's `Active` (when the
  macro track is gated) and `Snapshot` (per clip) through
  `macroNodeParameter()`, which again finds the macro from the root.
- Tempo, loop, markers, seek and the playhead belong to the main timeline
  only. A macro sequence is tracks + clip groups, nothing else.
- Copies of a **library** macro share one sequence (keyed by canonical
  folder). A **local** macro has its own.

### MacroMixer (core, `src/Nodes/MacroMixer*`, `ofxOceanodeMacroMixer.*`)

- A source is a folder with `modules.json`, `connections.json` and node
  settings. In practice Eloi's test bed uses **root preset folders** as
  sources (`Presets/BLOOBS_DEV/1--BLODEV_1`, `2--BLODEV_2_YINGYANG`, …), which
  carry `timeline.json`, not `sequence.json`.
- `MacroMixerSource::instantiate` builds a private host container per graph
  with canvas id `MacroMixer/<n>`, sets `setTimelineHost(&parent)` so the
  graph shares the root's automation extension, creates one ordinary
  `ofxOceanodeNodeMacro` inside it and calls `loadMacroSource(folder)`
  (`MacroMixerSource.cpp:240-246`). The host is **not a node of any canvas**,
  so nothing that walks from the root can see it.
- Mix: `current` and `next` graphs both run, each router output is combined by
  its policy over the fade. Morph fallback: same, plus the destination's
  controls start at the origin's captured values.
- Morph in place: one graph. `prepareMorphInPlace` matches saved nodes to live
  nodes (`matchMorphNodes`, `alignMorphDefinition`, which rewrites the target
  definition in **live node keys** such as `Oscillator_1`), stages added or
  replaced nodes, plans blends for changed cables and Portal buses, and
  `applyMorph(progress)` interpolates every matched editable control each
  frame (`MacroMixerSource.cpp:1450-1540`). `morphEditable` excludes cabled,
  read-only, transient and Skip/Structural parameters but **does not exclude
  timelined ones** (there is no timeline inside a mixer today, so there was
  nothing to exclude).
- The graph's update is driven by the mixer: `Graph::update()` calls
  `macro->update()`; the host container's own `update()` is never used, so the
  graph gets no automation pass of its own.

## What happens today when a source has a timeline

Tracing the load of `2--BLODEV_2_YINGYANG` as a mixer source:

1. `loadMacroSource` → `container->loadPreset(folder)` on the hidden macro
   container, which has a `timelineHost` → `automation->loadMacroPreset(*this,
   folder)`.
2. `loadMacroSequence` looks for `sequence.json` (the folder has
   `timeline.json`) and, before that, `owningMacro(root, macroContainer)`
   returns null because the mixer host is not a root node. It returns at once.
3. Result: no sequence, nothing in `macroSequences`, nothing played. The
   source's `timeline.json` is never read. Nested macros inside the source that
   have their own `sequence.json` are ignored for the same reason.
4. No warning is logged. Status shows the graph running normally.

So the answer to "is this a problem or an incompatibility" is: no crash, no
corruption, no interference with the parent's timeline. The only effect is
that a timelined source sounds or looks different inside the mixer than it did
as a preset, because its automation is gone. With `6--BLODEV_TIMELINED_1`
(tracks CIRCLE A → `Macro_1/DO_CIRCLES`, `Macro_3/DO_CIRCLES`; PINXOS →
`Ranger_6/Max_Out`) those parameters would just hold their saved values.

Two further observations about the current state:

- The parent timeline **can** automate the mixer's `Target`, `Mode`,
  `Duration_ms` and `Bi Pow` (int/float). It **cannot** press `Start!` or
  `Stop!` (void). So a parent timeline cannot sequence transitions yet.
- The mixer refuses router changes while a router is timelined
  (`ofxOceanodeMacroMixer.cpp:289`, `1091`) and removes bindings when a router
  is dropped (`:414`). That is about the mixer's *own* ports in the parent
  timeline and keeps working as is.

## What "morphing two timelined macros" can mean

Three different things, with very different cost:

| Scenario | What the timelines must do | Difficulty |
| --- | --- | --- |
| **Mix** (two graphs) | Each graph plays its own timeline on its own nodes; routers mix the results | Low: resolution + placement |
| **Morph fallback** (two graphs, controls morph, outputs crossfade) | Same as Mix; the destination's timeline starts when the destination loads | Low, same work as Mix |
| **Morph in place** (one graph) | A's sequence and B's sequence drive the *same* live nodes; the result must go from "A's automation" to "B's automation" over the fade | High: needs a weighted blend of two sequences and a path remap through the morph's node matching |

A fourth situation also matters: a **source without a timeline morphing into
one with a timeline** (or the reverse). In place, that is a blend between "no
automation, saved control value" and "B's automation", which is the same
mechanism as the in-place case with one side constant.

## Implementation topics

### 1. Loading a source's timeline into a mixer graph

Three concrete gaps, all small:

- **Reachability.** `owningMacro`, `containerWithCanvas`,
  `getContainerForCanvasID` and `findTimelineParameter` all start from the
  root's node list. The mixer host is invisible to them. Either the mixer
  registers its hidden hosts with the timeline manager (an explicit
  "foreign macro containers" list the walks also visit), or the macro-sequence
  API takes the macro node directly instead of re-finding it from the root
  (`loadMacroSequence(ofxOceanodeNodeMacro&, folder)`). The second is cleaner:
  the hook already runs on the macro's own container, and `ofxOceanodeNodeMacro`
  could pass itself. `MacroSequenceEntry` already stores the container as a
  `weak_ptr`, so lifetime handling is unchanged.
- **File format.** Sources are often root presets, which carry `timeline.json`
  (tempo, loop, markers, settings, tracks). A macro sequence is tracks + clip
  groups. `sequenceFromJson` reuses the preset codec on exactly those two keys,
  so reading `timeline.json`'s `tracks`/`clipGroups` as a sequence is a few
  lines, and root-relative paths (`Macro_1/DO_CIRCLES`) are already in the
  relative form a macro sequence uses. Everything else in `timeline.json`
  (tempo map, loop, markers, `playOnPresetLoad`) must be dropped: the parent
  owns time. The fallback order should be `sequence.json`, then
  `timeline.json`.
- **Kept-out nodes.** The mixer leaves blocked nodes (ILDA, SC Output, …) out
  of the graph. Bindings to them become `missingTarget` and are harmlessly
  skipped, which is the right behaviour. Nothing to do beyond not logging
  noise for them.

### 2. Resolving paths and applying values inside a mixer graph

The played copies are made absolute and resolved from the root. For a mixer
graph that does not work (`MacroMixer/1 / Macro 1::Node/Param` is not under
the root). Options:

- Give `PlayedMacro` a resolver base (the macro container) and keep its paths
  **relative**, resolving with `ofxOceanodeTimelineResolver::find`, which
  already calls `base->findTimelineParameter(path)` on any container. The
  evaluation already carries `placement.resolver`, so this is mostly about the
  apply pass (`findParameterCached`) and the scheduler using the resolver of
  the sequence a binding belongs to instead of the root. The parameter cache is
  keyed by path string; it would need to be keyed by (base, path).
- Alternatively, let `ofxOceanodeContainer::findTimelineParameter` descend
  into a registered set of foreign hosts when the first canvas level matches
  `MacroMixer/<n>`. Less invasive for the timeline, more coupling in core.

Also: `parameter->setTimelined(...)` is set from the apply pass; inside a mixer
graph that flag would start to appear on source nodes. Harmless, but
`morphEditable` should then skip timelined parameters (topic 5).

### 3. Placement: what plays a mixer source's timeline, and when

A macro timeline only plays where a macro clip places it. A mixer source has
no macro track anywhere. Choices:

- **Implicit placement:** the mixer plays each live graph's timeline as if a
  macro clip of its full length started at beat 0 of the main timeline and
  repeated forever, following the shared transport (the hidden host already
  shares `parent.getTransport()`). This matches what a root preset did when it
  was loaded as a preset: its timeline looped from the transport. It is the
  minimal and probably expected behaviour.
- **Explicit placement:** make the MacroMixer node placeable like a macro
  (`Add to Timeline as Macro Track`), so the parent timeline decides when the
  mixed timeline runs, with gating. More powerful, but `createMacroTrack`
  requires a macro container inside the sequence's canvas, and the mixer is not
  a macro. It would need a "mixer track" variant whose placement is forwarded to
  whichever graphs are live.
- Start with implicit placement. The transport beat is the same for both
  graphs, so A's and B's timelines are phase-aligned by construction. If
  the two sources have different timeline lengths (A loops at 16 beats, B at 24),
  each loops on its own cycle, as two independent presets would.

Phase questions to decide: does a graph's timeline start at the graph's
activation beat or at the absolute transport beat (modulo its length)? The
latter keeps A and B aligned with the parent timeline and with each other and
is what a preset-as-root did. A `Reset phase on activate` style option could
be added later.

### 4. Gating and snapshots

`updateMacroGating` drives `Active` and `Snapshot` of placed macros. With
implicit placement and no gating, nothing is written, which is right: the mixer
owns the graphs' lifecycle (an origin graph must keep running until the fade
completes, and the mixer already ignores saved `Active`/`Snapshot` as
"lifecycle controls", `MacroMixerSource.cpp:796`). Nested macros inside a
source *can* carry macro tracks with gating and per-clip snapshots in their own
sequences; those would keep working through the copies because the parameters
they touch (`Active`, `Snapshot` of nested macros) are reached through the
graph's own container. A per-clip snapshot recall during an in-place morph
would fight the morph's control interpolation on the recalled parameters; see
topic 5.

### 5. In-place Morph: blending two timelines on one graph

This is the only genuinely new mechanism.

- **Key remap.** B's `sequence.json` paths use B's *saved* node keys. After
  `alignMorphDefinition`, B's nodes live under A's keys (a `Ranger_6` in B may
  be `Ranger_2` live). The morph already produces the saved→live key map per
  container (`matches`), so B's sequence must be rewritten through the same map
  before it is attached, at the same nesting depth (macro-relative paths and
  `macroCanvas` of nested macro tracks, like `moveTracksIntoMacro` does when
  stripping prefixes). Added nodes get fresh runtime ids, which the map also
  covers. Nodes removed by the morph keep A's bindings until completion; nodes
  only in B get B's.
- **Blend.** The timeline combines several bindings on one parameter with
  `Replace/Add/Multiply/Min/Max` modes and "later sequence layers on top"
  ordering (`combineAutomationValues`). None of these is a weighted crossfade.
  Supporting the morph needs a **per-played-sequence weight** (0..1) applied
  when the sequences' values for one path are combined: value = lerp(A, B, w)
  for numeric types, switch at the eased midpoint for discrete types (bool,
  string, dropdowns, unequal vectors), exactly the rules `morphValue` uses for
  controls. Where only one sequence drives a path, the other side's value is the
  parameter's saved control value, which the morph is already interpolating, so
  the combined result is "automation × w + control × (1 − w)" and stays
  consistent with the control morph.
- **Who owns a parameter during the morph.** `applyMorph` writes matched
  controls every frame; the timeline's apply pass also writes bound parameters
  every frame (twice, before and after node updates). A parameter that is
  timelined in A or B *and* has different saved values in A and B would be
  written by both. The simplest rule: a parameter bound in either timeline is
  excluded from the control morph (`morphEditable` returns false when
  `isTimelined()`), and the timeline blend above takes the saved control value
  as the unbound side. This keeps one writer per parameter.
- **Scheduling.** The sample-accurate scheduler sends change beats to backends
  (SuperCollider) ahead of the playhead. A weighted blend changes continuously
  with progress, so scheduled values computed a block ahead would be computed
  with the weight of that moment, not of the playhead moment. For the fade's
  duration either accept this (the error is one scheduling block of a fade that
  lasts seconds) or suppress look-ahead scheduling for blended paths and fall
  back to per-frame application while `w` is strictly between 0 and 1. Piano
  roll gates/pitches are discrete and should switch, not blend, so B's notes
  take over at the midpoint; with "Switch at threshold" semantics this matches
  the Trigger policies the mixer already documents.
- **Completion and cancel.** At `progress >= 1` A's sequence is dropped and B's
  becomes the graph's sequence (as `liveMorphDefinition` replaces the
  definition). On cancel, B's sequence is detached and A's weight returns to 1.
  Both match the existing `commit()`/`cancelMorph()` points.
- **Alternative that avoids all of this:** treat "both sources have a
  timeline, and they differ" as a **fallback reason**, so the morph runs as
  the two-graph crossfade. Each graph keeps its own timeline, routers
  crossfade, no blend code in the timeline. Continuity of buffers and clocks is
  lost (as in every fallback), but it is correct and cheap. This is a
  reasonable first milestone; the in-place blend can come later with a Details
  entry ("Timeline: crossfaded" vs "Timeline: blended in place").

### 6. Shared library sequences

Copies of a library macro share one sequence object. If a mixer source is a
library macro also present on the canvas, the mixer's graph and the canvas
instance would share it, and an edit in the timeline editor would be heard in
the mixer too. That is consistent with how library macros behave elsewhere, but
the mixer's sources are meant to be read-only folder references ("Persistence
stores folder references only; every activation reads and validates the source
again"). Safer: mixer graphs always get a **private copy** of the sequence, never
a shared entry, so Refresh re-reads it from disk like the rest of the source.

### 7. Driving the mixer from the parent timeline

Independent of the above, and probably wanted anyway:

- `Start!` and `Stop!` are void. Options: a bool `Go` parameter (rising edge
  starts), or an int `Transition To` that starts a transition to that target on
  change. Either is bindable today (Toggle/Step/Number lanes).
- Automating `Target` alone does nothing until Start! is pressed. A timeline
  "Multi Value" clip on a new `Transition To` would give a clean "at bar 17 go
  to source 2 over Duration_ms" sequence.

### 8. Evaluation index invalidation and lifetime

`rebuildPlayedMacros` runs with the evaluation indexes. Graph creation,
promotion (`current = std::move(next)`), cancel and `clear()` must call
`invalidateEvaluationIndexes()` and `invalidateSchedule()` so copies follow the
live graphs. Entries key on the container address; `pruneMacroSequences`
drops expired `weak_ptr`s, which covers graph destruction if the mixer's
containers are registered as `shared_ptr` (they are: `graph->host`,
`graph->container`).

### 9. Undo, editing and the inspector

A mixer graph's timeline should not be editable from the timeline editor (the
source is a folder reference). `requestMacroScope` only reaches containers
found from the root, so the editor cannot open it by accident. The inspector's
contribution rows would show the blended value with its source sequences; that
falls out of the existing `inspectTarget` once sequences carry a weight.

## Recommended order of work

1. **Load + resolve + implicit placement** (topics 1, 2, 3, 8): a timelined
   source plays its timeline inside the mixer in Mix mode and in the Morph
   fallback. No new timeline semantics. Test with `6--BLODEV_TIMELINED_1` as a
   mixer source: `Ranger_6/Max_Out` and `Macro_1/DO_CIRCLES` must move as they
   did when the preset was loaded at the root.
2. **Morph: fall back when timelines differ** (topic 5, alternative): a Details
   entry explains it. Equal timelines (same tracks after key remap) can morph in
   place with the one sequence kept.
3. **Parent-timeline control of transitions** (topic 7).
4. **In-place timeline blend** (topic 5 proper): weighted sequences in the
   timeline, key remap in the mixer, one-writer rule in `morphEditable`.

## Tests to add

- Core `tests/macro_mixer_*`: a source folder with `sequence.json` and one with
  `timeline.json`; in Mix mode both graphs' bound parameters follow their
  clips at the shared transport beat; kept-out targets are skipped silently;
  promotion keeps the destination's timeline and drops the origin's.
- Timeline `tests/macro_playback_regression.py` pattern: a sequence attached to
  a container that is not under the root resolves through its own base.
- Morph: differing timelines force fallback with the right Details message;
  identical timelines morph in place; `morphEditable` excludes a timelined
  parameter.
- `tests/integration_compile.py --timeline ../ofxOceanodeTimeline` after any
  change to `ofxOceanodeAutomationExtension.h`.

## Files involved

Core: `src/Managers/ofxOceanodeAutomationExtension.h`,
`src/Managers/ofxOceanodeContainer.cpp` (load/save hooks, `findTimelineParameter`),
`src/Nodes/MacroMixerSource.cpp` (`instantiate`, `morphEditable`,
`alignMorphDefinition`, `prepareMorphInPlace`), `src/Nodes/MacroMixerRuntime.cpp`
(promotion/cancel/clear), `src/Nodes/ofxOceanodeMacroMixer.cpp` (new bindable
transition control).

Timeline: `src/Timeline/ofxOceanodeTimelineMacroSequences.cpp` (load without the
root walk, `timeline.json` fallback), `src/Timeline/ofxOceanodeTimelineEvaluation.cpp`
(`collectMacroSources`/`rebuildPlayedMacros`/`playedSequences` for implicit
placements, weighted combine), `src/Timeline/ofxOceanodeTimeline.cpp`
(`findParameterCached` through the sequence's resolver),
`src/Timeline/ofxOceanodeTimelineScheduler.cpp` (blended paths).
