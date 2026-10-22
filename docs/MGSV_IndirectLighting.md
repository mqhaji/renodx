# MGSV indirect lighting (screen-space diffuse gather)

Reference notes for the `IL` chain in `src/games/mgsv`. It exists to bounce the light the retail
deferred passes already produced; it is not a second lighting model.

## Pipeline

1. **Gather** (`il/mgsv_il.cs_5_0.hlsl`) - golden-angle disk of up to 16 taps at the gather
   resolution, each tap rejected or weighted by plane elevation, source facing and distance, and
   returning `albedo * diffuse + specular` of the tap. Alpha is validity.
2. **Accumulate** (`il/mgsv_il_accumulate.cs_5_0.hlsl`) - reprojects with the TAA velocity buffer,
   rejects on depth, normal and screen bounds, keeps a ping-ponged history, and writes the history
   length into alpha. Also maintains the luminance moments used by the denoiser.
3. **Denoise** (`il/mgsv_il_composite.cs_5_0.hlsl`) - 1-3 a-trous iterations at stride 1, 2, 4,
   weighted by depth, normal and the measured per-pixel noise.
4. **Resolve** (`il/mgsv_il_add.cs_5_0.hlsl`) - depth-joint bilateral upsample to the screen, then
   scaled by `GI Amount` and added to the light.

All five targets (gather, two history, two aux, two moments, filtered, accumulated) live at
`frame size / GI Resolution`.

## Things that were measured, not assumed

- **Projection jitter is not in the velocity buffer.** `b0.cCameraCenterOffset` is zero at the IL
  hook, the TAA jitter is published and active, and the decoded velocity at a still camera has a
  median of 0.03 px. The velocity pass runs with the jittered projection, so the jitter cancels
  inside the buffer. Do not subtract it here; TAA's own `current_jitter_uv` term exists to align
  its history resample to a fixed output grid, not to correct reprojection.
- **The packed normal has a negative z.** `DecodeNormal` computes `nz = enc.z^2 - 1`, so `nz` is
  always negative. Reconstructing a stored `xy` with a blind `+sqrt` mirrors the normal and rejects
  everything as a geometry change.
- **Non-finite values are permanent.** One bad value in the history is carried forward by the
  temporal blend, squared into infinity by the moments, and renders black. Every division that can
  produce one is guarded, and every early return in a filter writes a value instead of skipping.

## Known limitation: ground starvation

The gather's plane test (`smoothstep(0.02, 0.35, elevation)`) rejects samples lying on the same
surface. On flat ground almost every neighbour is on that surface, so the ground loses 26-73% of
its valid samples. The denoiser fills the holes, which is why the ground reads slightly flat
rather than broken.

A depth-based same-surface rejection was tried and made it worse: the ground then accepted only
samples at other depths, which is mostly sky, and the deferred light buffer carries no sky or
ambient term, so the gather returned valid-but-black (78% of the frame). Any future fix has to
supply real radiance (a hemisphere ambient estimate), not merely more accepted samples.

## Manual verification

See `src/games/mgsv/README.md`.
