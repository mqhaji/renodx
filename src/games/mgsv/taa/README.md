# MGSV Temporal Anti-Aliasing

Technical reference for MGSV's optional native-resolution temporal anti-aliasing path. The implementation is deliberately
default-Off and keeps the game's original FXAA and projection behavior whenever TAA is disabled.

Future quality work and the native-resolution DLAA plan are tracked in [ROADMAP.md](ROADMAP.md).

## User controls

**Temporal Anti-Aliasing** is under **Effects**. Jitter, resolve tuning, and motion unclamping are under
**TAA Diagnostics**; extended motion/jitter views are compile-time gated:

| Control | Behavior |
|---|---|
| **Temporal Anti-Aliasing** | Enables TAA. Defaults to **Off**. |
| **TAA Jitter Pattern** | **Halton (2,3) — 8 Phase** is the production sequence. **Off** keeps the resolve active with zero projection jitter for diagnosis. |
| **TAA Diagnostic View** | Gated diagnostic-build control selecting Temporal Resolve, current color, masks, or motion views. |
| **TAA Velocity View Range** | Gated diagnostic-build control setting the pixel-motion range represented by full intensity. |
| **TAA Clip Tightness** | Blends broad 3x3 history bounds toward tight cross-shaped bounds. Defaults to `0.50`. |
| **TAA History Clip Strength** | Blends between unmodified and fully color-box-clipped history. Defaults to `1.00`. |
| **TAA Current Frame Blend** | Sets the maximum adaptive filtered-current contribution after clipping. Defaults to `0.15`. |
| **Unclamp Motion Vectors** | Removes the unit-length saturate from object and camera velocity encoding while TAA is enabled. Defaults to **Off**. |

`ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` in `runtime/constant_buffers.hpp` controls the diagnostic-view selector, velocity
visualization range, object-motion selector, and the independent velocity, forward, model, alpha-model, overlay-model,
and local-light projection-jitter sliders. It defaults to `0`, which omits those controls and hardcodes Temporal Resolve,
an `8 px` visualization range, corrected native object motion, and `1x` on every known jitter path. Set it to `1` to
restore all of those diagnostics.

The three resolve-tuning sliders preserve the existing algorithm at their defaults. Lower clip tightness broadens the
accepted color range, lower clip strength retains more unmodified history, and lower current-frame blend exposes less of
each new jitter phase. Relaxing any of them can stabilize high-frequency detail but can also increase ghosting.

**Unclamp Motion Vectors** uses minimal replacements derived from the original dumped `GBufferVelocity`,
`GBufferMaskedVelocity`, and `MotionBlurCameraVelocity` pixel shaders. They preserve MGSV's render-size, `0.5`, and `/64`
velocity scaling and condition only the unit-length `saturate`. The RGBA16F targets can therefore retain encoded values
outside `[0,1]` for motion above approximately 64 pixels. The native clamp remains active whenever TAA or the option is
Off. Because MGSV's motion-blur passes consume the same signal, the option can also increase native motion blur and is
diagnostic by default.

Changing the jitter pattern or diagnostic view invalidates history. Preset Off disables TAA and restores the vanilla
projection path.

## Frame pipeline

The runtime executes this sequence while TAA is enabled:

1. The native `SetViewMatrixState` hook accepts only the proven gameplay projection callsite and viewport structure.
2. It captures no-jitter projection/view state, applies the current Halton offset to `ShaderManager+0x680`, and publishes
   the exact jitter with the current frame token, sample index, and viewport dimensions.
3. Scoped native hooks apply the published sample to known main-grid paths that recopy persistent viewport projection:
  velocity, forward/model/alpha/overlay setup, and the private local-light packet builder.
4. MGSV renders the jittered scene, depth, and native object velocity.
5. `MotionBlurCameraVelocity` writes the final camera/object velocity target. The addon captures its depth and
   object-velocity inputs and keeps both velocity stages in RGBA16F.
6. Immediately before `DOF_ScatterBakeFirst`, the addon dispatches the TAA resolve against the full-resolution scene.
7. The resolved image is copied back before MGSV creates its depth-of-field and motion-blur inputs.
8. The game's FXAA shader becomes a pass-through only for the enabled TAA frame.

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

- The main publication return address is the verified `buildRendering` callsite; additional setter paths must match an
  explicitly validated return address or callback detour.
- The viewport is enabled, full-resolution, perspective, and has a valid camera.
- The vanilla viewport projection exactly matches the active shader-manager projection.
- TAA is still enabled while the publication lock is held.

Setter-based hooks modify only projection elements 8 and 9 in the active `ShaderManager+0x680` copy and reassert the
associated dirty flags. The local-light callback temporarily applies the same terms to its viewport source and restores
the exact matrix before returning. Disabling TAA requires three exact unjittered main projection copies for the restoration
check. Persistent viewport projection remains unmodified outside that guarded local-light callback.

Current and previous no-jitter view-projection matrices are retained in double precision. A current matrix is promoted
to previous only after its matching TAA dispatch succeeds, preventing a skipped frame from corrupting camera history.

### Scoped native path corrections

VS `0x200DBED9` receives an unjittered `cVSScene` projection while the main scene and depth are jittered. Its former scoped
replacement proved that adding the same-frame offset fixes PS `0x6CA8AA97` light flicker; that shader replacement has now
been removed. Brief runtime testing indicates that **Alpha Model Projection Jitter** controls those affected lights. A
separate guarded `GrPluginLocalLight::MainExec` detour temporarily jitters `GrViewport+0x280` while the game builds its
private light packet, then restores all sixteen floats exactly; its independent necessity remains to be isolated.

The shared `SetViewMatrixState` detour also recognizes verified velocity, model, alpha-model, and overlay-model return
sites. `GrPluginForwardRendering` uses a tail jump, so its setup callback has a separate guarded detour. Every additional
path consumes the already-published main sample and never republishes camera history.

### Object-motion diagnostics

When `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` is `1`, the **TAA Object Motion Source** control compares native skinned
motion, matrix camera motion everywhere, and native motion adjusted by current jitter or current-minus-previous jitter
in both signs. The velocity projection slider applies same-frame jitter after `MakeVelocityBuffer` resets active
projection. Default native object motion automatically subtracts that current-jitter term in the resolve. The explicit
add/subtract modes expose raw sign and previous-phase diagnostics.

Additional views expose raw and nearest-depth-selected object masks, final selected velocity, and native-object velocity
minus matrix-camera velocity. In the repeatable seated-character menu scene, test Matrix Camera Everywhere first. If the
large blur disappears while local animation ghosts, native object motion is confirmed as the source. Then compare the
subtract/add current-jitter modes, followed by jitter-delta modes. The add variants are sign checks; the final solution
should correct synchronized current/previous velocity matrices upstream rather than keep resolve-side scalar offsets.

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
| `shaders/GBufferVelocity_0x9815404F.ps_5_0.hlsl` | Minimal standard object-velocity replacement with optional unclamping |
| `shaders/GBufferMaskedVelocity_0x58C10658.ps_5_0.hlsl` | Minimal alpha-tested object-velocity replacement with optional unclamping |
| `shaders/MotionBlurCameraVelocity_0xA13321B6.ps_5_0.hlsl` | Minimal final camera/object velocity replacement with optional unclamping |

## Validated behavior

- Eight bounded synchronization runs completed 600 temporal frames each with one history seed, no native publication or
  dimension rejects, and three exact restoration copies.
- The final velocity path was observed as RGBA16F rather than the original BGRA8 target, substantially reducing
  camera-wide wobble.
- Native jitter and the compute resolve agree on frame, sample, dimensions, and current camera state before dispatch.
- Brief testing showed stable menu-character motion with the known-path native jitter corrections enabled.
- The removed `0x200DBED9` replacement established the expected light-jitter sign and scale; current runtime evidence
  points to the alpha-model native path for the affected lights, while the local-light path still needs isolated A/B proof.
- Decoding history texels before Catmull-Rom reconstruction improved temporal quality over encoded interpolation.
- Selecting the largest raw reverse-Z depth preserves thin foreground lines that disappeared with the previous
  smallest-absolute-depth rule.

## Known limitations

- Previous history remains stored in MGSV's encoded scene domain, requiring sixteen point loads and per-texel decoding
  for correct linear-light Catmull-Rom reconstruction. Separate linear history could recover the optimized sampling path.
- There is no previous-depth history or explicit disocclusion mask.
- Thin features have no temporal lock/confidence mechanism and can be removed by current-frame RGB clipping.
- Large camera/FOV discontinuities do not yet trigger a dedicated camera-cut reset.
- Native object-velocity coverage remains a consumer limitation. The approximately 64-pixel packed-motion clamp is
  preserved by default and can only be bypassed by the default-Off shared-signal diagnostic; an owned canonical motion
  path is still required for a production unclamped solution.
- No sharpening is applied; sharpening is deferred until temporal stability is improved.

## Build and manual verification

Build the `mgsv` target with the configured CMake Tools profile. Inspect the addon plus generated `mgsv_taa`, `0x9815404F`,
`0x58C10658`, and `0xA13321B6` artifacts; `0x200DBED9` must no longer be embedded.

Minimum runtime verification:

1. Launch with TAA disabled and confirm vanilla FXAA and projection behavior.
2. Enable TAA with jitter **Off** and confirm the temporal resolve runs without projection motion.
3. Switch to **Halton**. With `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS=1`, test each additional jitter slider independently
  at `0x`, `1x`, and `-1x`; isolate Alpha Model versus Local Light on the formerly affected lights.
4. In that diagnostic build, confirm Velocity `1x` removes whole-model Raw Object Mask phase flicker while default native
  motion remains aligned.
5. Compare **Unclamp Motion Vectors** Off/On above approximately 64 pixels and check both TAA and native motion blur.
6. In that diagnostic build, exercise all gated views and verify velocity direction/magnitude during camera and character
  motion.
7. Test static and fast camera motion, aiming, binoculars, menus, camera cuts, DoF and motion blur on/off, and a resolution
   change.
8. Confirm no recurring publication, dimension, capture, setup, or dispatch warnings.
9. Disable TAA and confirm exact projection restoration with no stale-history frame.

For a stationary high-frequency grating, isolate the resolve parameters rather than changing several simultaneously:

1. Record the baseline at Clip Tightness `0.50`, History Clip Strength `1.00`, and Current Frame Blend `0.15`.
2. Test Clip Tightness `0.00`, then restore `0.50`.
3. Test History Clip Strength `0.00`, then restore `1.00`.
4. Test Current Frame Blend `0.00`, then restore `0.15`.
5. If one extreme stabilizes the grating, increase it from zero until flicker returns and check moving characters and
  camera motion for ghosting before considering a new default.