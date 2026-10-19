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

One track can contain several clips, and one clip can contain Step, Curve, Number and
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

## Number clips

Choose **Number** in the clip type selector or lane type menu. Click in the
editor to add a timed number and type its value. Each entry holds its absolute
value until the next entry; there is no interpolation or range mapping.
New lanes start with 0 at beat 0. The first entry also supplies the value before
its timestamp, and the last holds through the clip's end.

Drag a marker to move it using the timeline grid. Double-click or right-click
its marker to edit its value or exact source beat. Shift-click a marker, or use
**Delete number** in its popup, to remove it. Clip stretch, repetition, splitting
and preset save/load preserve the number sequence. Parameter range clamping
and track blend modes apply as usual.

The model API provides `setClipNumber(trackId, clipId, laneId, beat, value)`;
setting a number at an existing timestamp replaces that entry. Number lanes
store absolute values in `curvePoints`, with step interpolation.

## Dropdown clips

**Dropdown** sequences a dropdown parameter (an int or float parameter with
dropdown options): it is the default clip type when such a parameter is
published to the timeline. Click in the editor to choose an option from that
point on; the bound parameter's option names are listed in the popup. Each
choice holds until the next one and is drawn as a block labelled with the
option name, coloured per option. Moving, editing the beat and deleting work
as in Number clips (**Delete option change**).

A Dropdown lane is a Number lane holding option indices: the same
`curvePoints` storage, `setClipNumber` API, splitting and stretching. Values
are always applied as whole indices. Option names are read live from the
parameter, so a lane whose parameter is missing shows indices and edits them
numerically. Switching a Number lane to Dropdown rounds its values to indices.
