# Enhanced reflections

Enhanced replaces general and sortie-floor screen-space reflections with one GGX model and NRD denoising on D3D12. Diffuse Lighting handles diffuse AO/GI separately. Both features share material capture and scene ownership; reflections also reuse reconstruction's device, queue and GPU fences. Vanilla restores the original game path.

## Placement and inputs

Work runs before LocalReflectionAddBuffer (0xDC3A84C3) or LocalReflectionComposite (0x93B69040). The source contains visible sky, forward lighting and camera fog, without feeding the previous reflection result back into itself. Early deferred composition exports the replaceable environment light and receiver fog transmission. Supported early hashes are 0x410AE8C5, 0xC973024D, 0x2EA8F13F, 0x59B44963, 0xBDE1F4CD and 0x6E29F0AB.

| Input | Source and purpose |
|---|---|
| Albedo, diffuse, specular, depth | Deferred pixel t8–t11: native surfaces and accumulated lighting |
| Normal and material | Captured G-buffer views, including supported rain/decal writes |
| Clean material and owner | Base color without the material cube; exact surface qualification |
| Material palette | Associated with submitted specular; F0 tint and the second roughness input |
| Native scene constants | Actual bound range of deferred pixel b2: 480 bytes |
| Saved environment | Deferred output 1: linear specular already present; alpha is receiver fog transmission |
| Late scene and material | Reflection-compositor pixel t0/t2, matching the captured surface grid |
| Motion and camera | Same scene's velocity, object mask, depth, nonjittered matrices, jitter and reservation |

The late floating-point scene contains extended sRGB. Preparation decodes it once before sampling light. Albedo uses its native sRGB view. Normals, depth, motion and sampling weights receive no gamma conversion. RenoDX tonemapping keeps scene and environment in the same domain; unsupported early tone-curve paths do not qualify for replacement. See the [material contracts](../../materials/README.md) before changing interpretation.

## Follow the passes

1. **Prepare inputs.** `prepare_reflections.cs_5_0.hlsl` validates ownership and writes linear radiance, view normal/roughness, F0/reflection rate, reverse-Z depth and exact scene bytes. The same dispatch writes NRD motion, packed world normal/roughness and positive view Z. Invalid receivers lie outside NRD's denoising range. Shared allocations avoid full-resolution transfer copies and a separate guide pass.
2. **Build depth mips.** `reflection_mips.cs_6_6.hlsl` reduces to nearest reverse-Z depth, including odd edges. Source light stays at mip zero; ray directions already sample roughness.
3. **Trace specular light.** `sssr_trace.cs_6_6.hlsl` traces the selected number of stratified visible-GGX proposals per input pixel, defaulting to one. Each evaluates single and reciprocal multiple scattering over the matching GGX PDF; the broad scattering term is retained. Receiver data, the tangent frame, the noise lookup and the integrated response are shared by all rays. Samples use the original 2×2 pattern. Traversal uses reciprocal view depth and a finite depth slab. Backfaces block the environment but cannot supply color. In-view sky can supply visible sky light; unresolved rays use the saved environment approximation. Linear radiance is averaged by the actual sample count before packing. NRD's multi-path helper retains the nearest nonzero geometric hit for tracking; BRDF/PDF weights never change that distance.
4. **Denoise the complete estimate.** NRD REBLUR_SPECULAR receives nonnegative demodulated light and normalized first-bounce hit distance. Screen light and remaining environment are combined before official NRD packing. REBLUR retains its SDK defaults, including anti-firefly filtering. Hit-distance reconstruction is off: diffuse and specular are not selected probabilistically, and checkerboarding is not used. A GGX ray below the surface contributes zero light and zero hit distance, as NRD expects.
5. **Restore material response.** `sssr_resolve.cs_6_6.hlsl` decodes NRD and restores integrated GGX response and native reflection rate once. It uses original receiver roughness, not the filtered guide. Qualified zero-response surfaces are valid black; unavailable inputs preserve native environment.
6. **Compose once.** `composition.hlsli` restores receiver fog transmission and blends the replacement minus saved environment into the scene. Direct light, diffuse and fog in-scattering remain. The native draw still writes its own destination and alpha.

`sssr_brdf.cs_6_6.hlsl` builds a 128×128 table once per backend with 2,048 samples per texel. RG stores Schlick single-scattering coefficients; B stores their cosine-weighted average for unit Fresnel. Trace and resolve share it. Palette and G-buffer roughness follow the native contract, then the Blinn–Phong exponent is converted by matching the normal-incidence GGX peak. This is a documented fit, not a unique physical equivalence.

FAST noise is uploaded once as a shared atlas. Its 32 layers advance with scene camera samples, not Present. The saved native camera center includes projection jitter: position reconstruction subtracts that center, and projected lookups add it back. No second explicit jitter offset is needed. Noise selects reflection directions; jitter moves raster samples. The asset is labeled IS-FAST, but its exact generation settings remain unverified.

## Energy and limitations

Let `E` be integrated material response times native reflection rate, `T` receiver transmission, `B` saved environment, `w = BRDF * cosine / (PDF * integrated_response)`, `h` accepted screen confidence, and `V` remaining environment visibility. The demodulated estimate is `w * (h * screen_radiance + (1 - h) * B / (E * T) * V)`. Resolve restores `E`, composition restores `T`, and the correction is `strength * (replacement - B)`. Zero-response channels use zero fallback instead of division by zero.

Importance weights can exceed one; they are not clamped to coverage. The estimator preserves uniform light in expectation when source visibility and radiance are known. NRD's finite filtering and anti-firefly treatment can introduce bias. The raster pipeline is not an unbiased transport solution.

Lower input resolutions contain less surface detail and fewer reflection receivers. Curved glossy surfaces can still show slight movement artifacts after denoising, especially when magnified. More rays reduce sample noise without shortening history or changing material roughness; they do not recover geometry missing from the input.

Specular occlusion affects only remaining environment along confirmed geometry hits. It reuses the reflection hit test without another trace. Accepted screen light is not occluded twice, and diffuse AO is not multiplied into this different directional integral. Sky and unresolved directions retain environment; an unresolved ray proves neither a blocker nor an open path.

The saved native environment is integrated and shaded. Dividing by receiver response approximates constant incoming radiance; it cannot recover a directional probe. Missing outgoing color at a known blocker can become too dark. Screen color also contains camera fog, not a separately evaluated secondary-ray fog segment. Future environment/RT providers should supply incoming linear radiance. Hidden geometry still needs another scene layer or ray tracing.

The old sortie-floor shader 0xA621C684 traces the camera depth with a fixed floor plane and mirror direction; it does not render another camera. Enhanced uses GGX there too, without adding the native Fresnel, gain, roughness-mip bias or prefiltered mirror estimate afterward. This removes the inconsistent mixture and redundant native trace/pyramid. Vanilla keeps the native code.

## Ownership and reuse

`runtime.hpp` owns reflection resources and camera history. `rendering/nrd.hpp` loads the SDK and records its pipelines/pools through `rendering/d3d12_compute.hpp`. It creates no separate device, queue, interop backend or fence pair. Preparation, tracing, NRD and resolve use one bridge handoff; intermediate light and histories stay private to DX12.

The GPU scene block stores the view-to-world camera pose in rows; preparation uses that rotation for world normals. The CPU camera record stores column-major world-to-view and nonjittered projection matrices. MGSV's projection mirrors X, which confuses NRD's handedness detection. The adapter moves that reflection into the view matrix while preserving the combined projection and positive view Z. It also expresses both views relative to the current camera origin in double precision, preserving the previous camera's translation. This avoids losing subpixel motion at large world coordinates. World normals and UV motion keep their original coordinate system.

NRD receives sampling jitter in input pixels, with the opposite sign from our geometry projection shift. Its motion input is previous-minus-current UV without jitter. Static pixels use the matched camera transform; moving geometry uses native object motion with its current projection jitter removed once. Native previous-render matrices, reconstruction history and NRD's last submitted camera are separate records and must not be substituted without matching their scene endpoints.

Descriptors/uploads belong to retired bridge slots, which retain shared outputs and environment allocations until GPU retirement. NRD pool states persist across dispatches; external resources return to COMMON. Textures, pipelines and integration data are reused. Only the native material palette is retained or snapshotted; the unused custom Fresnel capture has been removed. Private passes upload shared constants once; adjacent NRD dispatches reuse identical constants.

History advances after successful submission for the same camera's next reservation. Skips, camera/viewport or generation changes, resize, visibility-strength changes and live shader reloads restart it. Replaying the latest matching view/result reuses output without advancing history or adding a handoff. Different or older views cannot borrow that history.

Missing/incompatible NRD or unmatched motion/camera ownership uses raw output with a warning. An SDK recording failure disables further NRD use while retaining allocations for safe retirement. Missing reflection inputs preserve environment light rather than expose a stale canceled native image.

Enhanced skips traces 0xD6714838, 0xBC4665A3, 0xB301C16D and 0xA621C684. Gaussian/reduction shaders 0x7B69DE2B, 0xE135BBD1, 0x051711F2 and 0xA8529A23, plus copies, are skipped only when descending from a canceled trace. Unrelated uses still run. Tracking belongs to command-list recording. GPU readiness has two states: unavailable (0) and enhanced complete (255).

## Controls and validation

Diffuse Lighting and Reflections have separate Vanilla/Enhanced controls because they run at different points and do not share a trace. Reflection Strength blends the complete replacement; it does not reduce work. Specular Occlusion Strength changes remaining environment and resets affected history. Both strengths are under Indirect Lighting (Advanced) and default to 100%. Enabled diffuse lighting is already in the reflection source.

Indirect Lighting lists Diffuse Lighting, Diffuse Lighting Resolution, Reflections, then Rays per Pixel. Rays per Pixel accepts 1–4 and defaults to 1. Lower values reduce trace work but increase noise and can make motion less stable; NRD and the other passes still run. The BRDF and expected reflected energy stay the same. Changing the count restarts reflection history once and prevents reuse of an output made with the old count.

Reflected Light shows the denoised estimate; Raw Reflections bypasses NRD for display while its history continues. Environment Weight can exceed one and remains raw. Reflection Coverage green means qualified inputs and a completed result, not a ray hit; magenta means unavailable inputs. Specular Visibility averages the sampled rays: black means confirmed geometry, white means sky/unknown directions, and gray means a mixture. NRD Validation shows upstream history, motion, normal and jitter diagnostics. Probe status counts NRD submissions and history restarts without image copies; queued work is not proof of GPU completion. The live probe atomically replaces preparation and all four custom DX12 stages; SDK shaders come from the DLL. Reload retires old GPU work and resets history. Normal gameplay does not compile shaders or watch files.

The development probe isolates NRD accumulation, stabilization and pre-pass filtering through live settings. These comparisons reset history and reuse the existing pipelines and pools; they are not gameplay controls and are not saved. Shorter history can reduce visible trailing without correcting an upstream defect, so it remains a diagnostic. The probe reports requested and submitted configuration revisions separately. Restore its `Default` mode after testing.

For camera audits, a bounded probe request records the original matrices, the native previous-render endpoint, the matrices passed to NRD and the SDK's generated constants. Native history is copied while the viewport is alive, rather than read through a retained address during later execution. Scene sample IDs connect these CPU records to the GPU preparation buffers. This distinguishes native history, reconstruction history and NRD history without changing any of them.

NRD is pinned to [4.18.0](https://github.com/NVIDIA-RTX/NRD/releases/tag/v4.18.0). The required interface files and shader packing configuration are bundled under [third_party/nrd](../../third_party/nrd/README.md), so building the addon does not require the full SDK or a repository submodule. Place the matching NRD.dll beside `mgsvtpp.exe` and retain the [NVIDIA RTX SDK license](../../third_party/nrd/LICENSE.txt). Other versions/packing are rejected. No NRD import library or NRI runtime is needed.

To build the runtime DLL yourself, obtain the pinned SDK and run [build_nrd.ps1](../../rendering/build_nrd.ps1) with `-SdkDirectory` pointing to that separate checkout from an x64 Visual Studio developer shell. The script also accepts an existing checkout under the ignored `build/dependencies/mgsv-nrd` cache. It builds a shared Release library with DXIL on, DXBC/SPIR-V off, NRI off, normal encoding 2, roughness encoding 1, and the upstream REBLUR quality mode. It checks the SDK interfaces and generated configuration against the bundled files. Output is `build/mgsv-nrd/bin/NRD.dll` and `NRD_LICENSE.txt`; `-BuildDirectory` can select another build folder. Deployment remains a separate step with the game closed.

Build `mgsv`, then check shader embeds. When using the local development probe, build and install its matching addon too. Local GPU fixtures cover guides, captured camera conventions, NRD dispatches, odd dimensions, reuse, replay and history resets with both API debug layers enabled. A separate camera regression checks the SDK's reprojection against double-precision projection during translation. Verify gameplay separately: ACC metal/skin, sortie floor, Mother Base rails/puddles, rain/decals, camera movement, Native/Auto and Vanilla/Enhanced. Compare raw, denoised and validation views. Measure fixed-camera frame times with probe capture idle and the overlay closed. Builds and synthetic checks do not establish visual quality or performance gains.

## References

- [Heitz, Sampling the GGX Distribution of Visible Normals (2018)](https://jcgt.org/published/0007/04/01/).
- [Filament rendering documentation](https://google.github.io/filament/main/filament.html): integrated response and reciprocal multiple scattering.
- [NRD integration guide](https://github.com/NVIDIA-RTX/NRD/tree/v4.18.0): packing, guides, REBLUR and validation panels.
- [AMD stochastic screen-space reflections](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/stochastic-screen-space-reflections/): hierarchical tracing reference.
