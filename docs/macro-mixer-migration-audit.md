# PresetMixer → MacroMixer migration audit

Audit date: 2026-10-17. No remotes were updated.

Each repository has a `codex/archive-preset-mixer-2026-10-17` snapshot of its
PresetMixer development and a `codex/macro-mixer` working branch at the baseline
below. Snapshot commits were created with a separate index, preserving the user’s
staging state. Original feature branches were not rewritten.

| Repository | MacroMixer base | Preserved snapshot | Result |
| --- | --- | --- | --- |
| ofxOceanode | `19630c5702b0` | `71ec24fef794` | Replaced custom preset importer/runtime interface with native Macro sources; retained type mixing, Inspector, transitions and persistence. Removed the experimental container Mode and connection adoption API. |
| ofxOceanodeCollection | `67e957c746e4` | `48a8488ca6ee` | Restored pre-feature registration header. No second computational-node registration API remains. |
| ofxOceanodeVectorGraphics | `98d49bddee98` | `2c4088ca5358` | Renamed/reused Fatline mixing adapter and existing type-registration integration. Unrelated Body Repulsion files, registration and documentation remain untouched. |
| ofxOceanodeSuperCollider | `b0a82c8306a2` | `300245b2ef03` | Renamed/reused ScBus bridge, graph leases, server allocation/rebuild acknowledgements and SynthDefs. Preserved instance teardown fixes required for retiring Macros. |
| ofxOceanodeTextures | `154121d57922` | `2fa9b3673e1f` | Retained per-instance texture lifecycle/receiver fixes, RGBA preservation, vector upload bounds and initialized metadata. These apply equally to native Macro instances and contain no preset-specific API. |
| ofxOceanodeLaser | `fc03a2320502` | `7e8c69d1b871` | Renamed capability declarations to macroMixerBlocker; retained external-output and Laser Simulator exclusions. |

The Oceanode baseline is `19630c57`, the parent of the original PresetMixer
commit `42d31b77`. Collection uses `67e957c`, the parent of its feature commit
`8bdbda8`. The other four repositories had uncommitted mixer changes, so their
pre-feature bases are their previously checked-out HEADs.

The audit searched sibling addon source trees for PresetMixer references and
inspected working diffs and recent histories. No mixer-specific source changes
were identified in ofxEloiNodes, ofxOceanodeLights, ofxOceanodeNDI, ofxOceanodeOsc,
ofxOceanodeMidi, ofxSantiNodes or ofxSuperCollider. Their existing unrelated work
was left in place.

Oceanode’s Noise smoothing change was bundled in the initial experimental commit
but is unrelated to mixing and remains intact. Oscillator/Phasor/router initial
value fixes remain because native Macro construction also runs those callbacks.
The child-container BPM fix keeps construction from overwriting parent tempo.
Unrelated untracked regression files and build output remain untouched.

Old experimental tests/fixtures and the implementation diary are available on
Oceanode’s archive branch. New tests cover native Macro execution and the adapted
type backends. The working implementation changes are uncommitted for review.

Validation completed under ASan/UBSan: native Macro lifecycle, extra/selected
routers, input injection, feedback, nested local/global Macros, positive global
reloads and forbidden-edit rejection, file-dependency cycle detection, source
failures, save/load, outer cable restoration and independent clipboard copies.
Numeric/Boolean policy checks, real CGL texture tests, real Fatline tests, captured
SC server/traversal tests and an actual offline SuperCollider audio render passed.
Core and addon integration syntax checks used the real sibling dependency headers.
The native windowed host and physical output devices were not run.
