# MacroMixer

MacroMixer normally runs two ordinary Macro instances and mixes their selected Routers.
It is registered under **Experimental → MacroMixer**. Declare router names, types
and Input/Output roles in its Inspector; add Macro source folders; choose Mode
(Mix or Morph) and Target and press **Start!**. Duration_ms, Bi Pow and **Stop!**
(finish now) control a fade, and Progress shows it. Each output has its own
type-specific policy and options.

The node shows Mode, Target, Progress, Duration_ms, Bi Pow, Start!, Stop! and
Status, then a **Routers** separator above the router ports. The Inspector has
four sections: **ROUTERS** (the router table with **Load Routers** and **Save
Routers**), **MIX/MORPH POLICIES** (**Default Mix/Morph policies**), **MACROS**
(**+ Add Folder**, **Clear Playback** and the sources) and **STATUS**.

A source is the folder containing a saved Macro's `modules.json`,
`connections.json` and node settings. A library Macro folder or a saved local
`Macro_N` folder can be used. Extra routers remain part of the Macro without
participating in the mix. Every selected router must exist once, with the expected
type. Input routers must have no incoming cable inside the Macro; the outer input
is supplied to both live instances. Output routers may carry constant values.

Execution uses `ofxOceanodeNodeMacro::loadMacroSource()` and the ordinary Macro
container loader, lifecycle hooks, typed cables, updates, draws, snapshot system
and parent transport. Nested Macros, feedback cables, free-running clocks,
registered addon models, dynamic parameters and Portals retain their native
behavior. There is no computational node allowlist, special numeric cable class,
graph sorting or feedback solver. Portals, shared resources, external inputs and
Macro controls have the same shared-state behavior they have in ordinary Macros;
MacroMixer does not make those services private.

Analysis and Refresh inspect files and registration metadata without constructing
source models. They never remove nodes or cables. A mismatching router or external
output rejects the entire source, leaving existing playback intact. Analysis
recurses through local Macro folders and global references resolved through the
normal Macro library. A copied node registry also checks source folders before native
loading/reloading, then guards factories during construction. A forbidden live
edit is rejected before the existing nested Macro is changed. The ordinary host registry remains
unrestricted.

`ofxOceanodeNodeModel::macroMixerBlocker()` supplies a stable reason when a node
cannot run inside a mixer. It is cached during normal registration, before setup.
The existing laser declarations carry over, including Laser Simulator's exclusion.
Legacy fallbacks cover ILDA Projector/Shape, Polylines OSC Sender, SC Output,
SC NRT Recorder, SC MultiTrack Rec, Texture Sender, Display Output, both Texture
Recorders, MIDI output nodes, Artnet/Artnode/E131 Sender, NDI Sender and OSC Sender
nodes. Put final hardware/network outputs after MacroMixer. Input nodes and whole
categories are not banned. Addons with other external outputs should declare the
capability on their models.

### Keeping blocked nodes out of the graph

Analysis reports every blocked node in the source, including nested Macros. When
an import, Refresh, Start! (Mix or Morph) or a preset recall
stops on blocked nodes, a modal asks about each one in turn, for example:
ILDA Debugger 3, its Macro folder and the reason. **Keep out of graph** leaves that saved node out
of this MacroMixer's graphs; **Keep all N out** answers the remaining ones and
**Cancel load** leaves the load stopped, with the running graph intact. Once every
blocked node has been kept out, the stopped import or activation is retried.

A kept-out node is identified by its canonical folder and saved node key
(`ILDA_Debugger_3`). Analysis, morph definitions and native folder loads
(including nested and global Macros and live-morph additions) skip the node and
every cable to or from it. The source folder is not modified. The choices are
saved in `MacroMixerState` as `keptOutNodes`; older states have none and ask
again. Each source's **Kept out** lists its kept-out nodes, including
those in nested Macros; **Restore** includes a node again from the next activation, and you are asked about it again while it still
blocks the mix. The dialog is drawn by `ofxOceanode` with the other
application-level dialogs and waits for any other open popup to close first.

## Morph mode

Set **Mode** to **Morph** to interpolate graph controls instead of mixing the
two graphs' router outputs. It defaults to Mix and is saved with the node; older
presets' Inspector checkbox "Morph macros" and "Duration Ms" load into Mode and
Duration_ms. The mode is captured when Start! is pressed; changing it during
playback affects the next requested transition.
Duration_ms, Bi Pow, Progress and Stop! retain their normal meaning
(Bi Pow zero uses the existing MacroMixer linear curve).

When node types/connection roles and saved non-control configuration match, the
running graph is reused, including compatible changes to control cables and
added or removed nodes, including whole nested Macros. Target control values are read from the saved
files without creating a second graph instance. Buffers retain their
recorded samples/timestamps, clocks keep their phase, and stateful nodes keep
running. Each native update/draw occurs once. Status shows **Morphing live graph**.
This is the same basic behavior as recalling a snapshot on an existing canvas.
Saved outer Macro input-pin values override the nested library/local defaults.

Changed control cables blend at their sink, using live source signals and the
saved fixed value on an unconnected side. Duration and Bi Pow apply to this
blend too. For example, a source-only Phasor → Oscillator → Ranger chain keeps
modulating a common Macro input while its influence fades toward the target's
fixed value. In the reverse direction, the new chain starts running immediately
and its influence grows. Source-only nodes retire at completion; new nodes retain
their target controls. Common buffers and clocks are preserved in both directions.
Temporary cables are removed and ordinary target wiring takes over at completion.

Changed texture connections use an owned GPU surface at the receiving parameter.
For example, changing `Gradient2 -> Texture router` into
`Gradient2 + Image Loader -> Mixer Alpha -> Texture router` blends the live
Gradient2 and Mixer Alpha images while the common graph keeps running. RGBA
Crossfade is the default; the existing router policy registry also supplies
Switch, Screen, Add, Multiply, Difference, Wipe, Noise Dissolve and FadeOut.
Rendering runs only during draw. Internal processing follows native frame order;
a downstream draw-driven node can observe the mixed image on the following frame.

Each source has a collapsible header in the node color (stronger while it
runs) with its index, name and badges in green, and refresh (circular arrow),
move up/down and **-** (remove) on the right. Choose the source with Target. Expanded, every source has the same layout.
Paths start at the data folder; hover for the full path.

- The source folder, followed by its problem in orange when it is missing or
  failed its last check.
- **Mix/Morph Policies**: the policies into this source, described below.
- **Details** (Mode Morph): the morph from the running graph into this
  source, as a *preview* or the *last transition* once one was requested. Its
  title is orange when a node is replaced and red when the graph falls back. It lists
  added/removed/replaced nodes, the receiving node/parameter and both endpoints of every
  changed connection with its policy, and every fallback reason, including mixes
  skipped because another incompatibility forced fallback.
- **Kept out**: the source's kept-out nodes, each with **Restore** (only when there are any).

Previewing does not construct nodes or alter the running graph. Refresh rereads
edited source files.

Changed connections blend with a policy per signal type, with the same option
controls as the Routers section. **Default Mix/Morph policies** sets
the default for every source. Each source's **Mix/Morph Policies**
can override it per type for transitions *into* that source; *Default*
returns a type to the default. A transition A → B uses B's policies, so every
transition into B behaves the same and B → A uses A's. Without a configured
policy, textures use Crossfade and Boolean/dropdown controls Switch. These
policies are independent of the output-router policies, are saved with the
mixer, and edits affect the next transition. Float,
Integer, their vector types, Boolean, Texture and registered value types with a
mixing definition reuse the existing policy implementations. Strings/colors without a registered adapter
retain native control interpolation. Changed **ScBus** connections use private
audio bridges with Linear (default), Equal Power, Switch or FadeOut policies and
the same channel-count, smoothing and threshold options as output routers. An
uncabled endpoint is silence. These connection policies are independent of the
output-router policies; the router continues to publish the live graph result.
Backend policies must provide readiness and retirement support to mix internal
resource connections; policies lacking that lifecycle support report fallback.

Whole local or global nested Macros can be added or removed with their incoming
controls and outgoing control connections. They load through the native Macro
lifecycle, including their child graph and published pins. An added Macro starts
at the beginning; a removed Macro keeps running until completion. Numeric signals
from their outputs blend at the receiving parameter, just as signals from any
other node do. Empty nested Macros saved by the native saver are also accepted.
Existing native update order still applies: a node that calculates during update
can observe an upstream node's value on the following frame.

A node whose saved configuration cannot morph (changed Inspector settings,
Macro lifecycle/reference settings or unknown custom-save data, such as a
SuperCollider node's own sound block) is replaced on its own. The running
instance keeps going and retires at completion; the saved one is loaded
natively next to it with its incoming cables, and every cable leaving it
blends where it arrives with the source's **Morph policies into this source**
for that type (a ScBus cable through a private SC bridge). The rest of the
graph keeps morphing in place. Details lists it as **Replaced node**
with the differing setting. If a blend is impossible (for example a changed
cable into a Global Portal), the whole graph falls back as described next.

Added and replaced nodes load their saved settings as a preset recall:
`ofxOceanodeShared::isPresetLoading()` is true while they load, as it is for
a native source load, so recall-time side effects stay off. A SuperCollider
box saved with a Snapshot number keeps its saved sound instead of recalling
that library slot over it (a slot still saved with the old `Env_Mode` would
otherwise switch the added box to Drone). The cables and blends of the morph
are created outside that scope, so backends such as the SC server graph
still react to them.

### Portal buses

A Portal is a shared bus, not a cable: setting one Portal's Value sets every
Portal with the same type, Name and scope. A Local bus the running graph uses
and the target changes is therefore blended as one signal. Changes include a
different saved value, a different cable into a Portal, or the cable moving
to another Portal. One Portal of the bus takes the blend into its Value, as a
changed cable would, with the source's **Morph policies into this source**
for the type, and the bus spreads it:

- From the origin's driver (the source of the cable into the bus's Portal),
  or the live bus value when nothing drives it.
- To the target's driver, or the target's saved value when nothing drives it
  (a knob Portal).
- The origin's cable into the bus is detached for the morph and its source
  feeds the blend; the target's cable is connected natively at completion.
  Cancellation reconnects the origin's cable.
- Added Portals on a live bus load without their saved value and take the
  bus's, so nothing jumps when they start.

Details lists it as **Portal bus** with both ends (`Bus value`, `Saved
value` or the driver's port). The planning runs when a morph is requested
(and once for the cached Details); during the morph each changed bus costs
one blend, like a changed cable, and nothing at all otherwise. A bus whose
drivers are unchanged, or whose saved value equals the live value, adds
nothing. A bus with several cabled Portals is fine while those cables stay
the same; when they change, the graph falls back (`changes drivers with
several cabled Portals`), as it does when the target's Portals on one bus were
saved with different values. Global buses keep the rules below.

A Macro's **Color** is cosmetic: a different saved colour in a nested Macro is
not a configuration change and never makes a morph replace the Macro or fall
back. The running Macro keeps its colour. A library Macro takes its title
from the library, so its saved **Local Name** is ignored the same way; a local
Macro's Local Name is its title and is still compared.

### Nodes that blend their own saved state

Many nodes keep their sound outside the parameter group, in their own saved
block (SuperCollider nodes such as SC polycombBox). They can opt in through
`ofxOceanodeNodeModel`; nodes that don't keep today's behaviour.

- `morphKeys()`: `blended` saved keys the node blends itself, and `ignored`
  saved keys without sound (view sizes, tabs, browser folders). Neither counts
  as a configuration change.
- `morphHint(parameter)` for parameters in the group: `Interpolate` (default),
  `Switch` (jumps half way), `Skip` (the node drives it, or it is not sound) or
  `Structural` (a different saved value replaces the node, for example a voice
  or channel count).
- `morphCheck(target, reason)`: whether the node can blend into those saved
  settings, without side effects. A refusal replaces just that node, with the
  reason in Details.
- `morphBegin(target)` captures the node's current state as the start;
  `morphApply(t, switched)` runs every frame with the eased progress and
  `switched` from half way; `morphEnd(completed)` either keeps the target or,
  on cancellation, restores the start.

MacroMixer owns the timing (Duration, Bi Pow, Finish, cancellation); the node
owns how its values blend. Details lists such a node as **Node morph**.

SC polycombBox blends its `polyComb*` block (every window control, the
modulation matrix, LFO/envelope/Ext counts and modulated controls' bases)
with its existing snapshot morph: numbers glide (times, rates and frequencies
in log), modes switch half way, its own Snapshot/Morph/Ext controls are left
alone, and published controls are blended with the rest of the sound. Voices
and Channels are Structural, so a change there replaces the polycombBox. Show is
skipped.

When the running graph cannot be reused, the morph falls back to a crossfade.
A fresh destination Macro is loaded next to the running one and both keep
running. The destination's matching controls start at the captured current
values and morph to their saved values, while every selected Output router
crossfades from the running graph to the destination with its own **Mix
policy** (the Routers table), over the same Duration and Bi Pow. Fatlines fade
by opacity, ScBus outputs mix through the private SC bridge, textures use their
texture policy. The Inspector STATUS shows the specific reason and the Status
output shows `Macro fall`. The current implementation falls back for:

- Changed Router or Portal settings (names, bounds, Inspector settings). These
  publish the Macro interface or a shared service and cannot run twice.
- Unmatched Router additions or removals in an existing container. These
  require reconciliation of the existing published interface; routers inside a
  whole newly added Macro load through its native lifecycle.
- An added Global Portal whose bus is live (a running Portal of the same
  type and Name) when loading it would set that bus: it has an incoming
  cable, or nothing else in the target drives the bus, so its saved value is
  the bus's value. An added Global Portal without a cable joins a live bus
  that the target drives from another, cabled Portal: it loads without its
  saved value and takes the bus's. Local buses blend instead (Portal buses
  above). Other unmatched Portals morph in place like any node: a removed
  Portal runs until completion (the bus keeps its last value), an added one
  on a free bus starts with the morph, its cables blending where they arrive.
- Genuinely changed cables into read-only/calculated or snapshot-excluded
  controls, Global Portals, or resource types without an internal
  connection mixing adapter/backend handoff (such as audio buses and buffer
  references). Changed control cables need compatible native
  source/sink conversions and a saved fixed value for each uncabled endpoint.
  Unsaved/transient numeric inputs are supported when both endpoints have live
  sources; no missing fixed endpoint is invented.
- Live node/cable structure diverging from the activated definition, invalid
  compatibility data, or an unsuccessful staged node/cable preparation.

Changing a node's saved ID alone does not cause fallback. The fallback's
buffers and clocks are fresh and destination wiring applies inside the new
graph from its first frame; the output crossfade hides that start instead of
preserving the continuity of a snapshot on one graph. Running two graphs costs
what ordinary mixing costs, including shared Portal state.

While a graph morphs in place, selected router outputs pass through without
output interpolation. Textures get
a private identity copy so no borrowed texture becomes invalid between update
and draw. Resource processors such as the SC audio bridge remain identity
adapters to preserve graph leases and backend readiness. Matching live graphs
retain their existing resource nodes and routes.

Matching is one-to-one and applies to every model type. Named Router/Portal
interfaces and unique model types anchor matches; matching input/output port
roles propagate those matches through repeated model types. Saved IDs break ties
in otherwise unanchored parts of a graph. Ambiguous unmatched ordinary nodes
remain additions/removals rather than being arbitrarily paired by type alone.

Live node IDs are kept private and stable; target definitions are mapped onto
those instances. A phasor feeding the same oscillator input keeps its native
connection and running phase even if its saved ID changes or is reused for a
new modulation branch. Two phase ramps are not blended for such ID-only changes.
The mapped definition persists through successive morphs and cancellation restores
the previous mapping. Saved sessions still refer to the original source folder.
Local and global nested Macros are matched recursively; their mirrored outer pins
are not morphed twice. Parameter names and value types must match.
Only editable node controls saved by the native preset system and without
incoming connections are included. Calculated outputs, read-only/transient values,
Inspector settings, triggers and unsupported resource types are skipped. Portal
values are not written as controls: a Local bus blends as a whole (Portal
buses above), and a Global one follows the shared Portal service. Router names/bounds remain
the destination's; router Values marked Exclude From Snapshots are also skipped.
Selected outer Input routers and connected nested Macro input pins remain live.

Floats, integers, equally sized numeric vectors and colors interpolate; integer
values round to the nearest integer. Booleans, strings, dropdown controls and
vectors with different lengths switch at halfway through the eased progress.
Non-finite controls are skipped. Unmatched destination controls retain their own
values. Changed cable blending supports these same control types, with native
scalar/vector conversions; registered types use their selected connection policy.
Changed cables into void inputs use the Trigger policies (see Triggers below).
Unsupported resource signals require the fallback crossfade.

During the fallback crossfade the origin keeps its native update/draw and is
released on completion; the destination keeps its live state. In a compatible live graph, completion
reconciles temporary nodes/cables and changes its source reference; common nodes
are never replaced. Missing/invalid sources preserve existing playback. Saving
during a morph restores a fresh destination, as with ordinary output mixing;
intermediate control values and elapsed morph time are not embedded. Older saved
sessions default to ordinary mixing.

## Triggers (void routers)

A router of type **Trigger** selects a void router (`Router v`, port `Val`).
Triggers are events, not values, so they are never interpolated. During a
transition each policy decides which events pass:

| Policy | During the transition |
| --- | --- |
| **Switch** (default) | A's events below Threshold, B's from Threshold on. Threshold 0 hands over to B at once, 1 keeps A until the end. |
| **Merge** | Events from A and from B. |
| **Mute during transition** | None, from the moment B exists until completion. |
| **Density** | Each A event passes with probability 1 − progress, each B event with probability progress. Deterministic for a given Seed. |

Rules shared by every policy:

- Changing progress, starting, finishing, cancelling, promoting B or rewiring
  never fires a trigger by itself. Triggers a graph fires while it loads are
  ignored.
- A missing graph or cable contributes no events. At rest only the running
  graph's events pass.
- Progress is the eased (Bi Pow) progress, as for the other Switch policies.
  Unlike value policies, the first transition frame already uses the policy, so
  Mute and Switch at 0 apply as soon as B exists.
- Events are counted per frame. Merge and Density pass the larger of A's and
  B's counts, so a beat that reaches both graphs in the same frame (a shared
  clock, an outer Input trigger) passes once, while either graph alone still
  passes. Two independent events in the same frame also collapse into one; the
  mixer cannot tell them apart.

Output routers latch the events their graph fires during update. MacroMixer
publishes them once, after the frame's mix, on its own void output; events
fired during draw are published on the next update. A held (failed) frame
publishes nothing and keeps its events for the next mixed frame. Input
routers forward an outer trigger to every live graph as it arrives, like a
native Macro pin.

In Morph mode a changed cable into a void input (an added, removed or
re-routed trigger source) no longer forces fallback. Its events pass as they
happen, inside the graph's update, with the source's Trigger policy. The
policy runs per frame on each side's count, so the result matches the router
policies above. At completion the native destination cable takes over.

## Node windows

Some nodes draw their own window, such as the SC instrument editors
(`getWindowTitle()`/`getWindowToggle()` on `ofxOceanodeNodeModel`). A source's
`layout.json` never applies inside MacroMixer, and a source's saved **Show**
is ignored: every node window in a source graph starts closed.

- Each source lists its windows under **Windows** while it plays; tick one to
  open it. Choices are kept per source, saved with the mixer state, and
  survive Clear Playback; removing the source forgets them.
- Only playing sources show their windows. During a transition, in place or
  in Mix mode, both sources show the windows ticked for them, so both
  instruments can be watched while both sound; the origin's close when the
  transition completes (or the destination's, when it is cancelled). In place
  the one graph plays both: a window belongs to the destination unless only
  the origin ticked it, so the windows that stay keep their name. Closing one
  with its close button during the transition unticks it for its graph's
  source (in place, for both sources). The window's Show is never morphed.
- Closing a window with its own close button unticks it. Writes to Show during
  update (a snapshot recall, a native reload, Mixer Pro's or the timeline's
  "go to window") are undone, so windows of a source open only from MacroMixer.
- Inside MacroMixer a window is named after its source ("AnalogBox 1
  (1--BLODEV_1)", plus the nested Macro path), from `getWindowId()`. It never
  shares a window with the patch's own node, and docking in the mixer's preset
  layout finds it again across sessions.

## Types and addon registration

Built-ins retain Float, Float Vector, Integer, Integer Vector, Boolean and Texture
policies, including vector resampling, whole-value Switch, and the existing GPU
transition modes. The default vector policy, Index / zero fill, blends index by
index; a 1-value vector counts as that value at every index, as nodes read it, so
a change to or from a single value approaches it smoothly instead of fading the
other indices to zero and snapping at completion. Only missing indices of longer
vectors are filled with zero. `registerType<T>(name, defaultValue, MacroMixerMixing::Definition<T>)`
adds a custom value adapter and default policy at the existing type registration.
Additional policies use `MacroMixerMixing::registry()`; nodes need no extra list.

VectorGraphics registers Fatline Vector opacity and Switch policies.
SuperCollider registers ScBus policies with a private bridge per output. Its final
SC Output stays outside the source Macro and determines server placement. The
bridge retains the existing SynthDef loading/rebuild acknowledgements and graph
leases. Internal ScBus morphs wait for required SynthDefs and the routing
`/synced` before starting their timed interpolation. Matching synths and their
buses remain in place. Completion restores the native destination cables;
cancellation restores the origin cables. Departing nodes and temporary bridges
remain owned until every involved server acknowledges that final routing. A
resource-allocation failure restores the origin wiring and reports the failure.
Renamed helpers live in the addon's `data/Supercollider/Synthdefs/Defaults/macromixer`
and can be rebuilt with `synthdefs/macromixer/build.scd`.

## Persistence and editing

The **Load Routers** and **Save Routers** buttons below the router table open file dialogs in
`data/routergroups` (created if needed). Router group JSON files store the ordered
router IDs, names, types, Input/Output roles, mixing policies and policy options.
They do not include Macro folders, playback or current input values. Load replaces
the selected router list after validation, retaining matching local ports and
cables. Existing interface restrictions still apply when Macros are imported;
invalid or incompatible files leave the current list intact.

Loading a preset or pasting a MacroMixer reads every source folder (analysis
only, no nodes are created) before playback starts. A source that fails shows
its reason in orange under its folder; it never stops the others or opens the blocked-node
dialog, which still appears when Start! activates that source.

The **Status** text output is one short, coloured word. During a morph it
describes that morph; otherwise, in Morph mode, the next morph into Target.

| Status | Colour | Meaning |
|---|---|---|
| `Ok` | green | Nothing falls back, or Mode is Mix |
| `Node fall` | orange | A node is replaced (see the source's Details) |
| `Macro fall` | red | The whole graph falls back to the output crossfade |
| `Error` / `Blocked` | red | An error, a failing Target, or a blocked node waiting in the dialog |
| `No sources` | grey | No Macro folders added |

The Inspector **STATUS** section starts with the same word, then the full text:
what runs, the destination and queue, how many sources passed their last check,
and any error.

Save/clipboard stores `MacroMixerState` version 1: router definitions, policies,
inputs, ordered Macro source references, selection, destination and optional
`morphPolicies` settings per connection type, both mixer-wide and on each Macro
entry. Older v1 states default to the type's usual policy (Texture Crossfade) and
have no per-source policies. Source folders and kept-out node folders inside the
data folder are saved relative to it (`Presets/BLOOBS_MACAU/12--Bloobs_9`) and
resolved against the current data folder on load; folders outside it stay
absolute, and older absolute paths still load. Reload creates
ports before external cables and starts a fresh native destination instance.
Resource input addresses are never saved; reconnect them after reload.
Save during a fade restores its destination. Each activation reads the source
again, including a transition back to the same source. Missing/invalid sources
leave already running instances intact; a fresh reload needs the original files.
Refresh checks edits without replacing a running instance.

Removing a router automatically disconnects its outer cables and removes its
Scope, Timeline and Custom GUI references. Other selected routers and imported
Macros remain; source graphs keep their internal routers. Removing the last
selected router clears playback. Removing a Macro that is running, is a fade
destination or is queued automatically clears playback before removing its list
entry. Other imported Macros and their source folders remain available.

Top-level source instances are loaded as local in-memory Macros. They do not
subscribe to global library replacements; use Refresh/Start! to apply edits.
Nested global Macros retain ordinary library behavior. Native loading and resets
retain ordinary Macro transport semantics. Shared-service behavior and the cost
of running both Macros must be considered when designing a source.

The PresetMixer node, capture format and saved experimental PresetMixer sessions
are not automatically migrated. The full development is preserved on the archive
branches listed in `macro-mixer-migration-audit.md`.

## Validation

- `python3 tests/macro_mixer_regression.py`: ASan/UBSan, production native Macro
  loading, selected/extra routers, nested macros, feedback, rejection without
  construction, private registry guards, transitions, missing sources, save/load,
  outer cable restoration, router group file round-trips/validation,
  automatic router/Macro removal cleanup and independent
  clipboard instances. Blocked nodes are reported individually; kept-out nodes
  and their cables are absent from definitions, activation and native reloads,
  the choices persist, a blocked activation resumes once every node is
  answered, and Cancel/Restore behave as described above.
- `python3 tests/macro_mixer_trigger_regression.py`: Trigger policies and
  options, Mix-mode void router outputs per frame (Switch thresholds, Merge
  with shared and distinct beats, Mute), outer Input triggers reaching both
  graphs once, the node publishing each event once across update/draw, and
  in-place morphs of a changed void cable without fallback.
- `python3 tests/macro_mixer_portal_morph_regression.py`: unmatched Portals
  (removed, and added on a free bus) morph in place with their cables blending;
  an uncabled Portal joins a driven live bus without publishing its saved
  value; Local buses blend a knob's saved value, an added knob on a live bus,
  a changed driver and a driver moved to an added Portal, end at the target's
  native wiring and cancel back to the origin's cable; several changing
  drivers fall back.
- `python3 tests/macro_mixer_windows_regression.py`: node windows start closed
  despite a saved Show, open per source, show for both sources during a
  transition in place and in Mix mode until the origin's close at completion,
  adopt a close from the window but not an update-time write, and persist with
  the mixer state.
- `python3 tests/macro_mixer_numeric_policies_regression.py`: numeric/Boolean
  policy rounding, endpoints, unequal vector sizes and option validation.
- `python3 tests/macro_mixer_morph_regression.py`: native nonlinear graph
  calculations, current-value capture, matching/missing/nested nodes, discrete,
  vector and color controls, live inputs, paused origin execution, Bi Pow,
  queued mode changes, finish, missing sources and preset/clipboard persistence.
  Also covers live modulation cable changes and node additions/removals in both
  directions, eased live signals, topology rollback, matching graph reuse without
  construction, native Buffer histories/timestamps, HeaderPro samples, phasor phase, nested pin overrides,
  Portal values, calculated output flags and configuration fallback. It also
  covers renamed IDs across nested Macro/Router/Buffer/Header nodes, swapped
  same-type roles, repeated clock-role changes, unsaved Phase continuity,
  save/restore and fallback value capture across renamed paths. Fallbacks keep
  the origin running and crossfade outputs with the router policy. A changed
  node configuration replaces only that node (nested HeaderPro, a probe with
  blended output) while Router changes still fall back. A mock node that blends
  its own hidden state covers the node morph hooks: ignored keys, Skip, Switch
  and Structural hints, a refused check and cancellation; its replacement loads
  as a preset recall (`isPresetLoading()`), keeping its saved state. Whole local,
  global and empty nested Macro additions/removals cover incoming controls,
  connected live outputs, native destruction, cancellation, failed preparation,
  alias collisions and repeated transitions. Different live sources driving an
  unsaved numeric sink are also covered. Source previews, retained fallback
  details, shared registry policy selection/options, per-source policy overrides
  driving a transition, rejected edits during fades, transactional policy
  validation and old/new preset persistence are checked.
- `python3 tests/macro_mixer_fatline_regression.py`: production Fatline adapters,
  opacity, Switch, native graphs, changed internal Fatline connections using the
  registered opacity/Switch policies, persistence and Macro embedding/copying.
- `python3 tests/macro_mixer_texture_regression.py`: real macOS CGL pixels,
  production texture nodes and policies, internal changed-cable GPU blending into
  routers and processing nodes, live input pixels, cancellation/native cable
  restoration, reverse retirement, saved connection policies, alpha/HDR/dimensions, input events and
  independent buffers after save/load and Macro/clipboard copies.
  Also verifies graph morph colors/dimensions, output-policy bypass and owned
  texture lifetime during completion and mode changes. Hardware GL exercises
  two active Mixer Alpha layers; the headless software renderer uses its native
  single-layer path because Apple's software driver aborts in the existing full
  composite shader. Connection policy shaders and their pixels are tested in
  both cases; windowed host visual verification remains separate.
- `python3 tests/macro_mixer_sc_regression.py`: production SC traversal, server
  graph manager, Output and bridge with a captured-command server adapter.
  Includes control morphing, destination-only output bridge gains, changed
  internal ScBus routing with all four policies, silence endpoints, stable common
  synths/buses, SynthDef and routing readiness, allocation rollback, cancellation,
  stale acknowledgements, two-server retirement, offline-capture cleanup,
  return to ordinary output mixing, replacing a single SC node whose saved
  settings differ, and the fallback crossfade of a morph that cannot reuse the
  running graph. It needs a SuperCollider addon with the MacroMixer routing
  fences (`serverManager::requestMixerRouting`).
- `python3 tests/macro_mixer_polycomb_regression.py`: production SC polycombBox,
  without a server, morphing its hidden sound on the running node: linear and
  log glides, a mode switching half way, Snapshot kept, cancellation, a
  Channels change replacing the node, and the replacement keeping its saved
  sound (Drone off) although its Snapshot number names a legacy drone slot of
  the library. Needs the SuperCollider, ofxSuperCollider and ofxOsc addons;
  builds as C++20.
- `python3 tests/macro_mixer_sc_audio_regression.py`: actual offline SuperCollider
  render of the renamed SynthDefs, without opening an audio device.

The C++ tests need the sibling openFrameworks and addon sources/libraries. Native
windowed-host interaction, actual Syphon publishers, DAC/MIDI hardware, live audio
and multi-server listening remain separate acceptance checks. Regenerate the host
project so it includes the renamed/new `.cpp` files and removes PresetMixer ones;
copy the updated SuperCollider data before testing audio.
