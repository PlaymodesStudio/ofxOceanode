# BIENAL playback OpenGL error investigation

Investigated `SHARED_DATA/Presets/BIENAL/1--BIENAL_1` on 2026-10-22.
The shared data root is
`/Users/santiagovilanova/PM Dropbox/PLAYMODES_STUDIO/2026/LIQUIDTILES_BIENAL/SOFT/SHARED_DATA`.
Lightnet's `ofApp.cpp` selects this same root and disables ARB textures;
its `main.cpp` requests OpenGL 4.1.

## Findings

Recursively followed local Macro folders and global `MacroPath` references into
`SHARED_DATA/Macros`. All 121 distinct container folders visited had readable
`modules.json` files. Repeated uses of a global definition were deduplicated for
this inspection, so these numbers are not runtime instance counts.

The texture graph extends through global definitions including
`LIGHTNET/MEGA_MIXER_LIGHTNET_v1`, `LIGHTNET/ABX_Manager2`,
`LIGHTNET/FULLER_TEXTURE_GENERATOR`, `LIGHTNET/KALEIDOS/KALEIDOS_MACRO_3`,
`LIGHTNET/BOLAS/2BOLAS_1`, and `LIGHTNET/MATTES/*`.
The scanned definitions contain 10 Mixer nodes, 63 Mixer Alpha nodes,
10 Oscillator Texture nodes, 9 Oscillator Texture 2 nodes, 15 Texture Readers,
and 13 Lightnet Retexturizers, among other texture processing nodes.
Lightnet's Retexturizer, Texture Easer and Texture Region Mapper implementations
are in `apps/Playmodes/Lightnet/src`, outside the texture addon.

The generic `OpenGL error: 1282` message is emitted by `mixer.h` and
`oscillatorTexture.cpp` while draining the context's accumulated error flags.
Without a checkpoint before each node, that message does not identify the
originating node. A previous node can leave an error that a later Mixer reports.
The reported adjacent stream-underflow message was unavailable for inspection;
no causal connection to the GL error has been established.

Compiled and linked 21 programs in a native macOS OpenGL 4.1 Metal context:
13 embedded texture shaders, the preset's Levels/HSLFilter/CircularGradient/
CurtainWipe/Invert effect files, and the three custom Lightnet shader pairs
mentioned above. All passed. Inspected active uniform types against their
setters; this check did not identify a mismatch.

A suspected Retexturizer setup problem (unbinding the element buffer after
VAO 0) returned **no error** on this machine. No Retexturizer change was made.
These isolated checks do not reproduce the complete live graph or establish
the cause of the intermittent playback error.

## Capture the next occurrence

Rebuild Lightnet with the updated core and texture addon. In **Profiling**, turn
on **Trace OpenGL errors**, load the BIENAL preset and play the sequence.
This toggle works independently of CPU profiling. Alternatively launch the
executable with `OCEANODE_GL_TRACE=1` to include loading/startup playback.

Core messages include the canvas display path, node type/identifier and callback
phase. Checkpoints run before and after model and GUI update/draw callbacks.
For nested Macros, a pending parent error is collected before entering a child,
so the child is not blamed for an inherited error. Synchronous downstream work
inside a callback is still part of that callback; these logs identify a
callback boundary, not the exact GL command. Errors outside callbacks are
explicitly marked as unattributed.

The existing Mixer and Oscillator Texture error drains now include their node
and canvas identifiers too. Other addon code that consumes `glGetError()` can
still hide errors from the outer tracer. GL polling consumes the error flags
and can affect frame timing; leave tracing off during ordinary operation.

Validation: `python3 tests/gl_diagnostics_regression.py` checks disabled behavior,
environment opt-in, inherited errors, nested attribution and callback phases.
The modified core and texture addon translation units were syntax checked
against the local openFrameworks headers. The full Lightnet application has
not been rebuilt or run with this preset as part of this investigation.

## Follow-up: localized playback error

The user's trace identifies
`E / Local / KALEIDOS_MACRO_3/Indexer Texture_1 | draw | after call`.
In the global `KALEIDOS_MACRO_3` definition, this indexer's Size.X is fed by
`Vector_Item_Operations_7` (multiplying the `SizeXGroup` vector by 42), and
Size.Y is fed by `Vector_Operations_2` (the vector size). Its output feeds
`Oscillator_Texture_1`. Thus dimensions can change as the group vector changes.

A native test of the production Indexer Texture did not reproduce 1282 during
ordinary resizing or reactivation. It did confirm that zero dimensions left the
previous texture published instead of suspending output. The source also
unconditionally dereferenced `max_element` for empty size vectors.

The node now suspends output for nonpositive/empty dimensions, preserves the
last valid parameter ranges, and resumes when valid dimensions return. Added
opt-in checkpoints distinguish initialization, resizing, buffer uploads (with
parameter, byte offset, upload size, buffer capacity and dimensions), binding
each texture buffer, uniforms, rectangle drawing, state restoration, and output
publication/downstream listeners. These use the same Profiling trace checkbox.
The exact operation producing the user's 1282 remains unconfirmed; the
invalid-size correction is not claimed as a reproduced fix for that error.

Validation: the old implementation failed the zero-dimension null-output check.
The updated production node passed a native CGL test under ASan/UBSan covering
the preset's 252x18 dimensions, non-square resizing, zero/negative/empty sizes,
recovery, pixel readback and repeated reactivation. The committed test is
`tests/indexer_texture_gl_regression.py`; it uses the existing OF native fixture
and requires the sibling texture addon. Checkpoint attribution is also covered
by `tests/gl_diagnostics_regression.py`.
