# MGSV rendering structure

## Ownership

| File or directory | Responsibility |
|---|---|
| `addon.cpp` | Settings storage, assembled controls, ReShade events, and device cleanup |
| `shared.h` | The single CPU/HLSL settings layout |
| `settings.hpp`, `shadows/settings.hpp`, `reconstruction/settings.hpp` | Core, shadow, and reconstruction controls; Reset All and Preset Off policy |
| `shaders.hpp` | Shader registrations and draw/dispatch callbacks |
| `inputs.hpp` | Selected device/swapchain, resize/reset versions, and native material/motion references |
| `resource_upgrades.hpp` | Shared precision upgrades, clone activation, and scoped copies |
| `gtvbgi/lighting.hpp` | GT-VBGI textures, controls, input checks, GPU commands, and game-state restoration |
| `gtvbgi/` | Combined trace, depth reductions, AO application, GI addition, and [reference explanation](../gtvbgi/README.md) |
| `reconstruction/` | Analytical TAA, FSR3, DLSS, input preparation, submission, and SDK lifecycle |
| `output/` | Output selection, display state, and final SDR/HDR encoding |

Precision upgrades support the whole mod, including output, reconstruction, shadows, and GT-VBGI. Each feature keeps its own controls and textures. Feature headers are included at file scope and do not rely on hidden variables in the addon entry point.

## Lighting order

The original second skin-scattering pass (`0xB2601810`) runs once per intercepted invocation. Its callback then records these added lighting passes:

1. Check the game's depth, normals, diffuse source/destination, and saved material references.
2. If enabled, build four smaller depth levels that keep the nearest surface in each region.
3. Search geometry once, writing separate AO visibility and GI light/validity textures.
4. Bring AO to full resolution and multiply existing diffuse RGB, preserving alpha.
5. Bring GI to full resolution and add its RGB, preserving alpha.
6. Restore the game's compute and graphics bindings.

The game then multiplies diffuse lighting by surface color, adds specular lighting, applies fog, and runs its early tone curve. Custom lighting adds three compute dispatches per eligible callback, or seven with smaller depth levels. The callback count can vary, and dispatch counts do not measure GPU time. The first scattering pass uses a generated replacement; the second keeps the native shader with a callback. These passes spread light under skin and are separate from AO.

GT-VBGI needs all its inputs to be present and compatible. Missing inputs or setup failures skip custom lighting while preserving the original game pass. There is no separate AO-only shader. Current lighting does not use motion, keep frame history, or run a denoiser.

The game's diffuse-AO bypass follows the Screen-Space Lighting setting. If GT-VBGI is selected but custom lighting cannot run, native diffuse AO is still skipped. Native AO continues to affect specular and separate rain reflections. Custom AO currently darkens all accumulated native diffuse, including direct light; GI is added afterward. Applying occlusion separately to ambient lighting and environment reflections remains future work.

Native captures show that later passes also copy the diffuse-light buffer to measure brightness. Applying AO/GI only during final lighting composition would leave that brightness measurement reading different data unless it is updated too.

## Input and layout contracts

- Lighting uses the scatter pass's existing 64-byte geometry constants at compute `b0`. Pixel shaders use `b0` for different data.
- `ShaderInjectData` is 75 floats: 300 CPU bytes and 304 padded GPU bytes at `b13`. `lighting_mode` is at byte 236 and the noise index at byte 296. The shared SDK `PrepareConstants` layout is separately 96 bytes.
- Lighting has seven texture-input slots (SRVs) and four writable-output slots (UAVs). Unused outputs use distinct placeholder textures so they cannot conflict with inputs or each other.
- Normals use the game's packing in camera coordinates. Position reconstruction keeps signed projection scales, including negative X, and the game's camera-offset convention. Matching jitter under live rendering still needs verification.
- The albedo sRGB view decodes base color. Native diffuse/specular lighting is linear and already scaled by exposure. Copied diffuse alpha identifies skin-scattering materials, not every shaded pixel. GI alpha identifies valid results and remains separate from AO visibility.
- The first accepted Present selects one device/swapchain. Resize and destruction invalidate old resource versions. Resizing textures does not rebuild unchanged shader pipelines.
- Holding COM texture references keeps objects alive while the game can still overwrite their pixels. Matching sizes or context/Present identifiers alone cannot prove matching scene content, fresh writes, or execution order.

## Settings and reconstruction

Screen-Space Lighting appears under Indirect Lighting, uses Off/GT-VBGI, defaults to GT-VBGI, and keeps the saved key `FxLightingMode`. Both strengths and all sampling/source controls appear while GT-VBGI is selected. Preset Off disables lighting and the six shadow switches while leaving reconstruction selected. Saved presets are not rewritten to convert old settings.

Controls refer to addon-owned storage that must outlive their callbacks. Separate control lists still share the same renderer. Reconstruction uses equal render/output dimensions, and only the selected method runs. Canceling reconstruction must preserve the native commands recorded before and after its insertion. See the [reconstruction guide](../reconstruction/README.md).

Earlier explicit shader registrations take priority over generated entries. `on_inject=false` disables settings injection; replacement bytecode can still run if supplied. Deferred callbacks upgrade scene color before retaining material references. Preserve that order.

## Verification limits

The Desktop On/Off RenderDoc captures show native rendering order and inputs. They omit addon replacements and added passes even though the ReShade log from that session records addon activity. The reason is unresolved. These captures cannot validate custom execution, GPU cost, upgraded texture contents, or histories.

Recording a GPU command does not prove that it executed or completed. Local shader/API tests and bytecode comparisons check specific behavior; gameplay testing checks the actual image. Compare generated shader headers, compiled outputs, and the deployed addon, especially after settings-layout changes. Keep tests and analysis local and remove temporary test registration before building the standalone addon. Never build or deploy while the game is running.
