# SuperCollider Non-Realtime Rendering with Oceanode

## Purpose

Add an offline audio-rendering workflow to Oceanode so a frame-mode texture render can produce a synchronized SuperCollider audio file without requiring realtime playback.

The intended result is:

- Oceanode advances its global transport deterministically, one logical frame at a time.
- Texture nodes render exactly one image per logical frame.
- SuperCollider receives an equivalent timestamped event stream.
- `scsynth` renders that event stream to a `.wav` file in non-realtime.
- The image sequence and WAV can be assembled into a synchronized movie afterward.

This design should reuse existing SynthDefs wherever possible. The main changes belong in the SuperCollider server/message layer and in the Oceanode SuperCollider integration, not in individual SynthDefs.

## SuperCollider mechanism

SuperCollider's non-realtime mode is provided by `scsynth -N`. It consumes a prebuilt binary OSC score instead of listening for live OSC messages. The score contains timestamped commands such as synth creation, parameter changes, buffer operations, and node frees.

The `Score.recordNRT` API in `sclang` is a convenience wrapper around this process. The important constraints are:

- all commands must be available before rendering begins;
- there is no network interaction with the NRT server while rendering;
- server replies and interactive feedback are unavailable;
- the output file is written directly by `scsynth` at the requested sample rate and channel count.

References:

- [SuperCollider Non-Realtime Synthesis](https://doc.sccode.org/Guides/Non-Realtime-Synthesis.html)
- [SuperCollider Score](https://doc.sccode.org/Classes/Score.html)
- [scsynth server architecture](https://doc.sccode.org/Reference/Server-Architecture.html)

## Existing Oceanode timing path

The current frame-mode path already provides the correct basis for this feature:

1. `ofxOceanodeTime::update()` scans the graph for `ofxOceanodeNodeModelFlags_ForceFrameMode`.
2. `textureRecorder` sets that flag while recording.
3. The shared `ofxOceanodeTransport` switches to `TransportDriverMode::FrameStep`.
4. The transport advances by `1 / targetFPS` per processed frame, independent of wall-clock rendering time.
5. `ofxOceanode::update()` then calls `ofxOceanodeContainer::update()`.
6. `ofxOceanodeTimelineManager::evaluateAutomation()` reads the same global transport position and applies the current timeline values before node updates.

This means frame mode should remain the authoritative capture clock. The NRT audio system must consume the same logical frame sequence; it must never derive event timestamps from how long the application took to render a frame.

## Existing SuperCollider path

The current SuperCollider integration is realtime-oriented:

- `ofxOceanodeSuperCollider/src/scStart.h` launches a normal UDP `scsynth` process.
- `ofxSuperCollider/src/ofxSCServer.*` sends OSC messages and bundles to that process.
- `ofxSCSynth`, `ofxSCBuffer`, buses, and node classes create and update live server objects.
- `timelineWaveTrack` and other nodes apply transport/timeline state to live synths during normal Oceanode updates.

The NRT implementation should preserve this node-facing API as much as possible. Nodes should not need to know whether they are talking to a realtime server or building an offline score.

## Proposed architecture

### 1. Add a server output mode

Introduce an explicit output mode in the SC server abstraction, conceptually:

```text
RealtimeServer
OfflineScore
```

The existing node calls—create synth, set parameter, free node, allocate/read buffer, send bundle—should be routed to one of two command sinks:

- the existing UDP sink for realtime playback;
- a score sink that serializes the equivalent OSC command with a logical timestamp.

This is preferable to rewriting every SynthDef node or duplicating the behavior of every SC node.

### 2. Add an Oceanode render context

During an offline render, the SC integration needs access to the current logical render position. The context should include at least:

- render frame index;
- target frame rate;
- logical elapsed seconds;
- current and previous transport state;
- current transport beat;
- render generation/session ID;
- whether the current frame is the first or final frame.

The logical elapsed time should be advanced by the frame-step transport. It must not use `steadyTimeUs` or another wall-clock value for score timestamps.

The render context should be set before graph evaluation and remain active while timeline automation and node listeners emit SC commands. Commands generated during that frame then receive the correct logical timestamp automatically.

### 3. Capture complete OSC lifecycle

The score sink must capture more than parameter changes. It must support:

- SynthDef loading or registration;
- buffer allocation and file loading;
- control/audio bus setup;
- synth creation and initial arguments;
- parameter changes;
- group ordering and node movement where required;
- synth release/free;
- a final command after the requested duration so NRT rendering does not stop early.

Resource IDs must be deterministic within a render session. Server queries and feedback-based allocation cannot be used during NRT capture.

### 4. Keep SynthDefs unchanged by default

Existing compiled SynthDefs should be reusable. The NRT server executes the same UGen graphs as the realtime server.

Potential SynthDef-specific exceptions should be documented rather than silently emulated. Examples include:

- hardware or microphone input;
- external realtime devices;
- VST or other plugins that are unavailable to the NRT process;
- behavior depending on `/reply`, `SendReply`, node notifications, or server queries;
- nondeterministic external state.

## Timeline and event semantics

### Continuous values

For ordinary automation, the offline backend can capture the value observed after each frame's timeline evaluation. The timestamp is the logical time of that frame.

The audio server will hold that value between events. This is appropriate for frame-rate controls, but it is not automatically equivalent to an audio-rate phasor or continuous modulation signal.

### Discrete events

Step sequences, piano-roll notes, gates, and triggers must not lose events merely because several logical timeline boundaries occur between two application updates.

The offline timeline path must therefore enumerate every crossed logical event between the previous and current transport positions, in order, and place each event at its exact logical time. It must not only capture the final state visible at the current frame.

This is especially important at high BPM, with coarse frame rates, or when multiple notes share a frame boundary.

### BPM automation

The score timestamp is elapsed render time, while timeline content is expressed in beats. The implementation must use one canonical beat-to-time mapping for both timeline evaluation and SC event timestamps.

For constant BPM, the mapping is straightforward. With BPM automation, the mapping must account for the tempo curve rather than multiplying every beat by the current BPM. The existing timeline `beatToSeconds()` logic is a useful reference, but the offline render context should ideally expose an unambiguous logical time directly.

### Loops and discontinuities

During a normal export, render time should remain monotonic even when the timeline playhead wraps from the loop end to the loop start. The score should contain events at increasing audio times; the beat position may jump while the render timestamp continues forward.

User seeks, manual scrubbing, and arbitrary transport jumps should either be disallowed during capture or explicitly represented as a new render segment. They should not silently produce out-of-order score timestamps.

## Audio output

No special patch node is required for the first version.

The normal SC output routing should feed the server's output buses. The NRT process should be launched with:

- output file path;
- WAV header format;
- sample format, preferably selectable between integer and float;
- sample rate;
- output channel count;
- total duration.

Conceptually this is equivalent to:

```text
scsynth -N score.osc _ output.wav 48000 WAVE float
```

The user-facing feature should be a render/export controller rather than a node. A future dedicated output-routing node could be added if users need to select a submix or render separate stems, but it is not necessary for synchronized stereo output.

## User workflow

Recommended first-version workflow:

1. The user builds the normal Oceanode patch, including `textureRecorder` and the desired SC output path.
2. The user opens an Oceanode render/export panel.
3. The panel exposes:
   - output directory and base name;
   - frame rate;
   - render range or duration;
   - render video frames;
   - render audio;
   - audio sample rate and channel count;
   - WAV sample format;
   - optional audio tail duration for reverb/release tails.
4. The user presses one `Render` button.
5. Oceanode prepares the render session, resets the transport to the requested start, and enters frame mode.
6. Each logical frame updates the graph, writes the texture frame, and captures the corresponding SC events.
7. After the last frame, Oceanode finalizes the score and launches the NRT SC render.
8. The panel reports progress through capture and audio rendering, then shows links to the image sequence, WAV, and optional movie.

The existing texture recorder can remain compatible with its current workflow. A useful integration is an explicit `Render Audio` option in the texture recording/export panel, rather than making every ordinary texture recording unexpectedly launch SuperCollider.

The render session should preserve and restore the user's prior play state, transport position, frame-mode setting, and active SC server mode after completion or cancellation.

## Render-session state machine

The implementation should expose clear states:

```text
Idle
Preparing
CapturingFramesAndScore
RenderingAudio
Finalizing
Complete
Cancelled
Failed
```

Errors should identify the stage and preserve intermediate artifacts where possible, especially the score file and captured image sequence.

## Suggested implementation phases

### Phase 1: command-capture abstraction

- Introduce a common SC command-sink interface.
- Keep existing realtime behavior unchanged.
- Add an in-memory score sink with deterministic ordering and timestamps.
- Add serialization tests for common OSC commands.

### Phase 2: standalone NRT audio test

- Render a minimal existing SynthDef to WAV from a manually constructed score.
- Validate sample rate, channel count, duration, buffer loading, and synth cleanup.
- Confirm that no SynthDef rewrite is required.

### Phase 3: Oceanode frame-mode capture

- Add the render context and session lifecycle.
- Capture commands emitted during frame-mode graph evaluation.
- Use logical transport time for timestamps.
- Ensure realtime scheduler dispatch is not used as the NRT delivery mechanism.

### Phase 4: timeline correctness

- Capture all crossed discrete timeline events.
- Cover BPM automation, loops, resets, and multiple events within one frame.
- Ensure parameter values generated by timeline automation reach the score sink in the same frame as the visual graph.

### Phase 5: export UI and combined workflow

- Add the render/export panel.
- Integrate texture sequence, WAV output, optional ffmpeg muxing, cancellation, and progress reporting.
- Keep ordinary realtime playback and ordinary texture recording unchanged.

### Phase 6: compatibility audit

- Test each SuperCollider node family.
- Mark nodes as NRT-compatible, partially compatible, or realtime-only.
- Provide a preflight report before rendering rather than failing deep into the render.

## Testing and acceptance criteria

The feature is ready for practical use when:

- the same existing SynthDef can render realtime and NRT without source changes;
- a known tempo-synced visual marker and SC click occur at the same timeline positions;
- heavy CPU load changes wall-clock render duration but not the resulting media duration;
- the number of texture frames equals the requested frame count;
- the WAV duration matches the logical render duration, plus any explicitly requested tail;
- timeline loops preserve monotonic audio time;
- no discrete note or trigger is lost when multiple events fall between application frames;
- buffer-based instruments render correctly;
- failures report a useful cause and leave recoverable intermediate files;
- cancelling a render restores the previous Oceanode/SC state.

## Non-goals for the first version

- rewriting SynthDefs into a special offline format;
- making SuperCollider audio itself advance one video frame at a time;
- supporting every realtime-only node or external device;
- rendering from live microphone input;
- replacing normal realtime playback;
- implementing multitrack/stem export before the stereo master path is reliable.

## Main design principle

Frame mode should remain Oceanode's deterministic control clock. SuperCollider NRT should be treated as a renderer of a complete, timestamped score generated from that clock—not as another realtime participant that is slowed down manually.
