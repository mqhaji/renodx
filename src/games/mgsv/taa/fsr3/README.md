# FidelityFX FSR3 Upscaler sources

## Provenance and versioning

The `ffx/` subtree is a source subset from AMD's official
[`FidelityFX-SDK` v2.3.0 tag](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v2.3.0),
commit [`60f4ea81909200d8542eca14dccb2628b763a9a3`](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/commit/60f4ea81909200d8542eca14dccb2628b763a9a3)
(`AMD FSR SDK 2.3.0`, 2026-06-24). The bundled headers expose several independent version numbers:

- `ffx_fsr3upscaler.h` declares FSR3 Upscaler **3.1.5**. MGSV calls this component API through
	`ffxFsr3UpscalerContextCreate`, `ffxFsr3UpscalerContextDispatch`, and `ffxFsr3UpscalerContextDestroy`.
- `ffx_interface.h` declares backend interface **2.3.0**. The local D3D11 backend implements that ABI.
- `ffx_upscale.h` declares generic upscaler API **4.1.1**. AMD bundled that header in the same SDK release, but MGSV does
	not select the effect through the generic API/version loader; it does not change the active 3.1.5 component version.

Therefore “FSR3 Upscaler 3.1.5 from FSR SDK 2.3.0” is accurate, but `3.1.5` and `2.3.0` describe different layers rather
than one product version.

The sources retain AMD's MIT license headers. RenoDX-specific integration lives outside `ffx/`, except for narrowly
scoped portability changes:

- public builds disable AMD-internal watermark and git-metadata dependencies;
- `ffx_core.h` resolves the vendored GPU support headers;
- MapleHinata's FXC-compatible reproject helper and explicit structure initialization avoid Shader Model 5
	partial-`inout` failures and undefined out-of-screen sample data;
- the host uses the local D3D11 backend's resource-description callback rather than the SDK provider layer.

The custom D3D11 backend follows the MIT-licensed backend design from MapleHinata/FidelityFX-SDK commit
[`8138c9dc086154706643a03def91f3d01d391cd0`](https://github.com/MapleHinata/FidelityFX-SDK/commit/8138c9dc086154706643a03def91f3d01d391cd0)
(`feat: update fsr3upscaler to 3.1.2`, 2024-10-30), ported to AMD's 2.3.0 interface and 3.1.5 host. MapleHinata's 3.1.2
effect is a design reference only; it is not the algorithm compiled here.

## MGSV integration

- AMD's 3.1.5 host owns the temporal pass schedule.
- `backend_dx11.cpp` implements the required D3D11 interface, including the Feature Level 11_0 fallback.
- `shader_blobs.cpp` supplies fixed FXC `cs_5_0`/FP32 permutations for every host-created pipeline.
- `../runtime/input_capture.hpp` validates one immutable game-native color/depth/motion/camera frame before dispatch.
- `fsr3_prepare_game_inputs` decodes MGSV scene color to linear RGB and builds signed RG16F current-to-previous motion.
- `fsr3_encode_game_output` restores MGSV's encoded RGBA16F scene contract and preserves current-frame alpha.
- The common coordinator copies that encoded output back, restores compute/resource state, commits camera history, and
	advances the sample. Render and output extents are identical, sharpening is disabled, and frame generation is absent.

MGSV currently supplies no external reactive or transparency/composition mask. The host's internal shading-change,
prepare-reactivity, disocclusion, motion-divergence, and luma-instability work remains active. The optional
generate-reactive and RCAS pipelines are available because the host creates them, but normal MGSV dispatch does not call
the generate-mask API and sets `enableSharpening = false`. Planned game-derived mask integration is tracked in
[`../ROADMAP.md`](../ROADMAP.md).

No external checkout, SDK library, or runtime binary is required.
