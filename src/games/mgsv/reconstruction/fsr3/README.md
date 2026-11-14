# FidelityFX FSR3 Upscaler sources

## Provenance and versioning

The `ffx/` subtree is a source subset from AMD's official [`FidelityFX-SDK` v2.3.0 tag](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v2.3.0), commit [`60f4ea81909200d8542eca14dccb2628b763a9a3`](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/commit/60f4ea81909200d8542eca14dccb2628b763a9a3) (`AMD FSR SDK 2.3.0`, 2026-06-24). The bundled headers expose several independent version numbers:

- `ffx_fsr3upscaler.h` declares FSR3 Upscaler **3.1.5**. MGSV calls this component API through `ffxFsr3UpscalerContextCreate`, `ffxFsr3UpscalerContextDispatch`, and `ffxFsr3UpscalerContextDestroy`.
- `ffx_interface.h` declares backend interface **2.3.0**. The local D3D12 backend implements that ABI.
- `ffx_upscale.h` declares generic upscaler API **4.1.1**. AMD bundled that header in the same SDK release, but MGSV does not select the effect through the generic API/version loader; it does not change the active 3.1.5 component version.

“FSR3 Upscaler 3.1.5 from FSR SDK 2.3.0” describes both layers; the numbers are not interchangeable.

The sources retain AMD's MIT license headers. RenoDX-specific integration lives outside `ffx/`, except for these portability changes:

- public builds disable AMD-internal watermark and git-metadata dependencies;
- `ffx_core.h` resolves the vendored GPU support headers;
- MapleHinata's FXC-compatible reproject helper and explicit structure initialization avoid Shader Model 5 partial-`inout` failures and undefined out-of-screen sample data;
- the host uses the local D3D12 backend's resource-description callback rather than the SDK provider layer.

The original custom D3D11 backend followed the MIT-licensed backend design from MapleHinata/FidelityFX-SDK commit [`8138c9dc086154706643a03def91f3d01d391cd0`](https://github.com/MapleHinata/FidelityFX-SDK/commit/8138c9dc086154706643a03def91f3d01d391cd0) (`feat: update fsr3upscaler to 3.1.2`, 2024-10-30), ported to AMD's 2.3.0 interface and 3.1.5 host. MapleHinata's 3.1.2 effect is a design reference only; it is not the algorithm compiled here.

## MGSV integration

- AMD's 3.1.5 host owns the temporal pass schedule.
- `backend_dx12.hpp` implements the same host interface with D3D12 resources, explicit states, and per-submission descriptor/constant storage. The previous D3D11 backend has been removed.
- `shader_blobs.cpp` supplies fixed FXC `cs_5_0`/FP32 permutations for every host-created pipeline.
- `../input_capture.hpp` validates a game-native color/depth/motion/camera bundle before dispatch; copied metadata and retained views do not freeze GPU texture contents.
- `../prepare_reconstruction.cs_5_0.hlsl` writes decoded color, reverse-Z depth, and signed RG16F current-to-previous motion directly into shared textures.
- `../encode_reconstruction.cs_5_0.hlsl` runs on D3D12 after FSR and writes the encoded scene destination while preserving current-frame alpha.
- The common coordinator writes that encoded output to the selected scene target, restores compute/resource state, commits camera history, and advances the sample. Native quality uses equal render/output dimensions; reduced-resolution modes reconstruct into a separate output-sized scene. Sharpening is disabled, and frame generation is absent.

MGSV supplies no external reactive or transparency/composition mask. The host's internal shading-change, prepare-reactivity, disocclusion, motion-divergence, and luma-instability work remains active. The optional generate-reactive and RCAS pipelines are available because the host creates them, but normal MGSV dispatch does not call the generate-mask API and sets `enableSharpening = false`. The [roadmap](../ROADMAP.md) tracks game-derived masks; the [reconstruction guide](../README.md) owns settings, lifecycle, and testing.

No external FSR checkout, library, or DLL is required for this checkpoint. The planned FSR DLL migration is separate; the current host version, shader permutations, exposure behavior, and tuning remain unchanged.
