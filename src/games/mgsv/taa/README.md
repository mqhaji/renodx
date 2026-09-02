# MGSV Temporal Anti-Aliasing

Technical reference for MGSV's optional native-resolution temporal anti-aliasing path. The implementation is deliberately
default-Off and keeps the game's original FXAA and projection behavior whenever TAA is disabled. Enabled users can select
AMD FSR2 2.3.4 or the established analytical resolve. FSR2 is the reconstruction fallback whenever no method key is
persisted, including configurations created before the selector existed; an explicitly persisted method overrides it.

Future quality work and the native-resolution DLAA plan are tracked in [ROADMAP.md](ROADMAP.md).

## User controls

All TAA controls are under **Temporal Anti-Aliasing**; extended motion/jitter views are compile-time gated:

| Control | Behavior |
|---|---|
| **Temporal Anti-Aliasing** | Enables TAA. Defaults to **Off**. |
| **Temporal Reconstruction Method** | Selects **Analytical TAA** or **AMD FSR 2.3.4**. Defaults to FSR2. |
| **TAA Jitter Pattern** | Analytical-only control. **Halton (2,3) — 8 Phase** is the production sequence; **Off** is a zero-jitter diagnostic. Hidden for FSR2, which always enforces Halton. |
| **TAA Diagnostic View** | Gated diagnostic-build control selecting Temporal Resolve, current color, masks, or motion views. |
| **TAA Velocity View Range** | Gated diagnostic-build control setting the pixel-motion range represented by full intensity. |
| **TAA Clip Tightness** | Blends broad 3x3 history bounds toward tight cross-shaped bounds. Defaults to `0.50`. |
| **TAA History Clip Strength** | Blends between unmodified and fully color-box-clipped history. Defaults to `1.00`. |
| **TAA Current Frame Blend** | Sets the maximum adaptive filtered-current contribution after clipping. Defaults to `0.15`. |
| **Unclamp Motion Vectors** | Removes the unit-length saturate from object and camera velocity encoding while TAA is enabled. Defaults to **Off**. |

`ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` in `runtime/state.hpp` controls the diagnostic-view selector, velocity
visualization range, object-motion selector, and the independent velocity, forward, model, alpha-model, overlay-model,
and local-light projection-jitter sliders. It defaults to `0`, which omits those controls and hardcodes Temporal Resolve,
an `8 px` visualization range, corrected native object motion, and `1x` on every known jitter path. Set it to `1` to
restore all of those diagnostics.

The three analytical resolve-tuning sliders are hidden while FSR2 is selected and preserve the existing algorithm at
their defaults. Lower clip tightness broadens the
accepted color range, lower clip strength retains more unmodified history, and lower current-frame blend exposes less of
each new jitter phase. Relaxing any of them can stabilize high-frequency detail but can also increase ghosting.

**Unclamp Motion Vectors** uses minimal replacements derived from the original dumped `GBufferVelocity`,
`GBufferMaskedVelocity`, and `MotionBlurCameraVelocity` pixel shaders. They preserve MGSV's render-size, `0.5`, and `/64`
velocity scaling and condition only the unit-length `saturate`. The RGBA16F targets can therefore retain encoded values
outside `[0,1]` for motion above approximately 64 pixels. The native clamp remains active whenever TAA or the option is
Off. Because MGSV's motion-blur passes consume the same signal, the option can also increase native motion blur and is
diagnostic by default.

Changing the jitter pattern, resolve tuning, motion-vector clamp, diagnostic view, object-motion mode, or per-path jitter
scale invalidates history. The velocity visualization range does not affect accumulation. Preset Off disables TAA and
restores the vanilla projection path.

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
6. At `DOF_ScatterBakeFirst`, the addon accepts only an invocation whose scene color dimensions match the captured
  full-resolution depth and motion inputs. Lower-resolution DoF invocations are skipped.
7. The selected analytical or FSR2 reconstruction writes the resolved image back before MGSV creates its depth-of-field
  and motion-blur inputs.
8. The game's FXAA shader becomes a pass-through while TAA is enabled.

The fallback insertion cascade is:

1. `DOF_ScatterBakeFirst` (`0xFE1DC3F8`)
2. The sequence-qualified `CopyRenderBuffer` after DoF
3. The sequence-qualified `CopyRenderBuffer` after motion-blur tile preparation
4. Tonemap or Tonemap 1D-LUT when the earlier passes are absent

Only one resolve may run per frame. The native projection hook and `MotionBlurCameraVelocity` capture can execute
immediately before `Present` while their matching insertion callback executes immediately afterward on another thread.
A publication or capture from the immediately preceding presentation epoch is therefore valid only when its Halton sample
still matches; older or differently sampled inputs remain rejected.

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

Both methods consume the same validated scene, velocity, depth, object-motion, jitter, and no-jitter camera publication.
The analytical shader uses the direct register contract below. FSR2 first converts those inputs into owned linear RGBA16F
color and normalized RG16F motion resources, then executes its depth, lock, luminance, and accumulation graph.

The analytical compute shader consumes:

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

Depth and object-velocity references are refreshed on every successful `MotionBlurCameraVelocity` capture. The capture
records both its presentation epoch and TAA sample so a one-epoch scheduling offset cannot admit motion from a previously
completed temporal frame. Stable color and velocity view caches are keyed by resource/view handles and cleared through
resource-destruction notifications; they avoid repeated descriptor queries and SRV creation without defining freshness.

The resolve point-loads and decodes the complete 4x4 history footprint before 16-tap Catmull-Rom reconstruction in linear
light. It clips history to equally blended broad/tight current RGB bounds and computes an adaptive blend from luminance
position and subpixel velocity. Scene alpha is copied exactly from the current frame because downstream MGSV passes use
it for highlight/emissive behavior.

## History and failure policy

The analytical method uses two RGBA16F histories in MGSV's encoded scene domain. FSR2 owns separate linear color, lock,
luma, motion, reconstructed-depth, luminance, and reactive-state resources. Only the selected reconstruction method keeps
its large temporal resources resident; switching methods or disabling TAA releases the inactive graph. History resets on
creation, resize, enable, method/settings changes, missing previous camera state, or a detected camera discontinuity.
Presents with no new full-resolution candidate preserve history, and rejected lower-resolution candidates do not reset it
while later candidates remain available.

Each insertion candidate fails closed when:

- The native jitter publication is missing, older than the permitted one-epoch ordering, or for a different sample.
- The publication dimensions do not match scene color.
- Current native camera state is unavailable.
- Velocity, depth, or object-velocity capture is missing, more than one presentation epoch old, or belongs to a different
  Halton sample.
- Input dimensions or required resource formats do not match.
- Compute pipeline or history setup fails.

Candidate rejection does not immediately reset valid history while a later insertion remains possible. `OnPresent`
invalidates history only if a full-resolution candidate was observed since the previous Present but no resolve completed.
Missing previous matrix history also invalidates and reseeds accumulation before using the valid current frame. No
temporal output is produced from mismatched frame data.

Method and jitter changes serialize through the resolve/publication locks and reset both matrix and sample history as one
transaction. FSR2 forces only the effective runtime pattern to Halton; it does not overwrite the saved analytical jitter
preference, which is restored when Analytical TAA is selected again. The injected compute graph captures and restores the
native D3D11 compute shader plus sampler, SRV, UAV, and constant-buffer slots it modifies.

## Runtime logging

`MGSV_TAA_LOGGING` defaults to `1`. The normal lifecycle is intentionally concise:

- **initial runtime state** reports whether a persisted TAA setting was already enabled when the addon attached.
- **TAA runtime enabled** records the transition reason, frame, and active jitter pattern.
- **waiting for first native jitter publication** is emitted once while startup rendering has not yet reached the proven
  native gameplay projection path.
- **TAA accumulation started** records the accepted insertion, callback frame, native-publication frame, sample, and
  dimensions after each analytical history seed.
- **created AMD FSR2 2.3.4 D3D11 SM5 pipelines** reports the active approximate-history, zero-mask, and fused-output
  specialization once per device pipeline lifetime.
- **AMD FSR2 accumulation started** records the accepted insertion and dimensions after each FSR2 reset/seed.
- **TAA runtime disabled** records the reason, frame, and number of completed temporal samples.
- **skipping non-full-resolution TAA insertion candidate** is expected at mixed-resolution DoF passes and is emitted only
  once per device lifetime.

One accumulation-start line after enabling, resizing, or intentionally changing a history-affecting setting is expected.
Repeated accumulation-start lines while standing still with unchanged settings prove that history is being invalidated and
should be paired with the preceding invalidation or warning. Recurring warnings remain actionable; successful dispatches
are not logged every frame. When TAA was restored from persisted configuration, **initial runtime state enabled=true**
replaces the interactive **TAA runtime enabled** transition line.

## Source layout

| File | Role |
|---|---|
| `taa.hpp` | Callback lifecycle, draw routing, and insertion cascade |
| `settings.hpp` | RenoDX controls and synchronized runtime transitions |
| `runtime/state.hpp` | Frame/sample state and Off/Halton jitter generation |
| `runtime/descriptor_tracker.hpp` | Per-command-list pixel SRV tracking |
| `runtime/projection_jitter.hpp` | Native hook, jitter publication, matrix history, and restoration checks |
| `runtime/resolve.hpp` | Shared input validation, analytical history/dispatch, and FSR2 callback routing |
| `fsr/runtime.hpp` | FSR2 D3D11 resources, pass graph, method lifetime, history, resets, and copy-back |
| `fsr/shaders/` | FSR2 SM5 entry points, fixed native-resolution specializations, and MGSV color/motion/output adapters |
| `fsr/vendor/FidelityFX/` | Locally pinned AMD FSR2 2.3.4 GPU source; no external SDK link required |
| `shaders/mgsv_taa.cs_5_0.hlsl` | Temporal resolve |
| `shaders/GBufferVelocity_0x9815404F.ps_5_0.hlsl` | Minimal standard object-velocity replacement with optional unclamping |
| `shaders/GBufferMaskedVelocity_0x58C10658.ps_5_0.hlsl` | Minimal alpha-tested object-velocity replacement with optional unclamping |
| `shaders/MotionBlurCameraVelocity_0xA13321B6.ps_5_0.hlsl` | Minimal final camera/object velocity replacement with optional unclamping |

## Validated behavior

- Initial bounded synchronization runs completed 600 temporal frames each with one history seed and three exact
  restoration copies. Later 4K traces proved mixed-resolution insertion candidates and one-epoch native/camera callback
  ordering. The current source selects only dimension-matched draws and requires matching samples for bounded one-epoch
  inputs; the camera-capture extension still needs the static-menu runtime recheck below.
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

- The FSR2 path has compiled successfully but still requires in-game validation. Its reactive and
  transparency/composition inputs are currently zero; its optimized depth pass still preserves motion/depth divergence.
  A full-mask shader is retained for deferred `TppFxRain` raindrop reactivity.
- FSR2 uses a clip-space discontinuity heuristic for camera-cut reset until a proven native MGSV camera-cut signal is
  available. The fallback tests mid-depth center/corner reprojection for large lateral, FOV, and roll changes without
  treating normal nonlinear near/far depth motion as a cut, and uses homogeneous-W change for large forward/backward
  discontinuities.
- FSR2 history reprojection uses AMD's approximate polynomial Lanczos instead of the reference trigonometric kernel; this
  performance/quality choice requires in-game comparison on thin detail, moving silhouettes, ringing, and disocclusions.
- RCAS source remains vendored for possible later use, but no RCAS pipeline is created or dispatched and FSR2 output is
  currently unsharpened.

The following resolve limitations apply to the analytical method:

- Previous history remains stored in MGSV's encoded scene domain, requiring sixteen point loads and per-texel decoding
  for correct linear-light Catmull-Rom reconstruction. Separate linear history could recover the optimized sampling path.
- There is no previous-depth history or explicit disocclusion mask.
- Thin features have no temporal lock/confidence mechanism and can be removed by current-frame RGB clipping.
- Large camera/FOV discontinuities do not yet trigger a dedicated camera-cut reset.
- Native object-velocity coverage remains a consumer limitation. The approximately 64-pixel packed-motion clamp is
  preserved by default and can only be bypassed by the default-Off shared-signal diagnostic; an owned canonical motion
  path is still required for a production unclamped solution.
- No analytical sharpening is applied.

## Build and manual verification

Build the `mgsv` target with the configured CMake Tools profile. Inspect the addon plus generated `mgsv_taa`,
`fsr2_prepare_inputs`, `fsr2_luminance_pyramid`, `fsr2_reconstruct_previous_depth`, `fsr2_depth_clip`,
`fsr2_depth_clip_zero_masks`, `fsr2_lock`, `fsr2_accumulate`, `0x9815404F`, `0x58C10658`, and `0xA13321B6` artifacts;
`0x200DBED9` must no longer be embedded.

Analytical TAA verification:

1. Launch with TAA disabled and confirm vanilla FXAA and projection behavior.
2. Enable TAA, select **Analytical TAA**, set jitter **Off**, and confirm one **TAA runtime enabled** line followed by one
  **TAA accumulation started** line; standing still must not repeatedly restart accumulation.
3. Switch analytical TAA to **Halton**. With `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS=1`, test each additional jitter slider independently
   at `0x`, `1x`, and `-1x`; isolate Alpha Model versus Local Light on the formerly affected lights.
4. In that diagnostic build, confirm Velocity `1x` removes whole-model Raw Object Mask phase flicker while default native
   motion remains aligned.
5. Compare **Unclamp Motion Vectors** Off/On above approximately 64 pixels and check both TAA and native motion blur.
6. In that diagnostic build, exercise all gated views and verify velocity direction/magnitude during camera and character
  motion.
7. Test static and fast camera motion, aiming, binoculars, menus, camera cuts, DoF and motion blur on/off, and a resolution
   change.
8. At 4K with DoF enabled, confirm lower-resolution DoF candidates do not reset accumulation and that there are no
  recurring stale-publication, missing-resolve, capture, setup, or dispatch warnings. Also leave a static menu camera
  running long enough to verify one-epoch camera captures remain accepted.
9. Disable TAA and confirm a **TAA runtime disabled** line, exact projection restoration, and no stale-history frame.

Default FSR2 verification:

1. Enable TAA with no persisted reconstruction-method key and confirm **AMD FSR 2.3.4** is selected and the jitter-pattern
  control is hidden; FSR2 still uses the eight-phase Halton sequence internally.
2. Confirm one pipeline/resource creation pair and one **AMD FSR2 accumulation started** line, with no recurring setup,
  format, or camera-matrix warnings.
3. Compare static detail, camera and character motion, thin wires, aiming/FOV transitions, camera cuts, DoF, motion blur,
  rain, menus, and resolution/device resets against analytical TAA.
4. Confirm the image remains in MGSV's expected encoded scene domain and downstream alpha-driven DoF/highlights remain
  unchanged apart from temporal reconstruction.
5. Confirm logs report `history_kernel=approximate input_masks=zero output_encode=fused`; rain reactivity remains deferred
  until the full-mask path is wired to a proven `TppFxRain` mask.

For a stationary high-frequency grating, isolate the resolve parameters rather than changing several simultaneously:

1. Record the baseline at Clip Tightness `0.50`, History Clip Strength `1.00`, and Current Frame Blend `0.15`.
2. Test Clip Tightness `0.00`, then restore `0.50`.
3. Test History Clip Strength `0.00`, then restore `1.00`.
4. Test Current Frame Blend `0.00`, then restore `0.15`.
5. If one extreme stabilizes the grating, increase it from zero until flicker returns and check moving characters and
  camera motion for ghosting before considering a new default.