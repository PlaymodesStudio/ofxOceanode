# MacroMixer

MacroMixer runs two ordinary Macro instances and mixes their selected Routers.
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

## Types and addon registration

Built-ins retain Float, Float Vector, Integer, Integer Vector, Boolean and Texture
policies, including vector resampling, whole-value Switch, and the existing GPU
transition modes. `registerType<T>(name, defaultValue, MacroMixerMixing::Definition<T>)`
adds a custom value adapter and default policy at the existing type registration.
Additional policies use `MacroMixerMixing::registry()`; nodes need no extra list.

VectorGraphics registers Fatline Vector opacity and Switch policies.
SuperCollider registers ScBus policies with a private bridge per output. Its final
SC Output stays outside the source Macro and determines server placement. The
bridge retains the existing SynthDef loading/rebuild acknowledgements and graph
leases. Renamed helpers live in the addon's `data/Supercollider/Synthdefs/Defaults/macromixer`
and can be rebuilt with `synthdefs/macromixer/build.scd`.

## Persistence and editing

The **Load** and **Save** buttons below Routers open file dialogs in
`data/routergroups` (created if needed). Router group JSON files store the ordered
router IDs, names, types, Input/Output roles, mixing policies and policy options.
They do not include Macro folders, playback or current input values. Load replaces
the selected router list after validation, retaining matching local ports and
cables. Existing interface restrictions still apply when Macros are imported;
invalid or incompatible files leave the current list intact.

Save/clipboard stores `MacroMixerState` version 1: router definitions, policies,
inputs, ordered Macro source references, selection and destination. Reload creates
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
  clipboard instances.
- `python3 tests/macro_mixer_numeric_policies_regression.py`: numeric/Boolean
  policy rounding, endpoints, unequal vector sizes and option validation.
- `python3 tests/macro_mixer_fatline_regression.py`: production Fatline adapters,
  opacity, Switch, native graphs, persistence and Macro embedding/copying.
- `python3 tests/macro_mixer_texture_regression.py`: real macOS CGL pixels,
  production texture nodes and policies, alpha/HDR/dimensions, input events and
  independent buffers after save/load and Macro/clipboard copies.
- `python3 tests/macro_mixer_sc_regression.py`: production SC traversal, server
  graph manager, Output and bridge with a captured-command server adapter.
- `python3 tests/macro_mixer_sc_audio_regression.py`: actual offline SuperCollider
  render of the renamed SynthDefs, without opening an audio device.

The C++ tests need the sibling openFrameworks and addon sources/libraries. Native
windowed-host interaction, actual Syphon publishers, DAC/MIDI hardware, live audio
and multi-server listening remain separate acceptance checks. Regenerate the host
project so it includes the renamed/new `.cpp` files and removes PresetMixer ones;
copy the updated SuperCollider data before testing audio.
