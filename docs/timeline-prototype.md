# Timeline

Each container owns an internal `ofxOceanodeTimelineManager`. The manager owns
tracks, parameter bindings, clips and their automation lanes; it is not a
canvas node.

## Minimal usage

```cpp
auto& timeline = oceanode.getTimelineManager();
std::string trackId = timeline.createTrack("Additive Automation");

auto& parameter = static_cast<ofxOceanodeAbstractParameter&>(
    node.getParameters().get("Pitch"));
std::string bindingId = timeline.addBinding(trackId, parameter);
std::string clipId = timeline.createClip(trackId, "Phrase", 0.0, 8.0);
std::string laneId = timeline.createLane(
    trackId, clipId, "Pitch", ofxOceanodeTimelineLaneType::Step);
timeline.addBindingToLane(trackId, clipId, laneId, bindingId);
timeline.setClipStep(trackId, clipId, laneId, 0.0, "60", 1.0);
timeline.setClipStep(trackId, clipId, laneId, 1.0, "64", 0.5);
timeline.setClipStep(trackId, clipId, laneId, 1.5, "67", 2.5);
```

One track can contain several clips, and one clip can contain Step, Curve and
Piano Roll lanes. Several lanes or overlapping clips assigned to the same
binding are summed. A parameter can also be bound on several tracks; each
binding's automation mode (`Replace`, `Add`, `Multiply`, `Min` or `Max`)
controls how those track-level contributions combine.

Clip source duration and visible duration are independent. Clips can be moved,
cropped, stretched or repeated without changing lane data. “Consolidate
content” permanently removes source data hidden beyond a cropped right edge.

Bindings persist in the preset's `timeline.json` file. Targets are referenced
through the same parameter path convention used by Custom GUI. Missing targets
remain in the timeline data and are marked in memory instead of deleting their
lanes.
