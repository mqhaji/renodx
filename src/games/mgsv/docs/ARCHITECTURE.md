# MGSV rendering architecture

This guide covers ownership and rules shared by multiple features. The [addon guide](../README.md) is the user entry point; the [lighting](../lighting/indirect/README.md), [shadow](../lighting/shadows/README.md), [reconstruction](../reconstruction/README.md), and [bloom](../postprocess/bloom/README.md) guides own their algorithms, controls, and detailed tests.

## Ownership

| File or directory | Responsibility |
|---|---|
| `addon.cpp` | Settings storage, assembled controls, ReShade events, and device cleanup |
| `shared.h` | The single CPU/HLSL settings layout |
| `settings.hpp` and feature settings headers | Controls, Reset All, and Preset Off policy |
| `shaders.hpp` | Explicit shader registrations and draw/dispatch callbacks |
| `rendering/inputs.hpp` | Selected device/swapchain, reset versions, and retained native material/motion views |
| `rendering/producer_records.hpp` | Camera/exposure, motion, diffuse-copy, prepared-source, and material-table records published in submission order |
| `rendering/resource_upgrades.hpp` | Shared precision upgrades, resource roles, clone activation, and scoped copies |
| `rendering/d3d11_state.hpp`, `rendering/geometry.hlsli` | D3D11 state preservation and native geometry reconstruction |
| `materials/capture.hpp`, `materials/native/` | Clean material/owner capture, native ports, and material registration inventory |
| `materials/brdf.hlsli`, `materials/gtvbgi_brdf.cs_5_0.hlsl` | Native material decoding, GGX response, and one-time integration table |
| `lighting/native/` | Native direct/environment light ports |
| `lighting/shadows/` | PCSS, Bend kernels, per-light state, batch atlas, native light-group hooks, and controls |
| `lighting/indirect/runtime.hpp` | Combined resources, controls, input checks, scheduling, and dispatches |
| `lighting/indirect/ambient_diffuse.hpp` | Native ambient snapshots and diffuse-copy observation |
| `lighting/indirect/source_preparation.hpp`, `source_radiance.hlsli` | Shared outgoing diffuse radiance beside the native diffuse copy |
| `lighting/indirect/environment_specular.hpp` | Environment capture, material lookup tables, and scoped composition inputs |
| `lighting/indirect/trace/`, `denoise/`, `compose/` | Shared estimators, temporal/spatial filtering, diffuse application, specular results, and reflection handoff |
| `lighting/compose/` | Native deferred lighting/fog composition ports, including their early tone curves |
| `sky/native/`, `water/native/`, `effects/` | Existing sky/cloud, ocean, and wormhole/sand-wind ports, not new rendering models |
| `subsurface/`, `motion/`, `copy/` | Native skin scattering, velocity, and copy shaders |
| `reconstruction/` | Methods, camera/sizing, backend lifecycle, submission, and output-sized post-processing support |
| `postprocess/`, `postprocess/bloom/` | Native DoF/motion-blur ports and bloom |
| `tonemap/` | Two named late Tonemap/1DLUT shaders and shared curve/grading helpers |
| `output/`, `UI/` | Display state, SDR/HDR encoding, and native UI compatibility |

Feature headers are included at file scope and do not depend on hidden addon globals. Existing `mgsv::ao`, `mgsv::il`, and `mgsv::lighting::temporal_inputs` namespaces remain in use. Submission stays in `reconstruction/coordinator.hpp` because that code also evaluates the selected backend. Native material decoding and GGX remain together in `materials/brdf.hlsli`; indirect resources, controls, and orchestration remain together in `runtime.hpp`.

## Frame order

The main path is camera/jitter setup and resource upgrades, native geometry/material data and motion, native AO, ambient/direct lighting, diffuse copy and source preparation, custom indirect lighting, both native skin scatters, deferred composition/fog, sky/forward effects, native reflections, ocean/remaining effects, exposure preparation, reconstruction, bloom/DoF/motion blur, late grading, FXAA, UI, and output encoding. Some phases are interleaved or conditional; this is not one fixed list of contiguous passes for every scene.

Custom indirect lighting runs after native diffuse copy `0xD71E843E` and before horizontal skin scatter `0x6DAF0C4A`. The horizontal replacement and original vertical shader `0xB2601810` each run once. Updating native diffuse before these consumers also keeps later brightness measurements consistent. The [lighting guide](../lighting/indirect/README.md#native-inputs-and-placement) describes the individual producer bindings and application equations.

Enhanced Shadows changes direct lighting inside native light draws, before indirect lighting. Deferred composition applies the screen/environment reflection split before fog and the early tone curve. Native SSR then preserves its contributing detail. A tone curve inside a sky, ocean, or material shader does not make that shader a late post-processing pass.

Reconstruction's preferred insertion is before bloom preparation `0x72C1D157`. Geometry, lighting, and native SSR use render dimensions; later scene-color processing and UI use output dimensions. Routing checks the actual producer/resource role, not every use of a shared copy shader or every Draw2D shader. Native hardware depth/stencil and sampled geometry depth are separate consumers. See the [reconstruction contracts](../reconstruction/README.md#frame-and-resource-contracts).

## Shared layouts and color

- `ShaderInjectData` contains 79 floats: 316 CPU bytes and 320 bytes after GPU alignment at `b13`. Shader controls use its defines in `shared.h`. Effect-local copies carry scale, history flags, filter spacing, and light type without modifying user settings.
- Lighting reuses the native scatter pass's 64-byte geometry buffer at compute `b0`. Pixel `b0` has a different meaning. Temporal lighting uses current/previous native scene buffers at `b1/b2`; the reflected scene layout is 480 bytes. Preserve actual D3D11.1 buffer ranges when copying and restoring them.
- FSR/DLSS share a 96-byte preparation layout. Post-processing copy helpers use their own 32-byte layout, not a second settings buffer.
- Native albedo's sRGB view decodes color once. Diffuse/specular lighting and prepared outgoing diffuse are linear and pre-exposed. Later floating-point scene color may be encoded. Depth, normals, visibility, motion, ownership codes, and masks are data, not color.
- Feature guides own image formats, missing-data markers, binding slots, and filtering rules. Do not infer those contracts from texture format alone or reuse an output as an unused fallback input.

## Submission and lifetime

The first accepted Present selects one device/swapchain. Resize and destruction invalidate its resources and recorded versions. Texture-size changes rebuild images/views without rebuilding unchanged shader pipelines. Feature controls reference addon-owned storage for the lifetime of their callbacks.

Retained COM objects protect lifetime, not pixels. Native producers may overwrite retained textures and constants. Camera, motion, copy, prepared source, and table records become available in native submission order; CPU recording order or a recent Present cannot replace that association. Copy and material-table records do not advance lighting camera history.

The coordinator preserves native prefixes and remainders around custom work. Lighting consumes a diffuse-copy association once to avoid repeated application or history advancement on cached-list replay. Bend batches intentionally recompute masks on every replay from that execution's depth. Reconstruction cancellation fills the selected output from current native scene data before continuing the remainder. These different replay rules are deliberate.

Immutable material lookup views may be reused directly. Mutable palette/Fresnel inputs require ordered GPU snapshots on each producer/replay. Their metadata belongs to the native specular accumulation allocation. Readiness and actual-composition masks remain separate: preparing a result does not prove a deferred pixel used it. Native surface writes and clears reset them in GPU order.

Every feature restores displaced graphics/compute state, including empty bindings, shader class instances, render targets, depth/stencil, and D3D11.1 constant-buffer ranges. Release replaced views outside shared-map locks so destruction callbacks cannot reenter a held lock.

Explicit registrations precede generated entries and win duplicate keys. `on_inject = false` disables settings injection, not supplied replacement bytecode. Deferred callbacks activate the scene-color clone before capturing their native inputs; preserve that order.

## Verification limits

Native RenderDoc captures establish the inspected game's order and bindings but omit addon work. Completed probe readbacks and local API/shader fixtures verify specific custom operations and controlled cases. Neither a retained view nor a command-recording log proves GPU completion, matching scene contents, gameplay quality, or a performance gain.

The [addon build instructions](../README.md#build-and-deploy) own build/embed/deployment checks. Feature guides own visual checks; [reconstruction limitations](../reconstruction/README.md#known-limitations-and-validation-status) and its [roadmap](../reconstruction/ROADMAP.md) track open problems. Keep historical findings in the native references and local analysis rather than copying investigation history into implementation comments.