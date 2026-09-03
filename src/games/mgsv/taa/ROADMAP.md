# MGSV Temporal Reconstruction Roadmap

Future work for improving the analytical TAA and FSR3 paths, adding game-derived reactive masks, and preparing reusable
native-resolution DLAA inputs. [README.md](README.md) describes the current implementation.

## Scope

- Keep **Off (Vanilla FXAA)** exact even though new profiles currently default to FSR3; migrated profiles preserve their
   previous enabled/disabled state.
- Preserve native render/output resolution. Game resolution scaling and dynamic resolution are out of scope.
- Keep MGSV's bone-aware object motion rather than replacing it with depth-only camera motion.
- Do not restore broad mapped-constant-buffer mutation.
- Keep the FSR3 3.1.5 source, custom D3D11 backend, and fixed SM5 permutations local to MGSV. AMD's host must continue
  to own the pass schedule; this is a source adaptation, not an AMD-supported D3D11 integration.
- Target DLAA specifically, not DLSS Super Resolution.

## Priority 1: signal correctness

1. **Linear analytical history**
   - Per-texel decode before 16-tap Catmull-Rom is validated and preserves correct linear-light reconstruction.
   - Store analytical history in linear RGBA16F to recover optimized Catmull-Rom sampling, then encode only the copy
     written back into MGSV's scene domain.
2. **Camera-cut reset**
   - Replace or supplement the current clip-space discontinuity heuristic with a proven native game signal if one exists.
   - Cover aiming, binocular transitions, cutscenes, teleportation, pause/resume, and display-mode changes.

## Priority 2: modern analytical history validation

1. Store or reconstruct previous-frame depth.
2. Compare reprojected expected depth against observed previous depth.
3. Produce a disocclusion confidence value rather than relying exclusively on color bounds.
4. Reject history at true surface changes while preserving it on depth-consistent geometry.
5. Move rectification from raw RGB toward a luminance/chroma representation once linear history is established.

Do not globally weaken the current AABB. Without depth validation, relaxed clipping is expected to reintroduce character
ghosting.

## Priority 3: optional thin-feature stability

Selecting the largest raw reverse-Z depth fixed the observed disappearing-wire failure by keeping nearest-surface motion
at thin foreground geometry. Add an analytical temporal lock only if other thin features still fail under intermittent
jitter coverage.

A native-resolution lock should track:

- Thin-feature detection from a local luminance neighborhood.
- Reprojected lock lifetime.
- Luminance recorded when the lock is created.
- Trust based on current shading stability.
- Immediate or accelerated unlock on disocclusion, camera reset, or material instability.

Locks should selectively protect depth-consistent thin detail. They should not become a global increase in history weight.

## Priority 4: canonical temporal inputs

The CPU now produces one immutable, device-checked `ValidatedFrameInputs` value shared by Analytical TAA and FSR3. It
snapshots color, depth, final velocity, object velocity, and camera publication together, then centralizes method
selection, copy-back, camera commit, and sample advancement. Its resources remain explicitly game-native; continue toward
the following canonical contracts before sharing converted resources with DLAA:

| Input | Target contract |
|---|---|
| Color | Full-resolution linear HDR, HUDless, before final output encoding |
| Depth | Full-resolution reverse-Z with explicit metadata |
| Motion | Signed RG16F current-to-previous motion, camera motion included, jitter excluded |
| Matrices | Current/previous no-jitter view, projection, VP, inverse VP, and clip transforms |
| Jitter | Exact applied offset in pixel and UV units |
| Frame state | Token, dimensions, previous-frame validity, camera-cut/reset state |

### Motion plan

- Derive background camera motion exactly from depth and no-jitter matrices.
- Preserve native current/previous bones and object transforms for object motion.
- Continue validating the scoped `MakeVelocityBuffer` correction now that its viewport-projection reset and setter
  boundary have been identified and jittered.
- Use the default-Off shared-signal **Unclamp Motion Vectors** option only as a diagnostic. A production implementation
  should remove the native packed-motion bias and approximately 64-pixel clamp on an owned temporal path instead.
- Keep native motion blur as a separate compatibility consumer until expanded vectors are proven safe.

### Reactive and transparency masks

FSR3 currently receives null external reactive and transparency/composition resources, which the AMD host maps to its
internal zero resource. Internal shading-change, prepare-reactivity, disocclusion, motion-divergence, and luma-instability
logic remains active. Add game-derived masks without replacing those mechanisms:

1. Identify material-stage signals for rain, particles, alpha blending, animated textures, reflections, and emissive
   transparency. Prefer proven material membership over final-image differences.
2. Start with the known `TppFxRain` raindrop path and confirm that its mask aligns with the native-resolution scene at the
   FSR3 insertion point.
3. Accumulate a bounded reactive mask and, where appropriate, a transparency/composition mask in owned full-resolution
   resources without modifying MGSV's original material targets.
4. Bind those resources through `FfxFsr3UpscalerDispatchDescription::reactive` and
   `FfxFsr3UpscalerDispatchDescription::transparencyAndComposition` only when their frame token, sample, and dimensions
   match the accepted color/depth/motion inputs.
5. Validate reduced ghosting without destabilizing foliage, thin geometry, opaque character motion, or exposure changes.

## Priority 5: native-resolution DLAA

After the canonical inputs are stable:

1. Add an isolated `dlss/module_hooks.hpp` for `nvngx_dlss.dll` discovery/export hooks and `dlss/runtime.hpp` for NGX
   parameters, feature ownership, evaluation, reset, and release. Do not mix NGX hooks with projection Detours.
2. Supply row-major no-jitter camera matrices and jitter separately in the SDK's documented units.
3. Validate motion direction, Y convention, scale, reverse-Z, reset, and frame-token lifetime with SDK diagnostics.
4. Establish a valid HUDless input and reintegration point before evaluating image quality.
5. Use auto exposure initially unless a proven MGSV exposure resource is available.
6. Add DLAA as one direct coordinator switch case; do not introduce a virtual method registry or another callback seam.
7. Compare analytical TAA, FSR3, and DLAA against the same validated inputs.

Add DLAA to the existing mode dropdown only when both `nvngx_dlss.dll` discovery and NVIDIA adapter capability are
probed. Keep the option visible but disabled when unavailable, with red hover text that distinguishes a missing DLL from
an unsupported GPU and can report both failures. Runtime selection must independently fail closed to Off. XeSS can use
the same option-level availability contract if it is integrated later.

No internal resolution changes, DLSS Super Resolution modes, or game viewport/culling modifications are planned.

## Optional finishing work

- Add luminance stability history for analytical shading changes and exposure transitions.
- Evaluate optional FSR3 RCAS only after wire stability, disocclusion, camera reset, and mask integration are working. The
  host creates the RCAS pipeline, but current MGSV dispatch sets `enableSharpening = false` and remains unsharpened.
- Continue comparing native-resolution FSR3 3.1.5 against the analytical resolve across the full validation matrix.

## Validation matrix

Every temporal-input or mask revision should cover:

- Static camera and geometry.
- Slow and fast camera pans.
- Slow character motion, idle animation, hair, clothing, and equipment.
- Rigid moving objects, foliage, fences, and thin wires.
- Aiming, first-person aim, binoculars, and abrupt FOV transitions.
- Pause/resume, cutscenes, camera cuts, and teleportation.
- DoF and motion blur enabled and disabled.
- Presents without a new full-resolution scene and render callbacks that straddle `Present`.
- Rain, particles, transparency, NoIR, sonar, reflections, and emissive effects.
- Resolution/display-mode changes.
- First frame after enable, resize, reset, camera cut, and failed input capture.

Required diagnostics should eventually include camera-only and object-only signed motion, motion validity, selected depth,
reverse-Z visualization, external mask coverage, disocclusion confidence, lock state, current jitter, and reset state.