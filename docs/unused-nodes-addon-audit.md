# Remove unused nodes: sibling addon audit

Read-only audit of the models registered by the sibling addons, against the
rules in `unused-node-cleanup.md`. Nothing in those repositories was changed:
several have uncommitted work, so applying this list is left to you. The audit
was done by reading code, not by running it, and line numbers are as found at
the time. Check each row before editing.

A node is kept as a sink when it is registered with the `Sink` flag, overrides
`isSink()`, or publishes no output pin. Everything else with an output pin is
treated as pure and is listed as removable when nothing consumes it.

The flag goes between the category and any constructor arguments:

```cpp
o.registerModel<ildaMask>("ILDA", ofxOceanodeNodeRegistrationFlags::Sink, ildaController);
```

## Status

The firm `Sink` flags in the next table, 26 registrations, are applied. Each
addon has them committed on a local branch named `unused-nodes-sinks`, one
commit per repo, nothing pushed: ofxOceanodeLights, ofxOceanodeLaser,
ofxOceanodeTextures, ofxSantiNodes, ofxEloiNodes, ofxOceanodeAstres and
ofxOceanodeSuperCollider. The judgement calls, the Playmodes output fix and the
deprecated flag in Collection are not applied.

## Totals

| Group | Models | Need `Sink` | Output fix | Judgement call |
| --- | --- | --- | --- | --- |
| SuperCollider | 62 | 11 | 0 | 1 |
| Textures, NDI, VectorGraphics | 87 | 4 | 0 | 3 |
| Lights, Laser, Osc, Midi, Forms | 33 | 4 | 0 | 0 |
| SantiNodes | 200 | 5 | 0 | 5 |
| Eloi, Playmodes, Astres, Graaf, Collection, others | 141 | 4 | 24 | 3 |

## Apply these first: need the `Sink` flag

| Addon | Model | Registration | Why |
| --- | --- | --- | --- |
| Lights | movingheadController | ofxOceanodeLights.cpp:29 | Calibration window, writes a calibration file, has output pins. |
| Laser | ildaMask | ofxOceanodeLaser.cpp:24 | Writes the shared mask into the laser output. |
| Laser | laserSimulator | ofxOceanodeLaser.cpp:25 | Draws its own window; its texture pin is usually unconnected. |
| Textures | textureRecorder | ofxOceanodeTexturesNodesMedia.cpp:29 | Writes images and video to disk. |
| Textures | textureRecorderFFmpeg | ofxOceanodeTexturesNodesMedia.cpp:30 | Runs ffmpeg and writes video. |
| Textures | videoPlayer | ofxOceanodeTexturesNodesMedia.cpp:24 | Plays a video file, audio included. |
| SantiNodes | BPMControl | ofxSantiNodesSequencing.cpp:74 (OSC guard) | Sends OSC and can set the BPM. |
| SantiNodes | framerateControl | ofxSantiNodesSequencing.cpp:80 | Sets the global frame rate. |
| SantiNodes | globalSnapshots | ofxSantiNodesUtility.cpp:138 | Writes other nodes' parameters and snapshot files. |
| SantiNodes | rotoControlConfig | ofxSantiNodesUtility.cpp:145 (MIDI guard) | Owns a serial device. |
| SantiNodes | vectorFile | ofxSantiNodesMath.cpp:127 | Appends to a file on a trigger. |
| EloiNodes | saveVectorsToFile | ofxEloiNodes.cpp:67 | Writes a CSV. |
| Astres | AstresCalibration | ofxOceanodeAstres.cpp:53 | Writes calibration state on the shared controller. |
| Astres | heliostatDebug | ofxOceanodeAstres.cpp:80 | Sends OSC on its trigger pins. |
| Astres | starwatcherDebug | ofxOceanodeAstres.cpp:83 | Sends OSC. |
| SuperCollider | pinkTromboneNode | ...NodesSynths.cpp:41 | Owns a CoreAudio output unit and plays regardless of the graph. |
| SuperCollider | scInfo | ...NodesUtils.cpp:36 | Persistent info synth outside the graph, own window. |
| SuperCollider | scFFT, scFFTHD, scSpectrogram, scWavescope | ...NodesAnalysis.cpp:34-37 | Own display windows and persistent analysis synths. |
| SuperCollider | scRecbufHold, scMultiTrackRecorder, scNRTRecorder | ...NodesUtils.cpp:42, 52, 53 | Write WAV files. |
| SuperCollider | scBufferAllocator | ...NodesUtils.cpp:40 | Persistent recorder synth per channel. Weakest case. |
| SuperCollider | scBufferBrowser | ...NodesUtils.cpp:41 | Own window and auditions to hardware. Judgement call. |

## Judgement calls

| Addon | Model | Lean | Note |
| --- | --- | --- | --- |
| Lights | followPoint | Sink | Only effect is a monitor window. |
| Textures | interactiveCanvas | Sink | Always opens a viewer window. |
| Textures | cameraInput, textureReceiver | Pure | Nothing outside the graph depends on them. |
| NDI | NDICamera | Pure | Receives into a texture only. |
| SantiNodes | histogramVector, pianoKeyboard | Sink | Viewer windows that also output. |
| SantiNodes | chordProgressions, multiSliderGrid, polyphonicArpeggiator | Pure | Write files only on a user click. |
| EloiNodes | skullIllusion, signesManualDrawer | Pure | Save on click; the second holds hand-drawn data. |
| Astres | AstresOrbitalObjects | Pure | Runs a download script only on a trigger. |
| SuperCollider | timelineWaveTrack | Pure | Registers as the timeline's wave provider; unclear whether the timeline sink rule covers that. |

## SuperCollider: why synth boxes are not flagged

The SC server graph is rooted only at `scOutput`. The graph builder walks back
from each output and only builds synths for nodes it reaches. A synth, effect,
mixer or sequencer box that feeds no output is inert, and one that does feed an
output is kept by the cable rule. So these boxes are pure, and are not flagged.
The cost of flagging them anyway would be that unwired boxes could never be
cleaned up. A few analyzers (`scA2k`, `scPitchTracker`, `scBeatTracker`) create a
persistent synth outside the graph but only publish pins, and are also pure.

## Needs a change in another repository

All 24 Playmodes video nodes publish their frame through a plain `addParameter`,
so the shape rule keeps them forever. Converting each frame pin to
`addOutputParameter` in the ofxPlaymodes sources makes them removable. The list:
LumaFilterNodeBased, VideoBufferNodeBased, VideoHeaderNodeBased,
FeedbackFilterNodeBased, RepeaterFilter, LooperFilter, VideoTrioRendererNodeBased,
FrameInspector, MultixFilter, EdgeBlendNodeBased, kaleidoscopeNodeBased,
GatorFilter, VideoPsLayer, VideoMixer2Ch, ColorCorrectFilter, GammaFilter,
VideoSpliter, VideoSwitcher, BeatControl, Multix3DFilter, HalfToneFilter,
inputProcessingFilter, MirrorRenderer, ImageFileGrabber.

Do not convert `VideoGrabberNodeBased` and `VideoFileGrabber` on their own:
they own a camera and play audio, so convert them together with the `Sink` flag.

## Registrations a flag edit needs care with

- Constructor arguments come after the flag: ReaperOSCChannel
  (ofxEloiNodes.cpp:61), Astres lines 53, 70, 79, 80 and 82, ponkShape.
- Conditional blocks: BPMControl (OSC), rotoControlConfig (MIDI), yoloPose,
  timelineWaveTrack.
- Models registered by name through `refreshModelNamed` (SuperCollider
  synthdefs, `simpleEffect` in Textures) keep only a flag they were first
  registered with. Those are pure, so nothing is needed.
- Some SuperCollider nodes add output pins after construction. The check runs at
  review time, so this is fine.
- `ofxOceanodeCollection.cpp:46-76` still passes the deprecated
  `CanRemoveWhenUnused` on all 31 lines. It is ignored and can be deleted.
