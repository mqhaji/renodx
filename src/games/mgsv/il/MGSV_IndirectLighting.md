# MGSV indirect lighting (screen-space diffuse gather)

Reference notes for screen-space indirect lighting (`IL`) in `src/games/mgsv`. It estimates bounced light using surfaces visible on screen and the game's existing deferred lighting.

## Pipeline

1. **Gather** (`il/mgsv_il.cs_5_0.hlsl`) - golden-angle disk of up to 16 taps at the gather resolution, each tap rejected or weighted by plane elevation, source facing and distance, and returning `albedo * diffuse + specular` of the tap. Alpha is validity.
2. **Accumulate** (`il/mgsv_il_accumulate.cs_5_0.hlsl`) - reprojects using the game's velocity view captured for reconstruction. It checks depth, normal, and screen bounds, alternates two history textures, and stores history length in alpha. It also tracks average luminance and squared luminance for denoising.
3. **Denoise** (`il/mgsv_il_composite.cs_5_0.hlsl`) - 1-3 a-trous iterations at stride 1, 2, 4, weighted by depth, normal and the measured per-pixel noise.
4. **Resolve** (`il/mgsv_il_add.cs_5_0.hlsl`) - upsamples using depth to avoid mixing across edges, scales the result by **Indirect Lighting Strength**, then adds it to the full-resolution light buffer.

All intermediate textures (gather, two history, two geometry-history, two luminance-moment, filtered, and accumulated textures) use **Indirect Lighting Resolution**: Full, Half, or Quarter. The resolve writes at full resolution. Unused output slots use separate placeholder textures, not textures that a pass reads. This matters especially after two denoise iterations, when the last result is in the accumulated texture rather than the filtered texture.

The velocity view and cached material views are not checked against a frame identifier here. Keeping a view alive also does not preserve a copy of its texture contents.

## Recorded observations and numerical safeguards

The measurements below describe development captures, not every frame or reconstruction mode.

- **The captured velocity was close to zero with a static camera.** In that capture, `b0.cCameraCenterOffset` was zero at the indirect-lighting hook, native jitter was active, and median decoded motion was 0.03 pixels. This is not proof of jitter handling for every object, frame, or reconstruction mode. The indirect-lighting shader currently applies no separate jitter correction.
- **Decoded normals point along negative or zero view-space z.** For normalized texture data, `DecodeNormal` computes `nz = enc.z^2 - 1`. Reconstructing a stored `xy` with only `+sqrt` would flip normals that should have negative z and can cause history rejection.
- **Non-finite values can spread through history.** Reusing an invalid value or squaring a large luminance value can produce further invalid values. The shaders include finite-value checks and numerical limits, but these do not prove that every history value is valid or explain every black pixel.

## Known limitation: ground starvation

The gather's plane test (`smoothstep(0.02, 0.35, elevation)`) rejects samples lying on the same surface. Recorded flat-ground tests lost 26-73% of the valid samples. Denoising reduced the visible gaps but left the ground looking slightly flat.

A depth-based same-surface rejection was tried and made it worse: the ground then accepted only samples at other depths, which is mostly sky, and the deferred light buffer carries no sky or ambient term, so the gather returned valid-but-black (78% of the frame in that test). That experiment did not establish a fix. Future work should verify that accepted samples carry useful lighting information rather than simply increasing their count.

## Manual verification

See `src/games/mgsv/README.md`.
