# MacroMixer

MacroMixer normally runs two ordinary Macro instances and mixes their selected Routers.
It is registered under **Experimental → MacroMixer**. Declare router names, types
and Input/Output roles in its Inspector; add Macro source folders; choose Target
and trigger Transition. Duration Ms, Bi Pow and Finish Transition control a fade.
Each output has its own type-specific policy and options.

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
an import, Refresh, Transition (ordinary or Morph macros) or a preset recall
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
again. Each source's **Source > Kept out** lists its kept-out nodes, including
those in nested Macros; **Restore** includes a node again from the next activation, and you are asked about it again while it still
blocks the mix. The dialog is drawn by `ofxOceanode` with the other
application-level dialogs and waits for any other open popup to close first.

## Morph macros

Enable **Morph macros** in the Inspector to interpolate graph controls instead
of mixing the two graphs' router outputs. It defaults
to off and is saved with the node. The mode is captured when Transition is
triggered; toggling it during playback affects the next requested transition.
Duration Ms, Bi Pow, Progress and Finish Transition retain their normal meaning
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

Each source has a header bar in the node color (stronger while it runs) with
its badges. **Macros > Source** has the same layout for every source. Paths
start at the data folder; hover for the full path.

- The source folder.
- **State**: *Checked* (read since it was added or refreshed), *Not checked yet*
  (restored from a preset or clipboard; Refresh or Transition reads it) or
  *Missing*.
- **Kept out**: the source's kept-out nodes, each with **Restore** (only when there are any).
- **Morph details** (Morph macros on): the morph from the running graph into this
  source, as a *preview* or the *last transition* once one was requested. It lists
  added/removed/replaced nodes, the receiving node/parameter and both endpoints of every
  changed connection with its policy, and every fallback reason, including mixes
  skipped because another incompatibility forced fallback.
- **Morph policies into this source**, described below.

Previewing does not construct nodes or alter the running graph. Refresh rereads
edited source files.

Changed connections blend with a policy per signal type, with the same option
controls as the Routers section. **Macros > Morph policies (mixer default)** sets
the default for every source. Each source's **Morph policies into this source**
can override it per type for transitions *into* that source; *Mixer default*
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
graph keeps morphing in place. Morph details lists it as **Replaced node**
with the differing setting. If a blend is impossible (for example a cable
into a trigger or Portal), the whole graph falls back as described next.

When the running graph cannot be reused, the morph falls back to a crossfade.
A fresh destination Macro is loaded next to the running one and both keep
running. The destination's matching controls start at the captured current
values and morph to their saved values, while every selected Output router
crossfades from the running graph to the destination with its own **Mix
policy** (the Routers table), over the same Duration and Bi Pow. Fatlines fade
by opacity, ScBus outputs mix through the private SC bridge, textures use their
texture policy. Status shows the specific reason and Mixer Status shows
`Xfade`. The current implementation falls back for:

- Changed Router or Portal settings (names, bounds, Inspector settings). These
  publish the Macro interface or a shared service and cannot run twice.
- Unmatched Router/Portal additions or removals in an existing container. These
  require reconciliation of the existing published interface or shared service;
  routers inside a whole newly added Macro load through its native lifecycle.
- Genuinely changed cables into read-only/calculated or snapshot-excluded
  controls, Portal services, triggers, or resource types without an internal
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
values follow the shared Portal service rather than being written by morphing. Router names/bounds remain
the destination's; router Values marked Exclude From Snapshots are also skipped.
Selected outer Input routers and connected nested Macro input pins remain live.

Floats, integers, equally sized numeric vectors and colors interpolate; integer
values round to the nearest integer. Booleans, strings, dropdown controls and
vectors with different lengths switch at halfway through the eased progress.
Non-finite controls are skipped. Unmatched destination controls retain their own
values. Changed cable blending supports these same control types, with native
scalar/vector conversions; registered types use their selected connection policy.
Triggers and unsupported resource signals require the fallback crossfade.

During the fallback crossfade the origin keeps its native update/draw and is
released on completion; the destination keeps its live state. In a compatible live graph, completion
reconciles temporary nodes/cables and changes its source reference; common nodes
are never replaced. Missing/invalid sources preserve existing playback. Saving
during a morph restores a fresh destination, as with ordinary output mixing;
intermediate control values and elapsed morph time are not embedded. Older saved
sessions default to ordinary mixing.

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

The **Load** and **Save** buttons below Routers open file dialogs in
`data/routergroups` (created if needed). Router group JSON files store the ordered
router IDs, names, types, Input/Output roles, mixing policies and policy options.
They do not include Macro folders, playback or current input values. Load replaces
the selected router list after validation, retaining matching local ports and
cables. Existing interface restrictions still apply when Macros are imported;
invalid or incompatible files leave the current list intact.

Loading a preset or pasting a MacroMixer reads every source folder (analysis
only, no nodes are created) before playback starts. A source that fails shows
its reason as its **State** in orange; it never stops the others or opens the blocked-node
dialog, which still appears when a Transition activates that source.

The **Mixer Status** text output summarises the node in about 28 characters,
for a narrow display: what runs, then how many sources passed their last check.

| Mixer Status | Meaning |
|---|---|
| `No sources` | No Macro folders added |
| `Idle \| 5/5 ok` | Nothing running |
| `Run 3 Bloobs_9 \| 5/5 ok` | Source 3 is running (long names end in `~`) |
| `Mix 3>4 62% \| 5/5 ok` | Ordinary transition from source 3 to 4 |
| `Morph 3>4 62% \| …` | Morph on the running graph |
| `Xfade 3>4 62% \| …` | Morph fell back: both graphs run and outputs crossfade |
| `Run 3 Bloobs_9 \| 4/5 ok` | One source failed its check; see its State |
| `Blocked, see dialog \| …` | A blocked node needs an answer |
| `Error, see Status \| …` | The inspector Status has the details |

Indices match the inspector's source list.

Save/clipboard stores `MacroMixerState` version 1: router definitions, policies,
inputs, ordered Macro source references, selection, destination and optional
`morphPolicies` settings per connection type, both mixer-wide and on each Macro
entry. Older v1 states default to the type's usual policy (Texture Crossfade) and
have no per-source policies. Reload creates
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
subscribe to global library replacements; use Refresh/Transition to apply edits.
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
  blended output) while Router changes still fall back. Whole local,
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
- `python3 tests/macro_mixer_sc_audio_regression.py`: actual offline SuperCollider
  render of the renamed SynthDefs, without opening an audio device.

The C++ tests need the sibling openFrameworks and addon sources/libraries. Native
windowed-host interaction, actual Syphon publishers, DAC/MIDI hardware, live audio
and multi-server listening remain separate acceptance checks. Regenerate the host
project so it includes the renamed/new `.cpp` files and removes PresetMixer ones;
copy the updated SuperCollider data before testing audio.
