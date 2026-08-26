# MGSV RenoDX Addon Documentation

## Overview

The MGSV RenoDX addon enhances **Metal Gear Solid V: The Phantom Pain** with HDR color grading and tone mapping. It upgrades SDR (8-bit BGRA) render targets to HDR (16-bit float RGBA) for key rendering passes, enabling:

- **Filmic tone mapping** from deferred rendering paths
- **LUT-based color grading** with extended color precision
- **Motion blur** with HDR-aware rendering
- **Depth-of-field effects** with an HDR pre-final scene copy
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

```bash
cd c:\Dev\renodx
build.cmd
```

Or use CMake Tools in VS Code:
```
Build (mgsv target)
```

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

---

## References

- **ReShade API**: https://github.com/crosire/reshade
- **RenoDX Project**: https://github.com/clshortfuse/renodx
- **MGSV Mod Community**: Discord / GitHub wikis

---

*Documentation generated for RenoDX MGSV addon. For questions or contributions, see the RenoDX GitHub repository.*
