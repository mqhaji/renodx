# MGSV Temporal Anti-Aliasing

Technical reference for MGSV's optional native-resolution temporal anti-aliasing path. The implementation is deliberately
default-Off and keeps the game's original FXAA and projection behavior whenever TAA is disabled.

Future quality work and the native-resolution DLAA plan are tracked in [ROADMAP.md](ROADMAP.md).

## User controls

The TAA controls are visible under **Effects**:

| Control | Behavior |
|---|---|
| **Temporal Anti-Aliasing** | Enables TAA. Defaults to **Off**. |
| **TAA Jitter Pattern** | **Halton (2,3) — 8 Phase** is the production sequence. **Off** keeps the resolve active with zero projection jitter for diagnosis. |
| **TAA Diagnostic View** | Selects Temporal Resolve, Raw Current, Filtered Current, Raw Velocity Direction, or Raw Velocity Magnitude. |
| **TAA Velocity View Range** | Sets the pixel-motion range represented by full intensity in the two velocity views. |

Changing the jitter pattern or diagnostic view invalidates history. Preset Off disables TAA and restores the vanilla
projection path.

## Frame pipeline

The runtime executes this sequence while TAA is enabled:

1. The native `SetViewMatrixState` hook accepts only the proven gameplay projection callsite and viewport structure.
2. It captures no-jitter projection/view state, applies the current Halton offset to `ShaderManager+0x680`, and publishes
   the exact jitter with the current frame token, sample index, and viewport dimensions.
3. MGSV renders the jittered scene, depth, and native object velocity.
4. `MotionBlurCameraVelocity` writes the final camera/object velocity target. The addon captures its depth and
   object-velocity inputs and keeps both velocity stages in RGBA16F.
5. Immediately before `DOF_ScatterBakeFirst`, the addon dispatches the TAA resolve against the full-resolution scene.
6. The resolved image is copied back before MGSV creates its depth-of-field and motion-blur inputs.
7. The game's FXAA shader becomes a pass-through only for the enabled TAA frame.

The fallback insertion cascade is:

1. `DOF_ScatterBakeFirst` (`0xFE1DC3F8`)
2. The sequence-qualified `CopyRenderBuffer` after DoF
3. The sequence-qualified `CopyRenderBuffer` after motion-blur tile preparation
4. Tonemap or Tonemap 1D-LUT when the earlier passes are absent

Only one resolve may run per frame.

## Native jitter contract

`runtime/projection_jitter.hpp` locates the projection commit using an executable-section AOB plus the complete preceding
projection-copy context. It detours the adjacent `SetViewMatrixState(float*)` function rather than patching a shared
constant buffer or modifying persistent viewport state.

The hook requires all of the following before writing:

- The return address is the verified `buildRendering` callsite.
- The viewport is enabled, full-resolution, perspective, and has a valid camera.
- The vanilla viewport projection exactly matches the active shader-manager projection.
- TAA is still enabled while the publication lock is held.

The hook modifies only projection elements 8 and 9 in the active `ShaderManager+0x680` copy and reasserts the associated
dirty flags. Disabling TAA requires three exact unjittered projection copies for the restoration check. The viewport's
persistent projection remains untouched.

Current and previous no-jitter view-projection matrices are retained in double precision. A current matrix is promoted
to previous only after its matching TAA dispatch succeeds, preventing a skipped frame from corrupting camera history.

### Scoped light correction

VS `0x200DBED9` receives an unjittered `cVSScene` projection while the main scene and depth are jittered. Its scoped
replacement adds the same-frame published offset to final clip XY. The injected offset is zero when TAA or projection
jitter is disabled. This producer-side correction fixed the projection-phase flicker observed on PS `0x6CA8AA97` lights.

## Resolve inputs

The compute shader consumes:

| Register | Input | Contract |
|---|---|---|
| `t0` | Current scene color | RGBA16F clone containing MGSV's sRGB-encoded scene color |
| `t1` | Previous history | RGBA16F in the same encoded scene domain |
| `t2` | Final velocity | RGBA16F preferred; MGSV's packed motion is decoded from `.ba` |
| `t3` | Depth | Full-resolution reverse-Z depth captured from `MotionBlurCameraVelocity` |
| `t4` | Object velocity | RGBA16F clone; `.r` marks pixels with native object motion |

The current frame is decoded before its 3x3 filter. Background pixels use depth-derived camera reprojection from the
native no-jitter matrix relation. Object-mask pixels retain MGSV's bone-aware velocity. Velocity selection currently uses
the center and four diagonal taps and chooses the largest raw depth, matching MGSV's positive reverse-Z nearest surface.

The resolve point-loads and decodes the complete 4x4 history footprint before 16-tap Catmull-Rom reconstruction in linear
light. It clips history to equally blended broad/tight current RGB bounds and computes an adaptive blend from luminance
position and subpixel velocity. Scene alpha is copied exactly from the current frame because downstream MGSV passes use
it for highlight/emissive behavior.

## History and failure policy

Two RGBA16F resources ping-pong as history. History is seeded from current scene color after creation, resize, enable,
pattern change, diagnostic change, or any rejected frame.

The dispatch fails closed and invalidates history when:

- The native jitter publication is missing, stale, or for a different sample.
- The publication dimensions do not match scene color.
- Current native camera state or previous matrix history is unavailable.
- Velocity, depth, or object-velocity capture is missing or stale.
- Input dimensions or required resource formats do not match.
- Compute pipeline or history setup fails.

No temporal output is produced from mismatched frame data.

## Source layout

| File | Role |
|---|---|
| `taa.hpp` | Settings, lifecycle, draw routing, and insertion cascade |
| `runtime/constant_buffers.hpp` | Frame/sample state and Off/Halton jitter generation |
| `runtime/descriptor_tracker.hpp` | Per-command-list pixel SRV tracking |
| `runtime/projection_jitter.hpp` | Native hook, jitter publication, matrix history, and restoration checks |
| `runtime/resolve.hpp` | Resource capture, history lifecycle, compute dispatch, and copy-back |
| `shaders/mgsv_taa.cs_5_0.hlsl` | Temporal resolve |
| `../fox3dfw_constant_srgb_vs_0x200DBED9.vs_5_0.hlsl` | Scoped jitter correction for the mismatched light producer |

## Validated behavior

- Eight bounded synchronization runs completed 600 temporal frames each with one history seed, no native publication or
  dimension rejects, and three exact restoration copies.
- The final velocity path was observed as RGBA16F rather than the original BGRA8 target, substantially reducing
  camera-wide wobble.
- Native jitter and the compute resolve agree on frame, sample, dimensions, and current camera state before dispatch.
- The `0x200DBED9` correction stabilizes the affected lights without changing their zero-jitter position.
- Decoding history texels before Catmull-Rom reconstruction improved temporal quality over encoded interpolation.
- Selecting the largest raw reverse-Z depth preserves thin foreground lines that disappeared with the previous
  smallest-absolute-depth rule.

## Known limitations

- Previous history remains stored in MGSV's encoded scene domain, requiring sixteen point loads and per-texel decoding
  for correct linear-light Catmull-Rom reconstruction. Separate linear history could recover the optimized sampling path.
- There is no previous-depth history or explicit disocclusion mask.
- Thin features have no temporal lock/confidence mechanism and can be removed by current-frame RGB clipping.
- Large camera/FOV discontinuities do not yet trigger a dedicated camera-cut reset.
- Native object-velocity coverage and the approximately 64-pixel packed-motion clamp remain consumer limitations.
- No sharpening is applied; sharpening is deferred until temporal stability is improved.

## Build and manual verification

Build the `mgsv` target with the configured CMake Tools profile. Inspect the addon plus generated `mgsv_taa` and
`0x200DBED9` shader artifacts.

Minimum runtime verification:

1. Launch with TAA disabled and confirm vanilla FXAA and projection behavior.
2. Enable TAA with jitter **Off** and confirm the temporal resolve runs without projection motion.
3. Switch to **Halton** and confirm history resets, edges converge, and corrected lights remain stable.
4. Exercise all five diagnostic views and verify velocity direction/magnitude during camera and character motion.
5. Test static and fast camera motion, aiming, binoculars, menus, camera cuts, DoF and motion blur on/off, and a resolution
   change.
6. Confirm no recurring publication, dimension, capture, setup, or dispatch warnings.
7. Disable TAA and confirm exact projection restoration with no stale-history frame.