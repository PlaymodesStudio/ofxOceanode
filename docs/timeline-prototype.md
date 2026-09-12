# Timeline prototype

The first timeline vertical is intentionally small and keeps the old timeline
nodes available. It adds an internal `ofxOceanodeTimelineManager` to each
container. The manager owns tracks, parameter bindings and step lanes; it is
not a canvas node.

## Minimal usage

```cpp
auto& timeline = oceanode.getTimelineManager();
std::string trackId = timeline.createTrack("Additive Automation");

auto& parameter = static_cast<ofxOceanodeAbstractParameter&>(
    node.getParameters().get("Pitch"));
std::string bindingId = timeline.addBinding(trackId, parameter);

auto* lane = timeline.getStepLane(trackId, bindingId);
lane->lengthBeats = 8.0;
lane->loop = true;
timeline.setStep(trackId, bindingId, 0.0, "60", 1.0);
timeline.setStep(trackId, bindingId, 1.0, "64", 0.5);
timeline.setStep(trackId, bindingId, 1.5, "67", 2.5);
```

Steps are sorted by `startBeat`. A positive `durationBeats` gives a variable
length step. A zero duration preserves `stepValueTrack` semantics and holds
the value until the next step or the end of the lane. Gaps use the lane
fallback value, or the binding default when no fallback is set. `Replace` is
the only automation mode in this phase and is applied after the container has
updated its nodes.

Bindings persist in the preset's `timeline.json` file. Targets are referenced
through the same parameter path convention used by Custom GUI. Missing targets
remain in the timeline data and are marked in memory instead of deleting their
lanes.
