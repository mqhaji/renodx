# MGSV Temporal Reconstruction Roadmap

Outstanding work only. [README.md](README.md) owns current defaults, architecture, safety contracts, and manual validation.
Keep native resolution, exact Off/vanilla behavior, bone-aware motion, current scene alpha, and preset-local settings.
No viewport/culling changes, broad constant-buffer mutation, or camera-only production motion replacement.

## 1. Unresolved character warble and pose ownership

- Root cause remains unidentified. Latest user clarification (2026-09-09): Analytical Jitter Off gave no improvement;
  repeated Auto/Direct comparisons showed no noticeable difference. Matrix Camera Everywhere was **inconclusive**:
  heavy ghosting prevented judging warble, so it is not evidence that bypassing native object motion helped.
  The velocity ownership correction did not visually fix the observed warble. Do not repeat those sweeps or claim a fix.
- The user tentatively thinks DLSS may not warble while Analytical TAA/FSR are noticeable; this is not confirmed.
  The native-pose coverage gaps below remain open, not presumed causal or prioritized by the camera-only test.
- For any native-pose investigation, establish same-character color/velocity correspondence using
  semantic IA elements, referenced VB/IB/index/skin data, object/instance identity, and contributing history.
  Shader input registers are not IA slots; neither all-32-binding equality nor POSITION-only matching is sufficient.
- Match current color to current velocity pose, and previous velocity pose to the prior actually accumulated color.
  Account for deferred execution, replays, skipped updates, and intervening resource writes through composite/Prepare.
  Successful readback, matching SRVs, first-bone values, and CPU commit order are not pixel-provenance proof.
- Native previous viewport matrices advance independently of addon history. The concrete external velocity dispatcher
  at plugin `+0x68` / virtual `+0x30` and per-object inclusion remain unresolved; the descriptor pool is not a bone uploader.
  Resolve that route or prove a usable same-character pair before requesting another generic diagnostic run.
- Direct Object (6) and Auto Center Pixel (7) stay diagnostics. Center-pixel selection has no established visual fix;
  selection differences alone do not establish causality. Do not change motion sign/scale or loosen epoch guards blindly.

## 2. Native projection coverage and transitions

- Validate the five implemented Analytical-only paths independently before any SDK promotion: sunlight/SH live draw
  counts and target grid, terrain inverse-plane versus raster consistency, general-light final consumers, and alternate
  world/UI/glyph separation. Their `1x` defaults do not constitute runtime validation.
- **Deferred SH packet remains blocked:** prove producer generation/sample through consumption at `0x27F450`.
  Latest-camera VP equality is insufficient; preserve inverse-lighting math and avoid guessed packet fields/pointer caches.
- **Thermography/night vision remains unidentified:** qualify target/depth ownership at setter returns `0x214531` and
  `0x214AC3` before adding a whitelist entry. Rapid skinned-mesh flicker is not proof of missing projection jitter.
- Check signed/fractional scales, all SH kinds, rejected/nested scopes, null/foreign cameras, device recreation, normal
  exit, and rapid preset/method/Off/reset transitions. Prove admitted old work cannot contaminate new history.
  Hooks remain process-lifetime pinned; restart for replacement/removal, never introduce unsafe live detach/reclamation.

## 3. SDK lifecycle and gameplay validation

- Run the [manual validation matrix](README.md#manual-validation) on the cleanup binary when deployment is authorized.
  The preceding `mgsv` build passed; it was not deployed or newly visually tested. No performance improvement is measured.
- Validate lazy DLSS cold/saved activation, cancellation, missing DLL/unsupported device reasons, and F-default support
  fallback. Exercise query spans, natural recording boundaries, insertion fallbacks, restore-state flags, and epoch lag.
- Validate DLSS suspension/resume across FSR/Analytical/Off, then model/resize recreation and device teardown separately.
  Preserve original command-list prefixes and cleanup ownership. One successful switch does not prove crash resolution.
- Compare all methods on identical qualified inputs, including HUDless insertion/reintegration, reverse-Z disocclusion,
  motion direction/scale, current alpha, and downstream effects. Retain auto exposure until an MGSV resource is proven.

## 4. Temporal quality after input correctness

- Find a proven native camera-cut signal for aim/binocular/FOV changes, cuts, teleportation, pause/resume, and display
  changes. Do not restore the removed clip-space heuristic that caused continuous SDK resets.
- Move Analytical history to linear RGBA16F for optimized reconstruction; preserve encoded game output and current alpha.
  Add previous-depth/disocclusion confidence before relaxing RGB clipping or adding depth-consistent thin-feature locks.
- Build owned frame/sample/dimension-matched reactive and transparency masks from proven material membership, starting
  with `TppFxRain`; extend to particles, animated textures, reflections, and emissive transparency. Preserve AMD's internal
  reactivity analysis and original material targets; verify reduced trails without destabilizing opaque motion or foliage.
- For production unclamped motion, use an owned temporal-only path; keep native motion blur a separate compatibility
  consumer. The shared Unclamp option remains default-Off diagnostic behavior.
- Consider luminance stability and optional sharpening only after motion, cuts, disocclusion, and masks are validated.