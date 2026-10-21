# Core transport and optional timeline dependencies

Transport is part of the supported ofxOceanode core. Consumers should include
its headers directly, rather than testing whether an experimental branch is
installed.

| Capability | Owner | Conditional compilation |
| --- | --- | --- |
| Shared clock, BPM, play/pause/stop, seek, looping, external clock, frame stepping | ofxOceanode | None |
| Deterministic transport random utilities | ofxOceanode | None |
| Timestamped backend registration and send suppression | ofxOceanode | None |
| Instrument parameter/value-scale metadata | ofxOceanode | None required; the legacy metadata marker is always defined by core |
| SuperCollider NRT recording and rendering | ofxOceanodeSuperCollider + core transport | None |
| Timeline clips, lanes, wave tracks and parameter publishing UI | ofxOceanodeTimeline | Explicit addon build capability |

`ofxOceanodeTimeline/addon_config.mk` exports `OFX_OCEANODE_HAS_TIMELINE=1`.
SuperCollider derives `OFXOCEANODESC_HAS_TIMELINE` from that flag. It no longer
probes timeline headers: a header on an include path does not establish that
the addon is linked. Projects must regenerate after changing `addons.make`.

SuperCollider's optional publishing menus, indicators and wave-track node also
check that a timeline service is attached to their host. Core scheduling is
registered regardless of whether that service exists.

Sequencers obtain optional bar-length metadata through the existing core
extension interface, using their existing 4/4 default without an extension.
They no longer include or call the concrete timeline manager for this purpose.
This audit does not relocate the timeline's meter settings or change playback.

The transport guards were removed from core's Counter/Ramp/Phasor helpers,
ofxSantiNodes and ofxOceanodeCollection. Existing free-running versus synced
choices remain runtime controls.

Validation from the core repository:

```sh
python3 tests/integration_compile.py
python3 tests/addon_dependencies_regression.py
python3 tests/transport_regression.py
python3 ../ofxOceanodeSuperCollider/tools/transport_sync_test/run.py
python3 ../ofxOceanodeSuperCollider/tools/rhythmbox_test/run.py
```

The addon matrix uses C++23, matching the local host project configurations.
It checks both timeline modes and rejects timeline-manager references in the
selected core-only object files. Its scheduling adapter tests use ASan/UBSan
without linking any timeline code. It does not open an audio device or exercise
the graphical editor.
