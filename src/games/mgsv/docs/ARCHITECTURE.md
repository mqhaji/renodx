# MGSV rendering architecture

This guide covers ownership and rules shared by multiple features. The [addon guide](../README.md) is the user entry point; the [lighting](../lighting/indirect/README.md), [shadow](../lighting/shadows/README.md), [reconstruction](../reconstruction/README.md), and [bloom](../postprocess/bloom/README.md) guides own their algorithms, controls, and detailed tests.

## Ownership

The [render map](render-map.html) follows these owners through the native frame, added lighting, worker recording, reconstruction, and display. It includes reference capture positions and a compact post-processing shader inventory.

| File or directory | Responsibility |
|---|---|
| `addon.cpp` | Settings storage, assembled controls, ReShade events, and device cleanup |
| `shared.h` | The single CPU/HLSL settings layout |
| `settings.hpp` and feature settings headers | Controls, Reset All, and Preset Off policy |
| `shaders.hpp` | Explicit shader registrations and draw/dispatch callbacks |
| `rendering/inputs.hpp` | Selected device/swapchain, reset versions, and retained native material/motion views |
| `rendering/producer_records.hpp` | Camera/exposure, motion, diffuse-copy, prepared-source, and material-table records published in submission order |
| `rendering/native_context.hpp` | Native renderer/view ownership, worker handoff, and copied scopes for submitted addon work |
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

Native ownership follows each renderer build into its views and worker jobs. It records renderer and viewport lifetimes, active view membership, scene/output sizes, and copied worker subcontexts. A node generation identifies each use of the native command pool. Worker scopes restore their previous association on exit. The coordinator carries copied ownership with queued addon work through command-list submission and deferred merging. The local probe connects native recording and submission to its shader/resource inventory through command-list IDs and per-operation owner records.

Ownership tracking runs throughout the selected device's lifetime. One pinned addon owns the hooks and native allocations. The game addon and local probe exchange copied scopes through the shared addon API. Detailed probe observation starts on request and stops at a bounded Present interval. Unknown cached lists, missing worker associations, changed branch matrices, failed finishes, and capture limits have explicit counters. Presentation matching uses canonical COM identity, the supported ReShade unwrap interface, and an exact backbuffer-identity fallback. Native hooks require the verified executable timestamp, image size, and entry-point bytes.

Camera and motion capture, submitted lighting input pairs, reconstruction jobs, and post-processing routes carry the native scene ID and presentation generation. A worker serving a 512×512 rain capture cannot acquire the main view's enlarged output even when both use the same pooled scene texture. Immediate reconstruction and indirect-lighting dispatches restore the scope copied into their recorded job. UI without a scene can still use compatible presentation storage after its actual resource bindings are checked.

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

## D3D12 bridge

The game, DLSS, FSR3, and Analytical TAA still render through D3D11. The optional bridge sends the completed reconstruction output through a D3D12 copy shader before native post-processing continues. It preserves color encoding and alpha. Moving reconstruction itself is the [next checkpoint](../reconstruction/ROADMAP.md#next-d3d12-reconstruction-checkpoint).

| File | Responsibility |
|---|---|
| `rendering/shared_texture.hpp` | Own the exported texture handle and cache its D3D12 import |
| `rendering/d3d12_bridge.hpp` | Create the same-adapter device and queue; share images, record commands, synchronize the APIs, and reuse completed work storage |
| `rendering/d3d12_bridge_test.hpp` | Record the copy pass, compare returned pixels, and collect bounded timing samples |
| `rendering/bridge_copy.cs_5_0.hlsl` | Copy every color and alpha channel without conversion |
| `reconstruction/coordinator.hpp` | Call the diagnostic at the executed scene boundary; keep the ordinary D3D11 copy when it is disabled or rejected |

### Resource ownership

Producers export one NT handle per shared allocation and carry it with `ColorTarget`. The allocation caches its D3D12 import, while each command slot retains the images it uses. Rotating scene outputs therefore does not reopen textures or depend on the command pool's size. Reconstruction's encoder outputs, Analytical TAA history, and the scaled scene destination are shareable; private SDK intermediates remain D3D11-only.

The bridge accepts separate, equally sized, single-sample, single-mip RGBA16F or RGBA32F images. A compatible shared input or destination is used directly. An unshared native image needs a shared intermediate and a copy at that boundary. Sharing exposes an allocation to both APIs; it does not copy, convert, resize, or synchronize its contents.

Device checks resolve ReShade's proxy and native D3D11 interfaces to the same COM identity. The bridge creates D3D12 on the game's adapter and retains the returned device. It uses the downstream interface only when the outer device is confirmed to be ReShade's published proxy, keeping private commands out of game callbacks. Other wrappers remain intact. MGSV settings injection applies only to D3D11 shader layouts.

### Submission order

The coordinator serializes the bridge with the D3D11 immediate context. The executed scene ID and presentation generation accompany each dispatch. Deferred contexts, foreign devices, missing scene identity, and incompatible images are rejected before submission.

1. Select a slot whose previous D3D11 and D3D12 uses have completed. Create or update its images and descriptors as needed. The pool holds at most four slots; if all are busy, keep the ordinary D3D11 copy instead of waiting.
2. Record and close the complete D3D12 command list before submitting work. The callback borrows its objects only for recording, returns shared images to `COMMON`, and keeps any additional resources alive through execution. A recording failure creates no queue dependency.
3. Copy an unshared source into its shared intermediate if needed, then signal the D3D11 fence. Queue a D3D12 wait before executing the recorded commands.
4. Signal the D3D12 fence after execution. Queue a D3D11 wait before native post-processing reads the result. With a shared destination, the result is already in place. Otherwise, copy it back and signal the D3D11 fence again to protect the intermediate until that copy finishes.
5. Reuse commands, descriptors, and image storage only after their completion values have passed. A partial submission failure disables the diagnostic and retains potentially active resources.

Each API signals its own timeline through RenoDX's [GPU fence helper](../../../utils/gpu_fence.hpp). Normal rendering uses GPU waits and nonblocking completion checks, without a CPU wait or extra flush. The game submits its D3D11 stream; standalone callers must submit it before waiting for completion. Explicit teardown may flush and wait outside `DllMain`.

### Diagnostics and validation

**D3D12 Bridge Test** is in Temporal Anti-Aliasing (Advanced), default Off.

| Mode | Behavior |
|---|---|
| Off | Return before locking, polling queries, or creating a D3D12 device. Existing bridge resources remain alive until teardown. |
| Copy | Run the identity shader with the same color and alpha. |
| Verify | Also compare source and returned pixels after GPU completion, once per output size or presentation generation and when re-enabled. |
| Profile | Warm up for 32 successful submissions, measure 64 samples, log the result, then continue copying without new measurements. |

The status and log report command slots, imports, transfer copies, busy skips, image checks, timings, and failures. Profile separates CPU preparation, recording, and queue calls from GPU dispatch and total handoff time. These intervals overlap and must not be added together as frame cost. Readbacks and query results are consumed without blocking normal rendering.

Local real-API tests cover direct sharing, copy fallbacks, changing sizes, overlapping submissions, pool limits, replay, rejected inputs, and teardown. Live ACC checks returned identical 4K pixels with no transfer copies on the shared route. This establishes the tested color round trip, not a reconstruction speedup or complete gameplay coverage.

For a frame-time comparison, use the same camera, quality settings, and probe state; close the overlay and compare Off / Copy / Off without alt-tabbing. Keep builds, captures, reloads, and profiling idle. The diagnostic still adds a measured handoff cost. Future passes should share one submission where dependencies allow it and return before the next native consumer. The [roadmap](../reconstruction/ROADMAP.md#d3d12-rendering-and-frame-generation) owns those migration plans.

## Verification limits

Native RenderDoc captures establish the inspected game's order and bindings but omit addon work. Completed probe readbacks and local API/shader fixtures verify specific custom operations and controlled cases. Neither a retained view nor a command-recording log proves GPU completion, matching scene contents, gameplay quality, or a performance gain.

The [addon build instructions](../README.md#build-and-deploy) own build/embed/deployment checks. Feature guides own visual checks; [reconstruction limitations](../reconstruction/README.md#known-limitations-and-validation-status) and its [roadmap](../reconstruction/ROADMAP.md) track open problems. Keep historical findings in the native references and local analysis rather than copying investigation history into implementation comments.
