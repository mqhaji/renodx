# Fox Engine Render Pipeline — Reverse Engineering Reference

## Table of Contents

- [Frame Orchestration](#frame-orchestration)
- [Plugin Architecture](#plugin-architecture)
- [Plugin Node Layout](#plugin-node-layout)
- [Plugin Hierarchy](#plugin-hierarchy)
- [Plugin Enable/Disable System](#plugin-enabledisable-system)
- [Key Structures](#key-structures)
- [Core Gr Entities](#core-gr-entities)
- [Terrain System](#terrain-system)
- [Wireframe Rendering](#wireframe-rendering)
- [DX11 Device Access](#dx11-device-access)
- [Shader Constants Pushed Per-Frame](#shader-constants-pushed-per-frame)
- [Key Function Addresses (Retail)](#key-function-addresses-retail)
- [Proven Frida Techniques](#proven-frida-techniques)
- [What Can Be Modified Live](#what-can-be-modified-live)
- [FoxCMD Roadmap](#foxcmd-roadmap)

---

## Frame Orchestration

**Confidence: 95%** — Decompiled from retail executable, cross-referenced with debug symbols. Flow confirmed via Frida instrumentation.

The top-level frame builder is `fox::gr::Render::BuildRenderingALL` (retail: `0x1437b9180`). Each frame executes in this order:

1. `GraphicsSettingManager::Update()` — gates rendering; returns early if no rendering should occur (minimized, lost device, etc.)
2. `Scene::ExecuteDelayedDequeue()` — flushes scene objects queued for removal
3. `Mutex::SpinLock(&g_renderMutex)` — serializes the frame build
4. `RenderContext::GetInstance()` → `vtbl+0x30` — BeginFrame/PrepareFrame
5. `PerformanceMeter::StartTotalGpuTimeStamp()` — brackets GPU workload
6. `dg::Dg::ClearLastBuildPrimitiveOutputJobPacket()` — resets previous frame's primitive output
7. **Scene update loop** — walks linked list of scenes (`first qword = next ptr`), calls `Scene::Update()` per scene
8. **Model setup job dispatch** — selects quality/LOD tier from `g_renderWork+0x20` offset `+0xD8`, dispatches `SetModelSetupLauncherJob` per scene, then `JobStack::Flush`
9. **Per-viewport rendering loop** — walks render work entries linked via `g_renderWork+0x20` offset `+0x8`:
   - `DgDx11::IsEnableRender` checks offset `+0x98` (swap chain/render target handle)
   - Disabled → `UpdateViewportList()` (refresh viewport state only)
   - Enabled → `buildRendering()` (full render pass dispatch)
10. `PerformanceMeter::EndTotalGpuTimeStamp()` + `vtbl+0x38` — EndFrame
11. `Mutex::Unlock(&g_renderMutex)` — conditional on lock success

### g_renderWork Structure

The `g_renderWork + 0x20` structure is a linked list of render work entries:

| Offset | Type | Description |
|--------|------|-------------|
| +0x08 | pointer | Next render work entry |
| +0x98 | uint64 | Render target / swap chain identifier |
| +0xC0 | pointer | JobStack for model setup |
| +0xD8 | int | Quality/LOD tier (2,3,4 → mapped to job tiers 5,6,4) |

---

## Plugin Architecture

**Confidence: 95%** — Dispatch flow decompiled from _Exec and ExecChildPlugin. Three-phase model confirmed via runtime logging.

### Dispatch Flow

Plugin execution is **hierarchical**, not flat. There are three distinct dispatch phases inside `buildRendering()`:

**Phase 1 — Global pre-pass (no viewport):**
Walks plugin list at `g_renderWork+8` index 0. For each plugin where `plugin[0x50] != 0` (enabled), calls `RenderPlugin::_Exec(plugin, renderWork, NULL)`.

**Phase 2 — Per-viewport initialization:**
Walks viewports at `param_1+0xA0`. For each enabled viewport, walks the tier-indexed plugin list calling `RenderPlugin::_InitRender`. Viewport validity requires: offset `+0x6E2` bit 0 set, `+0x5D8` nonzero, `+0x5DC` nonzero.

**Phase 3 — Per-viewport plugin execution:**
For each plugin in `list[tier_index]`:
- Check `plugin[0x50]` — master enable byte
- Check viewport validation via `thunk_FUN_1437ac360`
- Check viewport bitmask: `viewport[0x528 + plugin[0x4C] * 8] >> (plugin[0x48] & 0x3F) & 1`
- If bit set → `RenderPlugin::_Exec(plugin, renderWork, viewport)`
- If bit clear → `plugin->vtbl[0x58]()` (skip/cleanup path)

### _Exec Internals

`RenderPlugin::_Exec` (retail: `0x1bed20`) executes a plugin through its vtable chain:

1. Gets plugin name via `SharedString::CString(this + 0x40)`
2. Calls `vtbl+0x38` — pre-exec setup
3. Calls `vtbl+0x40` — main execution
4. Calls `ExecChildPlugin` — dispatches children
5. Calls `vtbl+0x50` — post-exec
6. Calls `vtbl+0x48` — cleanup

### Child Plugin Dispatch

`ExecChildPlugin` (retail: `0x37ba2d0`) walks children linked at `parent+0x60` via `child+0x30` next pointers. Each child follows the same bitmask check as Phase 3:

- `child[0x50]` — enable byte
- Viewport bitmask check using `child[0x48]` (bit index) and `child[0x4C]` (qword index)
- Enabled → `_Exec(child, render, viewport)`
- Disabled → `child->vtbl[0x58]()` (skip/cleanup — **must always be called, skipping causes crashes**)

---

## Plugin Node Layout

**Confidence: 90%** — Offsets confirmed via Frida memory reads on live plugin instances. Name reading confirmed for all 42 plugins.

| Offset | Type | Description |
|--------|------|-------------|
| +0x00 | pointer | Vtable pointer |
| +0x30 | pointer | Next sibling (linked list) |
| +0x40 | SharedString | Plugin name (double deref: `ptr->ptr->char*`) |
| +0x48 | uint32 | Bit index into viewport bitmask (masked to 0–63) |
| +0x4C | uint32 | Qword index into viewport bitmask array |
| +0x50 | byte | Master enable flag |
| +0x60 | pointer | First child plugin |

### Reading Plugin Names (Frida)

```javascript
const name = plugin.add(0x40).readPointer().readPointer().readUtf8String();
```

---

## Plugin Hierarchy

Complete plugin tree as observed at runtime. **Confidence: 95%+** — every node confirmed via live Frida instrumentation of `_Exec` and `ExecChildPlugin`, cross-referenced against the MGSV Modding Wiki entity list and retail symbol dump.

```
Top-level plugins (dispatched via _Exec):
├── VIEW_CALLBACK                    GrPluginViewCallback
├── MODEL_SETUP                      GrPluginModelSetup
├── OCCLUDER                         GrPluginOccluder
├── PRECOMPUTE_SKY                   GrPluginPrecomputeSky
├── GLOBAL_VOLUMETRIC_FOG            GrPluginGlobalVolumetricFog
├── DEFERRED                         GrPluginDeferredRendering
│   ├── GEOMETRY_PASS                GrPluginDeferredGeometry
│   │   ├── TERRAIN_DRAW_DEPTH       GrPluginTerrainDepth
│   │   ├── OPAQUE_PASS              GrPluginDeferredGeometryOpaque
│   │   ├── MASK_PASS                GrPluginDeferredGeometryMasked
│   │   ├── DEFERRED_CLONE           GrPluginCloneDeferred
│   │   ├── TERRAIN_DRAW             GrPluginTerrain
│   │   ├── DECAL_PASS               GrPluginDeferredGeometryDecal
│   │   ├── DECALS_DEFERRED          GrPluginDecal
│   │   ├── DEFERRED_CLONEDECAL      GrPluginCloneDeferred (decal)
│   │   ├── DEFERRED_RAWDECAL        GrPluginRawDecal
│   │   └── MATERIAL_LAYER           GrPluginMaterialLayer
│   ├── LINEINTEGRAL_SSAO            GrPluginLineIntegralSSAO
│   ├── AMBIENTOBSCURANCE_SSAO       GrPluginAmbientObscuranceSSAO
│   └── SHADING_PASS                 GrPluginDeferredShading
│       ├── PLUGIN_SPHERICALHARMONICS GrPluginSphericalHarmonics
│       ├── LOCAL_LIGHTS              GrPluginLocalLight
│       ├── SUN_SHADOW                GrPluginShadow
│       ├── SUNLIGHT                  GrPluginSunlight
│       └── SUBSURFACE_SCATTER        GrPluginSubSurfaceScatter
├── SKY                              GrPluginSky
├── ALPHA_MODEL                      GrPluginAlphaModel
├── WORMHOLE                         GrPluginWormhole
├── LOCAL_REFLECTION                 GrPluginLocelReflection [sic]
├── PRIMITIVES                       GrPluginPrimitive
├── OPTICAL_CAMOUFLAGE               GrPluginOpticalCamouflage
├── THERMOGRAPHY                     GrPluginThermography
├── POSTFILTER                       GrPluginPostFilter
│   ├── TONEMAP                      GrPluginTonemap
│   ├── DEPTH_OF_FIELD               GrPluginDepthOfField
│   ├── DRAW2D_SHRINK                GrPlugin2DShrink
│   ├── MOTION_BLUR                  GrPluginMotionBlur
│   └── COLOR_CORRECTION             GrPluginColorCorrection
├── FXAA                             GrPluginFxaa
├── OVERLAY_MODEL                    GrPluginOverlayModel
├── PRIMITIVES_UNFILTERED            GrPluginPrimitiveUnfiltered
├── DRAW2D                           GrPlugin2D
├── PRIMITIVE_DEBUG                  GrPluginPrimitiveDebug
├── DRAW2D_FRONTMOST                 GrPlugin2DFrontmost
└── SCREEN_CAPTURE                   GrPluginScreenCapture
```

### Render Pipeline Execution Order

The tree above also represents execution order. The deferred rendering pipeline flows:

1. **Geometry** — terrain depth, opaque meshes, masked meshes, clones, terrain, decals, material layers
2. **Ambient Occlusion** — two SSAO passes (line integral + ambient obscurance)
3. **Shading** — spherical harmonics (ambient), local lights, sun shadow, sunlight, subsurface scatter
4. **Sky** — atmospheric sky layer
5. **Transparency** — alpha models, wormhole effect
6. **Post-processing** — reflections, optical camo, thermography, then tonemap → DOF → motion blur → color correction → FXAA
7. **Overlays** — overlay models, 2D elements, debug primitives, screen capture

### Wiki Entity ↔ Runtime Name Mapping

| Wiki Entity | Runtime Plugin Name | Confirmed |
|------------|-------------------|-----------|
| GrPluginViewCallback | VIEW_CALLBACK | ✓ Live |
| GrPluginModelSetup | MODEL_SETUP | ✓ Live |
| GrPluginOccluder | OCCLUDER | ✓ Live |
| GrPluginPrecomputeSky | PRECOMPUTE_SKY | ✓ Live |
| GrPluginGlobalVolumetricFog | GLOBAL_VOLUMETRIC_FOG | ✓ Live |
| GrPluginDeferredRendering | DEFERRED | ✓ Live |
| GrPluginDeferredGeometry | GEOMETRY_PASS | ✓ Live |
| GrPluginTerrainDepth | TERRAIN_DRAW_DEPTH | ✓ Live |
| GrPluginDeferredGeometryOpaque | OPAQUE_PASS | ✓ Live |
| GrPluginDeferredGeometryMasked | MASK_PASS | ✓ Live |
| GrPluginCloneDeferred | DEFERRED_CLONE | ✓ Live |
| GrPluginTerrain | TERRAIN_DRAW | ✓ Live |
| GrPluginDeferredGeometryDecal | DECAL_PASS | ✓ Live |
| GrPluginDecal | DECALS_DEFERRED | ✓ Live |
| GrPluginRawDecal | DEFERRED_RAWDECAL | ✓ Live |
| GrPluginMaterialLayer | MATERIAL_LAYER | ✓ Live |
| GrPluginLineIntegralSSAO | LINEINTEGRAL_SSAO | ✓ Live |
| GrPluginAmbientObscuranceSSAO | AMBIENTOBSCURANCE_SSAO | ✓ Live |
| GrPluginDeferredShading | SHADING_PASS | ✓ Live |
| GrPluginSphericalHarmonics | PLUGIN_SPHERICALHARMONICS | ✓ Live |
| GrPluginLocalLight | LOCAL_LIGHTS | ✓ Live |
| GrPluginShadow | SUN_SHADOW | ✓ Live |
| GrPluginSunlight | SUNLIGHT | ✓ Live |
| GrPluginSubSurfaceScatter | SUBSURFACE_SCATTER | ✓ Live |
| GrPluginSky | SKY | ✓ Live |
| GrPluginAlphaModel | ALPHA_MODEL | ✓ Live |
| GrPluginWormhole | WORMHOLE | ✓ Live |
| GrPluginLocelReflection | LOCAL_REFLECTION | ✓ Live |
| GrPluginPrimitive | PRIMITIVES | ✓ Live |
| GrPluginOpticalCamouflage | OPTICAL_CAMOUFLAGE | ✓ Live |
| GrPluginThermography | THERMOGRAPHY | ✓ Live |
| GrPluginPostFilter | POSTFILTER | ✓ Live |
| GrPluginTonemap | TONEMAP | ✓ Live |
| GrPluginDepthOfField | DEPTH_OF_FIELD | ✓ Live |
| GrPlugin2DShrink | DRAW2D_SHRINK | ✓ Live |
| GrPluginMotionBlur | MOTION_BLUR | ✓ Live |
| GrPluginColorCorrection | COLOR_CORRECTION | ✓ Live |
| GrPluginFxaa | FXAA | ✓ Live |
| GrPluginOverlayModel | OVERLAY_MODEL | ✓ Live |
| GrPluginPrimitiveUnfiltered | PRIMITIVES_UNFILTERED | ✓ Live |
| GrPlugin2D | DRAW2D | ✓ Live |
| GrPluginPrimitiveDebug | PRIMITIVE_DEBUG | ✓ Live |
| GrPlugin2DFrontmost | DRAW2D_FRONTMOST | ✓ Live |
| GrPluginScreenCapture | SCREEN_CAPTURE | ✓ Live |

### Wiki Entities NOT Observed at Runtime

These plugins exist in the entity system but were not seen in the active plugin tree. They may be conditionally loaded, scene-specific, or unused in TPP:

| Wiki Entity | Notes |
|------------|-------|
| GrPluginScreenSpaceAmbientOcclusion | Third SSAO variant — possibly debug/legacy, replaced by LineIntegral + AmbientObscurance |
| GrPluginForwardRendering | Forward pass exists but not active in normal deferred pipeline |
| GrPluginLightAccumulateLayer | May be folded into SHADING_PASS or conditionally loaded |
| GrPluginClone | Base clone — DEFERRED_CLONE (CloneDeferred) is what runs |
| GrPluginCloneWireframe | Debug visualization — likely dev-only |
| GrPluginExtendFxModel | VFX model extension — may activate with specific effects |
| GrPluginModel | Base model plugin — likely parent class, not directly instantiated |
| GrPluginSeflShadowOfTerrain | Terrain self-shadowing [sic] — may be folded into SUN_SHADOW |

---

## Plugin Enable/Disable System

**Confidence: 90%** — Bitmask system decompiled from ExecChildPlugin and buildRendering. Skip path crash behavior confirmed experimentally (skipping vtbl+0x58 = crash).

### Native Bitmask System (Cleanest)

Each viewport has a bitmask array at `viewport+0x528`. Each plugin checks its specific bit to determine execution. Clearing a bit causes the engine to call the skip/cleanup path (`vtbl+0x58`) instead of `_Exec`.

To disable a plugin for a specific viewport:
```
bit_index = plugin[0x48] & 0x3F
qword_index = plugin[0x4C]
viewport[0x528 + qword_index * 8] &= ~(1 << bit_index)
```

### Master Enable Byte

Setting `plugin[0x50] = 0` skips the plugin entirely with no cleanup call. Simpler but may cause issues if the plugin needs its cleanup path called.

### Hook-Based Filtering

For top-level plugins, hook `_Exec` and match by name. For child plugins, hook `ExecChildPlugin` and filter. **Critical: always call `vtbl+0x58` for skipped children — dropping them entirely causes crashes.**

### SkyDrawContext Flag System

The sky plugin has its own enable system separate from bitmasks:

- `SkyDrawContext` is a scene parameter fetched via `Viewport::GetScene()` → `Scene::GetParameter<>(hash)`
- Enable flag: bit 2 of `uint32` at `SkyDrawContext+0x1E0`
- `SetEnableSkyFlag(bool)` at retail `0x39e2150` — sets/clears bit 2
- `IsEnableSky()` at retail `0x39e17b0` — returns `(*(uint*)(this + 0x1E0) >> 2) & 1`
- Hooking `IsEnableSky` to return 0 disables the atmospheric sky layer (not the skybox geometry)

---

## Key Structures

**Confidence: 85%** — Offsets derived from decompiled code. Viewport offsets partially confirmed via AnimateExposure hook work. Some field names are inferred from usage context.

### Viewport (GrViewport)

| Offset | Description |
|--------|-------------|
| +0x30 | Next viewport pointer (linked list) |
| +0x528 | Plugin bitmask array (per-plugin enable bits) |
| +0x5D8 | Validity check int (must be nonzero) |
| +0x5DC | Validity check int (must be nonzero) |
| +0x6E2 | Flags byte (bit 0 = enabled) |
| +FlagsMaybe | Viewport flags (bit 16 = 0x10000 checked in buildRendering) |
| +WidthCpy / HeightCpy | Viewport dimensions (must be nonzero) |
| +ProjMatrix | 4x4 projection matrix (copied to ShaderManager per frame) |
| +ViewMatrix | 4x4 view matrix |
| +Camera | Pointer to GrCamera |
| +FogColor | Vector4 (x, y, z, w) |

### GrCamera

| Offset | Description |
|--------|-------------|
| +InvViewMatrix.m30–m33 | Camera world position |

### TerrainMap

| Offset | Type | Description |
|--------|------|-------------|
| +0x80 | float | 65535.0 / heightRange (normalization scale) |
| +0x84 | float | -(65535.0 / heightRange) * minHeight (normalization offset) |
| +0x88 | float | heightRange * 1.5259022e-05 (inverse normalization) |
| +0x8C | float | minHeight |
| +0x90 | float | heightRange (maxHeight - minHeight) |
| +0x94 | float | minHeight (duplicate) |

### TerrainDraw2Parameters Defaults

| Offset | Value | Likely Meaning |
|--------|-------|----------------|
| +0x00 | 128 | Tile size X |
| +0x04 | 128 | Tile size Y |
| +0x08 | 2.0 | Height scale |
| +0x0C | 1.0 | Unknown scale |
| +0x10 | 16 | Subdivision level |
| +0x30 | -32768.0 | Height minimum |
| +0x34 | 32767.0 | Height maximum |
| +0x38 | 25 | Unknown |

---

## Terrain System

**Confidence: 95%** — ChangeHeightMapRange decompiled and confirmed via live memory writes. Real-time terrain deformation verified visually in-game.

### ChangeHeightMapRange

`fox::gr::TerrainMap::ChangeHeightMapRange(float minHeight, float maxHeight)` at retail `0x3c61ff0`.

Called once at startup before the main menu spawns. Sets the 16-bit heightmap texel (0–65535) to world-height mapping. All derived values at offsets `+0x80` through `+0x94` are computed from `minHeight` and `maxHeight`.

**Live modification is possible** by writing directly to the TerrainMap instance. Changes take effect immediately — terrain mesh updates in real-time. The instance address changes each launch due to ASLR; capture it by hooking `ChangeHeightMapRange` and reading `args[0]` (the `this` pointer).

### Terrain Height Manipulation

**Scaling both min and max** shifts the entire terrain (entities spawn underground at high multipliers).

**Compressing the range** (multiply range by < 1.0) exaggerates peaks while keeping the base level stable:

```javascript
const t = ptr("<TerrainMap address>");
const minH = t.add(0x8c).readFloat();
const range = t.add(0x90).readFloat();
const newRange = range * 0.5; // Compress = exaggerate peaks

t.add(0x90).writeFloat(newRange);
t.add(0x80).writeFloat(65535.0 / newRange);
t.add(0x84).writeFloat(-(65535.0 / newRange) * minH);
t.add(0x88).writeFloat(newRange * 1.5259022e-05);
```

## Core Gr Entities

### GrDaemon

**Confidence: 95%** — Confirmed via constructor decompilation and init function analysis.

The graphics system singleton. `s_Instance` at `142b77ac0`. Despite its central role, GrDaemon is a thin wrapper — it inherits from `Entity`, holds two jobs, and registers itself as a global singleton. It does NOT own plugin lists or render state directly.

**Layout:**
| Offset | Type | Description |
|--------|------|-------------|
| +0x00 | Entity | Base entity |
| +0x30 | SharedPtr | Main render job (wraps BuildRenderingALL) |
| +0x38 | SharedPtr | KickerJob (signals render completion) |

The init function (`143a79020`) is pure job system plumbing — creates two jobs, wires sync packets for frame ordering, and adds both to the default JobPool. No plugin creation, no viewport management, no render state. `BuildRenderingALL` is where all rendering logic lives.

**Verdict: Dead end for modding. All useful render control is in BuildRenderingALL and the plugin dispatch system.**

### GrCamera

**Confidence: 80%** — Layout partially mapped from buildRendering viewport access.

The camera entity attached to each viewport at `viewport->Camera`. Contains view and inverse view matrices. The camera's world position is extracted from `InvViewMatrix.m30–m33` and pushed as shader constant `0x142` every frame.

| Symbol | Address (Retail) |
|--------|-----------------|
| vtable | 0x1420f0858 |
| kDefaultFocalLength | 0x1429e5050 |
| kDefaultFoV | 0x142b73144 |
| SetFlag_4 | 0x14377c350 |
| g_UiGrCamera | 0x142c8fab0 |

### GrTools

**Confidence: 85%** — Lua bindings confirmed via mangled symbol names.

A utility class exposing graphics functions to the Lua scripting system. Functions take a `Lua*` parameter — they're designed to be called from Lua scripts but can be called directly via Frida with the right Lua state pointer.

| Address (Retail) | Function | Notes |
|-----------------|----------|-------|
| 0x143a7da50 | GetDeviceName | Returns DX11 device name |
| 0x143a80be0 | SetEnableNoRejectOnShadow | Shadow quality toggle |
---

## Wireframe Rendering

**Confidence: 95%** — StartWireframeCloneObject decompiled and called successfully in-game. Produces correct wireframe overlay on any plugin pass.

### Engine's Wireframe System

The Fox Engine has a built-in wireframe mode via `GrPluginCloneWireframe`, but it's not registered in the active plugin tree at runtime (dev/debug feature). The underlying DX11 functions are still present and callable:

| Address (Retail) | Function |
|-----------------|----------|
| 0x3dccc00 | `CloneRenderingDx11::StartWireframeCloneObject` |
| 0x3dcb970 | `CloneRenderingDx11::EndWireframeCloneObject` |

Both are void functions with no parameters. `StartWireframeCloneObject` manipulates the engine's abstracted rasterizer state through `RenderContext`, not raw DX11 calls:

1. Gets `RenderContext::GetInstance()`
2. Reads render state struct at `RenderContext+0x138`
3. Flips fill mode bits in the packed 64-bit rasterizer state at `state+0x10`
4. Writes identity selection vectors to `ShaderManager+0x740–0x770` (wireframe color/channel setup)
5. Commits state via `vtbl+0x138`, sets shader via `vtbl+0x1F0`, calls `UpdateMatrix`

### Usage

Wrap any `_Exec` call between `StartWireframeCloneObject` and `EndWireframeCloneObject` to render that plugin in wireframe mode. Can be applied selectively per-plugin or globally.

### Selective Wireframe (Terrain Only)

```javascript
const base = Process.enumerateModules()[0].base;
const _Exec = base.add(0x1bed20);
const startWF = new NativeFunction(base.add(0x3dccc00), 'void', []);
const endWF = new NativeFunction(base.add(0x3dcb970), 'void', []);

Interceptor.revert(_Exec);
const orig = new NativeFunction(_Exec, 'void', ['pointer', 'pointer', 'pointer']);

Interceptor.replace(_Exec, new NativeCallback(function(self, render, viewport) {
    try {
        const name = self.add(0x40).readPointer().readPointer().readUtf8String();
        if (name === "DEFERRED") {
            startWF();
            orig(self, render, viewport);
            endWF();
            return;
        }
    } catch(e) {}
    orig(self, render, viewport);
}, 'void', ['pointer', 'pointer', 'pointer']));
```

### Global Wireframe

```javascript
Interceptor.replace(_Exec, new NativeCallback(function(self, render, viewport) {
    startWF();
    orig(self, render, viewport);
    endWF();
}, 'void', ['pointer', 'pointer', 'pointer']));
```

---

## DX11 Device Access

**Confidence: 85%** — Device and context pointers confirmed. Render target access mid-frame requires further work with engine's internal RT management.

### Global Pointers

| Address (Retail) | Type | Description |
|-----------------|------|-------------|
| 0x142c6b860 | `ID3D11Device*` | `fox::gr::dg::s_D3D11Device` — raw DX11 device |
| 0x142b77f58 | `RenderContext*` | `fox::gr::RenderContext::s_Instance` — engine render context singleton |

### Getting the Immediate Context

```javascript
const device = base.add(0x2c6b860).readPointer();
const deviceVtbl = device.readPointer();
// ID3D11Device::GetImmediateContext = vtable index 40
const getImmCtx = new NativeFunction(deviceVtbl.add(40 * 8).readPointer(), 'void', ['pointer', 'pointer']);
const ctxOut = Memory.alloc(8);
getImmCtx(device, ctxOut);
const ctx = ctxOut.readPointer();
```

### Engine's Context Wrapper

`GetCurrentGnDeviceContext` at `0x3d02430` returns the engine's wrapped DX11 context from `RenderContext`. Returns null outside the active render loop — must be called during plugin execution (inside `_Exec` hook). The returned pointer is the engine's `GnDeviceContext` abstraction, not a raw `ID3D11DeviceContext` — vtable offsets differ.

### Render Target Access — Current State

Attempting to call `OMGetRenderTargets` (vtable index 73 on `ID3D11DeviceContext`) mid-frame returns null — the engine manages render targets through its own abstraction layer, not standard DX11 OM bindings. Accessing render targets for custom drawing will require:

1. Reverse engineering how `RenderContext` tracks bound render targets internally, OR
2. Hooking `OMSetRenderTargets` to capture targets as they're bound, OR
3. Using the engine's own RT management functions (needs further RE)

This is a prerequisite for custom plugin injection (e.g. SMAA replacement for FXAA, custom post-processing).

### Key DX11 Functions

| Address (Retail) | Function |
|-----------------|----------|
| 0x142c6b860 | `s_D3D11Device` (global pointer) |
| 0x1419f4df0 | `GetD3D11Device` (debug) / `0x14c160e20` (retail) |
| 0x3d02430 | `DgDx11::GetCurrentGnDeviceContext` |

---

## Shader Constants Pushed Per-Frame

Inside `buildRendering`, these constants are pushed to the GPU via `param_2->vtbl+0x128`:

| Constant ID | Data | Description |
|-------------|------|-------------|
| 0x142 | Camera world position | From `GrCamera->InvViewMatrix.m30–m33` |
| 0x172 | Exposure vector | From `GrViewport::GetExposureVector()` (or (1,1,1,1) for quality tiers 5–6) |
| 0x182 | Fog factor | From `GrViewport::GetFogFactor()` |
| 0x1B2 | Fog color | From `GrViewport->FogColor` (Vector4 XYZW) |

The projection matrix is also copied to `ShaderManager+0x680` (4x4 float matrix) every frame.

---

## Key Function Addresses (Retail)

### Frame / Render Pipeline

| Address | Function |
|---------|----------|
| 0x1437b9180 | `BuildRenderingALL` — top-level frame orchestrator |
| 0x1bed20 | `RenderPlugin::_Exec` — plugin execution dispatch |
| 0x37ba2d0 | `RenderPlugin::ExecChildPlugin` — child plugin dispatch loop |
| 0x37c0da0 | `RenderPlugin::_InitRender` — plugin initialization |

### Sky System

| Address | Function |
|---------|----------|
| 0x39e17b0 | `SkyDrawContext::IsEnableSky` — reads bit 2 of flags at +0x1E0 |
| 0x39e2150 | `SkyDrawContext::SetEnableSkyFlag` — sets/clears bit 2 at +0x1E0 |
| 0x39e1db0 | `GrPluginPrecomputeSky::RegisterEntity` |
| 0x3a05cf0 | `GrPluginSky::Render` |
| 0x40735400 | `GrPluginSky::RenderInner` |

### Terrain System

| Address | Function |
|---------|----------|
| 0x3c61ff0 | `TerrainMap::ChangeHeightMapRange` — sets height normalization values |
| 0x3a0afb0 | `TerrainDraw2Parameters::TerrainDraw2Parameters` — default terrain config |
| 0x3a11860 | `GrPluginTerrain::RegisterEntity` |
| 0x3a14820 | `TerrainDraw::UpdateHeightMapTexture` |

### Plugin String Labels

| Address | String |
|---------|--------|
| 0x1420f98d0 | "GrPluginPrecomputeSky" |
| 0x1420fb5f8 | "GrPluginSky" |
| 0x1420fbec8 | "GrPluginTerrain" |
| 0x1420fc9b0 | "GrPluginTerrainDepth" |

---

## Proven Frida Techniques

### Live REPL Plugin Toggle (Recommended Workflow)

```javascript
const base = Process.enumerateModules()[0].base;
const _Exec = base.add(0x1bed20);
const ExecChildPlugin = base.add(0x37ba2d0);

global.disabled = new Set();
global.disable = function(name) { global.disabled.add(name); console.log("[+] Disabled: " + name); };
global.enable = function(name) { global.disabled.delete(name); console.log("[-] Enabled: " + name); };
global.list = function() { console.log("Disabled: " + [...global.disabled].join(", ")); };

Interceptor.revert(_Exec);
Interceptor.revert(ExecChildPlugin);

const origExec = new NativeFunction(_Exec, 'void', ['pointer', 'pointer', 'pointer']);

function getName(plugin) {
    try { return plugin.add(0x40).readPointer().readPointer().readUtf8String(); }
    catch(e) { return null; }
}

Interceptor.replace(_Exec, new NativeCallback(function(self, render, viewport) {
    const name = getName(self);
    if (name && global.disabled.has(name)) return;
    origExec(self, render, viewport);
}, 'void', ['pointer', 'pointer', 'pointer']));

Interceptor.replace(ExecChildPlugin, new NativeCallback(function(parent, render, viewport) {
    let child = parent.add(0x60).readPointer();
    while (!child.isNull()) {
        const name = getName(child);
        if (name && global.disabled.has(name)) {
            try {
                const vtbl = child.readPointer();
                const skipFn = new NativeFunction(vtbl.add(0x58).readPointer(), 'void', ['pointer', 'pointer', 'pointer']);
                skipFn(child, render, viewport);
            } catch(e) {}
        } else {
            origExec(child, render, viewport);
        }
        child = child.add(0x30).readPointer();
    }
}, 'void', ['pointer', 'pointer', 'pointer']));
```

Usage from REPL:
```
disable('SKY')
disable('TONEMAP')
disable('SUNLIGHT')
enable('SKY')
list()
```

### Wireframe Mode

```javascript
const base = Process.enumerateModules()[0].base;
const _Exec = base.add(0x1bed20);
const startWF = new NativeFunction(base.add(0x3dccc00), 'void', []);
const endWF = new NativeFunction(base.add(0x3dcb970), 'void', []);

Interceptor.revert(_Exec);
const orig = new NativeFunction(_Exec, 'void', ['pointer', 'pointer', 'pointer']);

Interceptor.replace(_Exec, new NativeCallback(function(self, render, viewport) {
    startWF();
    orig(self, render, viewport);
    endWF();
}, 'void', ['pointer', 'pointer', 'pointer']));
```

### Disable Atmospheric Sky

```javascript
const base = Process.enumerateModules()[0].base;
const IsEnableSky = base.add(0x39e17b0);

Interceptor.attach(IsEnableSky, {
    onLeave(retval) { retval.replace(0); }
});
```

Disables the atmospheric sky layer. Skybox geometry still renders. Can be injected at any time — takes effect next frame.

### Dump All Plugin Names

```javascript
const base = Process.enumerateModules()[0].base;
const _Exec = base.add(0x1bed20);
let count = 0;

Interceptor.attach(_Exec, {
    onEnter(args) {
        if (count++ > 30) return;
        try {
            const name = args[0].add(0x40).readPointer().readPointer().readUtf8String();
            console.log(name);
        } catch(e) {}
    }
});
```

### Dump Child Plugins of a Parent

```javascript
const base = Process.enumerateModules()[0].base;
const _Exec = base.add(0x1bed20);
let count = 0;

Interceptor.attach(_Exec, {
    onEnter(args) {
        if (count++ > 60) return;
        try {
            const plugin = args[0];
            const name = plugin.add(0x40).readPointer().readPointer().readUtf8String();
            if (name !== "TARGET_PARENT_NAME") return;
            let child = plugin.add(0x60).readPointer();
            let i = 0;
            while (!child.isNull() && i < 15) {
                try {
                    const cn = child.add(0x40).readPointer().readPointer().readUtf8String();
                    console.log("child[" + i + "]: " + cn);
                } catch(e) {}
                child = child.add(0x30).readPointer();
                i++;
            }
        } catch(e) {}
    }
});
```

### Live Terrain Manipulation

```javascript
// Step 1: Capture TerrainMap address (hook before map load)
const base = Process.enumerateModules()[0].base;
Interceptor.attach(base.add(0x3c61ff0), {
    onEnter(args) { console.log("TerrainMap @ " + args[0]); }
});

// Step 2: After loading, poke directly (replace address each session)
const t = ptr("<address from step 1>");
const minH = t.add(0x8c).readFloat();
const range = t.add(0x90).readFloat();
const newRange = range * 0.5; // < 1.0 = exaggerate peaks, > 1.0 = flatten

t.add(0x90).writeFloat(newRange);
t.add(0x80).writeFloat(65535.0 / newRange);
t.add(0x84).writeFloat(-(65535.0 / newRange) * minH);
t.add(0x88).writeFloat(newRange * 1.5259022e-05);
```

---

## What Can Be Modified Live

### Via Plugin Disable (hook _Exec or ExecChildPlugin)

**Top-level plugins (hook _Exec):**

| Plugin | Effect |
|--------|--------|
| PRECOMPUTE_SKY | Disables atmospheric scattering precomputation |
| GLOBAL_VOLUMETRIC_FOG | Removes volumetric fog |
| SKY | Removes atmospheric sky (blue background remains) |
| LOCAL_REFLECTION | Disables screen-space reflections |
| OPTICAL_CAMOUFLAGE | Disables camo shimmer effect |
| THERMOGRAPHY | Disables thermal vision rendering |
| FXAA | Removes anti-aliasing post-process |
| POSTFILTER | Removes ALL post-processing (tonemap, DOF, motion blur, color correction) |
| ALPHA_MODEL | Removes transparent/alpha geometry |
| WORMHOLE | Removes wormhole effect |
| OVERLAY_MODEL | Removes overlay geometry |

**DEFERRED children (hook ExecChildPlugin):**

| Plugin | Parent | Effect |
|--------|--------|--------|
| TERRAIN_DRAW | GEOMETRY_PASS | Removes terrain geometry |
| TERRAIN_DRAW_DEPTH | GEOMETRY_PASS | Removes terrain depth pass |
| OPAQUE_PASS | GEOMETRY_PASS | Removes opaque geometry |
| MASK_PASS | GEOMETRY_PASS | Removes masked geometry |
| DECAL_PASS | GEOMETRY_PASS | Removes deferred decals |
| DECALS_DEFERRED | GEOMETRY_PASS | Removes decal accumulation |
| DEFERRED_RAWDECAL | GEOMETRY_PASS | Removes raw decals |
| MATERIAL_LAYER | GEOMETRY_PASS | Removes material layers |
| DEFERRED_CLONE | GEOMETRY_PASS | Removes cloned deferred geometry |
| LINEINTEGRAL_SSAO | DEFERRED | Disables line integral ambient occlusion |
| AMBIENTOBSCURANCE_SSAO | DEFERRED | Disables ambient obscurance AO |
| PLUGIN_SPHERICALHARMONICS | SHADING_PASS | Removes ambient lighting (spherical harmonics) |
| LOCAL_LIGHTS | SHADING_PASS | Disables all local/point/spot lights |
| SUN_SHADOW | SHADING_PASS | Removes sun shadows |
| SUNLIGHT | SHADING_PASS | Removes sunlight/directional light |
| SUBSURFACE_SCATTER | SHADING_PASS | Disables skin SSS |

**POSTFILTER children (hook ExecChildPlugin):**

| Plugin | Effect |
|--------|--------|
| TONEMAP | Removes tonemapping (raw HDR output) |
| DEPTH_OF_FIELD | Removes depth of field blur |
| MOTION_BLUR | Removes motion blur |
| COLOR_CORRECTION | Removes color grading/LUT |
| DRAW2D_SHRINK | Removes 2D shrink pass |

### Via Viewport Parameters

Exposure, bloom intensity, ambient occlusion strength, fog color/density — all accessible through the GrViewport structure offsets already mapped from AnimateExposure hook work.

### Via Shader Constants

Camera position (0x142), exposure (0x172), fog factor (0x182), fog color (0x1B2) — intercept the `vtbl+0x128` push calls in `buildRendering` to feed arbitrary values to the GPU.

### Via Projection Matrix

The projection matrix is copied from `GrViewport->ProjMatrix` to `ShaderManager+0x680` every frame. Modifying it enables FOV changes, fisheye distortion, or other projection warping.

---

## Notes

- All addresses are for the **retail** MGSV:TPP executable unless otherwise noted
- ASLR is active — all addresses are relative to module base; runtime instances change each launch
- The scene graph and plugin lists are singly-linked lists (next pointer at +0x30 for plugins, first qword for scenes)
- `g_renderWork` is the central render state structure; `+0x8` holds the plugin list array, `+0x20` holds the render work entry list
- Terrain height modification updates in real-time — no reload required
- Plugin filtering via `ExecChildPlugin` hook **must** call the skip path (`vtbl+0x58`) for filtered children to avoid crashes

---

## FoxCMD Roadmap

### Vision

A standalone program (DLL + optional external GUI) that provides a programming interface to the Fox Engine's rendering pipeline. Replaces Frida as the runtime tool with a proper native mod.

### Phase 1 — Native DLL (Port from Frida)

Port all proven Frida hooks to a C++ DLL using MinHook:
- `_Exec` hook → per-plugin enable/disable by name
- `ExecChildPlugin` hook → child plugin filtering with cleanup path
- `ChangeHeightMapRange` hook → terrain height manipulation
- `StartWireframeCloneObject`/`EndWireframeCloneObject` → wireframe toggle
- `IsEnableSky` hook → atmospheric sky toggle

Load via IHHook or custom injector. All hooks translate ~1:1 from Frida `Interceptor.replace` to MinHook detours.

### Phase 2 — Console Interface

ImGui overlay rendered through a DX11 Present hook (device pointer at `s_D3D11Device` `0x142c6b860`):
- Checkboxes for all 42 plugins (hierarchical tree view matching the plugin tree)
- Sliders for terrain height range
- Wireframe mode toggle (global / per-plugin)
- Viewport parameter sliders (exposure, bloom, AO, fog)
- Real-time readout of frame timing, plugin execution counts

Alternative: external GUI communicating with the injected DLL via named pipes or shared memory.

### Phase 3 — Custom Plugin Injection

Allocate plugin nodes matching the engine's layout:
- `+0x00` vtable (custom, with Setup/Execute/cleanup functions)
- `+0x40` SharedString name
- `+0x48`/`+0x4C` bitmask indices
- `+0x50` enable byte
- Link into the plugin list at the desired position

Custom plugin's Execute function receives `RenderContext` and viewport — can issue DX11 draw calls. First target: SMAA post-process replacing FXAA's slot.

**Prerequisite:** Reverse engineer how render targets are managed internally (current blocker — raw DX11 `OMGetRenderTargets` returns null during plugin execution).

### Phase 4 — Advanced Features

- Shader hot-reload (intercept shader compilation, inject modified HLSL)
- Custom render pass insertion (shadow map visualization, G-buffer debug views)
- Per-viewport plugin configuration (different settings per viewport)
- Save/load configuration profiles
- Plugin execution profiling (per-plugin GPU timestamps)

### Architecture Notes

```
┌─────────────┐     ┌──────────────┐     ┌─────────────────┐
│   FoxCMD    │────▶│  Injected    │────▶│  Fox Engine      │
│   GUI       │     │  DLL         │     │  Render Pipeline  │
│  (ImGui or  │     │  (MinHook    │     │                   │
│   External) │     │   detours)   │     │  _Exec            │
└─────────────┘     └──────────────┘     │  ExecChildPlugin  │
                                         │  RenderContext     │
                                         │  ShaderManager     │
                                         │  s_D3D11Device     │
                                         └─────────────────┘
```

### Known Blockers

1. **Render target access** — Engine uses internal RT abstraction. Raw DX11 OMGetRenderTargets returns null during plugin execution. Need to hook RT binding or reverse the engine's RT manager.
2. **Plugin registration** — How the plugin linked list is built at startup. Need to understand node allocation and list insertion to safely add custom plugins.
3. **Shader binding** — How plugins bind shaders for draw calls. StartWireframeCloneObject uses `vtbl+0x1F0` to set a shader — need to map the full shader management interface.

---

## Notes

- All addresses are for the **retail** MGSV:TPP executable unless otherwise noted
- ASLR is active — all addresses are relative to module base; runtime instances change each launch
- The scene graph and plugin lists are singly-linked lists (next pointer at +0x30 for plugins, first qword for scenes)
- `g_renderWork` is the central render state structure; `+0x8` holds the plugin list array, `+0x20` holds the render work entry list
- Terrain height modification updates in real-time — no reload required
- Plugin filtering via `ExecChildPlugin` hook **must** call the skip path (`vtbl+0x58`) for filtered children to avoid crashes
- `Interceptor.revert()` must be called before `Interceptor.replace()` on already-hooked functions
- `StartWireframeCloneObject`/`EndWireframeCloneObject` can wrap any plugin execution — no parameters needed
- Weather is NOT a render plugin — it's a gameplay system (`TppWeatherManager`) that feeds parameters into rendering plugins
- GrDaemon is a thin entity wrapper around two jobs — all rendering logic is in `BuildRenderingALL`
- The "Large alloc" messages from Frida are harmless — suppress with `2>/dev/null` or `--runtime=v8`
