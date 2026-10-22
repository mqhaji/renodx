# MGSV addon

Build target: `mgsv` (Release). Output: `renodx-mgsv.addon64`, deployed next to the game's exe
together with the ReShade runtime.

```powershell
cmake --build build.vs --config Release --target mgsv
Copy-Item build.vs\Release\renodx-mgsv.addon64 "C:\Steam\steamapps\common\MGS_TPP\renodx-mgsv.addon64" -Force
```

Debug builds abort on MSVC iterator asserts (pre-existing); verify with Release.

## Indirect lighting

Design and measured behaviour: `docs/MGSV_IndirectLighting.md`.

Settings that matter:

- **GI Amount** - percentage of the gathered mean added to the light. It is a fraction, not a sum.
- **GI Resolution** - Full, Half or Quarter. The whole chain scales with it.
- **GI Samples / GI Min Samples** - gather taps, and how many must survive rejection before a
  pixel contributes at all.
- **GI Radius** - gather reach as a fraction of screen height.
- **GI History** - ceiling on the temporal weight. The effective weight is
  `min(this, 1 - 1/length)`, so a just-reset pixel trusts the new sample.
- **GI Denoise Passes** - 1-3 a-trous iterations, stride 1/2/4. 3 is visibly cleaner at
  Full resolution.

### Verification

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
