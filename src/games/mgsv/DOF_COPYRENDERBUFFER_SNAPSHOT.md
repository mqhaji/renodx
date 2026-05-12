# MGSV DoF `CopyRenderBuffer` Snapshot Notes

Date inspected: May 12, 2026

This note documents the DoF `CopyRenderBuffer` issue that caused the pre-final DoF scene copy to stay SDR/BGRA8, plus the earlier rain/material corruption caused by stale clone hot-swap state.

## Current verified snapshot

Latest DevKit snapshot after the fix:

- Device index: `0`
- Captured draws: `2801`
- Resource usage rows: `37882`
- Tracked shaders: `301`
- `CopyRenderBuffer_0x83272BCB` appears `9` times:
  - `10`, `53`, `54`, `91`, `130`, `2464`, `2515`, `2549`, `2579`

Relevant DoF order:

| Draw | Pixel shader | Role |
|---:|---|---|
| `1986` | `0xE2D609B1` | `DOF_ScatterCompositeNear` opens the DoF-copy window early. |
| `2447` | `0x7C017264` | `DOF_ScatterCompositeFar` refreshes the DoF-copy window immediately before the target copy. |
| `2464` | `0x83272BCB` | **Correct pre-final `CopyRenderBuffer`; this writes DoF Final `t1`.** |
| `2480` | `0xFC5542BB` | `DOF_ScatterCompositeFinal`; this samples draw `2464` as `t1`. |
| `2515` | `0x83272BCB` | Post-Final scene copy/blend; not the DoF Final `t1` producer. |

## Verified fixed behavior

Draw `2464` (`CopyRenderBuffer_0x83272BCB`) is now upgraded correctly:

- RTV0 original resource: `0x00000000BA7C15E0`
- RTV0 original format: `b8g8r8a8_typeless`
- RTV0 view format: `b8g8r8a8_unorm`
- RTV0 dimensions: `3840x2160`
- RTV0 clone resource: `0x00000000BD3C5660`
- RTV0 clone format: `r16g16b16a16_float`
- SRV0 original resource: `0x00000000B763C960`
- SRV0 clone resource: `0x00000000A973E1E0`
- SRV0 clone format: `r16g16b16a16_float`

Draw `2480` (`DOF_ScatterCompositeFinal_0xFC5542BB`) consumes the upgraded result:

- RTV0 original resource: `0x00000000B763C960`
- RTV0 clone resource: `0x00000000A973E1E0`
- SRV1 original resource: `0x00000000BA7C15E0`
- SRV1 clone resource: `0x00000000BD3C5660`
- SRV1 is used by the active shader and is the exact output of draw `2464`.

This confirms the intended path:

```text
scene HDR clone -> CopyRenderBuffer draw 2464 -> DoF Final t1 clone -> DOF_ScatterCompositeFinal draw 2480
```

## Post-Final `CopyRenderBuffer` note

Draw `2515` is also a `CopyRenderBuffer` and appears to write an upgraded/cloned full-resolution scene target:

- RTV0 original resource: `0x00000000B763C960`
- RTV0 clone resource: `0x00000000A973E1E0`

This is **not** the important DoF Final `t1` producer. It happens after `DOF_ScatterCompositeFinal`, so upgrading this draw alone does not fix DoF Final. The key draw is the `CopyRenderBuffer` between `DOF_ScatterCompositeFar` and `DOF_ScatterCompositeFinal`.

## Root cause of the long debugging loop

The final bug was addon-side source detection, not a confirmed RenoDX core clone-creation failure.

The gate for `UpgradeDofFinalCopyRenderBuffer()` checked SRV0 with D3D11 `PSGetShaderResources()`, then asked whether that SRV matched `scene_tonemap_upgrade_info` and had `clone_enabled` state.

That missed the correct draw because, after RenoDX rewrites descriptors, `PSGetShaderResources()` can return the **already-rewritten clone SRV** instead of the original SRV.

Important detail:

- Original SRV/resource views can have `clone_enabled = true`.
- Clone SRV views have `is_clone = true` and `clone_target`, but they do not necessarily carry `clone_enabled = true` on the clone view itself.
- The old gate only accepted active original views/resources.
- Therefore the correct pre-Final `CopyRenderBuffer` was rejected even though it was actually reading the active HDR scene clone.

The fix was to make `ResourceViewMatchesCloneTarget()` accept clone SRVs by checking the clone view's `original_resource` and confirming that the original resource:

1. is not destroyed,
2. has `clone_target == &scene_tonemap_upgrade_info`,
3. has `clone_enabled == true`, and
4. owns the same clone resource returned by the clone SRV.

After that, the pre-Final `CopyRenderBuffer` passed the source gate and upgraded correctly.

## Things that were ruled out or refined

### `CopyRenderBuffer` cannot be globally upgraded

`CopyRenderBuffer_0x83272BCB` is generic and appears many times per frame. Globally upgrading or carrying its state across unrelated draws causes unrelated texture/rain/fog corruption.

### Previous-frame `t1` learning is unreliable

An earlier snapshot showed:

| Draw | Pixel shader | Role |
|---:|---|---|
| `1279` | `0x7C017264` | `DOF_ScatterCompositeFar` |
| `1280` | `0x83272BCB` | Correct pre-Final `CopyRenderBuffer` |
| `1281` | `0xFC5542BB` | `DOF_ScatterCompositeFinal` |

Draw `1280` wrote resource `0x00000000BDD758A0`, and draw `1281` sampled that same resource as `t1`, but it was not upgraded. Carrying a learned `t1` handle from a previous frame missed it because MGSV can rotate/recreate these full-resolution BGRA resources.

The current approach is frame-local instead.

### Arming from `Near` helps, but was not the final root cause

`DOF_ScatterCompositeNear` opens the window earlier and `DOF_ScatterCompositeFar` refreshes it near the target copy. This is safer than relying on a single marker, but the target draw still failed until clone SRV source detection was fixed.

### `__ALL_CUSTOM_SHADERS` stays last

MGSV follows the existing RenoDX game pattern where explicit game entries come before `__ALL_CUSTOM_SHADERS`, and `__ALL_CUSTOM_SHADERS` remains last.

## Required addon behavior

1. Do **not** globally upgrade every `CopyRenderBuffer` draw.
2. Do **not** rely on a previous-frame learned `DOF_ScatterCompositeFinal t1` resource handle.
3. Open a same-frame DoF-copy window from `DOF_ScatterCompositeNear` and refresh it from `DOF_ScatterCompositeFar`.
4. Upgrade only a full-resolution BGRA `CopyRenderBuffer` whose SRV0 is the active scene-color clone.
5. When checking SRV0, accept both:
   - the original SRV/view with active clone state, and
   - the already-rewritten clone SRV whose original resource is actively clone-hot-swapped.
6. Close the DoF-copy window at `DOF_ScatterCompositeFinal` so later `CopyRenderBuffer` draws cannot consume stale state.
7. Store only the resource activated by the successful current-frame DoF copy.
8. Disable clone hot-swap for that activated DoF-copy resource on every `OnPresent()`.
9. Keep the clone target/allocation for reuse; do not leave hot-swap enabled between frames.

## Earlier rain/material corruption context

In the corruption snapshot, draw `6` (`TppRainFilterGBufferMaterial_0x2C230875`) sampled the same full-resolution BGRA resource that later became DoF Final `t1`:

- Rain/material SRV8 resource: `0x00000000BDC475A0`
- Later DoF Final `t1` resource: `0x00000000BDC475A0`
- Stale clone resource: `0x00000000BE12F6E0`

The DoF clone target/allocation was not itself the problem. The problem was leaving clone hot-swap enabled into the next frame, causing early rain/material work to read stale HDR DoF-copy data. `OnPresent()` must disable hot-swap for the DoF final-copy resource after each frame.