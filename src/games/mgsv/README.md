# MGSV addon

Build target: `mgsv` (Release). Output: `renodx-mgsv.addon64`, deployed next to the game's exe
together with the ReShade runtime.

```powershell
cmake --build build.vs --config Release --target mgsv
Copy-Item build.vs\Release\renodx-mgsv.addon64 "C:\Steam\steamapps\common\MGS_TPP\renodx-mgsv.addon64" -Force
```

Debug builds abort on MSVC iterator asserts (pre-existing); verify with Release.

## Ambient occlusion

- **AO Resolution** [Half] - the occlusion is computed at half resolution and resolved with a
  depth-joint bilateral. Full is available to compare.
- **AO Temporal** [on] - accumulates the occlusion across frames. The pass rotates its sample
  pattern every frame, so the residual averages out; with it off the frame is written through
  unchanged.
- **AO History** [90] - ceiling on the previous frame's weight, `min(this, 1 - 1/length)`.

## Indirect lighting

Design and measured behaviour: `docs/MGSV_IndirectLighting.md`.

Settings that matter (defaults in brackets):

- **GI Amount** [100] - percentage of the gathered mean added to the light. It is a fraction,
  not a sum.
- **GI Resolution** [Quarter] - Full, Half or Quarter. The whole chain scales with it.
- **GI Samples / GI Min Samples** [16 / 1] - gather taps, and how many must survive rejection
  before a pixel contributes at all. One keeps grazing ground from dropping out.
- **GI Radius** [80] - gather reach as a percentage of screen height.
- **GI History** [90] - ceiling on the temporal weight. The effective weight is
  `min(this, 1 - 1/length)`, so a just-reset pixel trusts the new sample.
- **GI Denoise Passes** [3] - 1-3 a-trous iterations, stride 1/2/4. 3 is visibly cleaner at
  Full resolution; at Quarter the difference is small.

### Verification

For the AO: with a still camera, toggling **AO Temporal** should not change the image — the
accumulated and the raw pass converge to the same contact shadows. Pan or walk and no occlusion
should trail behind the character; if it does, the depth/normal rejection is failing.

1. Load into gameplay with the camera still. Open **GI Debug -> History length**: the frame should
   be bright red nearly everywhere, with thin green lines only on silhouettes. Green fills a whole
   moving object means object motion is not being reprojected; green everywhere means rejection is
   failing.
2. Pan slowly. The green should track silhouettes and refill within about half a second.
3. Walk behind a pillar. No bounce should trail behind it - that would be the temporal history
   surviving a disocclusion it should have rejected.
4. **GI Debug -> Variance** should be dark on smooth lit surfaces and bright where the gather is
   noisy. If it is uniformly saturated the scale or the moment weighting is wrong.
5. **GI Debug -> Reprojection** shows the offset the history is dragged through, in pixels. At a
   still camera it should be near zero.
6. Toggle **GI Denoise Passes** between 1 and 3: noise must drop noticeably with 3.
7. Set **GI Amount** to 0 and confirm the image returns to vanilla, which checks the add pass is
   not leaking.

### Known limitations

- Flat ground loses most of its gather samples to the plane test (26-73%), so its bounce is
  thinner than it should be. The denoiser hides it; the fix needs an ambient radiance source, not
  a weighting change.
- The deferred light buffer has no sky/ambient and no emissive fixtures, so neither can bounce.
