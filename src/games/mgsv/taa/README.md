# MGSV Temporal Anti-Aliasing

Technical reference for MGSV's native-resolution temporal anti-aliasing path. One top-level **Temporal Reconstruction**
dropdown selects **Off (Vanilla FXAA)**, **Analytical TAA**, **AMD FSR 3.1.5**, or **NVIDIA DLSS**. New profiles default
to FSR3; Off keeps the game's original FXAA and projection behavior. Existing profiles migrate their former `FxTaa` enable state plus
`FxTemporalReconstructionMethod` or `FxTaaReconstructionMethod` selection into the versioned
`FxTemporalReconstructionMode` key, so disabled profiles remain Off and old AMD values continue to mean FSR3.

Future quality work is tracked in [ROADMAP.md](ROADMAP.md).

**Current lifecycle:** DLSS startup checks only the rendering device's ReShade `vendor_id` property. DLL discovery,
version inspection, NGX calls and native DLSS hooks are deferred until an explicit or restored DLSS selection.
The old blanket isolation flag is removed; archived isolated builds remain diagnostic references. Legacy Compute State
remains the FSR default, with explicit saved Off preferences preserved. These changes do not establish a flicker fix.

## User controls

All TAA controls are under **Temporal Anti-Aliasing**; extended motion/jitter views are compile-time gated:

| Control | Behavior |
|---|---|
| **Temporal Reconstruction** | Selects **Off (Vanilla FXAA)**, **Analytical TAA**, **AMD FSR 3.1.5**, or **NVIDIA DLSS**. New profiles default to FSR3. |
| **DLSS Model** | DLSS-only selector for presets A-F and J-M supported by the discovered feature-DLL generation. Defaults to E when available, otherwise the DLL default. |
| **FSR Legacy Compute State** | On (default) uses the compute-only native call ranges from `mgsv-old`, reducing light flicker in testing. Off retains extended graphics/OM preservation for A/B. Changing it resets FSR history; Analytical/DLSS always use extended preservation. |
| **TAA Jitter Pattern** | Analytical-only control. **Halton (2,3) — 8 Phase** is the production sequence; **Off** is a zero-jitter diagnostic. Hidden for FSR3 and DLSS, which enforce Halton. |
| **TAA Diagnostic View** | Gated diagnostic-build control selecting Temporal Resolve, current color, masks, or motion views. |
| **TAA Velocity View Range** | Gated diagnostic-build control setting the pixel-motion range represented by full intensity. |
| **TAA Clip Tightness** | Blends broad 3x3 history bounds toward tight cross-shaped bounds. Defaults to `0.50`. |
| **TAA History Clip Strength** | Blends between unmodified and fully color-box-clipped history. Defaults to `1.00`. |
| **TAA Current Frame Blend** | Sets the maximum adaptive filtered-current contribution after clipping. Defaults to `0.15`. |
| **Unclamp Motion Vectors** | Removes the unit-length saturate from object and camera velocity encoding while TAA is enabled. Defaults to **Off**. |
| **Projection Jitter Path sliders** | Analytical-only `-2x` to `2x` controls for Velocity, Forward, Model, Alpha Model, Overlay Model, and Local Light paths. Defaults to `1x`. |

`ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS` in `runtime/state.hpp` controls the diagnostic-view selector, velocity
visualization range, and object-motion selector. It is temporarily `1` to isolate night-vision object flicker. Production
builds normally set it to `0`, which hardcodes Temporal Resolve, an `8 px` visualization range, and corrected native
object motion.
`ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS` independently exposes
the six known-path sliders and is temporarily `1` for Analytical TAA testing. Their values are ignored by FSR3 and DLSS,
which always use the production `1x` path scales.

The three analytical resolve-tuning sliders are hidden while FSR3 or DLSS is selected and preserve the existing algorithm at
their defaults. Lower clip tightness broadens the
accepted color range, lower clip strength retains more unmodified history, and lower current-frame blend exposes less of
each new jitter phase. Relaxing any of them can stabilize high-frequency detail but can also increase ghosting.

**Unclamp Motion Vectors** uses minimal replacements derived from the original dumped `GBufferVelocity`,
`GBufferMaskedVelocity`, and `MotionBlurCameraVelocity` pixel shaders. They preserve MGSV's render-size, `0.5`, and `/64`
velocity scaling and condition only the unit-length `saturate`. The RGBA16F targets can therefore retain encoded values
outside `[0,1]` for motion above approximately 64 pixels. The native clamp remains active whenever TAA or the option is
Off. Because MGSV's motion-blur passes consume the same signal, the option can also increase native motion blur and is
diagnostic by default.

Changing the reconstruction mode, jitter pattern, resolve tuning, motion-vector clamp, diagnostic view, object-motion
mode, or per-path jitter scale invalidates history. The velocity visualization range does not affect accumulation.
Preset Off selects **Off (Vanilla FXAA)** and restores the vanilla projection path.

The dropdown owns per-option availability rather than disabling the whole control. DLSS is visible but disabled for an
unidentified or non-NVIDIA rendering device. NVIDIA vendor `0x10DE` makes it selectable, not proven supported. The first
selection checks `nvngx_dlss.dll` beside `mgsvtpp.exe`, its version, and NGX GPU/driver capabilities. Missing-DLL, GPU,
driver and initialization failures are cached with red hover explanations; they do not retry every Present.
NGX has no preset-enumeration API, so saved model hints remain intact without DLL inspection until selection; then the
DLL's Windows file version filters the hints, refined if feature creation rejects a requested preset.

### Lazy DLSS activation

- The vendor check runs once on the coordinator's game rendering device. No DLL filesystem check, version read,
  load or NGX call occurs during Off/FSR/Analytical startup. Merely opening the dropdown does not inspect the DLL.
- Choosing DLSS, including restoring a saved DLSS mode, queues activation for Present. Requested mode stays DLSS;
  the effective mode and shader binding stay Off while pending. No SDK initialization happens in a settings callback
  or draw. A newer Off/other-method choice cancels the request rather than being overwritten by an old snapshot.
- Present probes support, then installs the native query bridge only on success and an uncanceled request. Draws
  require cached readiness. Failure returns the stored/effective mode to Off and disables DLSS until restart.
- Lazy installation cannot assume existing recordings are query-free: a query may have begun before interception.
  Each deferred context must be created after installation or complete a natural successful `FinishCommandList`
  before its next recording can be split. The post-Finish ReShade event establishes this boundary with either restore
  flag; a failed Finish, reset notification alone, or another Present does not. Untracked candidates are skipped.
- Bridge event callbacks are registered at attach but return on an atomic gate before QI, allocation or locking until
  first activation. Native Begin/End detours do not exist before that request. Once installed, they remain until
  device teardown: ordinary mode changes do not detach hooks, forget query scopes, or discard original game prefixes.
- Switching away still suspends the one DLSS resource bundle rather than releasing it from Present. Thus a fresh
  never-DLSS process is distinct from switching back to FSR/Analytical after DLSS has been used.

## Frame pipeline

The runtime executes this sequence while TAA is enabled:

1. The native `SetViewMatrixState` hook accepts only the proven gameplay projection callsite and viewport structure.
2. It captures no-jitter projection/view state, applies the current Halton offset to `ShaderManager+0x680`, and publishes
   the exact jitter with the current frame token, sample index, and viewport dimensions.
3. Scoped native hooks apply the published sample to known main-grid paths that recopy persistent viewport projection:
  velocity, forward/model/alpha/overlay setup, and the private local-light packet builder.
4. MGSV renders the jittered scene, depth, and native object velocity.
5. `MotionBlurCameraVelocity` writes the final camera/object velocity target. The addon captures its depth and
   object-velocity inputs, the matching camera publication, and the final RGBA16F velocity as one immutable frame.
6. At `DOF_ScatterBakeFirst`, the addon accepts only an invocation whose scene color dimensions match the captured
  full-resolution depth, final velocity, object velocity, and camera dimensions. Lower-resolution DoF invocations are
  skipped.
7. The coordinator dispatches Analytical TAA, FSR3, or DLSS from that validated value, copies the encoded output back,
  restores common resource/compute state, commits camera history, and advances the sample once.
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

`runtime/projection_jitter.hpp` owns executable discovery and the native projection Detours.
`runtime/camera_state.hpp` separately owns the lock-free publication and double-precision matrix history. The hook locates
the projection commit using an executable-section AOB plus the complete preceding projection-copy context and detours the
adjacent `SetViewMatrixState(float*)` function rather than patching a shared constant buffer.

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

Historical captures showed VS `0x200DBED9` receiving an unjittered `cVSScene` while main depth was jittered. The original
scoped proof established the correction sign; it does not establish the cause of the current regression. The later
target-based replacement has been removed because of injection ownership and decompiler defects, so this build uses the
original light VS. Earlier testing indicated that **Alpha Model Projection Jitter** affected those lights. A
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

All methods consume one immutable `ValidatedFrameInputs` value containing the same scene, velocity, depth,
object-motion, jitter, and no-jitter camera publication.
The analytical shader uses the direct register contract below. The FSR3 boundary adapter decodes MGSV scene RGB into an
owned linear RGBA16F input and combines matrix camera motion with native deformation-aware object motion in signed RG16F.
AMD's 3.1.5 host then schedules the fixed SM5 prepare-input, luminance, shading-change, prepare-reactivity,
luma-instability, and accumulation passes through the local D3D11 backend. Render and output extents are identical.

The FSR3 dispatch currently leaves external reactive, transparency/composition, and exposure resources null. AMD maps the
two mask inputs to its zero resource while retaining internal shading-change, disocclusion, motion-divergence, and
luma-instability handling. `preExposure` is `1`, sharpening is disabled, and the linear output is encoded back to MGSV's
scene domain with current-frame alpha preserved. Game-derived masks are planned in [ROADMAP.md](ROADMAP.md).

The DLSS boundary uses the same linear color and canonical signed RG16F motion construction, plus an owned R32F copy of
MGSV's reverse-Z depth. NGX runs at equal render/output dimensions with its native-resolution quality value, auto
exposure, no sharpening, and the selected DLSS model hint. MGSV records the insertion on a deferred D3D11 context, so
the addon splits that command list at the reconstruction boundary and evaluates NGX on the immediate context between the
prefix and post-insertion list. Unsupported submission/state/query conditions or NGX failures queue a coherent
transition to Off on the next present.

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

Depth, object velocity, final velocity, and camera state are snapshotted together at `MotionBlurCameraVelocity`. The input
capture validates their device, resource type, sample count, dimensions, presentation epoch, and temporal sample once;
method runtimes do not reach back into mutable capture state. The owned velocity SRV is cleared through resource-view
destruction notifications.

The resolve point-loads and decodes the complete 4x4 history footprint before 16-tap Catmull-Rom reconstruction in linear
light. It clips history to equally blended broad/tight current RGB bounds and computes an adaptive blend from luminance
position and subpixel velocity. Scene alpha is copied exactly from the current frame because downstream MGSV passes use
it for highlight/emissive behavior.

## History and failure policy

The analytical method uses two RGBA16F histories in MGSV's encoded scene domain. FSR3 owns its component context,
host-created internal resources, linear boundary textures, motion, reconstructed depth, and caller-owned shared dilated
resources. DLSS owns its NGX feature/parameter map and linear color, motion, depth, output, and encoded-output textures.
Inactive analytical and FSR3 graphs are released. DLSS instead retains one suspended feature/texture bundle across
method switches, avoiding NGX teardown and GPU waits from `Present`. A compatible device/context, size and model reuse
that bundle with fresh history; incompatible requests recreate it during immediate-context dispatch. Device shutdown
releases it. Actual DLSS teardown retains the bundle and disables DLSS if its bounded GPU-completion wait fails.
This transition-crash mitigation retains approximately 253 MiB of adapter textures at 3840x2160, plus NGX's internal
allocations, even while another method or Off is selected. The bundle count does not grow with repeated switches.
History resets on
creation, resize, enable, method/settings changes, or missing previous camera state.
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

Mode and jitter changes serialize through the coordinator/publication locks and reset method and camera history as one
transaction. FSR3 forces only the effective runtime pattern to Halton; it does not overwrite the saved analytical jitter
preference. DLSS follows the same effective Halton policy. One common completion path restores D3D11 compute/OM state, copies the encoded method output, commits camera
history, and advances the sample.

## Runtime logging

`MGSV_TAA_LOGGING` defaults to `1`. The normal lifecycle is intentionally concise:

- **initial temporal state** reports the active persisted mode and jitter pattern when the addon attaches.
- **temporal reconstruction selected** records a non-Off mode transition, frame, and active jitter pattern.
- **waiting for first native camera publication** is emitted once while startup rendering has not yet reached the proven
  native gameplay projection path.
- **TAA accumulation started** records the accepted insertion, callback frame, native-publication frame, sample, and
  dimensions after each analytical history seed.
- **FSR3.1 D3D11 context probe succeeded** reports feature level, dimensions, host version 3.1.5, and backend interface
  version 2.3.0 whenever the FSR3 context is created.
- **AMD FSR3.1 accumulation started** records the accepted insertion and dimensions after each FSR3 reset/seed.
- **DLSS startup=vendor-only** and **DLSS adapter gate** identify the no-DLL startup path.
- **servicing requested NVIDIA DLSS activation** precedes request-time DLL/NGX work.
- **NVIDIA DLSS runtime ready** follows successful capability probing and bridge installation.
- **NVIDIA DLSS available** records successful adapter/driver/NGX capability probing and the discovered DLL version.
- **NVIDIA DLSS feature created** records native dimensions and the effective model.
- **NVIDIA DLSS accumulation started** records each DLSS reset/seed.
- **suspended NVIDIA DLSS; retaining resources** records leaving DLSS without transition-time teardown.
- **resumed NVIDIA DLSS with retained resources** records compatible reuse, followed by a fresh history seed.
- **temporal reconstruction disabled** records the reason, frame, and number of completed temporal samples.
- **skipping non-full-resolution TAA insertion candidate** is expected at mixed-resolution DoF passes and is emitted only
  once per device lifetime.

One accumulation-start line after selecting a temporal method, resizing, or intentionally changing a history-affecting
setting is expected. Repeated accumulation-start lines while standing still with unchanged settings prove that history is
being invalidated and should be paired with the preceding invalidation or warning. Recurring warnings remain actionable;
successful dispatches are not logged every frame.

## Source layout

| File | Role |
|---|---|
| `taa.hpp` | Callback lifecycle, draw routing, and insertion cascade |
| `settings.hpp` | Top-level mode dropdown, split-setting migration, availability UI, and coherent settings snapshots |
| `analytical/runtime.hpp` | Analytical history, pipeline, tuning constants, and method dispatch |
| `runtime/state.hpp` | Frame/sample state and Off/Halton jitter generation |
| `runtime/descriptor_tracker.hpp` | Per-command-list pixel SRV tracking |
| `runtime/camera_state.hpp` | Coherent camera publication and double-precision temporal matrix history |
| `runtime/projection_jitter.hpp` | Native projection discovery, Detours, path correction, and restoration checks |
| `runtime/frame_inputs.hpp` | Immutable game-native input and encoded method-output contracts |
| `runtime/input_capture.hpp` | Velocity/depth/object/camera capture, lifetime tracking, and validation |
| `runtime/coordinator.hpp` | Method switch, common transitions/copy-back, camera commit, and sample advancement |
| `runtime/d3d11_compute_state.hpp` | Compute pipeline and descriptor preservation shared by temporal methods |
| `fsr3/runtime.hpp` | FSR3 context lifetime, MGSV boundary conversion, host dispatch, and encoded output |
| `dlss/runtime.hpp` | NGX discovery/capability probing, DLSS feature/model lifetime, native dispatch, and failure policy |
| `dlss/shaders/` | MGSV-to-DLSS linear color, canonical motion, R32 depth, and encoded-output boundary passes |
| `fsr3/backend_dx11.*` | Custom FidelityFX 2.3.0 D3D11 backend with Feature Level 11_0 support |
| `fsr3/shader_blobs.cpp` | Static host pipeline metadata and fixed SM5 bytecode provider |
| `fsr3/shaders/` | FSR3 3.1.5 SM5 wrappers plus MGSV color/motion/output adapters |
| `fsr3/ffx/` | Locally pinned AMD FSR SDK v2.3.0 source subset; no external SDK link required |
| `fsr3/README.md` | Exact AMD/MapleHinata provenance and version-layer explanation |
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
- The historical `0x200DBED9` proof established light-jitter sign/scale. The later target-based replacement did not
  resolve the general-gameplay regression and is no longer installed; native hook counters alone do not prove that an
  affected draw received the right projection.
- Decoding history texels before Catmull-Rom reconstruction improved temporal quality over encoded interpolation.
- Selecting the largest raw reverse-Z depth preserves thin foreground lines that disappeared with the previous
  smallest-absolute-depth rule.
- Runtime testing confirms that FSR3 creates and dispatches on D3D11 Feature Level 11_0 using MGSV's
  `R32_FLOAT_X8X24_TYPELESS` depth plane and native-resolution inputs.

## Known limitations

- FSR3 receives no game-provided reactive or transparency/composition mask yet. Material-derived mask integration,
  beginning with proven paths such as `TppFxRain`, remains planned; the AMD host's internal reactivity analysis is active.
- FSR3 and DLSS do not yet have a proven native camera-cut signal. The former clip-space heuristic was removed because it
  produced false positives and reset SDK accumulation every frame.
- The custom D3D11 backend and fixed FXC `cs_5_0` permutations are a local source adaptation, not an AMD-supported D3D11
  integration. They require continued runtime comparison on thin detail, moving silhouettes, ringing, and disocclusions.
- The AMD host creates its RCAS and optional generate-reactive pipelines, but current MGSV dispatch enables neither
  sharpening nor the separate generate-mask API. Output is unsharpened.
- No game exposure resource is supplied. The current path uses `preExposure = 1`.

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
`fsr3_prepare_game_inputs`, `fsr3_encode_game_output`, all eleven `fsr3sdk_*` pass artifacts, `0x9815404F`,
`0x58C10658`, and `0xA13321B6` artifacts. The eleven SDK passes are prepare inputs, luma pyramid, shading-change pyramid,
shading change, prepare reactivity, luma instability, accumulate, accumulate-sharpen, RCAS, generate reactive, and debug
view.

DLSS deployment additionally requires a compatible `nvngx_dlss.dll` next to `mgsvtpp.exe`. The addon uses the installed
NVIDIA NGX core and never ships or loads a bundled feature DLL automatically.

### DLSS transition-crash retest

The 2026-09-05 DLSS-to-FSR crash dump reports `0xC0000374` / `HEAP_FAILURE_MULTIPLE_ENTRIES_CORRUPTION` on an NVIDIA
driver worker after DLSS resources were released. The corrupt allocation is not captured, so the original corrupting
write is unknown. Suspending DLSS removes that teardown from the switch path; it is a mitigation pending runtime proof,
not a confirmed diagnosis of the heap writer. Pending game command-list prefixes remain owned and still execute after
mode/generation changes.

1. Build only `mgsv` with the established MSVC/Ninja Release comparison profile, with MGSV stopped; deploy one addon
  and keep live shader overrides disabled. No shader changes are part of this mitigation.
2. Keep **FSR Legacy Compute State On**, the existing FSR tuning, resolution and DLSS Preset E unchanged. Start in FSR,
  select DLSS, allow several seconds of accumulation, then switch directly back to FSR.
3. Repeat the FSR/DLSS round trip several times. Expect one initial feature creation, suspension on exit, and
  retained-resource resume plus a fresh seed on return, rather than release/recreate at every switch.
4. Separately check DLSS-to-Off-to-DLSS and DLSS-to-Analytical-to-DLSS. Off must retain vanilla FXAA/projection behavior;
  retained DLSS allocations must not cause evaluations while inactive. Exit normally and retain `ReShade.log`.
5. Only after mode-switch stability is confirmed, check a DLSS model change and a resolution change separately; those
  deliberately exercise actual teardown/recreation and are not covered by a passing same-model switch test.
6. On another crash, stop the test and preserve the matching dump/log. A successful build or a single switch does not
  establish that the heap corruption is fixed.

### General-gameplay light-flicker A/B

**Initial state-preservation report (superseded):** user first reported no fix with Legacy Compute State. The 15:39-15:41 log from
addon SHA-256 `1E4790DA013B99DAA926B5AEEA35E215377C4F19C528552E4F60D0E7551F69A8` confirms repeated legacy/extended
switches and FSR history seeding at the switches, not continuous resets. That process started in DLSS and later switched
to FSR, so it did not isolate FSR from prior NGX initialization or the installed query hooks.

**Revised result (2026-09-05):** user reports Legacy On definitely reduces general-gameplay light flicker versus Off,
with a small residual in some cases. The 16:08-16:11 log from addon SHA-256
`FCC417919F65ADC577A835E4190112CC3EEF3E6DAD9FE19AF6522A7FFD99C1B2` starts in FSR with DLSS runtime isolation enabled
and confirms several legacy/extended changes with history seeded only at startup/toggles. This establishes a useful
state-profile difference even without active NGX. It does not establish that residual flicker is an algorithm limit.

Settled valid-viewport counter windows (first window after each toggle excluded) reported:

| Native path | Legacy application rate | Extended application rate |
| --- | --- | --- |
| Alpha Model | 98.02% | 99.22% |
| Local Light | 98.09% | 99.24% |
| Velocity | 98.72% | 99.64% |
| Overlay Model | 0% | 0% |

These are aggregate CPU hook reports, not per-light GPU proof. They do not support explaining the improvement by more
successful native corrections; the extended profile actually has slightly higher rates in this run. Overlay sample
rejections and occasional stale epochs remain separate open issues. FSR tuning is unchanged at shading-change scale
`0.15` and accumulation increment `1/3`.

Legacy **On** is the new FSR default/recommended baseline. The original checkbox A/B below is retained for reference.

This is an isolation build, not a claimed light-flicker fix. `mgsv-old` remains untouched. Use one addon and no live
shader overrides; the generated shader registry must not include `0x200DBED9` even if a stale file remains in the build
directory. The user selected the existing MSVC/Ninja Release configuration to match earlier comparison artifacts.

1. Select **AMD FSR 3.1.5**, without switching to DLSS during this comparison. Keep the camera, lighting, resolution,
  other settings and affected light source unchanged.
2. Leave **FSR Legacy Compute State** **Off** for 10 seconds. This is the extended-state control with the later light
  workaround removed. Note whether removal alone changed the flicker.
3. Turn it **On** for 10 seconds, then **Off** again for 10 seconds. Ignore the immediate history reseed after each
  toggle. Compare stationary lights and a slow pan. If rendering becomes corrupt, return it to Off.
4. On saves/restores CS sampler slots 0-1, SRVs 0-15, UAVs 0-7 and CBs 0-2 plus the shader/classes and tracked compute
  layout, matching the old helper's API call sequence. It does not call graphics-SRV/OM getters/setters or explicitly
  unbind OM. Off retains the extended implementation. Both modes share the same backing state structure, so this
  isolates native state operations, not every byte of the old implementation's CPU overhead.
5. Jitter hooks/guards, camera capture/commit, motion/FSR shaders, history-reset policy, query tracking and DLSS
  scheduling are unchanged between the two checkbox settings. Analytical TAA and DLSS always use extended preservation.
6. Log markers are **light-flicker isolation: original light VS**, **FSR state preservation selected**, and the existing
  projection-path reports with `mode` and `fsr_legacy_compute`. A 300-Present counter window can straddle a toggle;
  use settled windows and the transition markers rather than attributing a mixed window to one profile.

If On alone stabilizes the light, the state-preservation difference is implicated, not necessarily the underlying
reason (binding hazards versus changed recording timing still need separation). If both fail, do not loosen jitter
guards or change temporal tuning; next isolate DLSS query/secondary-list overhead, then camera-history ownership.
The original comparison and remaining candidates are in [LIGHT_FLICKER_COMPARISON.md](LIGHT_FLICKER_COMPARISON.md).

### Cold-start DLSS runtime isolation

**Historical isolation tests (superseded by lazy activation):** the flag below belongs to archived builds, not the current
source. Current Off/FSR/Analytical startup does only the vendor check; DLL/NGX/hook work is selection-driven.

**Reported result (2026-09-05):** the normal-runtime suspension build
`D4C586FA504361BD32C1AC0FF2D9EAEA7BE95F4AAACEF207ECFD1D8C237D9500` still flickers in startup FSR with Legacy On,
DLSS, and Analytical. The user confirms Off is stable. Its 22:37 run reaches FSR after DLSS without another crash and
exits normally, but that single switch is not a stability guarantee. The Analytical reset bursts coincide with logged
object-motion/clip-tightness setting changes; they do not prove a spontaneous-reset defect. The subsequent 23:27 run
starts in FSR, switches to Off and Analytical, and never creates a DLSS feature; NGX probing/query hooks still run.

The archived isolated counterpart sets **only** `RUNTIME_DISABLED_FOR_FSR_ISOLATION=true`. No camera capture/commit,
jitter eligibility, state-preservation implementation, shaders, tuning, or DLSS suspension policy changes accompany it.
DLSS is intentionally unavailable. This tests the combined NGX-startup/query-list activity, not either one separately.

1. Cold-start **Analytical TAA** at the same light/camera/lighting state, with the existing diagnostic view, object-motion
  mode, clip/filter controls and Halton left fixed. Compare against the normal-runtime run and Off. Do not drag sliders
  during the comparison; their intentional history resets are a separate effect.
2. Select FSR with **Legacy Compute State On**, exit and launch again for the FSR cold-start comparison at that same light.
  Keep the existing `0.15` shading-change scale and `1/3` accumulation increment.
3. Verify **DLSS runtime isolation=true** and no bridge-install, NGX-available or feature-created messages. Use one game
  addon and no live shader overrides. Keep DLSS unselected; it is disabled in this binary by design.
4. If both methods improve, split NGX probing from query/list instrumentation next. If neither improves, investigate the
  remaining shared input/camera path independently. If only one improves, keep its state-preservation difference separate.
  Do not combine this control with a camera-commit rollback or an Analytical legacy-state change.

The following describes the **previous** normal-runtime follow-up, now superseded by that result and isolated test:

The follow-up holds the improved **Legacy On** profile fixed and reintroduces normal DLSS runtime startup. Do not
compare Legacy On/no-DLSS against Legacy Off/normal-DLSS; that changes two variables.

1. Fully close MGSV before replacing the addon. Use the same MSVC/Ninja Release configuration, game settings and light
  scene as the previous test, and no DevKit live overrides.
2. Select **AMD FSR 3.1.5** and set **FSR Legacy Compute State** **On**. If either was not the startup state, exit and
  launch once more before measuring. Do not use Reset All; that changes unrelated comparison settings.
3. View the affected light stationary and during a slow pan for at least 15 seconds. No projection, motion, camera-
  capture/commit or FSR shader/reset behavior is changed by this diagnostic. The original light VS is still in use.
4. In the retained no-DLSS variant, verify **DLSS runtime isolation=true**, effective FSR3 and **FSR state=legacy-compute**.
  There must be no bridge-install, DLSS-available, feature-created, scheduling or evaluation messages. Game/ReShade
  queries still run; only the addon's DLSS instrumentation and NGX startup are excluded.
5. If the stored mode was DLSS, the selector shows **Off (DLSS isolation build)** until another mode is chosen. The
  disabled build does not erase the stored DLSS request/model automatically. Choosing FSR explicitly saves that choice.
6. The deployed follow-up sets only `RUNTIME_DISABLED_FOR_FSR_ISOLATION=false` relative to the matched no-DLSS build.
  Start fresh in **FSR3 with Legacy Compute State On**. Expect **isolation=false**, the usual capability/bridge startup,
  but no DLSS feature evaluation until explicitly selecting DLSS. Compare FSR first before switching methods.

If the Legacy On improvement survives the normal runtime, retain it and investigate the specific extended graphics/OM
state operations independently. If it regresses, return to the matched no-DLSS variant and isolate NGX initialization
from query/list tracking. DLSS still uses extended preservation and needs its own validation; do not switch NGX to the
smaller FSR profile or retune FSR to mask an unresolved state/input defect.

### Other temporal verification

Analytical TAA verification:

1. Select **Off (Vanilla FXAA)** and confirm vanilla FXAA and projection behavior.
2. Select **Analytical TAA**, set jitter **Off**, and confirm one **temporal reconstruction selected** line followed by
  one **TAA accumulation started** line; standing still must not repeatedly restart accumulation.
3. Switch analytical TAA to **Halton**. With `ENABLE_TAA_PROJECTION_JITTER_DIAGNOSTICS=1`, test each additional jitter slider independently
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
9. Select Off and confirm a **temporal reconstruction disabled** line, exact projection restoration, and no
  stale-history frame.

Default FSR3 verification:

1. Launch with no persisted temporal-mode or legacy temporal keys and confirm **AMD FSR 3.1.5** is selected and the
  jitter-pattern control is hidden; FSR3 uses the eight-phase Halton sequence internally.
2. Confirm one **FSR3.1 D3D11 context probe succeeded** line with `host_version=3.1.5` and
  `backend_interface=2.3.0`, followed by one **AMD FSR3.1 accumulation started** line and no recurring setup, format, or
  camera-matrix warnings.
3. Compare static detail, camera and character motion, thin wires, aiming/FOV transitions, camera cuts, DoF, motion blur,
  rain, menus, and resolution/device resets against analytical TAA.
4. Confirm the image remains in MGSV's expected encoded scene domain and downstream alpha-driven DoF/highlights remain
  unchanged apart from temporal reconstruction.
5. Pay particular attention to rain, particles, reflections, emissive transparency, and animated textures while external
  masks are null. Repeat those scenes when each planned game-derived mask is connected.
6. Confirm switching to Analytical TAA releases the FSR3 context, exposes the jitter selector, and seeds analytical
  history once; switching back recreates FSR3 and restores its enforced Halton sequence.

DLSS verification:

1. Cold-start Off, FSR or Analytical. Expect **DLSS startup=vendor-only** and one **DLSS adapter gate**; no activation,
  NGX-available, bridge-install, or feature-created message before selecting DLSS. Opening the selector changes nothing.
2. On a non-NVIDIA adapter or failed vendor query, DLSS stays gray with red hover text and no DLL/NGX work. On NVIDIA
  it is initially selectable even with the DLL absent: absence is intentionally discovered only after a request.
3. Select DLSS without the DLL present: expect a cached missing-DLL reason, Off, no hook installation, and no repeated
  initialization. Only remove/replace DLLs while the game is stopped; restart after installing a compatible DLL.
4. With the DLL installed, select DLSS from FSR/Analytical. Expect activation, NGX availability, bridge install, runtime
  readiness, then feature creation and a history seed after a known deferred-recording boundary. Existing recordings
  must not be forcibly split to warm tracking. Check cancellation by selecting Off before pending work is serviced.
5. Restart with DLSS saved and verify request-time activation still occurs, but never dispatches before readiness.
  Check supported model hints A-F/J-M, E default when available, and independent model/resolution recreation.
6. Check DLSS-to-FSR/Off/Analytical and back: prefixes still execute, incompatible generations do not evaluate NGX,
  compatible bundles resume with fresh history, and no ordinary switch tears down the native hooks or retained feature.
7. Validate static detail, camera/object motion, reverse-Z disocclusion, jitter stability, camera cuts, alpha-driven
  effects, DoF, motion blur, state restoration and normal exit. Startup/lazy activation changes are not a proven flicker fix.

For a stationary high-frequency grating, isolate the resolve parameters rather than changing several simultaneously:

1. Record the baseline at Clip Tightness `0.50`, History Clip Strength `1.00`, and Current Frame Blend `0.15`.
2. Test Clip Tightness `0.00`, then restore `0.50`.
3. Test History Clip Strength `0.00`, then restore `1.00`.
4. Test Current Frame Blend `0.00`, then restore `0.15`.
5. If one extreme stabilizes the grating, increase it from zero until flicker returns and check moving characters and
  camera motion for ghosting before considering a new default.