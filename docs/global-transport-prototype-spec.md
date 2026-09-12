# Global Transport Prototype for ofxOceanode

> Purpose of this document: capture the current architectural understanding for a prototype `ofxOceanode` branch that introduces a new global transport service. This is not the final `playNodes` timing architecture. It is a focused prototype to test a shared playhead/clock, preserve current Oceanode behavior, and reveal the needs of future rate-aware graph evaluation.

---

## 1. Scope

This document is for an AI agent or developer starting a new `ofxOceanode` branch to prototype a **global transport**.

The branch should:

- introduce a new core transport service, tentatively named `ofxOceanodeTransport`
- keep current Oceanode compatible enough to test real nodes against it
- preserve the current meaning of `frame mode`
- avoid turning this branch into the final multi-rate graph architecture planned for `playNodes`

This branch should **not** try to solve the full future problem of consumer-driven multi-rate graph evaluation. The goal is to add a shared global playhead and shared timing state in a way that current Oceanode can use and evaluate.

---

## 2. Core Conclusion

The transport should live in a new core class such as:

- `src/Managers/ofxOceanodeTransport.h`
- `src/Managers/ofxOceanodeTransport.cpp`

It should **not** live inside `ofxOceanodeTime`.

`ofxOceanodeTime` should become:

- a consumer of transport
- a controller/UI layer around transport
- a bridge to legacy timing behavior where needed

The transport should be the owner of:

- play / stop state
- BPM
- global beat position / playhead
- reset / seek generation state
- transport driver mode

---

## 3. Important Current Oceanode Context

Before implementing, understand these existing timing facts.

### 3.1 Ordinary node outputs are effectively frame-latched

For standard Oceanode node parameters and outputs, the practical assumption is:

- observable output updates happen on the main/update side
- downstream nodes typically see values on the frame grid
- current parameter outputs are not a true timestamped event delivery system

This means a global transport can improve internal timing logic immediately, but it does not automatically create sub-frame output delivery for ordinary params.

### 3.2 `phasor` timing is special and currently more precise

Read:

- [`src/Nodes/Default_Nodes/Generators/phasor.cpp`](../src/Nodes/Default_Nodes/Generators/phasor.cpp)
- [`src/Nodes/Default_Nodes/Base/basePhasor.h`](../src/Nodes/Default_Nodes/Base/basePhasor.h)
- [`src/Nodes/Default_Nodes/Base/basePhasor.cpp`](../src/Nodes/Default_Nodes/Base/basePhasor.cpp)
- [`src/ofxOceanodeTime.cpp`](../src/ofxOceanodeTime.cpp)

`phasor` is a wrapper over `basePhasor`.

`basePhasor` is advanced from BPM/division math and is serviced by `ofxOceanodeTime`. When Oceanode is not in frame mode, it is advanced from the audio callback path in:

- `ofxOceanodeTime::audioOut(...)`

This is currently the closest existing timing mechanism to a more precise shared playhead.

### 3.3 Current `frame mode` has a specific meaning and must be preserved

Do not interpret current Oceanode `frame mode` as only "offline rendering exists".

Read:

- [`src/ofxOceanodeTime.cpp`](../src/ofxOceanodeTime.cpp)
- [`../../ofxOceanodeTextures/src/textureRecorder.cpp`](../../ofxOceanodeTextures/src/textureRecorder.cpp)

Current frame mode means:

- graph timing is advanced on the frame grid
- phasors are advanced by `advanceForFrameRate(targetFR)`
- visually sensitive nodes can remain coherent and safe on frame boundaries
- workflows such as texture recording depend on stable frame-synchronous graph state

`textureRecorder` explicitly sets `ofxOceanodeNodeModelFlags_ForceFrameMode` while recording. Even though the force-frame-mode handling in `ofxOceanodeTime` appears partially commented at the moment, the conceptual contract is clear:

- some workflows require the graph to be evaluated in frame-synchronous mode

This must survive the transport prototype.

---

## 4. What the Prototype Is Trying to Prove

This branch is a discovery prototype. It should help answer:

1. Is a global beat/playhead snapshot enough for most sequencing and timing nodes?
2. Do nodes mostly need:
   - current beat position
   - current BPM
   - play state
   - reset/seek generation
   - previous frame beat position
3. How many nodes need a **precise transport state** versus only a **frame-latched transport snapshot**?
4. How well can nodes like arpeggiators and sequencers work by comparing playhead crossings rather than receiving explicit trigger pulses?
5. How should `ofxOceanodeTime` evolve once transport exists externally?

---

## 5. Non-Goals for This Branch

Do not turn this branch into:

- the final `playNodes` graph evaluator
- a generic multi-rate dependency graph
- a timestamped event-stream graph API
- a full sub-frame delivery system for standard node outputs
- a transport callback mechanism that pushes arbitrary off-thread state into nodes

Also avoid:

- direct transport-thread mutation of OpenGL/texture-facing node state
- assuming all nodes are safe to read or write outside the main thread

---

## 6. Recommended Conceptual Model

There should be **one global transport** and **one authoritative transport state**.

That same transport should expose:

- a current precise state
- a frame-latched snapshot of that same state

This is not "two transports". It is one transport with two ways of being consumed safely.

### 6.1 Precise transport state

Used by timing-sensitive logic.

It should contain at least:

- `bool isPlaying`
- `float bpm`
- `double beatPosition`
- `uint64_t steadyTimeUs`
- `uint64_t generation` or similar reset/seek counter
- `TransportDriverMode driverMode`

Possible driver modes:

- `RealTime`
- `FrameStep`
- future: `External`

### 6.2 Frame-latched transport snapshot

Used by:

- update-driven nodes
- UI nodes
- texture/OpenGL nodes
- recorder workflows

This should be copied once per frame from the same authoritative transport state.

It should contain at least:

- current frame transport state
- previous frame transport state

This allows nodes to detect beat/division crossings between frames without needing direct off-frame callbacks.

---

## 7. Why Both Precise and Frame-Latched Consumption Are Needed

Different node families need different guarantees.

### 7.1 Timing-sensitive nodes

Examples:

- arpeggiators
- sequencers
- event generators

These may want the most accurate beat position possible so they can detect:

- crossed next step
- crossed several steps since last update
- reset/seek changes

### 7.2 Rendering-sensitive nodes

Examples:

- texture nodes
- FBO/OpenGL pipelines
- texture recorder

These should not be driven by arbitrary off-frame state changes. They need:

- a stable transport state for the current frame
- predictable frame-synchronous playhead progression

This is why the prototype should explicitly preserve the current frame-mode semantics.

---

## 8. Relationship to `ofxOceanodeTime`

`ofxOceanodeTime` currently mixes multiple concerns:

- timeline/play state UI
- time propagation
- phasor servicing/discovery
- frame-mode stepping
- audio callback timing

In the prototype, transport should be extracted conceptually from this class.

Recommended direction:

### 8.1 `ofxOceanodeTransport`

Owns:

- authoritative transport state
- transport stepping logic
- driver mode
- frame-latched snapshot generation

### 8.2 `ofxOceanodeTime`

Becomes:

- transport UI/controller
- graph/timeline-facing coordination layer
- legacy bridge for existing systems during the prototype

### 8.3 `basePhasor`

Current phasor timing is a useful reference.

The prototype may either:

- keep using current phasor machinery as-is while transport is introduced
- or progressively move phasor timing to read from the new transport

Do not force this migration too early if it makes the branch harder to evaluate.

---

## 9. Current Frame Mode Must Remain First-Class

This is a hard requirement.

The prototype transport must support at least two transport driver modes:

### 9.1 Real-time mode

Driven by:

- audio callback timing
- or high-resolution steady-clock timing where needed

Purpose:

- interactive playback
- shared precise playhead

### 9.2 Frame-step mode

Driven by:

- frame count / target frame rate
- explicit frame-advanced graph evaluation

Purpose:

- texture recording
- deterministic frame-by-frame playback
- non-real-time rendering workflows
- any graph that must remain visually coherent and stable on frame boundaries

Frame-step mode is not a degraded fallback. It is a valid transport driver mode with its own semantics.

---

## 10. How Nodes Should Use the Prototype Transport

### 10.1 Preferred pattern for most nodes

Nodes should **query** transport, not receive transport-thread callbacks.

Typical node usage:

1. read frame-latched current snapshot
2. read previous frame snapshot
3. compare beat/division crossings
4. update ordinary outputs on the frame side

This is the safest pattern for current Oceanode.

### 10.2 Pattern for timing-sensitive logic

For an arp/sequencer-like node:

- cache last consumed transport beat
- compute current beat
- detect one or more logical step crossings
- process them in order

This allows better internal timing logic even if ordinary outputs remain frame-latched.

### 10.3 Pattern to avoid

Avoid:

- pushing arbitrary off-thread gate changes into current node outputs
- transport callbacks that mutate OpenGL/texture state
- assuming nodes can safely "subscribe" in the sense of receiving arbitrary background execution

---

## 11. What the Prototype Should Probably Expose First

Keep the first API small.

Suggested minimum transport state:

- `isPlaying`
- `bpm`
- `beatPosition`
- `steadyTimeUs`
- `generation`
- `driverMode`

Suggested minimum frame-latched state:

- `current`
- `previous`

Possible API shape:

```cpp
struct ofxOceanodeTransportState {
    bool isPlaying = true;
    float bpm = 120.0f;
    double beatPosition = 0.0;
    uint64_t steadyTimeUs = 0;
    uint64_t generation = 0;
    TransportDriverMode driverMode = TransportDriverMode::RealTime;
};

struct ofxOceanodeFrameTransportState {
    ofxOceanodeTransportState previous;
    ofxOceanodeTransportState current;
};
```

Possible container-facing access:

```cpp
const ofxOceanodeTransportState &getTransportState() const;
const ofxOceanodeFrameTransportState &getFrameTransportState() const;
```

The exact names can change. The important part is:

- one authoritative state
- one frame-latched snapshot

---

## 12. How It Should Be Passed Around

Preferred path:

- transport owned at `ofxOceanode` core level
- container has access to transport
- node models can access it through their container

Avoid:

- global free-for-all singleton access in node code
- passing transport through ordinary parameter connections

Transport is engine context, not patch data.

---

## 13. Integration Strategy for the Prototype

Recommended order:

1. add `ofxOceanodeTransport`
2. wire it into `ofxOceanodeTime` without deleting current systems yet
3. support both real-time and frame-step driver modes
4. expose transport state through the container/node-model path
5. test with a small set of timing-sensitive nodes
6. test with texture/frame-mode workflows

Good test consumers:

- `phasor`
- one arpeggiator/sequencer node
- `textureRecorder`
- at least one texture/OpenGL-sensitive chain

---

## 14. Open Questions the Branch Should Help Answer

These do not need to be fully solved before implementation.

### 14.1 Should phasor become a transport consumer?

Unknown yet:

- phasor may remain a special timing primitive for a while
- or it may be progressively refactored to derive from transport

The branch should reveal whether that migration feels natural.

### 14.2 How much does current Oceanode need push-style transport events?

Current recommendation:

- start with query/snapshot transport

But the branch should observe whether nodes quickly need:

- reset events
- play/stop notifications
- seek notifications

### 14.3 How should frame mode interact with forced frame mode flags?

`textureRecorder` currently uses `ofxOceanodeNodeModelFlags_ForceFrameMode`, but the detection logic in `ofxOceanodeTime` looks partially commented out.

This branch should clarify:

- whether forced frame mode remains a node flag
- whether it becomes a transport driver request
- whether transport mode switching is controlled globally elsewhere

### 14.4 Is a global beat position enough for most sequencing nodes?

This is one of the main questions the branch exists to test.

---

## 15. Explicit Guidance for AI Agents Working on the Branch

When implementing this prototype:

- do not assume this is the final `playNodes` architecture
- preserve current frame-mode semantics
- treat `textureRecorder` and texture/OpenGL safety as first-class constraints
- do not push off-thread updates into ordinary node outputs
- prefer transport snapshots over callback-driven node mutation
- keep the API minimal and learn from real nodes before generalizing

If a design choice is unclear, prefer the option that:

- keeps transport core small
- keeps current Oceanode stable
- preserves determinism in frame-step workflows
- leaves room for future `playNodes` multi-rate evolution

---

## 16. Summary

The prototype should introduce:

- a new `ofxOceanodeTransport`
- one authoritative global transport state
- one frame-latched snapshot of that same state
- support for both real-time and frame-step transport modes
- `ofxOceanodeTime` refocused as a consumer/controller instead of the transport owner

It should preserve:

- current frame-mode semantics
- texture/OpenGL/frame-synchronous safety
- compatibility with current frame-latched output assumptions

It should deliberately postpone:

- generic multi-rate graph evaluation
- sub-frame ordinary output delivery
- final `playNodes` architecture decisions

This branch is a transport prototype meant to reveal the right future abstractions by testing real Oceanode behavior.
