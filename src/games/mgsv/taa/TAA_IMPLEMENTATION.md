# MGSV TAA Implementation

Design notes for adding temporal anti-aliasing to METAL GEAR SOLID V: THE PHANTOM PAIN
under RenoDX. The architecture is adapted from the Alien Isolation port at
[src/games/alienisolation/aliasisolation/TAA_IMPLEMENTATION.md](../../alienisolation/aliasisolation/TAA_IMPLEMENTATION.md);
this document captures only what differs because of MGSV's render pipeline.

This is a **live design doc** — updated as the implementation grows. Entries marked
**TBD** are decisions still to validate.

## Why a custom TAA at all

MGSV ships a TAA-style shader (`TemporalAA_0x8C1B990B`) and a velocity-copy helper
(`TemporalAA_CopyVelocity_0x2E75B173`), but neither is dispatched in observed
captures. The only anti-aliasing actually running on the post-tonemap path is FXAA
(`FXAA_0x900968FF` at draw index ~1302). FXAA runs after tonemap and is mediocre.

There is no projection jitter anywhere in MGSV — the camera cbuffer matrices are
plain, non-offset values. So adding TAA requires us to also inject jitter ourselves.

The plan:

- Disable FXAA at runtime (or visually accept it; FXAA-on-top-of-TAA is
  fine short-term).
- Inject sub-pixel projection jitter on every frame.
- Dispatch a compute resolve at the earliest point where the full-res HDR
  scene is complete and about to be downsampled for DoF / MB — see
  Insertion-Point Strategy below.
- Copy the resolved color back into the HDR scene resource so subsequent
  passes read it naturally.

## Frame Map (observed)

Captured with motion blur and DoF both active. Resolutions are expressed
as fractions of the user's chosen backbuffer resolution: "full" is the
native backbuffer size, "half" is the backbuffer downsampled to ½ width
and ½ height, "quarter" is ¼ each axis, etc. Specific pixel counts in any
example trace will scale with the player's settings.

| Draw idx | Pass | Hash | Scale | Notes |
|---|---|---|---|---|
| 0–1 | Frame setup | | | |
| 2 | `MotionBlurCameraVelocity` | `0xA13321B6` | full | Writes camera-aware velocity to RTV0 `0xB9BB9E60`. Runs every frame regardless of motion-blur setting. |
| 9–21 | `GBufferVelocity` / `GBufferMaskedVelocity` | `0x9815404F`, `0x58C10658` | full | Per-object velocity (44 + 4 draws of VS `0x1D2531B7`). |
| 310  | `DR_VolFog_TppTonemap` | `0x2EA8F13F` | full target | Finalizes deferred lighting + volumetric fog into the main scene clone. Image is "largely formed" after this. Documented fallback candidate. |
| 311–727 | Particles / decals / materials | various | full | Composite onto the main scene. |
| 728  | `DOF_ScatterBakeFirst` | `0xFE1DC3F8` | half MRT | **Primary TAA insertion point.** Reads full-res HDR scene at SRV t0; writes to 3 half-res RTVs (DoF pyramid base, DoF intermediate, MB scene input). One hook covers both DoF and MB. |
| …    | Geometry, lighting, bloom | `DeferredRenderingFilmic_0x410AE8C5` + family | varies | Produces the full-res HDR scene at `0xB9BBB9E0`. **When custom tonemap is on, the mod replaces the filmic tonemap here so the scene stays linear HDR.** |
| 1131 | `DOF_ScatterCompositeNear` | `0xE2D609B1` | half target | Composites near-DoF into half-res DoF accumulator `0xBD553860`. Reads half-res scene downsample. |
| 1283 | `DOF_ScatterCompositeFar` | `0x7C017264` | half target | Composites far-DoF into the same accumulator (blend src=`one_minus_dest_alpha`). |
| 1285 | `DOF_ScatterCompositeFinal` | `0xFC5542BB` | full target | Composites the half-res DoF accumulator into full-res output `0xB9BBA3E0`. |
| 1292 | velocity tile-max | `0xF05DCBFD` | 1/32 tile | McGuire tile downsample of velocity to a max-magnitude buffer (each tile ≈ 32×32 source pixels). Pure velocity prep. |
| 1293 | velocity tile-max refine | `0x512E2B48` | 1/32 tile | Second tile-velocity refinement using depth. Pure velocity prep. |
| 1295–1296 | `MotionBlurMcGuire` | `0xBFC7D3C2` | half | Reads half-res HDR scene clone `0xB9BC3B20` + tile velocity, writes blurred half-res `0xB9BC3860`. |
| 1300 | `Tonemap` (LUT + final tonemap) | `0xE04D1471` | full | Reads full-res HDR scene `0xB9BBB9E0` at SRV t0, writes full-res `0xB9BBA3E0`. With custom tonemap on: applies a transient Reinhard smooth-clamp → LUT sample → invert clamp → psycho17 tonemap (see [Tonemap_0xE04D1471.ps_5_0.hlsl:57-160](../tonemap/Tonemap_0xE04D1471.ps_5_0.hlsl#L57-L160)). **SRV t0 is linear HDR — the right TAA input.** |
| 1302 | `FXAA` | `0x900968FF` | full | Final AA on the resolved color. We disable this when TAA is on. |

Velocity is fully produced by draw 21 and survives to all later passes (still bound at draw 1300).

`Tonemap_0xE04D1471` is the LATE pass that does LUT sampling + color grading + (when the custom tonemapper is on) the actual final tonemap. The mod skips the game's earlier filmic tonemap in `DeferredRenderingFilmic` and family when `RENODX_TONE_MAP_TYPE != 0`, so the scene at this draw's SRV t0 is **linear HDR**, not LDR. That's the ideal state to feed into TAA: no compression, no LUT applied, ready for temporal accumulation.

## Scene-Color Pipeline (full trace)

The complete data flow for the full-res HDR scene, verified by MCP draw inspection:

```
0xB9BBA3E0 ← lighting + bloom + filmic + particles + decals (built over draws 21..~1130)
        │
        ├─→ [full → half downsamples at unidentified mid-frame draws]
        │       → 0xBD552520 (DoF input pyramid base)
        │       → 0xB9BC3B20 (motion-blur input)
        │
        │ Draws 628..1130: DoF gather pyramid + particle composites into 0xB9BBA3E0
        │
        ├─ Draw 1284 (CopyRenderBuffer_0x83272BCB):
        │     0xB9BBA3E0 → 0xB9BC32E0  (pre-DoF-Final scene snapshot, full → full)
        │
        ├─ Draw 1285 (DOF_ScatterCompositeFinal):
        │     composites half-res DoF accumulator + 0xB9BC32E0 → 0xB9BBA3E0
        │
        ├─ Draws 1292..1296 (motion blur):
        │     ping-pong half-res 0xB9BC3B20 ↔ 0xB9BC3860
        │
        ├─ Draw 1297 (CopyRenderBuffer_0x83272BCB):
        │     0xB9BC3B20 (half-res MB result) → 0xB9BBA3E0 (full-res)
        │
        ├─ Draw 1299 (D3D11 CopyResource — no shader):
        │     0xB9BBA3E0 → 0xB9BBB9E0
        │
        └─ Draw 1300 (Tonemap_0xE04D1471):
              reads 0xB9BBB9E0 at SRV t0, writes 0xB9BBA3E0
```

The final color the player sees is what tonemap reads at `0xB9BBB9E0`, which
is a `CopyResource` snapshot of the fully-composited scene including
DoF and motion blur effects.

## Insertion-Point Strategy

DoF and motion blur are **temporally unstable** post-effects: a pixel's blur
amount changes frame-to-frame as depth or velocity changes. If TAA runs
after them, the per-frame blur differences trip TAA's history clip box and
produce ghosting / flicker on every DoF and MB transition. Classic TAA
placement is **before** DoF/MB precisely to avoid this. We follow that.

The challenge in MGSV is identifying the right pixel-shader draws to hook,
because both DoF and motion blur read from **half-res downsamples** of the
full-res scene, each produced by a separate downsample step earlier in the
frame.

The right TAA insertion point depends on which post-effects are running.
The gate priority is **sequence-based**, with the best placement first:

#### Priority 1: DOF_ScatterBakeFirst (primary)

`DOF_ScatterBakeFirst_0xFE1DC3F8` is an MRT pixel shader that reads the
**full-res HDR scene** at SRV t0 and writes to **three half-res RTVs in a
single draw** — the DoF pyramid base, an additional DoF intermediate, and
the motion-blur scene input. In observed captures it runs at draw 728,
well before any DoF or MB pass.

Hooking here is ideal: TAA runs on the full-res scene at SRV t0, then
ScatterBakeFirst proceeds and downsamples the **TAA-resolved** color into
**both** DoF and MB inputs in one go. Both effects consume TAA-resolved
content with a single hook.

The gate is the shader hash itself — no sequence flags needed, no future
state to wait for. By the time the shader fires, the full-res scene is
complete (lighting, fog, particles, decals all composited into it).

#### Priority 2: CRB after DoF (fallback)

Used only if `DOF_ScatterBakeFirst_0xFE1DC3F8` did not fire this frame
(e.g. a DoF variant with a different bake hash, or an MGSV update). A
DoF compositing pass (`0xE2D609B1` Near, `0x7C017264` Far, `0xFC5542BB`
Final) flips `dof_fired`; the next `CopyRenderBuffer_0x83272BCB` is the
fallback insertion. Observed at draw 1284 in the bake-less scenario.

#### Priority 3: CRB after MB tile prep (fallback)

Used when neither ScatterBakeFirst nor any DoF pass fires but motion blur
is still active. McGuire tile-prep passes (`0xF05DCBFD`, `0x512E2B48`)
are unique to MB and flip `mb_tile_prep_fired`; the next CRB is the MB
scene downsample. Observed at draw 1294.

#### Priority 4: Tonemap (last fallback)

Used when none of the above fired — menus, cutscene transitions, or
graphics settings with both DoF and MB disabled. `Tonemap_0xE04D1471` or
`Tonemap_1DLUT_0xC0C26E46` triggers TAA on its bound pixel SRV t0.

#### Considered but unused: DR_VolFog_TppTonemap

`DR_VolFog_TppTonemap_0x2EA8F13F` at draw 310 finalizes deferred lighting +
volumetric fog into the main scene, after which the image is "largely
formed." A pending-flag pattern could TAA on the next draw after this
shader, providing an alternative fallback for the case where neither
ScatterBakeFirst nor DoF/MB fire but particles do.

This is **not currently used** because ScatterBakeFirst already covers the
DoF and MB cases with better placement (after all particles/decals/etc.
composite), and the tonemap fallback handles the no-post-effects case.
Documented here in case a future MGSV variant breaks the primary gate.

### Properties of the four-tier gate

- **Robust across resource-handle changes** between game sessions.
- **No assumptions about resolution or clone-upgrade flags** of the CRB's
  RTV/SRV. If MGSV ever shuffles internal resolutions (DLSS-like passes,
  etc.) the gate still works as long as DoF/MB are consumers.
- **No future-knowledge needed** — by the time the CRB fires, the relevant
  DoF or MB shader has already fired in the same frame.
- **Mutually exclusive** — only one of the three gates ever fires TAA per
  frame, enforced by `taa_ran_this_frame`.

Order of detection per frame (first matching gate wins, mutually exclusive):

| Priority | Predicate | When it fires | What it fixes |
|---|---|---|---|
| 1 | `DOF_ScatterBakeFirst_0xFE1DC3F8` | The MRT bake that produces BOTH DoF and MB inputs — observed at draw 728 | Both DoF and MB consume TAA-resolved content. Best placement. |
| 2 | `CRB && dof_fired` (and ScatterBakeFirst didn't run) | The first CRB after any DoF pass — observed at draw 1284 | Fallback if a DoF variant uses a different bake hash. |
| 3 | `CRB && mb_tile_prep_fired` (and no DoF this frame) | The first CRB after MB tile prep — observed at draw 1294 | MB-only paths where ScatterBakeFirst is absent. |
| 4 | `Tonemap_PS` (nothing above fired) | Right before LUT/color grading | High-frequency aliasing on menus/cutscenes/disabled-effects. |

### What was rejected

Several alternative insertion ideas turned out impractical:

- **Insert before particle composites finish (~draw 1130).** The scene
  is still being written to with particles, decals, materials through
  ~draw 1130. TAA at any earlier point would temporally accumulate an
  incomplete scene, then have particles drawn on top of the TAA'd result.
- **Hook the D3D11 `CopyResource` at draw 1299.** It's a copy call, not
  a shader draw — there's no pixel-shader hash to hook. The `copy_resource`
  ReShade event is generic and fires for many unrelated copies.
- **Insert at tonemap only.** Leaves both DoF and MB transitions unstable
  (every focus change or motion-blur sample mismatch trips TAA's clip
  box). Now only used as a fallback when no DoF/MB markers fire.
- **Hook the DoF compositing passes directly** (`0xE2D609B1`, `0x7C017264`,
  `0xFC5542BB`) or `MotionBlurMcGuire_0xBFC7D3C2`. By the time these run,
  the half-res downsamples they consume are already produced. Too late.
   | 6 | `0xBFC7D3C2` MotionBlurMcGuire | Reads half-res scene clone |
   | 7 | `0xE04D1471` Tonemap | **V1 insertion** |
   | 8 | `0xC0C26E46` Tonemap_1DLUT | V1 insertion fallback |

But the four points above make V2 not actually deliver better quality — see
"Insertion-Point Strategy" above for the trace and reasoning. The candidate
table is preserved here for future reference in case a specific workflow
demands it.

The two velocity-tile prep passes (`0xF05DCBFD`, `0x512E2B48`) **do not read
the scene color**, so there is no need to insert TAA before them even when
motion blur is active.

## Inputs at the Tonemap Draw

All four inputs we need are still bound when MGSV's tonemap pixel shader is about to
draw. We can read them directly off the tonemap draw's own descriptor state — no
separate capture pass needed.

| Input | Slot at tonemap | Resource | Format | Notes |
|---|---|---|---|---|
| HDR color | pixel `t0` | upgraded clone of `0xB9BBB9E0` | r16g16b16a16_float | Already cloned by [addon.cpp](../addon.cpp) (`scene_tonemap_upgrade_info`). |
| Camera velocity | pixel `t2` | `0xB9BB9E60` (RTV0 of `MotionBlurCameraVelocity`) | b8g8r8a8_unorm | Velocity packed as `0.5 + 0.5 * v_scaled` in `.b/.a`. |
| Depth | pixel `t3` | `0xB9BBCA60` | r32_float_x8_uint | 32-bit float depth + 8-bit stencil. |
| (optional) Per-object velocity | pixel `t14` | `0xB9BBB720` | b8g8r8a8_unorm | Same encoding. Not used; camera velocity is the composite. |

Slot positions verified live for this capture; cbuffer/slot stability across all
scenes is **TBD** (requires probing more game states).

## Camera Cbuffer Layout

The cbuffer struct is the same for vertex and pixel stages and lives at register
`b2` on both stages. Size **480 bytes**.

| Float4 register | Offset | Field | TAA action |
|---|---|---|---|
| `cb2[0..3]` | 0 | `m_projectionView` | **Patch with current-frame jitter** |
| `cb2[4..7]` | 64 | `m_projection` | **Patch with current-frame jitter** |
| `cb2[8..11]` | 128 | `m_view` | Leave alone |
| `cb2[12..15]` | 192 | `m_shadowProjection` | Leave alone |
| `cb2[16..19]` | 256 | `m_shadowProjection2` | Leave alone (used by camera-velocity reproject) |
| `cb2[20]` | 320 | `m_eyepos` | Leave alone |
| `cb2[21]` | 336 | `m_projectionParam` | Leave alone (depth linearization params) |
| `cb2[22]` | 352 | `m_viewportSize` | Leave alone |
| `cb2[23]` | 368 | `m_exposure` | Leave alone |
| `cb2[24..26]` | 384 | `m_fogParam[3]` | Leave alone |
| `cb2[27]` | 432 | `m_fogColor` | Leave alone |
| `cb2[28]` | 448 | `m_cameraCenterOffset` | Leave alone |
| `cb2[29]` | 464 | `m_shadowMapResolutions` | Leave alone |

MGSV uses **multiple cbuffer resources** for this struct across passes (e.g.
`0xBD54FEA0` at `MotionBlurCameraVelocity`, `0xBD5D6A20` at `GBufferVelocity`'s pixel
stage). The jitter runtime must track every cbuffer whose size matches and patch all
of them on unmap.

## Matrix-Usage Map (from asm)

From `MotionBlurCameraVelocity_0xA13321B6.ps_5_0.asm`:

```
dp4 r3.x, r4.xyzw, cb2[16].xyzw   // m_shadowProjection2
dp4 r4.x, r3.xyzw, cb2[8].xyzw    // m_view
```

The camera-velocity composite uses **only** `m_shadowProjection2` and `m_view`. It
does not touch `m_projection` or `m_projectionView`, so leaving those unjittered
means the camera-velocity output is identical to vanilla — **camera velocity is
clean.**

From `GBufferVelocity` VS `0x1D2531B7`:

```
dp4 r4.x, r3.xyzw, cb2[4].xyzw    // m_projection      (current pos)
dp4 r3.x, r0.xyzw, cb2[12].xyzw   // m_shadowProjection (prev pos)
dp4 r0.x, r3.xyzw, cb2[16].xyzw   // m_shadowProjection2 (prev pos)
```

Per-object velocity uses `m_projection` for current and `m_shadowProjection × m_shadowProjection2` for previous. If we patch `m_projection`/`m_projectionView` with
current-frame jitter and leave the prev matrices alone, the resulting per-object
velocity equals `pure_motion + current_jitter` (because curr is jittered but prev
is not).

The compute shader compensates: **`taa_velocity = decoded_velocity - prev_jitter`**.
This yields the correct sub-pixel reprojection delta `curr_jitter - prev_jitter +
pure_motion`. Both current and previous jitter offsets are passed to the compute
shader via push constants each frame.

## Velocity Encoding

Empirically verified via DevKit `analyze_resource` on the camera-velocity RTV with
no motion in scene: R = 0, G = 0, B ≈ 0.5, A ≈ 0.5. The shader stores `(0, 0,
encoded_vX, encoded_vY)` in `o0.xyzw`. In a BGRA8 buffer that lands at memory
positions `(R, G, B, A) = (0, 0, encoded_vX, encoded_vY)`.

The shader's encoding chain:

```
v_scaled.xy = ndc_delta.xy * m_renderInfo.xy / 128.f
encoded.xy = 0.5 + 0.5 * (v_scaled.x, -v_scaled.y)   // note Y sign flip
```

Compute-shader decode (treating UV.y as flipped relative to NDC.y, which cancels the
shader's Y flip):

```hlsl
float4 raw = velocity_texture.SampleLevel(point_sampler, uv, 0);
float2 encoded = raw.ba;                                  // .b = vX, .a = vY
float2 uv_velocity = (encoded * 2.0 - 1.0) * 64.0 / float2(width, height);
```

Note: `m_renderInfo.xy` is the render buffer size in pixels. The `* 64 / size`
inverse-scales the shader's `* size / 128` and adds the `* 0.5` NDC-to-UV factor.

Max representable motion ≈ `64 / width` per frame in UV space, which is
~64 pixels at any backbuffer resolution. Plenty for TAA's sub-pixel
concerns.

## V1 Insertion Point (before tonemap)

Before either `Tonemap_0xE04D1471` or `Tonemap_1DLUT_0xC0C26E46` runs:

1. Read the four inputs from the tonemap draw's current descriptor state.
2. Validate the velocity/depth resources match the same frame.
3. Run the TAA compute resolve.
4. Copy the resolved current-history texture back into the HDR color resource.
5. Return `false` so MGSV's tonemap continues normally.

In observed captures only one of `0xE04D1471` / `0xC0C26E46` runs per frame.
We register both as insertion points; whichever fires first wins.

See **Insertion-Point Strategy** above for V2 plans to move the insertion
earlier (before DoF / motion blur).

## Jitter Sequence

Same Hammersley-permuted 16-sample sequence as Alien Isolation, converted to UV
deltas:

```
sample = (taa_sample_index * 7) % 16
offset.x = ((sample + 0.5) / 16  - 0.5) * 2 / width
offset.y = (HammersleySample(sample, 0x0E348C73) - 0.5) * 2 / height
```

`taa_sample_index` advances only after a successful dispatch + copy-back.

## File Layout

```
src/games/mgsv/taa/
├── TAA_IMPLEMENTATION.md            # this file
├── taa.hpp                          # entry point — namespace `taa` (HandleDraw, Use, AppendSettings)
├── runtime/
│   ├── logging.hpp                  # logging helpers
│   ├── constant_buffers.hpp         # enable, frame state, jitter sequence
│   ├── descriptor_tracker.hpp       # per-command-list register tracking
│   ├── jitter.hpp                   # cbuffer map/unmap patching
│   └── resolve.hpp                  # compute resource management + dispatch (namespace `taa::resolve`)
└── shaders/
    └── mgsv_taa.cs_5_0.hlsl         # TAA compute shader
```

The MGSV reference HLSL/asm dumps for the velocity, DoF, motion-blur and
tonemap passes live in `C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP\renodx-dev\dump`
under their hash filenames (e.g. `0xA13321B6.ps_5_0.{hlsl,asm}`). They are not
needed at build time.

## Shader Hashes

Insertion gates actioned by `HandleDraw`:

| Priority | Hash | Gate condition | When |
|---|---|---|---|
| 1 | `0xFE1DC3F8` | Shader hash alone (MRT bake of DoF + MB inputs) | At ScatterBakeFirst draw — observed at draw 728 |
| 2 | `0x83272BCB` | `dof_fired` (any of DoF Near/Far/Final ran) and priority 1 didn't | First CRB after a DoF pass — observed at draw 1284 |
| 3 | `0x83272BCB` | `mb_tile_prep_fired` and nothing above fired | First CRB after MB tile prep — observed at draw 1294 |
| 4 | `0xE04D1471` / `0xC0C26E46` | Tonemap fires and nothing above fired | Right before LUT pass at draw 1300 |

Shaders that flip the frame-state markers (always run, never insertion
points themselves):

| Hash | Stage | Flag set |
|---|---|---|
| `0xE2D609B1` | Pixel | `dof_fired` (DoF Near) |
| `0x7C017264` | Pixel | `dof_fired` (DoF Far) |
| `0xFC5542BB` | Pixel | `dof_fired` (DoF Final) |
| `0xF05DCBFD` | Pixel | `mb_tile_prep_fired` (McGuire tile-max) |
| `0x512E2B48` | Pixel | `mb_tile_prep_fired` (McGuire tile refine) |

Velocity / depth / cbuffer identification (always run, never insertion):

| Hash | Stage | Purpose |
|---|---|---|
| `0xA13321B6` | Pixel | `MotionBlurCameraVelocity` — final velocity output (runs every frame) |
| `0x1D2531B7` | Vertex | Identifies `cVSScene` cbuffer at vertex `b2` |
| `0x9815404F` | Pixel | `GBufferVelocity` (per-object velocity) |
| `0xF05DCBFD` | Pixel | McGuire velocity tile-max downsample (does not read scene) |
| `0x512E2B48` | Pixel | McGuire velocity tile refinement (does not read scene) |

Observed but irrelevant to TAA:

| Hash | Stage | Purpose |
|---|---|---|
| `0xE2D609B1` | Pixel | `DOF_ScatterCompositeNear` (writes half-res DoF accumulator) |
| `0x7C017264` | Pixel | `DOF_ScatterCompositeFar` |
| `0xFC5542BB` | Pixel | `DOF_ScatterCompositeFinal` (writes full-res scene with DoF composited) |
| `0xBFC7D3C2` | Pixel | `MotionBlurMcGuire` (reads/writes half-res scene clones) |

Recommend disabling when TAA is on:

| Hash | Stage | Purpose |
|---|---|---|
Velocity / cbuffer identification (always run, never insertion points):

| Hash | Stage | Purpose |
|---|---|---|
| `0xA13321B6` | Pixel | `MotionBlurCameraVelocity` — produces final velocity (always runs) |
| `0x1D2531B7` | Vertex | Identifies `cVSScene` cbuffer at vertex `b2` |
| `0x9815404F` | Pixel | `GBufferVelocity` (sibling of the above VS) |
| `0xF05DCBFD` | Pixel | McGuire velocity tile-max downsample — sets `mb_tile_prep_fired` |
| `0x512E2B48` | Pixel | McGuire velocity tile refinement — sets `mb_tile_prep_fired` |

Recommended off when TAA is on:

| Hash | Stage | Purpose |
|---|---|---|
| `0x900968FF` | Pixel | `FXAA` — pass-through replacement when our TAA owns the AA |

## Open Items / TBD

- [ ] Implement the compute resolve. `taa.hpp` has the skeleton for
      `EnsureComputePipeline`, `EnsureHistory`, and `DispatchCompute` — they
      need bodies ported from the Alien Isolation port, with the MGSV
      velocity decode and jitter compensation in the compute shader.
- [ ] Verify input slot stability (`t0` color, `t2` velocity, `t3` depth)
      across cutscenes, menus, weather changes.
- [ ] Verify camera-velocity always runs across all gameplay states.
- [ ] Investigate whether `DOF_ScatterBakeFirst` has variants for different
      DoF presets (e.g. `0xFE1DC3FB` was once mentioned). Register all
      variants under priority 1 if found.
- [ ] Consider `DR_VolFog_TppTonemap_0x2EA8F13F` as a pending-flag fallback
      for the rare case where ScatterBakeFirst is skipped but DoF/MB still
      run.
- [ ] Disable FXAA: shader-bytecode swap to pass-through, or skip the draw
      entirely.
- [ ] Decide whether to feed per-object velocity (`t14`) into TAA in
      addition to camera velocity. Probably unnecessary — camera velocity
      is already the composite.

## Differences vs Alien Isolation Port

| Concern | Alien Isolation port | MGSV port |
|---|---|---|
| Jitter cbuffers | 3 separate structs (XSC, VSC, PSC) | Single struct `cVSScene`/`cPSScene` at b2 |
| Jitter cancellation | Patches prev-frame matrices CPU-side; keeps no-jitter shadows | Patches only current; compensates in compute shader via `taa_velocity -= prev_jitter` |
| Velocity decode | Signed UV deltas in `.xy` | `0.5 + 0.5 * scaled` in `.ba` of BGRA8 |
| Input capture | Captures from `CAMERA_MOTION_PS` and `DOF_ENCODE_PS` draws | Reads straight off the insertion draw's descriptor state |
| History format gate | Multiple HDR formats supported | MGSV color is already RGBA16F via existing upgrade |
| FXAA / SMAA interaction | Requires game-AA setting `SMAA T1x` | Recommend FXAA off |

## Quick Read Path

1. [taa.hpp](taa.hpp) — `HandleDraw`, gating cascade (namespace `taa`)
2. [runtime/constant_buffers.hpp](runtime/constant_buffers.hpp) —
   `FrameState::dof_fired`, `FrameState::mb_tile_prep_fired`, jitter sequence
3. [runtime/resolve.hpp](runtime/resolve.hpp) — `MaybeRun`, `DispatchCompute` (namespace `taa::resolve`)
4. [runtime/jitter.hpp](runtime/jitter.hpp) — cbuffer patching
5. [shaders/mgsv_taa.cs_5_0.hlsl](shaders/mgsv_taa.cs_5_0.hlsl) — compute resolve

All four gates call the same `MaybeRun(cmd_list, command_data)`. The
difference is the *gating predicate*; the resource bound at pixel SRV t0
is the right TAA target in every case. All sources are clone-upgraded
RGBA16F at the same resolution, so the same history textures work for any
path.
