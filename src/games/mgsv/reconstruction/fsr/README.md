# FSR

MGSV loads AMD's FSR API on the same DirectX 12 device and queue used by DLSS and TAA. The addon contains the API headers and a small adapter, not an FSR implementation or shader bundle.

## Installation

Place these two matching DLLs beside `mgsvtpp.exe`:

- `amd_fidelityfx_loader_dx12.dll`
- `amd_fidelityfx_upscaler_dx12.dll`

Use the signed files from [`Kits/FidelityFX/signedbin` in AMD's FSR SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/tree/v2.4.0/Kits/FidelityFX/signedbin). No frame-generation, denoiser, or radiance-cache DLL is needed for upscaling. Restart the game after installing or replacing the files. The menu explains missing files; runtime failures are recorded in `ReShade.log`.

The menu calls this option **FSR**. We leave version selection to AMD: the SDK chooses the newest compatible provider and can use a newer driver-provided implementation. SDK 2.4.0 includes FSR 4.1.2 and FSR 3.1.6. Compatible AMD hardware can use FSR 4; other supported hardware uses a compatible older provider. The selected provider is logged when its context is created. We do not force a version based on the GPU vendor or bypass AMD's capability checks.

## Data and lifetime

1. Shared input preparation supplies linear HDR color, reverse-Z depth, and unjittered current-to-previous motion in UV units. The adapter supplies the pixel scale, camera range, field of view, frame time, and camera jitter to the DLL.
2. AMD owns its private history, intermediate textures, pipelines, and dispatch schedule. We use its default accumulation and shading-change behavior, automatic exposure, unit pre-exposure, and no optional sharpening or external masks.
3. FSR writes a private linear RGBA16F output. The shared encoder writes the game's encoded scene and current DoF alpha in the same command list. This adds no transfer copy or separate queue to the existing reconstruction route.
4. Switching away waits for submitted work and destroys the context and output. The DLLs stay loaded until device teardown; they are not dispatched while another method is selected. Returning to FSR creates a fresh context. Size changes and discarded recording also rebuild the context. Creation descriptors remain at stable addresses for each context's entire lifetime.
5. The adapter owns references to both DLLs by loading the upscaler before the loader borrows it. Device teardown destroys the context, unloads the loader, then releases the upscaler. It never removes another consumer's references. Static destruction does not unload DLLs under Windows' loader lock; if explicit cleanup cannot finish, Windows reclaims the remaining module references at process exit.

Device teardown catches failures at the ReShade callback boundary. If GPU completion or SDK destruction fails, cleanup stops with the remaining resources intact and reconstruction stops accepting work. The log records the failure and requests a restart. Successful teardown allows reconstruction to initialize on a new device.

## Source and license

The four headers in `sdk/` are unchanged public API headers from AMD's [FSR SDK 2.4.0](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v2.4.0). They retain AMD's MIT license. Their upstream directory structure preserves relative includes. The signed runtime binaries are distributed separately by AMD; consult the SDK's license before redistributing them. No import library or bundled FSR shader build is needed.

See AMD's [API guide](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v2.4.0/Kits/FidelityFX/docs/getting-started/ffx-api.md) for provider selection and DLL loading, and its [FSR 4 guide](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/v2.4.0/Kits/FidelityFX/docs/techniques/super-resolution-ml.md) for supported inputs and hardware models.
