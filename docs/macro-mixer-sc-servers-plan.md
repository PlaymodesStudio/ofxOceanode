# MacroMixer: deploying transition audio on separate SuperCollider servers

Implementation plan. Written 2026-11-06 for an implementing agent. Read
`docs/macro-mixer.md` (sections *Morph mode*, *Types and addon registration*,
*SuperCollider*) and the files named below before changing anything.

## Problem

Every MacroMixer transition today puts both sides of the audio on the same
scsynth server. The server that is playing the origin also has to allocate
buses, load SynthDefs, create and order the destination's synths, and run both
graphs' DSP until the fade completes. In the BLOOBS show (presets in
`PM Dropbox/.../BLOOBS_MACAU/SHARED_DATA/data/Presets/BLOOBS_NEW`), a
transition into `8--BLOOBS_2_CELLS_AUDIO` or `12--BLOOBS_2_GHOSTS_BIONIC_AUDIO`
adds a nested Macro (`BLOOBS_AUDIO_ENGINES`) holding about 25 SC nodes and five
library Macros, on the server that is still playing the SC analogBox of the
origin. The result is audible glitching during the transition, and a Start
frame of about 190 ms on the host (measured by the "prepared in N ms" context
line in Details).

The host app boots up to three scsynth servers. Only one is used by the mixer.
The goal: the mixer places the incoming audio graph on a server that is not
playing, crossfades across servers, and keeps the whole thing at 60 fps.

## How audio placement works today (read this first)

- **One serverManager per scsynth process**, all opening the same audio
  device. `ofxOceanodeSuperColliderController` owns `outputServers`
  (`ofxOceanodeSuperColliderController.cpp:1012`, `:1030`); the device plays
  every server at once and the master volume applies to all of them
  (`:48`, comment at `:565`). Per-server CPU is available from
  `ofxSCServer::getAvgCPU()` / `getPeakCPU()` (`ofxSuperCollider/src/ofxSCServer.h:216`).
- **A server's graph is whatever its SC Output nodes pull.**
  `scOutput` registers itself on the server chosen by its `Server` parameter
  (`nodes/routing/scOutput.cpp:36-55`). `serverManager::recomputeGraph()`
  (`serverManager.cpp`, search `void serverManager::recomputeGraph()`) walks
  upstream from each registered Output with `appendOrderedNodes`, allocates a
  128-channel bus per node output, frees synths of nodes that left, creates
  synths of nodes that joined, orders them, and requests `/sync`. An scNode
  therefore has one `State`/synth **per server that pulls it**; nothing stops a
  node from being pulled by two servers at once (it then runs twice).
- **A node can take part in a server's walk selectively.** During the walk,
  `scNode::getGraphTraversalServer()` returns the server being built
  (`scNode.h`, `ScopedGraphTraversal`). SC Mixer Pro overrides
  `appendOrderedNodes` and `getConnections` to expose, on server k, only the
  inputs whose track is assigned to k (`nodes/routing/scMixerPro.cpp:261-287`).
  **This is the hook the plan builds on.**
- **The mixer's audio bridge.** `integration/MacroMixerSCBus.cpp` registers the
  ScBus policies (Linear, Equal Power, Switch, FadeOut). Each ScBus Output
  router of a MacroMixer gets a private `AudioBridge` (an scNode with inputs
  A and B and one output) whose synth `oceanodeMacroMixer<channels>` applies
  `gainA`/`gainB`. The bridge keeps a `State` per server it is pulled on, sets
  `ready=false` on `graphWillRebuild`, requests a routing ticket when its
  sources change, and reports `ready()` only once every server it is on has
  `/synced` (`mixerRoutingReady`). The runtime holds the fade clock while a
  bridge is not ready (`MacroMixerRuntime.cpp`, search `resourcesReady`).
- **Where the bridge lives.** The host patch connects the mixer's ScBus output
  to an SC Output node (`3--MORPH_1`: `MacroMixer_1.ScBus -> SC_Output_1.In`,
  Server 0). The SC Output inside each source is kept out of the mixer
  (`scOutput::macroMixerBlocker`). So the bridge, and through it **both** source
  graphs' audio, are pulled onto server 0.
- **Two transition shapes.** (1) *Fallback crossfade*: two whole graphs run; the
  router-level bridge mixes `current`'s ScBus against `next`'s. (2) *Morph in
  place*: one graph; when the cable into the ScBus router changes (BLOOBS:
  `SC_analogBox_1.Out` to `Macro_12.Out_S1`), the topology creates an **internal**
  `PolicyMorphSignal` that is another `AudioBridge` inside the graph
  (`MacroMixerSource.cpp`, `blendInput` lambda in
  `MacroMixerMorphTopology::prepare`). The added SC nodes are staged into the
  live container and get pulled onto the same server as the origin's audio.

## Design

### Principle: affinity decided by the bridge, placement decided by the walk

Give every `AudioBridge` a **server affinity per input**: input A belongs to
server `sa`, input B to server `sb` (either may be "any", meaning today's
behaviour). In `appendOrderedNodes` / `getConnections`, the bridge exposes input
A only when `getGraphTraversalServer()` is `sa`, and input B only on `sb`
(same pattern as SC Mixer Pro). Consequences:

- Server `sa` builds the origin's chain, server `sb` builds the destination's
  chain. The playing server never rebuilds for the incoming graph.
- The bridge synth exists on both servers. On `sa` its `inB` is unmapped
  (silence) and it applies `gainA`; on `sb` `inA` is silence and it applies
  `gainB`. The two outputs are summed by the audio device. Equal Power,
  Linear, FadeOut and Switch keep their meaning unchanged.
- Nothing moves between servers during a fade. Audio is never routed between
  scsynth processes (impossible without a loopback device), only gains change.

For this to play, **each server involved needs an SC Output pulling the
bridge**. See *Output stages* below.

### Allocation policy (`MacroMixerServerAllocator`, new, in the SC addon)

Inputs: the list of serverManagers, the server each live source currently
occupies, per-server CPU. Rules, in order:

1. The origin keeps its server. Never move a playing graph.
2. The destination gets a server that holds **no graph nodes**
   (`serverManager::hasGraphNodes()` false) if one exists, preferring the one
   with the lowest `getAvgCPU()` and whose required SynthDef folders are already
   loaded (`areRequiredSynthdefsLoading()` false).
3. Otherwise the server with the lowest average CPU that is not the origin's.
4. If only one server exists, or the user set the mixer's new `Audio Servers`
   control to *Single*, return the origin's server: today's behaviour, no
   affinity (both inputs "any").
5. At completion the origin's chain is freed by the walk of its server (its
   nodes left the graph), so that server becomes free again. Record the
   destination's server as the mixer's current audio server. The next
   transition alternates.

Expose the decision in Details and Status: "audio: server 0 -> 1".

### Output stages

The bridge must be pulled on every server it may be assigned to. Two options;
implement the first, keep the second as a follow-up.

1. **User-placed SC Outputs.** The host patch holds one SC Output per server,
   all fed by the mixer's ScBus output (Server 0, 1, 2). Document it in
   `macro-mixer.md`. With affinity, a server whose SC Output pulls the bridge
   but has no graph assigned builds just the bridge synth with silent inputs,
   which costs nothing. The allocator only considers servers that have an SC
   Output pulling the bridge; derive that set from the bridge's own `states`
   after the first walk, or from `serverManager::hasConnectedOutput()` plus a
   check that the Output's input chain reaches the bridge.
2. **Mixer-owned stages (later).** The mixer creates hidden `scOutput`s in its
   own host container, one per server, fed by its bridge, so the user needs
   nothing in the patch. `scOutput::macroMixerBlocker` only blocks Outputs
   inside *sources*; the mixer's `graph->host` container is not a source.
   Needs the controller to allow programmatic Outputs and volume/delay to
   follow the host's master. Skip until option 1 is proven.

### Morph in place: when can the new audio go elsewhere?

The internal bridge created for a changed ScBus cable gets the same affinity:
A = origin's server, B = allocated server. The destination's SC nodes are
staged into the live container as today, but the walk of the origin's server
never reaches them (the internal bridge hides input B there), and the walk of
the allocated server reaches only them. Two conditions must hold, checked in
`planMorphCable` for the ScBus type before choosing multi-server:

- **The audio island is replaced wholesale.** Every scNode upstream of the
  ScBus router on the B side must be an *added* node, and every scNode on the
  A side a *removed* node. An SC node that is matched (kept) on both sides
  would have to exist on both servers at once and be re-created at completion,
  which is the glitch we are removing. If any SC node is shared, use a single
  server (today's behaviour). BLOOBS Init to 2_CELLS_AUDIO satisfies the
  condition: analogBox removed, Macro_12 added.
- **At completion the island stays on its server.** When the internal bridge
  is removed and `Macro_12.Out_S1` connects natively to the router, the
  router-level bridge's affinity for input A becomes the allocated server, so
  the origin's server does not pull the new island (it would otherwise rebuild
  it there). Persist this per source graph: `Graph::audioServer`.

Nodes that are not scNodes (Rangers, Portals, Envelope Generator feeding the
analogBox Gate) are unaffected: control cables are host-side; only the
scsynth placement follows the bridge's walk.

### SynthDef readiness

A free server may never have loaded the SynthDefs the destination needs, so
its first rebuild waits for `/d_loadDir` + `/sync` (tens to hundreds of ms,
during which the fade clock holds at 0). Preload: when the mixer imports a
source, call `requireSynthdefFolder` for every folder its SC nodes report
(`scNode::requiredSynthdefFolder()`) **on every server**, not only the one
currently pulling; and at boot the controller already loads all registered
folders on all servers (`loadDefs`). Verify with a cold start: the first morph
into an audio preset must not wait for SynthDefs.

### Timing across servers

All servers open the same CoreAudio device, so they run on one hardware clock:
no drift, at most one hardware buffer of constant offset between them. A
crossfade therefore stays phase-coherent enough for an equal-power sum. Note it
in the doc; no compensation is needed. NRT rendering already treats every
server as its own scsynth job and sums the files afterwards
(`ofxOceanodeSuperColliderController.cpp:556-625`), so recording keeps working
once each server's stems are captured.

### What stays single-server

- Sources that pull audio from a *shared* resource (Portals carrying ScBus,
  buffers on a `Server` parameter such as `scBufferAllocator`, `scFFTHD`):
  these nodes name a server explicitly; if a source contains one, pin that
  source to that server and do not alternate. Report it in Details.
- Trigger (void) and Texture policies: unaffected.
- SC Mixer Pro inside a source: its tracks name servers themselves; leave them.

## Implementation steps

Work in `ofxOceanodeSuperCollider` unless stated. Keep each step buildable.

1. **Bridge affinity.** `AudioBridge` gains `std::array<ofxSCServer*,2>
   affinity{nullptr,nullptr}` (nullptr = any). Override `appendOrderedNodes` and
   `getConnections` following `scMixerPro.cpp:261-287`: on server k, skip input
   i when `affinity[i] && affinity[i]!=k`. In `send()`, keep `gainA`/`gainB`
   as they are; an unmapped input is silent (`resetInputBusses` already maps
   both to the silent bus). Add `setAffinity(ofxSCServer* a, ofxSCServer* b)`
   that requests a routing ticket on every involved server, like a source
   change does.
2. **Allocator.** New `integration/MacroMixerServerAllocator.{h,cpp}` with the
   policy above. Input: `std::vector<serverManager*>`, the origin's server,
   the set of servers that pull the bridge. Output: destination server or
   nullptr (single-server). Pure function plus a small struct; unit-test it
   with fake CPU numbers without a server.
3. **Processor wiring.** In `registerMacroMixerSCBus`, the processor lambda
   receives `(a, b, t, options)`. It needs the servers of A and B. Add to the
   `Value` for ScBus, or to `MacroMixerMixing::Object`, an optional
   `ofxSCServer* server` tag that the bridge sets on its output and that a
   Graph sets on its ScBus router reads (see step 4). When sources change
   (`updateSources`, `changed==true`), ask the allocator for B's server given
   A's, call `setAffinity`, and store the choice so `ready()` waits on both
   servers' tickets.
4. **Graph audio server (core, `MacroMixerSource`).** Add
   `ofxSCServer* audioServer=nullptr` to `MacroMixerSource::Graph` (opaque
   `void*` or a small interface in core, since core must not include SC
   headers; use the existing `ofxOceanodeAutomationExtension`-style pattern:
   a `MacroMixerMixing::Registry` hook `placementOf(Value)` /
   `setPlacement(Value, ...)` registered by the SC addon). The runtime passes
   `current`'s placement when it reads `b` from `next`. On completion
   (`current=std::move(next)`), the placement moves with the graph.
5. **Internal bridge in morphs in place.** In `planMorphCable` for type
   `ScBus`, compute the island condition (all-added on B, all-removed on A,
   using `edit.added`/`edit.removed` and `dynamic_cast<scNode*>`); when it
   holds, mark the plan `multiServer=true`. In `blendInput`, after creating the
   `PolicyMorphSignal`, apply affinity A = graph's `audioServer`, B = allocated.
   At `commit()` of the topology, set the graph's `audioServer` to B's server
   and re-affine the router-level bridge (step 3) before the native cable is
   restored, so the origin's server never pulls the new island. Add a Details
   entry: "ScBus: destination island on server N".
6. **Mixer control.** `ofxOceanodeMacroMixer` gains a dropdown `Audio Servers`
   {Single, Alternate} (default Alternate when more than one server exists),
   saved with the node, read by the runtime at request time like `Log Blend`.
   Status shows the server move during a transition.
7. **SynthDef preload on import.** In `MacroMixerSource::importFolder` (or a
   SC-addon import hook), collect `requiredSynthdefFolder()` of every scNode
   model the source would create and call `requireSynthdefFolder` on all
   servers. Confirm `areRequiredSynthdefsLoading()` is false on the allocated
   server before Start; if not, Details says "loading SynthDefs on server N"
   and the fade waits as today.
8. **Docs.** Update `docs/macro-mixer.md` (*SuperCollider* section and the
   *Source windows* list): the per-server SC Output requirement, the
   allocation rules, what stays single-server, the clock note. Update the SC
   addon README if it documents MacroMixer.

## Tests (python3 scripts in `tests/`, ASan/UBSan harnesses; do not run them
unasked, the user runs them)

- `macro_mixer_sc_regression.py` and `macro_mixer_sc_audio_regression.py`
  already build with the SC addon and a fake/NRT server. Add:
  - Bridge affinity: with two serverManagers, a bridge affined A=s0, B=s1 is
    walked by s0 with only A's chain and by s1 with only B's; both bridge
    states report ready after their tickets; gains apply on both.
  - Allocator unit test: free server preferred over loaded; lowest CPU among
    busy; single server returns nullptr; pinned source stays.
  - Fallback crossfade across servers: origin on s0, destination on s1; at
    completion s0 has no graph nodes; the next transition goes s1 -> s0.
  - Morph in place with an island swap (analogBox-like removed, Macro-like
    added): the added SC nodes are created on s1 only; the origin's s0 never
    rebuilds during the morph (count `graphWillRebuild` calls on s0 == 0 after
    Start); at completion the island remains on s1.
  - Shared SC node across sides: single-server path chosen, Details says why.
- `macro_mixer_regression.py` covers the mixer node's parameter list; add
  `Audio Servers` to the reserved names and order.

## Acceptance

- Transition Init -> 2_CELLS_AUDIO in the BLOOBS MORPH_1 host with SC Outputs on
  Server 0 and 1: no audible glitch on the playing server, Details shows
  "audio: server 0 -> 1", "prepared in" drops because no synths are created on
  the playing server during the Start frame (the SC node creation itself still
  costs host time; measure and report).
- Back-to-back transitions alternate servers and never leave a graph running on
  two servers (check `hasGraphNodes()` per server after completion).
- With one server configured, behaviour is byte-for-byte today's.
- 60 fps hold: the Start frame cost for the audio preset should drop well
  below 100 ms; report the before/after "prepared in" numbers.

## Risks and open points

- **Per-server SC Outputs** change show patches: MORPH_1 needs two or three SC
  Output nodes. Until option 2 (mixer-owned stages) exists, document it loudly.
- **Bus capacity per server** (`-a numAudioBusChannels`, 128 per node output):
  spreading graphs across servers relieves it, but a server's capacity still
  bounds one graph.
- **Ordering of commit vs. affinity change**: if the router-level bridge is
  re-affined after the native cable is restored, the origin server walks the new
  island once (creates then frees its synths): a glitch. Re-affine first, then
  restore the cable, inside one `setBLatency` bundle if possible.
- **Retirement leases**: `AudioBridge::retire` requests routing on every
  server that has nodes; keep that so freed nodes on the old server are
  released only after that server's `/synced`.
- Nested MacroMixers (a mixer inside a source) inherit no placement; leave
  them single-server and note it.
