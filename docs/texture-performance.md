# Color texture precision and transfers

Config > **Color texture precision** is a shared setting, saved by **Save Config**
and restored by **Load Config**. It defaults to RGBA32F, including when loading
an older config without the setting. Unsupported saved precision values use 32F.
Changing it reallocates participating color render targets on their next render,
without editing individual nodes. The source texture of a bypass/passthrough
retains its own format; it is not copied just to change precision.

| Setting | Nominal bytes per RGBA pixel | Tradeoff |
| --- | --- | --- |
| RGBA32F (default) | 16 | Existing floating-point precision and HDR range. |
| RGBA16F | 8 | Lower floating-point precision/range; retains alpha and HDR. |
| RGBA8 | 4 | Normalized 8-bit channels; clamps to [0,1], may cause banding. |

These are storage sizes, not measured speedups. Driver storage, GPU work,
readback waits and the rest of the graph determine the actual frame rate.
This setting preserves the alpha channel; RGB32F is not offered because dropping
alpha would change compositing and may not produce a smaller hardware layout.

Participating nodes in the sibling `ofxOceanodeTextures` addon: Color Texture,
Color By Value, Gradient, Gradient 2, Noise Texture, Mixer, Mixer Alpha, Simple
Effect (including render/history buffers), Texture Channels, Inject Alpha,
Sub Texture, Texture Flip, Texture Resizer, Texture Unifier, Vector to Texture,
Texture Receiver and Texture Sender's compositing buffer. The core MacroMixer
texture compositor and Lightnet's Texture Region Mapper, Retexturizer and
Texture Easer also use the setting. Effect history is discarded on format
changes; Texture Easer's captured snapshot takes the new format on its next
capture. Other addons must explicitly adopt the shared format helper.

Oscillators, index textures and other numerical/analysis targets keep their
explicit formats. Recording retains its own format controls. Small fixed black
fallback textures are unchanged. No oscillator shader or algorithm is modified.

## Transfer options

**Texture Reader > Inspector > Async Readback** defaults to off. Synchronous
readback reuses its pixel storage. The optional desktop-GL async path uses two
PBOs and fences. It submits the current image, then consumes a completed earlier
image on a subsequent draw: normally one frame of latency, potentially longer
when the GPU is busy. It never waits for an unfinished fence. When both slots are
busy it skips submission and holds the previous output. Mode/input allocation,
format, dimension or separator changes discard pending samples. Until a sample
is ready, outputs retain their prior values (initially the node defaults).
Deactivation/destruction releases pending buffers/fences. OpenGL ES and desktop
contexts without the required sync/map-buffer capabilities use synchronous reads.
Single/dual-channel input is treated as grayscale; RGB outputs and separators
are filled consistently in both modes.

**Texture Sender > Inspector > Direct Publish (Keep Alpha)** defaults to off.
When enabled at opacity 1, publishes the source texture directly to Syphon,
avoiding the intermediate draw. This keeps source alpha; the original route
composites over black and can change alpha according to the host blend state.
Other opacity values retain the original compositing path. This is opt-in to
avoid changing existing transparent outputs.

Vector to Texture retains upload storage and only rebuilds/uploads when its
input, dimensions or format change. It retains the compositing pass to preserve
existing padded-pixel alpha, blend and tint behavior. The unused depth/stencil
attachments are omitted. Texture Region Mapper retains its quad, pads uniform
arrays safely, and caches mapping and opacity preparation separately. Shared
mapping uniforms are uploaded only when mapping parameters change.

## Verification

`python3 tests/texture_performance_gl_regression.py` uses production nodes in a
native macOS CGL context. It checks upload reuse, long/short vectors against the
previous upload+FBO rendering, RGB/separator output, async latency and resize,
monochrome parity, live 32F/16F/8/32F changes, alpha and mapping-cache pixels.
It runs with ASan/UBSan; libc++ container annotations are disabled across the
precompiled, uninstrumented OF library boundary, while ASan heap checks remain.
The existing `tests/macro_mixer_texture_regression.py` covers the default 32F
MacroMixer shader output and numeric texture readback integration.

These checks do not measure Lightnet's live frame rate or verify a live Syphon
receiver. Rebuild core, texture addon and Lightnet together before comparing the
same patch with profiling stopped. Show Mode omits Scope, controls and canvas
rendering; it continues container processing and texture output.
