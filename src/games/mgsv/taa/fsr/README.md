# Experimental AMD FSR2 D3D11 Port

This folder contains an experimental, native-resolution D3D11/Shader Model 5 port of AMD FSR2 2.3.4. It is isolated from
the existing analytical MGSV TAA behind a reconstruction-method selector while its runtime behavior is validated. FSR2
is used when the reconstruction-method key is absent; the master TAA control itself remains default-Off.

## Source and license

The files under `vendor/FidelityFX` are copied from AMD FSR SDK tag [`v2.3.0`](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/v2.3.0),
resolved to commit `60f4ea81909200d8542eca14dccb2628b763a9a3`.
They retain AMD's per-file MIT notices. The applicable notice is also reproduced in [LICENSE-AMD.txt](LICENSE-AMD.txt).
No external repository, submodule, runtime DLL, or linked FidelityFX library is required.

Copied AMD files are kept unchanged. The port adds only
`vendor/FidelityFX/upscalers/fsr3/include/gpu/fsr2/spd/ffx_spd.h`, a forwarding include required by AMD's nested
luminance header. D3D11-specific entry points and configuration live under `shaders/`, so updates can otherwise be
compared against the pinned source without mixing MGSV adaptations into AMD's algorithm headers.

RCAS and reactive/TCR sources are retained for deferred sharpening and `TppFxRain` mask work. AMD pass wrappers replaced
by the MGSV SM5 entry points, debug-blit code, and the unused maximum-bias CPU table are not vendored.

## Port configuration

`shaders/fsr2_sm5_config.hlsli` selects:

- `cs_5_0`/DXBC and explicit D3D11 register bindings.
- FP32 math (`FFX_HALF=0`).
- SPD's groupshared fallback (`FFX_SPD_NO_WAVE_OPERATIONS=1`) instead of SM6 wave operations.
- Linear HDR accumulation, render-resolution motion, reverse-Z, unjittered motion vectors, and no RCAS.
- Approximate polynomial Lanczos for current-frame reconstruction and history reprojection. The history choice is an
  intentional performance/quality trade from AMD's reference trigonometric kernel and requires in-game A/B validation.
- Fixed native-resolution dimensions, normalized motion scale, unity exposure/pre-exposure, and an eight-phase sequence
  are specialized after AMD's callbacks while retaining their constant-buffer ABI.

`runtime.hpp` owns the D3D11 resource graph, linear clamp sampler, constant buffers, pass scheduling,
ping-pong history, reset/resize behavior, and encoded-scene copy-back. It dispatches:

1. MGSV color/motion preparation.
2. FSR2 luminance pyramid and auto exposure analysis.
3. Previous-depth reconstruction and motion/depth dilation.
4. Depth clipping and input-color preparation.
5. Thin-feature lock generation.
6. Temporal accumulation with fused exact MGSV output encoding.

The unsharpened AMD accumulation path invokes an MGSV `StoreUpscaledOutput` callback that sRGB-encodes final linear RGB
and restores current scene alpha. No RCAS pipeline is created or dispatched. Vendored RCAS source is intentionally kept
with the pinned SDK so optional sharpening can be evaluated later without restoring external dependencies.

Only the selected reconstruction method retains its large temporal resources. FSR2 resources are released when TAA is
disabled or analytical TAA is selected, and analytical history is released before FSR2 allocation. The output encoder
uses FSR2's final linear history value directly for exact sRGB encoding and copies current scene alpha, avoiding a
duplicate full-resolution linear-output texture, history read, and standalone dispatch.

MGSV's pre-DoF scene texture is already tonemapped and sRGB-encoded by deferred lighting. The input adapter decodes it
to a linear RGBA16F proxy and uses unity exposure/pre-exposure. The fused accumulation output callback restores the
encoded MGSV scene domain and copies current scene alpha, which downstream DoF/highlight processing still consumes.

FSR2 requires and always enforces the eight-phase Halton sequence, so the jitter-pattern selector is hidden while FSR2 is
active. The selector and zero-jitter **Off** diagnostic are available only with Analytical TAA. Large clip-space
reprojection discontinuities reset FSR2 history, and dispatch fails closed unless the scene target is copy-compatible
RGBA16F.

The analytical jitter preference remains stored while FSR2 forces Halton and is restored when users switch methods.

## UI naming

The selector identifies this path as **AMD FSR 2.3.4**. This documentation retains the explicit experimental D3D11/SM5
port disclosure because this is not AMD's supported D3D11 integration. AMD's current recommended public product name is
**AMD FSR Upscaling (non-ML)**; the explicit version remains useful while users compare it with the analytical TAA.

## Deferred reactive-mask work

The initial runtime selects a zero-input-mask depth-clip permutation. It folds out the 3x3 mask/color-similarity subtree
while preserving motion/depth divergence in FSR2's accumulation-mask channel. A compiled full-mask permutation remains
available for later `TppFxRain` raindrop reactivity and other proven alpha/particle classes. That work must select and
create/bind the full runtime path only when proven masks exist; it should not turn final-image differences into global
history rejection. The current zero-mask shader needs no dummy mask texture.

## Verification status

The `mgsv` target compiles and links seven `cs_5_0` entry points: preparation, luminance, reconstruction, optimized
zero-mask depth clip, full-mask depth clip, lock, and fused accumulation/output. In-game validation remains
required for resource creation, pass output, camera-cut thresholds, temporal stability, rain blur, device reset, and
resolution changes before the FSR2 default can be considered production-validated.
