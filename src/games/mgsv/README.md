# MGSV RenoDX Addon Documentation

## Overview

The MGSV RenoDX addon enhances **Metal Gear Solid V: The Phantom Pain** with HDR color grading and tone mapping. It upgrades SDR (8-bit BGRA) render targets to HDR (16-bit float RGBA) for key rendering passes, enabling:

- **Filmic tone mapping** from deferred rendering paths
- **LUT-based color grading** with extended color precision
- **Motion blur** with HDR-aware rendering
- **Depth-of-field effects** with an HDR pre-final scene copy
- **Optional temporal anti-aliasing** with native projection jitter and high-precision velocity
- **Contact-hardening and screen-space shadows** for the sun, spot lights and point lights
- **Ambient occlusion** from the visibility-bitmask paper, applied after the game's subsurface scatter
- **Screen-space indirect lighting** with temporal accumulation, a-trous denoise and a depth-joint resolve
- **User-configurable settings** via the ReShade overlay UI

---

## Architecture

See the [current ownership map](docs/ARCHITECTURE.md), [AO findings](lighting/ao/MGSV_AmbientOcclusion.md), [indirect-lighting notes](lighting/il/MGSV_IndirectLighting.md), and [reconstruction guide](reconstruction/README.md). Feature notes preserve historical measurements; they are not fresh validation of the current consolidation.

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
- Captures the final camera/object velocity target and routes validated inputs to Analytical TAA, FSR3, or DLSS
- Requires exact frame, sample, and render-dimension agreement between the native jitter and resolve
- Defaults new profiles to FSR3 while preserving vanilla FXAA and unjittered projection in Off

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

### velocity_upgrade_info

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

### LUT Builder (`lut_builder_upgrade_info`)

**Purpose**: Marks 256×16 LUT builder target as RGBA16F for internal precision.

The descriptor lives in root `resource_upgrades.hpp`; `addon.cpp` still registers it once during setup.

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
| 0x9815404F | GBufferVelocity | Deformation-aware object velocity | ✓ | Identified producer | RGBA16F precision independent of AA; native encoding and optional unclamp unchanged |
| 0x58C10658 | GBufferMaskedVelocity | Alpha-tested object velocity | ✓ | Identified producer | Precision independent of AA; native alpha tests and optional unclamp unchanged |
| 0xA13321B6 | MotionBlurCameraVelocity | Final full-resolution velocity | ✓ | Identified producer | Precision independent of AA; RGBA16F `.ba` velocity and optional unclamp unchanged |
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
| 0x5AFD2E17 | SunLight_DL_CS_ES | Sun shadow and lighting | ✓ | No | PCSS contact hardening and screen-space shadows |
| 0x08292A5E | SSLighting2_Spot | Spot shadow and lighting | ✓ | No | PCSS contact hardening and screen-space shadows |
| 0x1D4DB9C5 | SSLighting2_Spot (distance attenuation) | Spot permutation | ✓ | No | Same shadow code as 0x08292A5E |
| 0x80F0A0BC | SSLighting2_Point | Point shadow and lighting | ✓ | No | Dual-paraboloid map; PCSS and screen-space shadows |

**Legend:**
- **Hash**: CRC32 of compiled DXBC bytecode
- **Clone?**: Whether clone/hot-swap is activated
- **Conditional**: Activated only when a same-frame shader sequence proves the resource role
- **RTV0 only**: Only first render target upgraded (others are material/GBuffer targets)

---

## Clone Activation Flow

### Marking Phase (Per-Draw)

When a shader in the assembled `custom_shaders` map is intercepted:

1. **Get RTVs**: Retrieve bound render targets via `GetRenderTargets(cmd_list)`
2. **Mark Target**: Use `rendering::resource_upgrades::MarkSceneTonemapCloneTarget()` or `MarkCloneTarget()` with the appropriate role for a sequence-proven destination
   - Checks if the underlying resource matches upgrade criteria (format, size, usage)
   - Records upgrade info pointer in resource metadata
3. **Activate Clone**: Call `ActivateCloneHotSwapIfTracked(device, rtv)`
   - Queries metadata to see if this resource is a clone candidate
   - Calls `renodx::utils::resource::upgrade::ActivateCloneHotSwap()` 
   - ReShade internally switches render target binding to the HDR clone

### Reset Phase (Per-Present)

`ResetTonemapCopyTracking()` clears frame-local state:
- `dof_copy_state.upgrade_allowed` → `false`
- `motion_blur_copy_state.upgrade_allowed` → `false`
- `motion_blur_copy_state.copy_back_pending` → `false`
- `tonemap_copy_state.propagation_allowed` → `false`
- `tonemap_copy_state.source` → null

These role states group the existing tracking fields without changing reset behavior. Present does not clear the motion-blur resource list; its existing copy-back cleanup remains separate.

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

## Temporal Anti-Aliasing

The **Temporal Anti-Aliasing** section is the first addon settings section. Its single **Temporal Reconstruction**
dropdown selects **Off (Vanilla FXAA)**, **Analytical TAA**, **AMD FSR 3.1.5**, or **NVIDIA DLSS**. New profiles default
to FSR3. Off keeps original FXAA and unjittered projection. These controls are global, appear above the preset slider,
and are not changed by RenoDX preset selection.

- **DLSS Model** defaults/resets to F (6), subject to feature-DLL support. DLSS performs only a vendor check at startup;
  DLL/NGX/query-hook work waits for explicit or restored DLSS selection.
- Both diagnostic UI macros remain enabled. The ten remaining Analytical projection sliders default/reset to `1x`; FSR3/DLSS use `1x` on the original six and four enabled experimental paths. Alternate Stream correction is disabled for every method, its slider is removed, and old `FxTaaAlternateCameraJitterScale` keys are ignored. The four enabled experimental paths still need visual validation.
- Analytical exposes Halton/Off jitter and resolve tuning. FSR3/DLSS enforce eight-phase Halton without changing the
  saved Analytical preference. **Unclamp Motion Vectors** remains a shared default-Off diagnostic.
- The legacy FSR compute-only path and toggle are removed after successful user validation with Legacy Off. All methods
  use extended compute/graphics/OM preservation, including depth-stencil state; old saved legacy keys are ignored.
- Temporary pose/jitter-link readbacks, dumps, and per-path telemetry are absent. Normal camera/input ownership,
  DLSS query tracking, original command-list preservation, and SDK cleanup remain.

Temporal reconstruction preserves native resolution, bone-aware object motion, and current scene alpha. Invalid or
mismatched inputs fail closed; lower-resolution DoF candidates are skipped. **Character warble and rapid night-vision
skinned-mesh flicker remain unresolved.** Neither diagnostic defaults nor successful builds establish a visual fix.
FSR external reactive/transparency masks and a proven native camera-cut signal remain pending.

**Restart MGSV to replace or remove the addon.** Native projection hooks and the module are pinned for process lifetime;
device destruction stops admission rather than unpatching/freeing trampolines. Live native-hook unloading is unsupported.

See the [temporal reference](reconstruction/README.md) for controls, motion/jitter ownership, SDK lifecycle, and limitations; the [roadmap](reconstruction/ROADMAP.md) contains only outstanding work.

---

## Shadows

The sun, spot and point light shaders carry two optional terms.

**Contact hardening** replaces the fixed-radius PCF with a PCSS blocker search: the penumbra grows with the separation between caster and receiver, so contact edges stay sharp and farther shadows soften. Each light type has its own toggle and softness; response, blur radius and darkness are in the Advanced group.

- Sun: cascade shadow atlas.
- Spot: one projective map per light; also exposes cone-edge falloff.
- Point: dual-paraboloid atlas. Softness is approximate near the hemisphere seam because texel size compresses toward the rim.

**Screen-space shadows** march the depth buffer toward the light and attenuate where the ray passes behind a nearer surface. They run for the sun once per frame and for each spot and point draw. Strength and reach are per light type; thickness, fade end and the material limit are in Advanced. The material limit gates both receiver and caster so characters do not shadow themselves while still casting onto the ground.

Settings are under **Contact-Hardening Shadows** and **Screen-Space Shadows**, each with an Advanced section. Controls for each feature are shown while it is enabled. Disabling contact hardening restores the game's shadow filter; disabling screen-space shadows removes only the added screen-space effect.

---

## Ambient Occlusion

The two compute passes at `0x6DAF0C4A` and `0xB2601810` perform the game's screen-space **subsurface scattering**, which contributes to skin shading. The first has a generated replacement from its port; the second has a callback-only registration that runs added lighting afterward. This structural cleanup preserves those registrations and does not replace scattering with AO.

After stage 2, the addon calculates AO in a separate texture, then applies it to the light buffer. It darkens RGB and preserves alpha. The existing scattering registrations are preserved and AO runs afterward, darkening their result, including directly lit surfaces.

The technique is based on *Screen Space Indirect Lighting with Visibility Bitmask* (Therrien, Levesque, Gilet, The Visual Computer 2023), with five documented differences. See [MGSV_AmbientOcclusion.md](lighting/ao/MGSV_AmbientOcclusion.md) for the paper comparison, earlier measurements, and remaining questions. Read those notes before changing the back-sample direction, sector rounding, or slice weights.

Use **Visibility-Bitmask Ambient Occlusion** in the **Ambient Occlusion** section to enable or disable this effect. Turning it off skips only the added AO passes; indirect lighting and other enhancements keep their own settings.

---

## Indirect Lighting

The addon estimates bounced light from surfaces visible on screen, using the game's existing lighting. It runs after the game's subsurface scattering and cannot include off-screen surfaces.

The chain has four stages; denoising records one dispatch per iteration:

1. **Gather** — samples nearby surfaces in a disk, rejecting out-of-bounds and back-facing samples. Radiance comes from the deferred composite's albedo and specular inputs, so highlights and emissive surfaces contribute.
2. **Accumulate** — reprojects the previous result with the game's velocity buffer, rejects depth/normal mismatches, and tracks history length and luminance moments.
3. **Denoise** — one to three a-trous iterations weighted by depth, normal and measured per-pixel variance.
4. **Add** — uses depth to upsample the filtered result to full resolution, then adds it to the light buffer.

Settings are in **Indirect Lighting**: the toggle, **Indirect Lighting Strength**, and **Indirect Lighting Sampling Radius**. Sample count, resolution, denoise iterations, history weight, and debug views are in **Indirect Lighting (Advanced)**. Gather, history, and denoising use the selected Full, Half, or Quarter resolution; the default is Quarter. The defaults are strength 80, sampling radius 80, 8 samples, and 3 denoise iterations.

Indirect lighting is added before AO is applied, so AO darkens both direct and indirect light. AO and indirect lighting are independent toggles.

---

## Code Structure

The shared precision upgrades are a rendering foundation, not an HDR-output feature. Output, reconstruction, and lighting effects use that foundation but own different controls and resources.

| Location | Responsibility |
|---|---|
| `addon.cpp` | Owns shader data, output and reconstruction settings state, and assembled registries; utility setup, shared descriptor/draw registrations and lifecycle order |
| `inputs.hpp` | Shared descriptor tracking; retained latest `Motion`, `lighting_motion` compatibility and `Materials`; first-present single-device/swapchain admission, generations and publication tickets |
| `settings.hpp` | Core output, tone-mapping, grading and existing-effect controls; Reset All and Preset Off policy |
| `shadows/settings.hpp` | Contact-hardening and screen-space-shadow controls |
| `resource_upgrades.hpp` | Scene, DoF, motion-blur, and velocity roles/clone handling; retains `mgsv::rendering::resource_upgrades` |
| `lighting/lighting.hpp` | `mgsv::lighting` support, admitted retained-input reads and GI-then-AO orchestration; AO/IL implementations/settings keep `mgsv::ao` and `mgsv::il` |
| `d3d11_compute_state.hpp` | Shared native preservation; retains `mgsv::rendering::d3d11_compute_state` |
| `shaders.hpp` | Registry and callback references; retains `mgsv::game::CreateShaders` |
| `output/output.hpp` | SDR/HDR output selection and brightness synchronization |
| `reconstruction/settings.hpp` | Reconstruction descriptor assembly and migration; migration is invoked explicitly during addon setup |
| `reconstruction/runtime.hpp` | Settings state/writes, transitions, lifecycle and merged MGSV routing in `mgsv::game::reconstruction` |
| `reconstruction/input_capture.hpp` | Retained-motion adapter with camera/frame/sample annotations and insertion-time matching dimensions and single-mip/layer/sample checks |
| `reconstruction/frame_inputs.hpp` | Backend input/output contract and shared 96-byte SDK `PrepareConstants` |
| `reconstruction/prepare_inputs.hlsli`, `reconstruction/encode_output.hlsli` | Shared SDK boundary math; backend entry shaders, resources and lifetimes stay distinct |
| `reconstruction/dlss/model.hpp` | Pure model metadata, default and lookup without SDK/runtime dependencies |
| `lighting/ao/`, `lighting/il/` | AO/IL shaders and research notes; C++ resources, execution and controls live in `lighting/lighting.hpp` |
| `reconstruction/analytical/` | Analytical runtime and `mgsv_taa.cs_5_0.hlsl` resolve shader |
| `motion/`, `subsurface/` | Native velocity replacements and subsurface-scatter ports respectively |
| `reflections/`, `gbuffer/` | Local-reflection pair and rain-material pair respectively |
| `copy/`, `output/` | Shared copy shaders and final GammaControl output shader respectively |
| `docs/` | Architecture, [engine pipeline reference](docs/Fox_Engine_Render_Pipeline_Reference.md), and [retail shader name map](docs/MGSV_PixelShader_NameMap.md) |

See [the architecture guide](docs/ARCHITECTURE.md) for the dependency map and remaining limitations. Shader registration is a hash lookup, not the order in which render passes execute. The standard `mgsv` build target discovers the shaders and generates their embedded headers without requiring local development tests.

The consolidation reduced authored MGSV `.cpp`/`.hpp` files from 27 to 24; root `inputs.hpp` brings the current count to 25, excluding the FSR vendor subtree. Obsolete copies under the old `game/`, `rendering/`, `ao/` and `il/` roots are removed. Lighting support, AO, IL and their orchestration share `lighting/lighting.hpp`; the game draw router is merged into `reconstruction/runtime.hpp`. Root `shaders.hpp` holds registration and callback wiring, without forwarding replacements. Heavy backends and the intact FSR vendor subtree remain separate. `LightPass` caches only a pipeline, independent of effect dimensions; texture resize/history rules remain feature-owned.

Shared input ownership now exists, but qualified independent GI is **BLOCKED**. Identified precision preparation precedes AA-enable and descriptor-data returns under the existing execution guard. Effective reconstruction or GI can demand motion observations; Off+GI updates latest only and never changes the retained `lighting_motion` compatibility selection used by GI. Materials are retained pairs, not borrowed cache handles, and GI no longer reads reconstruction storage. The owner uses native getters plus explicit enabled-clone pinning under each clone's own live record, not exact parity with old descriptor-state handles. It has no per-context retained cache or demand-off clearing; references remain bounded until replacement/reset/retirement.

First-present single-device/swapchain admission gates lighting allocations: work skips before the first selected Present and during retirement/resize. Shared selection also guards reconstruction's Present-time device binding. Shared motion publication does not reject mismatched depth/object extents before GI compatibility publication; matching dimensions and single-mip/layer/sample requirements remain reconstruction adapter checks at insertion. Retention is **not pixel-content, freshness or GPU-order proof**. Shader/history algorithms, defaults/settings order, SDK activation timing and registry precedence are unchanged.

The [current ownership checkpoint](docs/ARCHITECTURE.md#current-ownership-checkpoint--offline-validation-completed) records the standalone build, all 21 local checks rerun against the latest code, and the successful user-reported gameplay smoke test on 2026-10-30. The log corroborates AO/GI recording, 4K DLSS/Analytical transitions, DLSS F/M changes and normal exit; FSR and cold Off+GI were not demonstrated in that run. Hardware fixtures cover actual immediate/deferred wrappers, clone metadata and readbacks for ownership only. Live producer/content/order proof and the cold-history defect still block qualified GI handoff.

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

With MGSV fully stopped, use CMake Tools in VS Code and build only `mgsv`. Use the repository's Clang debug profile for development; the structural cleanup was also checked with the existing approved MSVC/Ninja Release comparison profile. Build and offline-test success do not establish temporal visual stability. Inspect these outputs for the chosen configuration:

- `build/Release/renodx-mgsv.addon64` (or the matching configuration directory)
- `build/mgsv.include/embed/mgsv_taa.cso`
- `build/mgsv.include/embed/mgsv_taa.h`
- `build/mgsv.include/embed/fsr3_prepare_game_inputs.cso` and `fsr3_encode_game_output.cso`
- `build/mgsv.include/embed/dlss_prepare_game_inputs.cso` and `dlss_encode_game_output.cso`
- `build/mgsv.include/embed/fsr3sdk_prepare_inputs.cso`, `fsr3sdk_luma_pyramid.cso`,
  `fsr3sdk_shading_change_pyramid.cso`, `fsr3sdk_shading_change.cso`, `fsr3sdk_prepare_reactivity.cso`,
  `fsr3sdk_luma_instability.cso`, `fsr3sdk_accumulate.cso`, `fsr3sdk_accumulate_sharpen.cso`, `fsr3sdk_rcas.cso`,
  `fsr3sdk_generate_reactive.cso`, and `fsr3sdk_debug_view.cso`
- `build/mgsv.include/embed/0x9815404F.cso`, `0x58C10658.cso`, and `0xA13321B6.cso`

### Deploy

With MGSV still stopped, copy the matching `renodx-mgsv.addon64` beside `mgsvtpp.exe`, then restart the game.
Use one game addon and disable live shader overrides for validation. DLSS additionally needs a compatible
`nvngx_dlss.dll` beside the executable; it is not bundled by the addon. Replace either DLL only while the game is stopped.

### Enable in ReShade

1. Launch MGSV with ReShade
2. Press `Home` to open overlay
3. Navigate to **Addons** section
4. Enable **RenoDX** checkbox
5. Configure tone mapping / color grading settings

### Manual TAA Verification

1. Confirm the addon loads and settings match the selected preset. Check Off for vanilla FXAA and no residual shift.
2. Check FSR3/Analytical accumulation seeds once per intended reset, not repeatedly in a static scene. Validate DLSS
   request-time activation and suspension/resume separately from model/resize recreation.
3. Inspect static and moving detail, skinned characters, menus, DoF/motion blur, rain/transparency, night vision, camera
   transitions, and resolution changes. Keep known warble/ghosting distinct from startup or history-reset failures.
4. Follow the [detailed temporal validation matrix](reconstruction/README.md#manual-validation) for coverage and lifecycle checks.
   No automatic diagnostic capture is present, and this documentation cleanup adds no runtime test result.

### Manual Lighting Verification

Follow the build and deployment steps above. Use the matching `renodx-mgsv.addon64` with live shader overrides disabled.

**In-game checks**

- Compare AO only, indirect lighting only, both enabled, and both disabled. Keep other settings unchanged.
- Test **AO Depth Mip Sampling** on and off, and test Full, Half, and Quarter resolution for each effect.
- Test **Indirect Lighting Denoise Iterations** at 1, 2, and 3, especially 2, which previously had an input/output conflict.
- Check skin shading, rain, fog, night vision, menus, and resolution changes. Test Preset Off and repeated effect toggles.
- Hold the camera still and compare indirect-lighting noise and stability with the previous build. The input-handling changes do not alter the shader's history-rejection rules or tuning.

**Developer checks**

- Follow the [two-frame GI qualification plan](docs/ARCHITECTURE.md#manual-gi-qualification-plan--not-yet-completed): trace `0xB2601810` -> GI against `0xA13321B6` motion, all writes/clears/copies and sampled subresources, materials/native b0/depth, and first history contents over at least two frames, paired with an untraced control. This is outstanding work, not fixture coverage or approval to feed Off+GI latest motion into GI.
- Look for `MgsvLighting:` in `ReShade.log`. Texture and pipeline creation is logged whenever it actually happens, with dimensions; changing indirect-lighting dimensions also logs history invalidation before resource setup. Strength and radius changes do not rebuild resources and are not logged. Missing-input and setup-failure messages are limited to one report per diagnostic per addon load, as are the initial “commands recorded” summaries. Those summaries do not track later options or prove GPU execution or valid temporal history. These messages cover AO and indirect lighting, not execution of the shadow shaders.
- In a D3D11 GPU capture, confirm that the game's scatter dispatch is followed by indirect lighting, then AO, when enabled. Check that the final indirect-lighting pass reads the last denoise result and never binds that texture as an output.
- Use API tracing or callback instrumentation to compare the bound depth, normal, and light views before and after the added passes. A GPU capture alone does not show when the CPU callback reads these bindings.
- Use the D3D11 debug layer, when available, to check for conflicting input and output bindings.

---

## References

- **ReShade API**: https://github.com/crosire/reshade
- **RenoDX Project**: https://github.com/clshortfuse/renodx
- **MGSV Mod Community**: Discord / GitHub wikis
