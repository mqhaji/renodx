# MGSV Temporal Reconstruction Roadmap

Future work for improving the current analytical TAA, validating the experimental native-resolution FSR2 D3D11 port,
and preparing reusable native-resolution DLAA inputs. [README.md](README.md) describes the current implementation.

## Scope

- Keep all temporal features default-Off until their input and restoration contracts are validated.
- Preserve native render/output resolution. Game resolution scaling and dynamic resolution are out of scope.
- Target DLAA specifically, not DLSS Super Resolution.
- Keep MGSV's bone-aware object motion rather than replacing it with depth-only camera motion.
- Do not restore broad mapped-constant-buffer mutation.
- Keep the manually adapted FSR2 SM5 path isolated and experimental until its complete D3D11 pass/resource contract is
  proven in game; do not represent it as AMD's supported D3D11 integration.
- FSR2 is the reconstruction fallback when no method key is persisted, including configurations predating the selector.
  This does not replace runtime quality, reset, resource, and restoration validation. The master TAA control remains Off.

## Priority 1: signal correctness

1. **Linear history reconstruction**
   - Per-texel decode before 16-tap Catmull-Rom is validated and now preserves correct linear-light reconstruction.
   - Store temporal history in linear RGBA16F to recover optimized Catmull-Rom sampling, then encode only the copy written
     back into MGSV's scene domain.
2. **Camera-cut reset**
   - Detect large view/projection/FOV discontinuities and invalidate history before accumulation.
   - Cover aiming, binocular transitions, cutscenes, teleportation, pause/resume, and display-mode changes.

## Priority 2: modern history validation

1. Store or reconstruct previous-frame depth.
2. Compare reprojected expected depth against observed previous depth.
3. Produce a disocclusion confidence value rather than relying exclusively on color bounds.
4. Reject history at true surface changes while preserving it on depth-consistent geometry.
5. Move rectification from raw RGB toward a luminance/chroma representation once linear history is established.

Do not globally weaken the current AABB. Without depth validation, relaxed clipping is expected to reintroduce character
ghosting.

## Priority 3: optional thin-feature stability

Selecting the largest raw reverse-Z depth fixed the observed disappearing-wire failure by keeping nearest-surface motion
at thin foreground geometry. Add temporal locks only if other thin features still fail under intermittent jitter coverage.

An FSR2-inspired native-resolution lock should track:

- Thin-feature detection from a local luminance neighborhood.
- Reprojected lock lifetime.
- Luminance recorded when the lock is created.
- Trust based on current shading stability.
- Immediate or accelerated unlock on disocclusion, camera reset, or material instability.

Locks should selectively protect depth-consistent thin detail. They should not become a global increase in history weight.

## Priority 4: canonical temporal inputs

Create a consumer-independent input layer shared by custom TAA and DLAA:

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

### Deferred masks

Reactive and transparency/composition masks are useful for particles, alpha blending, reflections, and animated textures,
but they should not block the required color/depth/motion/reset contract. Material-derived masks are preferable to
final-image heuristics when they become necessary. The FSR2 port retains both its optimized zero-input-mask permutation
and a full-mask permutation; connect a proven `TppFxRain` raindrop mask by selecting the full path only on affected frames.

## Priority 5: native-resolution DLAA

After the canonical inputs are stable:

1. Integrate Streamline/DLAA with identical render and output extents.
2. Supply row-major no-jitter camera matrices and jitter separately in the SDK's documented units.
3. Validate motion direction, Y convention, scale, reverse-Z, reset, and frame-token lifetime with SDK diagnostics.
4. Establish a valid HUDless input and reintegration point before evaluating image quality.
5. Use auto exposure initially unless a proven MGSV exposure resource is available.
6. Compare custom TAA and DLAA against the same canonical inputs.

No internal resolution changes, DLSS Super Resolution modes, or game viewport/culling modifications are planned.

## Optional finishing work

- Add reactive/transparency handling for proven material classes.
- Add luminance stability history for shading changes and exposure transitions.
- Add optional RCAS only after wire stability, disocclusion, and camera reset are working. RCAS source remains vendored,
  but the current runtime creates no RCAS pipeline and produces unsharpened output.
- Validate the default native-resolution FSR2 2.3.4 SM5 adaptation against the analytical resolve. Its source-only D3D11
  scheduler must remain local and must not introduce an external SDK link.

## Validation matrix

Every temporal-input revision should cover:

- Static camera and geometry.
- Slow and fast camera pans.
- Slow character motion, idle animation, hair, clothing, and equipment.
- Rigid moving objects, foliage, fences, and thin wires.
- Aiming, first-person aim, binoculars, and abrupt FOV transitions.
- Pause/resume, cutscenes, camera cuts, and teleportation.
- DoF and motion blur enabled and disabled.
- Presents without a new full-resolution scene and render callbacks that straddle `Present`.
- Rain, particles, transparency, NoIR, and sonar.
- Resolution/display-mode changes.
- First frame after enable, resize, reset, camera cut, and failed input capture.

Required diagnostics should eventually include camera-only and object-only signed motion, motion validity, selected depth,
reverse-Z visualization, disocclusion confidence, lock state, current jitter, and reset state.