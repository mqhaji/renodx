# MGSV RenoDX Addon Documentation

## Overview

The MGSV RenoDX addon enhances **Metal Gear Solid V: The Phantom Pain** with HDR color grading and tone mapping. It upgrades SDR (8-bit BGRA) render targets to HDR (16-bit float RGBA) for key rendering passes, enabling:

- **Filmic tone mapping** from deferred rendering paths
- **LUT-based color grading** with extended color precision
- **Motion blur** with HDR-aware rendering
- **Depth-of-field effects** (currently SDR-clamped)
- **User-configurable settings** via the ReShade overlay UI

---

## Architecture

### Key Components

**1. Resource Cloning & Hot-Swap**
- Uses ReShade's `resource_view_cloning` and `resource_view_hot_swap` features
- Creates HDR (RGBA16F) clones of original SDR (BGRA8) resources
- Activates clones conditionally on specific shader draws
- Allows multiple upgrade infos to share the same physical resource

**2. Shader-Based Control Flow**
- Custom shader replacements intercept draws via `on_draw` callbacks
- Callbacks mark render targets as clone candidates
- Separately activate clones only when rendering tone mapping / color grading
- Ensures HDR precision for critical rendering, SDR for everything else

**3. Event Hooks**
- `init_swapchain`: Initialize upgrade infos and mark resource candidates
- `present`: Reset frame-local tracking state
- ~~`copy_resource`: (removed) Was used to propagate clones but caused corruption~~

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
- `0x410AE8C5`, `0xC973024D`, `0xBDE1F4CD`, `0x2EA8F13F`, `0x59B44963` — Deferred rendering tone mapping
- DOF passes (fire-and-forget, then clamp to SDR)

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
- `0xBFC7D3C2` (MotionBlur) — Full-res motion blur accumulation

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
| 0x7C017264 | DOF_ScatterCompositeFar | Depth-of-field far pass | ✓ | Yes | HDR clone, but DOF output remains SDR |
| 0xFC5542BB | DOF_ScatterCompositeFinal | DOF final composite | ✓ | Yes | SDR-clamped output |
| 0xBFC7D3C2 | MotionBlur | Motion blur full-res | ✓ | Yes | Marked + activated for HDR |
| 0x410AE8C5 | DeferredRenderingFilmic | Deferred tone mapping | ✓ | Yes | Primary deferred path |
| 0xC973024D | DeferredRenderingFilmic_VolFog | Deferred + volumetric fog | ✓ | Yes | Deferred with volumetric lighting |
| 0xBDE1F4CD | DR_TonemapRainFilter | Deferred + rain | ✓ | Yes | Only RTV0 upgraded (not material maps) |
| 0x6E29F0AB | DR_TonemapRainFilter_NoIR | Deferred rain (no IR) | ✓ | Track | Marked but not activated (material target) |
| 0x2EA8F13F | DR_VolFog_TppTonemap | Volumetric fog tone mapping | ✓ | Yes | Vol fog tone mapping pass |
| 0x59B44963 | DR_VolFog_TppTonemap_MD | VolFog + multi-distance | ✓ | Yes | Multi-distance variant |
| 0x637BB745 | LUT Builder (Pass 1) | Color grading LUT construction | ✓ | Yes | Generic LUT builder |
| 0xCA7F0E3D | LUT Builder (Pass 2) | Color grading LUT construction | ✓ | Yes | Generic LUT builder |
| 0x900968FF | FXAA Pass | Antialiasing | ✓ | Yes | Final screen-space AA |
| 0xE04D1471 | Tonemap (LUT/Grading) | Late LUT & color grading | ✓ | Yes | Primary color grading output |

**Legend:**
- **Hash**: CRC32 of compiled DXBC bytecode
- **Clone?**: Whether clone/hot-swap is activated
- **Track**: Marked as candidate but not activated on this draw
- **RTV0 only**: Only first render target upgraded (others are material/GBuffer targets)

---

## Clone Activation Flow

### Marking Phase (Per-Draw)

When a shader in the `custom_shaders` array is intercepted:

1. **Get RTVs**: Retrieve bound render targets via `GetRenderTargets(cmd_list)`
2. **Mark Target**: For each RTV, call `MarkSceneTonemapCloneTarget()` or `MarkMotionBlurCloneTarget()`
   - Checks if the underlying resource matches upgrade criteria (format, size, usage)
   - Records upgrade info pointer in resource metadata
3. **Activate Clone**: Call `ActivateCloneHotSwapIfTracked(device, rtv)`
   - Queries metadata to see if this resource is a clone candidate
   - Calls `renodx::utils::resource::upgrade::ActivateCloneHotSwap()` 
   - ReShade internally switches render target binding to the HDR clone

### Reset Phase (Per-Present)

`ResetTonemapCopyTracking()` clears frame-local state:
- `tonemap_copy_tracking_armed_this_frame` → `false`
- `tonemap_copy_tracking_allowed` → `false`

This ensures one-shot arming gates (like DOF) only fire once per frame.

---

## Current Limitations

### DOF is SDR-Clamped (Unorm)

**Why**: Removed `CopyRenderBuffer` (0x83272BCB) tracking to fix white texture corruption.

**Effect**:
- DOF renders into SDR (8-bit BGRA) targets
- Depth-of-field effects do not benefit from HDR precision
- Output is tone-mapped (clamped) to SDR range [0, 1]

**Workaround**: None currently; DOF remains SDR for now.

**Future**: Dedicated HDR-aware DOF shader replacement or explicit marking without copy propagation.

---

## Why CopyRenderBuffer Tracking Was Removed

### The Problem

`CopyRenderBuffer` (0x83272BCB) is a **generic copy shader** used by many unrelated rendering operations:
- Rain/fog deferred paths copy materials and intermediate buffers
- Motion blur passes copy scene temps
- Tone mapping operations copy intermediate results

**Issue**: When tracked for one operation (e.g., DOF → motion blur), the global `copy_resource` event hook would indiscriminately copy SDR data back over HDR clone targets for *all* CopyRenderBuffer uses, causing:
- **White texture corruption** (unorm [0,1] range written to float [0,16] range)
- **Flickering/intermittent corruption** (race conditions between copies and draws)
- **Cascading failures** even with one-shot gating

### The Solution

**Accepted Trade-off**: Allow DOF to remain SDR. Instead of tracking generic copies, explicitly mark specific render targets in shader callbacks:
- Deferred tone mapping shaders mark their own outputs
- Motion blur shader marks its own intermediates
- LUT builders mark their own targets

**Result**: No white corruption, but DOF is SDR-clamped until a proper HDR DOF solution is implemented.

---

## Code Structure

### State Variables

```cpp
bool tonemap_copy_tracking_armed_this_frame;  // One-shot flag per frame
bool tonemap_copy_tracking_allowed;            // Gating flag for DOF post-processing
```

### Key Functions

**Marking Functions:**
- `MarkSceneTonemapCloneTarget(rtv, device)` — Mark target for scene tonemap upgrade
- `MarkMotionBlurCloneTarget(rtv, device)` — Mark target for motion blur upgrade
- `MarkCloneTarget(rtv, upgrade_info, device)` — Generic marking function

**Activation Functions:**
- `ActivateCloneHotSwapIfTracked(device, rtv)` — Activate clone if marked
- `PixelShaderResourceMatchesCloneTarget(cmd_list, upgrade_info)` — Check PS slot 0

**Arming/Reset:**
- `ArmTonemapCopyTrackingAfterDofNear(cmd_list)` — Called after DOF_ScatterCompositeNear draw
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

// Track but do not activate (used for material maps that shouldn't clone)
#define TrackDeferredTonemapOutputRTV(value, name) { ... }

// Motion blur variant
#define UpgradeMotionBlurRTV(value) { ... }
```

### Custom Shaders Array

The `custom_shaders` array defines the shader interception pipeline. **Order matters**:
1. DOF & motion blur upgrades (marking + activation)
2. Deferred tone mapping upgrades
3. LUT builder and FXAA upgrades
4. Final LUT/color-grading upgrade
5. `__ALL_CUSTOM_SHADERS` (must be last — includes all embedded shader codes)

---

## Next Steps / Future Work

### Proper DOF HDR Support

Option A: **Dedicated DOF shader replacement**
- Create custom DOF shader that renders directly to RGBA16F
- Replace all three DOF passes (Near, Far, Final)
- Requires algorithm port from existing shader

Option B: **HDR-aware CopyRenderBuffer**
- Create custom copy shader that intelligently routes SDR vs. HDR copies
- Register only when needed, not globally
- Safer than generic event hook

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
