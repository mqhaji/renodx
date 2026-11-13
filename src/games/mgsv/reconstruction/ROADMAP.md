# MGSV reconstruction roadmap

This page tracks outstanding work. The [reconstruction guide](README.md) owns current behavior, defaults, [known limitations](README.md#known-limitations-and-validation-status), and the [manual test matrix](README.md#manual-validation). Resolution scaling and basic quality switching have gameplay coverage, but cold startup, loading, resizing, special views, DoF focus edges, and missing-input recovery still need broader checks. No new performance gain is established by the file cleanup.

## Rain, scene ownership, and transitions

- The latest saved-DLSS-Auto ACC check passed image, local-light, and startup-performance validation with scene-owned jitter, successful-output history commit, retained allocations, and late-worker flare-clear retention. Extend that coverage through gameplay transitions. Offline checks cover overlapping scenes, repeated setup across Present, cached-list lifetime, loading UI, and reset rejection. Continue tracing the actual writers behind each GPU consumer when investigating flashes.
- Investigate rain inside the ACC at reduced input resolution, intermittent Mother Base flashes even at Native quality, and sortie horizon/ground flashes separately until evidence connects them. Match actual color/depth producers, resource roles, native planar reflections, and submitted command lists before changing routing.
- Extend loading/save-reload, alt-tab/resize, device recreation, and rapid method/quality/reset coverage. Confirm canceled reconstruction preserves native prefixes/remainders and current scene alpha. Unsupported DoF paths must keep requesting native-size recovery rather than redirecting unknown resources.
- The retired state-profile/cold-start flicker sweep is not a current test request. Legacy compute-only state restoration and the `0x200DBED9` light-VS workaround were removed; do not restore them as speculative fixes. Those removals did not establish a fix for character wobble or rapid night-vision skinned-mesh flicker.

## Character motion and pose correspondence

- Establish same-character color/velocity correspondence through semantic vertex elements, referenced vertex/index/skin data, object/instance identity, and the history actually accumulated. Shader input registers are not vertex-buffer slots, and matching all bindings or POSITION alone is insufficient.
- Match the current color pose to current velocity and the previous velocity pose to the last accumulated color. Account for deferred execution, cached-list replay, skipped updates, and writes between motion composition and SDK preparation. Matching SRVs, first-bone values, CPU ordering, and successful readback do not establish matching pixels.
- Native previous viewport matrices advance independently of addon history. The external velocity dispatcher at plugin `+0x68` / virtual `+0x30` and per-object inclusion remain unresolved. The descriptor pool is not a bone uploader. Prove a usable same-character pair before requesting another generic diagnostic run.
- Analytical Jitter Off and Auto/Direct motion comparisons did not resolve wobble; Matrix Camera Everywhere caused too much ghosting to judge it. DLSS has also shown wobble. Keep Direct Object (6) and Auto Center Pixel (7) as diagnostics, and do not change motion sign/scale or relax frame/reset guards without evidence.

## Projection coverage

The current CPU checkpoint adds a producer-owned deferred SH projection correction and aligns sky inverse-camera rays with the scene sample. The user confirmed normal ACC and Mother Base images, including reduced and Native DLSS input sizes. Captures matched the SH and sky matrices, recorded draw grids, scene ownership, and executed command lists. Native tracing also identified the terrain worker's separate raster matrices and the source of scatter camera-center constants. A follow-up scene confirmed exact terrain packet/raster matrix agreement at 1920x1080 input. The [reconstruction guide](README.md#experimental-projection-paths) describes the implementation.

- Local Light now corrects the owned draw packet before it reaches shadowed and unshadowed workers. The packet hook passed the ACC image, local-light alignment, and saved-DLSS-Auto startup check. Continue validation at Mother Base and through weather and special-view transitions. Offline checks cover scene/camera mismatches, duplicate requests, retained packet contents, and resets while waiting for publication.
- Validate sunlight/SH draw grids and terrain inverse-plane versus raster agreement independently. Their `1x` defaults do not establish correct alignment in every scene.
- Keep Alternate Stream disabled after the settled `0x`/`1x` waveform comparison. Its hooks belong to Draw2D billboard and world-coordinate commands, and the renderer also handles glyphs and UI. Any selective correction requires the actual command type, scene owner, target, depth state, and position relative to reconstruction. Track older Preset M rail flicker separately.
- Extend the verified SH packet route `281EF0 -> 283B90 -> 280060 -> 27F450` beyond volume kind 0. Captures covered native SH blending and SH occlusion. Their color and depth/stencil targets use half the scene dimensions on each axis; sampled depth uses the scene dimensions.
- Sky inverse rows at `1F50D0 -> 306D10` matched the SH scene sample at reduced and Native input sizes. Terrain packet/raster agreement at `292D70` passed at reduced input size. Extend that check to Native input size. Native scatter reads camera-center offsets from camera `+118`; existing temporal lighting removes current jitter during position reconstruction. Keep any future shared camera-constant change paired with its consumers.
- Thermography's two guarded native setters have matching clip-position evidence, but the alternate branch still needs gameplay coverage. Check signed/fractional scales, SH kinds, rejected/nested scopes, foreign or null cameras, teardown, and normal exit.
- Hooks remain loaded for the process lifetime. Restart for replacement/removal; do not add unsafe live detach or resource release while old work can still execute.

The next CPU checkpoint is an audit of repeated size and constant preparation. Reuse the owning scene's verified dimensions and camera values where several consumers prepare the same data. Keep utility views, native buffer layouts, command-list lifetime, and reset boundaries explicit. Measure redundant preparation before adding another cache. Keep changes within the verified scene and consumer contracts.

## SDK lifecycle and validation

- Exercise lazy DLSS startup from saved and manual selection, cancellation, missing-DLL/unsupported-device explanations, model-F fallback, query boundaries, insertion fallbacks, state restoration, and callbacks crossing Present.
- Validate suspension/resume across DLSS, FSR, Analytical, and Off, then test model/size recreation and device teardown separately. One successful switch does not prove the earlier crash is resolved.
- Compare methods on the same verified inputs: motion direction/scale, reverse-Z depth, current alpha, and downstream scene/UI output. Keep automatic exposure until a suitable MGSV exposure resource is proven.
- Shared indirect lighting has its own submitted producer/history ledger and controlled replay coverage. Do not treat the older AA-Off motion compatibility concern as the current implementation or assume scene-reconstruction history proves lighting history. See the [lighting guide](../lighting/indirect/README.md).

## Temporal quality after input correctness

- Find a native camera-cut signal for aim/binocular/FOV changes, cuts, teleportation, pause/resume, and display changes. Do not restore the clip-space heuristic that reset SDK history continuously.
- Consider linear RGBA16F Analytical history while preserving encoded game output and current alpha. Add previous-depth/disocclusion checks before relaxing RGB clipping or adding thin-feature locks.
- Build reactive/transparency masks from verified material membership, starting with `TppFxRain`, then particles, animated textures, reflections, and emissive transparency. Preserve AMD's internal reactivity analysis and original material targets; test trails and stability separately.
- A production unclamped-motion path should own temporal-only data rather than changing native motion blur. The shared Unclamp option remains a default-Off diagnostic. Consider luminance stabilization and optional sharpening only after motion, cuts, disocclusion, and masks are verified.

## Future frame generation

Identify a final post-processed/graded scene image without UI. Native 2D shaders also implement camera filters, so they do not define a universal HUD boundary. Before choosing a Streamline integration, verify API support, the scene-copy encoding/exposure contract, motion/depth, UI handling, and synchronization. Convert to PQ only if the selected SDK contract requires it. Frame generation is not part of the current reconstruction path.
