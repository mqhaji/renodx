# MGSV RenoDX Addon Documentation

## Overview

The MGSV RenoDX addon enhances **Metal Gear Solid V: The Phantom Pain** with HDR color grading and tone mapping. It upgrades SDR (8-bit BGRA) render targets to HDR (16-bit float RGBA) for key rendering passes, enabling:

- **Filmic tone mapping** from deferred rendering paths
- **LUT-based color grading** with extended color precision
- **Motion blur** with HDR-aware rendering
- **Depth-of-field effects** with an HDR pre-final scene copy
- **Optional temporal anti-aliasing** with native projection jitter and high-precision velocity
- **User-configurable settings** via the ReShade overlay UI

---

## Architecture

### Key Components

**1. Resource Cloning & Hot-Swap**
- Uses ReShade's `resource_view_cloning` and `resource_view_hot_swap` features
- Creates HDR (RGBA16F) clones of original SDR (BGRA8) resources
- Activates clones conditionally on specific shader draws
- Rejects conflicting clone roles for resources reused by unrelated passes

**2. Shader-Based Control Flow**
- Custom shader registrations intercept draws via `on_draw` callbacks
- Callbacks mark render targets as clone candidates
- Separately activate clones only when rendering tone mapping / color grading
- Ensures HDR precision for critical rendering, SDR for everything else

**3. Event Hooks**
- `init_swapchain`: Detect peak brightness and swapchain upgrade state
- `present`: Deactivate the frame-local DoF-copy target and reset copy gates
- `copy_resource`: Propagate an armed scene-color clone to the matching copy destination

**4. Temporal Anti-Aliasing**
- Installs a narrowly validated native projection hook during device initialization
- Captures the final camera/object velocity target and routes validated inputs to FSR3 or analytical history
- Requires exact frame, sample, and render-dimension agreement between the native jitter and resolve
- Leaves TAA disabled by default and preserves the original FXAA path while disabled

---

## Resource Upgrades

### scene_tonemap_upgrade_info

**Purpose**: Marks and activates BGRA8 render targets as HDR RGBA16F clones for tone mapping operations.

```cpp
// Old format: 8-bit BGRA (SDR, unorm)
// New format: 16-bit RGBA float (HDR)
// Size: Full backbuffer (4K @ 3840×2160)
// Features: Clone + hot-swap enabled
// Usage: Render-target exclusive
```

**Activated by these shaders:**
- `0xE04D1471` (Tonemap) — Late LUT/color-grading pass
- `0x410AE8C5`, `0xC973024D`, `0xBDE1F4CD`, `0x6E29F0AB`, `0x2EA8F13F`, `0x59B44963` — Deferred rendering tone mapping

### dof_final_copy_upgrade_info

**Purpose**: Preserves the full-resolution pre-final DoF scene copy in RGBA16F without globally upgrading the generic copy shader.

```cpp
// Old format: 8-bit BGRA typeless
// New format: 16-bit RGBA float
// Size: Full backbuffer
// Features: Clone + hot-swap enabled
// Lifetime: Successful pre-final DoF copy through presentation
```

`DOF_ScatterCompositeNear` and `DOF_ScatterCompositeFar` arm the copy window. The destination is upgraded only when `CopyRenderBuffer` SRV0 is the active scene-tonemap clone. `DOF_ScatterCompositeFinal` closes the window, and `OnPresent()` disables hot-swap for the activated destination while retaining its clone allocation.

### motion_blur_upgrade_info

**Purpose**: Marks and activates BGRA8 render targets as HDR RGBA16F for motion blur rendering.

```cpp
// Old format: 8-bit BGRA (SDR, unorm)
// New format: 16-bit RGBA float (HDR)
// Size: Backbuffer aspect ratio (intermediate targets)
// Features: Clone + hot-swap enabled
// Usage: Render-target exclusive
```

**Activated by:**
- `0xF05DCBFD` / `0x512E2B48` — Arm the next motion-blur input copy
- `0x83272BCB` — Activate the armed half-resolution motion-blur input target
- `0xBFC7D3C2` — Render the two half-resolution motion-blur ping-pong passes
- The post-motion-blur `0x83272BCB` draw — Copy the result back, then deactivate the motion-blur clones

### taa_velocity_upgrade_info

**Purpose**: Preserves both the full-resolution deformation-aware object-velocity target and the final camera/object
velocity composite at RGBA16F precision for temporal reprojection.

```cpp
// Old resource format: 8-bit BGRA typeless
// New format: 16-bit RGBA float
// Size: Full backbuffer
// Features: Clone + hot-swap enabled
// Velocity channels: .ba
```

The clone descriptor deliberately matches the underlying `b8g8r8a8_typeless` resources rather than their typed
render-target views. The addon activates the clone before the first `GBufferVelocity` or `GBufferMaskedVelocity` draw and
keeps it active through `MotionBlurCameraVelocity`, which composites camera and object motion into a separate RGBA16F
clone for TAA. This avoids losing object motion to BGRA8's approximately 0.502-pixel code spacing before the final float
write. Runtime validation confirmed that both the object input and final velocity are sampled through RGBA16F clones.
Three minimal pixel-shader replacements preserve the native encoding and `/64` scale while allowing the existing
unit-length motion clamp to be bypassed by a default-Off TAA diagnostic.

### LUT Builder (Inline Upgrade)

**Purpose**: Marks 256×16 LUT builder target as RGBA16F for internal precision.

```cpp
// Old format: 8-bit BGRA typeless (SDR)
// New format: 16-bit RGBA float (HDR)
// Size: Fixed 256×16
// Features: Clone + hot-swap enabled
// Initialized in DllMain during addon setup
```

---

## Shader Reference

| Hash | Name | Purpose | RT0 | Clone? | Notes |
|------|------|---------|-----|--------|-------|
| 0xE2D609B1 | DOF_ScatterCompositeNear | Depth-of-field near pass | ✓ | Yes | Arms post-DOF tracking flag |
| 0x7C017264 | DOF_ScatterCompositeFar | Depth-of-field far pass | ✓ | Yes | Refreshes the DoF-copy window |
| 0xFC5542BB | DOF_ScatterCompositeFinal | DoF final composite | ✓ | Yes | Closes the DoF-copy window |
| 0x9815404F | GBufferVelocity | Deformation-aware object velocity | ✓ | TAA | Minimal replacement; shared RGBA16F clone and optional motion unclamp |
| 0x58C10658 | GBufferMaskedVelocity | Alpha-tested object velocity | ✓ | TAA | Minimal replacement preserving native alpha tests and optional motion unclamp |
| 0xA13321B6 | MotionBlurCameraVelocity | Final full-resolution velocity | ✓ | TAA | Minimal replacement; RGBA16F `.ba` velocity and optional camera-motion unclamp |
| 0xF05DCBFD | Motion-blur tile max | Velocity tile preparation | — | No | Arms the next motion-blur input copy |
| 0x512E2B48 | Motion-blur tile refine | Velocity tile refinement | — | No | Refreshes the motion-blur input-copy gate |
| 0x83272BCB | CopyRenderBuffer | Generic scene/intermediate copy | Conditional | Conditional | Upgraded only inside a proven DoF or motion-blur window |
| 0xBFC7D3C2 | MotionBlurMcGuire | Motion-blur ping-pong | ✓ | Yes | Half-resolution HDR intermediates |
| 0x410AE8C5 | DeferredRenderingFilmic | Deferred tone mapping | ✓ | Yes | Primary deferred path |
| 0xC973024D | DeferredRenderingFilmic_VolFog | Deferred + volumetric fog | ✓ | Yes | Deferred with volumetric lighting |
| 0xBDE1F4CD | DR_TonemapRainFilter | Deferred + rain | ✓ | Yes | Only RTV0 upgraded (not material maps) |
| 0x6E29F0AB | DR_TonemapRainFilter_NoIR | Deferred rain (no IR) | ✓ | Yes | RTV0 upgraded; replacement supplies explicit output encoding |
| 0x2EA8F13F | DR_VolFog_TppTonemap | Volumetric fog tone mapping | ✓ | Yes | Vol fog tone mapping pass |
| 0x59B44963 | DR_VolFog_TppTonemap_MD | VolFog + multi-distance | ✓ | Yes | Multi-distance variant |
| 0x637BB745 | LUT Builder (Pass 1) | Color grading LUT construction | ✓ | Yes | Generic LUT builder |
| 0xCA7F0E3D | LUT Builder (Pass 2) | Color grading LUT construction | ✓ | Yes | Generic LUT builder |
| 0x900968FF | FXAA Pass | Antialiasing | ✓ | Yes | Final screen-space AA |
| 0xE04D1471 | Tonemap (LUT/Grading) | Late LUT & color grading | ✓ | Yes | Primary color grading output |

**Legend:**
- **Hash**: CRC32 of compiled DXBC bytecode
- **Clone?**: Whether clone/hot-swap is activated
- **Conditional**: Activated only when a same-frame shader sequence proves the resource role
- **RTV0 only**: Only first render target upgraded (others are material/GBuffer targets)

---

## Clone Activation Flow

### Marking Phase (Per-Draw)

When a shader in the `custom_shaders` array is intercepted:

1. **Get RTVs**: Retrieve bound render targets via `GetRenderTargets(cmd_list)`
2. **Mark Target**: Call `MarkSceneTonemapCloneTarget()`, `MarkMotionBlurCloneTarget()`, or `MarkCloneTarget()` for a sequence-proven copy destination
   - Checks if the underlying resource matches upgrade criteria (format, size, usage)
   - Records upgrade info pointer in resource metadata
3. **Activate Clone**: Call `ActivateCloneHotSwapIfTracked(device, rtv)`
   - Queries metadata to see if this resource is a clone candidate
   - Calls `renodx::utils::resource::upgrade::ActivateCloneHotSwap()` 
   - ReShade internally switches render target binding to the HDR clone

### Reset Phase (Per-Present)

`ResetTonemapCopyTracking()` clears frame-local state:
- `dof_final_copy_upgrade_allowed` → `false`
- `motion_blur_copy_upgrade_allowed` → `false`
- `motion_blur_copy_back_pending` → `false`
- `tonemap_copy_resource_propagation_allowed` → `false`
- `tonemap_copy_resource_source` → null

`OnPresent()` also disables hot-swap for the successfully activated DoF final-copy resource. Its clone target and allocation remain available for reuse.

---

## Scoped `CopyRenderBuffer` and Clone Lifetimes

### The Problem

`CopyRenderBuffer` (0x83272BCB) is a **generic copy shader** used by many unrelated rendering operations:
- Rain/fog deferred paths copy materials and intermediate buffers
- Motion blur passes copy scene temps
- Tone mapping operations copy intermediate results

Globally upgrading this shader, or leaving clone hot-swap active after a resource changes roles, can expose stale or domain-incompatible data as rain materials, sonar textures, SSAO, menu post-processing, or later scene color.

### The Solution

The addon upgrades `CopyRenderBuffer` only inside sequence-proven windows:

1. `DOF_ScatterCompositeNear` and `DOF_ScatterCompositeFar` arm the DoF window.
2. A candidate DoF copy must still read SRV0 from the active scene-tonemap clone.
3. The successful full-resolution DoF-copy target remains active through the final composite, then is disabled at presentation.
4. Motion-blur tile preparation arms only its immediately following input copy.
5. That copy activates the half-resolution motion-blur target without depending on a specific full-resolution clone role, because MGSV rotates the scene source between compatible roles.
6. `MotionBlurMcGuire` tracks both active ping-pong targets.
7. The copy-back draw consumes an active motion-blur SRV0, then its post-draw callback disables all motion-blur hot-swaps.

Clone allocations and targets are retained; only active redirection is disabled. This preserves HDR through DoF and motion blur while preventing those resources from remaining redirected when MGSV reuses them for unrelated work.

The native `copy_resource` callback is separately armed from a proven scene-tonemap output. It marks the matching destination as a scene clone and copies clone-to-clone. Shared resource-upgrade handling continues to redirect copies whose endpoints are already active.

---

## Temporal Anti-Aliasing (Default Off)

The addon includes optional native-resolution temporal reconstruction under **Temporal Anti-Aliasing**. It defaults to
**Off**. While disabled, MGSV keeps its original FXAA path and the native projection remains unmodified. Enabled users
can select **AMD FSR 3.1.5** or the established analytical TAA. FSR3 is the default reconstruction method when no method
key is persisted. The former FSR2 implementation has been removed; both legacy AMD selector values migrate to FSR3, while
an explicitly persisted Analytical TAA value remains analytical.

When enabled, the TAA path:

1. Applies an eight-sample base-(2,3) Halton sequence first to the proven gameplay projection copy at
   `ShaderManager+0x680`.
2. Publishes the exact applied jitter together with its frame token, sample index, and render dimensions.
3. Reuses that publication at guarded velocity, forward/model/alpha/overlay, and local-light native boundaries that
   otherwise recopy persistent unjittered viewport projection.
4. Captures vanilla no-jitter projection/view matrices at the main boundary, computes the current inverse and
   previous VP relation in double precision, and promotes current VP only after a successful temporal dispatch.
5. Captures the final `MotionBlurCameraVelocity` target, depth, object velocity, and matching camera publication into one
   validated game-native frame.
6. Builds linear color and signed RG16F motion, then runs AMD's FSR3 3.1.5 host schedule through the custom D3D11/SM5
   backend or the optional analytical history resolve. Both retain exact matrix camera motion for background pixels and
   MGSV's deformation-aware object motion.
7. Resolves at the first `DOF_ScatterBakeFirst` invocation whose scene color matches the full-resolution depth and motion
   inputs. Lower-resolution DoF invocations are skipped so the fallback cascade can continue.
8. Bypasses the original FXAA filter while TAA is enabled.

**TAA Jitter Pattern** is visible only with Analytical TAA and exposes the production eight-phase Halton sequence plus an
**Off** diagnostic that leaves the analytical resolve active with zero projection jitter. FSR3 always uses eight-phase
Halton and hides the selector. The default build hides the extended diagnostic view, velocity range, object-motion selector, and per-path jitter controls behind
`ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS=0`; their runtime values are pinned to production defaults. Analytical resolve-tuning
controls are hidden while FSR3 is selected. The default-Off **Unclamp Motion Vectors** experiment remains shared. Relevant
changes invalidate and reseed history.
FSR3 forces only the effective runtime pattern, so switching back to Analytical TAA restores its persisted Off/Halton
preference.

The current FSR3 dispatch does not provide external reactive or transparency/composition masks. AMD's internal
shading-change, prepare-reactivity, disocclusion, motion-divergence, and luma-instability passes remain active. Planned
game-derived mask integration, beginning with proven material paths such as `TppFxRain`, is tracked in
[`taa/ROADMAP.md`](taa/ROADMAP.md). Sharpening is disabled even though the host creates its RCAS pipeline.

The coordinator fails closed: a missing camera publication, stale capture, device mismatch, or resource mismatch skips
that insertion candidate instead of blending unrelated inputs. Camera, depth, final velocity, and object velocity are
snapshotted together and validated once before method dispatch. MGSV callbacks may trail `Present` by one epoch only when
their Halton sample still matches. Presents without a new full-resolution insertion candidate preserve history; history
resets only after a matching candidate is seen but cannot resolve. Enable/disable transitions also reset temporal state,
and disabling verifies exact restoration of the vanilla projection copy. A former scoped
replacement for VS `0x200DBED9` previously proved the missing light jitter and has been removed. Brief runtime testing indicates that the native
alpha-model correction controls the affected lights; the separate guarded local-light callback remains an additional
known-path correction pending isolated runtime classification.

Default builds log explicit TAA enable/disable transitions. FSR3 logs **FSR3.1 D3D11 context probe succeeded** when its
context is created and **AMD FSR3.1 accumulation started** after each reset; analytical TAA logs **TAA accumulation
started**. Repeated
accumulation-start lines while settings and resolution are unchanged indicate that history is still being reset.
Persisted startup state and waiting for the first native publication are logged once. The expected lower-resolution DoF
candidate is also logged only once per device lifetime.

See [`taa/README.md`](taa/README.md) for the current implementation and validation contract. Future temporal quality and
native-resolution DLAA work is tracked in [`taa/ROADMAP.md`](taa/ROADMAP.md).

---

## Code Structure

### State Variables

```cpp
bool dof_final_copy_upgrade_allowed;
reshade::api::resource dof_final_copy_active_resource;
std::vector<reshade::api::resource> motion_blur_active_resources;
bool motion_blur_copy_upgrade_allowed;
bool motion_blur_copy_back_pending;
bool tonemap_copy_resource_propagation_allowed;
reshade::api::resource tonemap_copy_resource_source;
```

### Key Functions

**Marking Functions:**
- `MarkSceneTonemapCloneTarget(rtv, device)` — Mark target for scene tonemap upgrade
- `MarkMotionBlurCloneTarget(rtv, device)` — Mark target for motion blur upgrade
- `MarkCloneTarget(rtv, upgrade_info, device)` — Generic marking function

**Activation Functions:**
- `ActivateCloneHotSwapIfTracked(device, rtv)` — Activate clone if marked
- `PixelShaderResourceMatchesCloneTarget(cmd_list, upgrade_info)` — Check PS slot 0
- `UpgradeCopyRenderBufferTarget(cmd_list)` — Upgrade an armed DoF or motion-blur copy target

**Arming/Cleanup:**
- `ArmDofFinalCopyRenderBufferWindow(cmd_list)` — Arm the next qualifying DoF copy
- `ArmMotionBlurCopyRenderBufferWindow(cmd_list)` — Arm the next motion-blur input copy
- `DeactivateDofFinalCopyRenderBufferTarget()` — Disable the frame-local DoF-copy target
- `DeactivateMotionBlurTargets()` — Disable motion targets after copy-back
- `ResetTonemapCopyTracking()` — Called on present

### Macros

```cpp
// Activate clone on all RTVs (standard pattern)
#define UpgradeRTVReplaceShader(value) { ... }

// Activate clone + call callback (used for DOF arming)
#define UpgradeRTVReplaceShaderCallback(value, callback) { ... }

// Mark + activate for tonemap (full marking + activation)
#define UpgradeTonemapOutputRTV(value) { ... }

// Mark + activate for deferred tonemap (RTV0 only)
#define UpgradeDeferredTonemapOutputRTV(value, name) { ... }

// Arm the next motion-blur input copy from a tile-preparation marker
#define TrackMotionBlurTilePrep(value) { ... }

// Route only sequence-proven DoF/motion-blur copies through HDR targets
#define UpgradeCopyRenderBufferRTV(value) { ... }

// Motion blur variant
#define UpgradeMotionBlurRTV(value) { ... }
```

### Custom Shaders Array

The `custom_shaders` array defines the shader interception pipeline. **Order matters**:
1. DoF windows, motion-blur sequence markers, and scoped `CopyRenderBuffer` handling
2. Deferred tone mapping upgrades
3. LUT builder and FXAA upgrades
4. Final LUT/color-grading upgrade
5. `__ALL_CUSTOM_SHADERS` (must be last — includes all embedded shader codes)

---

## Next Steps / Future Work

### Extended Color Grading

- Multi-LUT stacking
- Per-zone color grading (outdoor, indoor, etc.)
- HDR-specific grade operators (Rec.2020 gamut mapping, etc.)

### Performance Optimization

- Profile clone activation overhead
- Consider lazy activation (mark only when needed)
- Batch descriptor updates

### Testing & Validation

- Automated screenshot comparisons (vanilla vs. RenoDX)
- Memory profiling (clone resource footprint)
- Frame time analysis (draw call overhead)

---

## Building & Deployment

### Build

Use CMake Tools in VS Code, select the desired configuration, and build the `mgsv` target. Inspect these outputs:

- `build/Release/renodx-mgsv.addon64` (or the matching configuration directory)
- `build/mgsv.include/embed/mgsv_taa.cso`
- `build/mgsv.include/embed/mgsv_taa.h`
- `build/mgsv.include/embed/fsr3_prepare_game_inputs.cso` and `fsr3_encode_game_output.cso`
- `build/mgsv.include/embed/fsr3sdk_prepare_inputs.cso`, `fsr3sdk_luma_pyramid.cso`,
  `fsr3sdk_shading_change_pyramid.cso`, `fsr3sdk_shading_change.cso`, `fsr3sdk_prepare_reactivity.cso`,
  `fsr3sdk_luma_instability.cso`, `fsr3sdk_accumulate.cso`, `fsr3sdk_accumulate_sharpen.cso`, `fsr3sdk_rcas.cso`,
  `fsr3sdk_generate_reactive.cso`, and `fsr3sdk_debug_view.cso`
- `build/mgsv.include/embed/0x9815404F.cso`, `0x58C10658.cso`, and `0xA13321B6.cso`

### Deploy

Copy the built addon to the game folder:
```
copy build\Release\renodx-mgsv.addon64 "C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP\"
```

### Enable in ReShade

1. Launch MGSV with ReShade
2. Press `Home` to open overlay
3. Navigate to **Addons** section
4. Enable **RenoDX** checkbox
5. Configure tone mapping / color grading settings

### Manual TAA Verification

1. Start with **Temporal Anti-Aliasing** disabled and confirm the original FXAA presentation is stable.
2. Enable TAA with the default **AMD FSR 3.1.5** method. Confirm the jitter-pattern control is hidden, one **FSR3.1 D3D11
   context probe succeeded** line appears, and one **AMD FSR3.1 accumulation started** line is logged. Standing still
   should not restart it.
3. Inspect static edges, thin wires, foliage, slow and fast camera pans, aiming, binoculars, menus, camera cuts, DoF, and
   motion blur. Pay particular attention to moving silhouettes, disocclusions, rain, particles, and transparency while
   external reactive masks are not yet connected.
4. Switch to **Analytical TAA**, verify one reset and that **TAA Jitter Pattern** becomes visible, then compare it with
   FSR3. Analytical jitter **Off** remains diagnostic; FSR3 always enforces the eight-phase Halton sequence.
5. Compare **Unclamp Motion Vectors** Off/On during motion above approximately 64 pixels; verify object motion and native
   motion blur, then return it to its default **Off** state.
6. With `ENABLE_TAA_MOTION_JITTER_DIAGNOSTICS=1`, exercise all diagnostic views and per-path controls; otherwise verify
   the production defaults and confirm the log has no recurring publication, capture, setup, or dispatch warnings.
7. Disable TAA and confirm a **TAA runtime disabled** line, then verify that the scene returns without a persistent
   subpixel shift, stale-history frame, or freeze.
8. Repeat an enable/disable cycle after a resolution or display-mode change to verify history is recreated at the new size.

---

## References

- **ReShade API**: https://github.com/crosire/reshade
- **RenoDX Project**: https://github.com/clshortfuse/renodx
- **MGSV Mod Community**: Discord / GitHub wikis
