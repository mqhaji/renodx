# Material inputs and BRDF conversion

This folder records the cube-free color of the materials that bake a cube map into albedo, and translates the available specular parameters for enhanced reflections. It does not replace every native lighting model. Direct lighting, wrapped diffuse/translucency, skin scattering, water, glass still have their own native calculations. A normalized reflection BRDF alone does not make the entire frame physically based.

## Follow the data

1. **Load assets and select a technique.** Models supply material parameter arrays and textures. The CPU selects the shader technique, including applicable preblended-reflection variants. Terrain separately supplies material selection, weights, indices, and packed textures. The global material palette is a texture asset loaded by `ShaderTextures::SetMaterialTexture`.
2. **Pack technique-specific constants.** `ShaderTechnique::BuildPackedParameter` maps source parameter components to the technique's float4 registers. `MaterialResource::BindParameter` updates those mapped components. `CommandBuffer::SetMaterial` binds the selected shader, packs its constants into pixel b4, and binds its textures. A b4 offset has meaning only within that technique; it is not a universal material structure.
3. **Evaluate material shaders.** SRM textures, artist constants, layers, dirt, incidence color, and family-specific code produce the three native G-buffer outputs. Some material shaders also bake a local cubemap into their albedo output. These results are already processed; sampling the source SRM again would discard those adjustments.
4. **Record cube materials.** Most material shaders run as the game compiled them, and their albedo is already free of baked light. The 59 ports in `native/` are the shaders that add a local cube map to albedo. Each keeps its native outputs and also writes a record: the color it would have written without the cube, the three values it did write, and its depth. Later writers never update a record, and the G-buffer itself is never altered. `material_source.hlsli` decides what a record means. No record, or a record left by a surface that is no longer visible, means the visible albedo needs no correction. A record whose albedo still matches within one UNORM code supplies the cube-free color. A record whose albedo was changed afterwards, for example by a decal, leaves the cube's remaining share unknown, and that pixel keeps native reflections. A record belongs to the visible surface when its depth matches exactly, or, for a cube draw that does not write depth, when all three stored values still match. The two rain ports carry a matching record to the wet values. They read copies of the records made just before the rain draw, because that draw writes the originals.
5. **Decode the specular material once.** Reflection preparation reads the final native normal/material images and the palette associated with that scene's submitted specular allocation. `brdf.hlsli` produces RGB normal-incidence reflectance, isotropic GGX roughness, and the separate native reflection multiplier. Tracing and resolve consume those prepared values; they do not reinterpret the native channels independently.
6. **Keep lighting separate from material data.** The late scene is decoded from extended sRGB to linear incoming-radiance estimates. The saved environment image contains already shaded light, not raw probes or material parameters. Composition replaces its contribution once. See the [reflection pipeline](../lighting/reflections/README.md) for its fallback and visibility rules.

## Packed fields

The following are shader-visible RGBA channels, after any view conversion. BGRA readback byte order is different. The material and palette inputs are data: do not apply an sRGB decode to them. Native albedo is sampled through an sRGB view; captured clean color is linear.

| Input | Meaning and source | Use in the enhanced opaque reflection model |
|---|---|---|
| Material R | Final native roughness after the material shader and later wet/decal changes | One input to the native power-width conversion |
| Material G | Native specular/reflection multiplier | Applied once, separately from RGB F0; some special shaders bake a view/light-dependent adjustment here |
| Material B | Normalized 8-bit palette index | Rounded to the nearest entry in the 256-entry palette |
| Material A | Native translucency/material-specific output | Preserved for native lighting; not a metallic value or a general transmission BSDF |
| Normal RGB | View normal encoded as `(Nx/2+1/2, Ny/2+1/2, sqrt(Nz/2+1/2))` | Decoded for the reflection frame |
| Normal A | `CalcedReflectionTh`, often derived from SRM B and material-specific reflection code | Not used as a second roughness channel; it is not universal roughness |
| Palette row 0 RGB | Native `SPECULER_COLOR` | Specular tint multiplied by the authored F0 |
| Palette row 0 A | Native `TANSLUCENT` | Native wrapped diffuse/translucency; not specular roughness |
| Palette row 1 R | Native `F0` | Authored scalar normal-incidence reflectance |
| Palette row 1 G/B | Not read by the audited deferred direct-specular block | No physical meaning assigned by this conversion |
| Palette row 1 A | Native `ANISO_ROUGHNESS` | Second input to native specular width; must not be ignored |
| Native `g_lut1` | 64×64 Fresnel lookup | Native consumers retain it; enhanced GGX evaluates Schlick from authored F0 |
| Native `g_lut2` | 64×16×16 power lookup | Native direct-light consumers use both roughness inputs; its captured values inform the analytical conversion |

The ordinary `fox3ddf_blin` writer takes specular rate from SRM R, roughness from SRM G, and `CalcedReflectionTh` from SRM B. This is a producer-specific mapping, not permission to assume every shader's input textures or output alpha mean the same thing. Terrain can blend the SRM/material selection differently, and hair has additional directional work.

The native albedo and clean-color result are also not guaranteed to be intrinsic diffuse reflectance. Incidence tint, dirt, native diffuse allocation, baked lighting, and specialized shaders can alter them. Do not infer conductor/dielectric class or IOR from their brightness. The palette supplies a specular-color workflow; a physical metallic workflow would need an explicit material classification and conversion.

## What native lighting calculates

The direct-specular shaders read the palette tint, translucency, F0, and anisotropic-roughness value. They evaluate Fresnel from `g_lut1`, and sample `g_lut2` at half-texel-adjusted squared coordinates `(NoH², material_roughness², palette_aniso_roughness²)`. The returned R/G components are squared, then divided. This response is multiplied by specular rate, palette tint, light color, shadows, and a `saturate(4 * NoL)` error mask. Finite-light direction adjustments occur before the half-vector calculation. Native diffuse uses its separate wrapped/translucent `NoL` expression; it is not the Lambert term of our GGX model.

The captured power lookup is closely described by `p = 2^(12 - 11*r)`, `q = 2^(12 - 11*max(r,a))`, and `((sqrt(p*q) + 2) / 8) * NoH^q`, where `r` is final material roughness and `a` is palette row 1 alpha. This is a numerical reconstruction of the captured lookup, not recovered generator source. Quantization and trilinear interpolation make the actual table differ from the continuous formula. Its extra amplitude when `a > r` must not be copied into physical reflectance. Although the field is named anisotropic roughness, this deferred lookup receives no tangent or azimuth; its width and gain do not provide enough data for a true anisotropic GGX frame.

The captured Fresnel lookup agrees with endpoint-grid Schlick within one 8-bit code. The native shader's normalized texture coordinates and UNORM storage introduce interpolation/quantization differences. Enhanced reflections use `F0_rgb = saturate(palette_tint * palette_F0)` directly. They use RGB Schlick with a white grazing limit rather than multiplying a scalar Fresnel result by tint at every angle. This is a standard specular-color microfacet approximation, not recovered conductor optical constants.

Environment lighting uses another approximation: the sun/environment path samples a prepared directional atlas and applies a native roughness fade, Fresnel floor, palette tint, and artist gains. Material shaders can add their own local cube; rain composition can add a wet-ground cube independently of accumulated specular. Native SSR and the dedicated floor trace have additional Fresnel, distance, edge, mip, and gain controls. Those already shaded terms cannot be decoded back into unique incident radiance or a unique physical BRDF. Spherical harmonics supply diffuse environment lighting; they do not replace the missing directional specular environment.

## Chosen GGX conversion

The enhanced reflection model uses `r_native = max(material.R, palette_row_1.A)`, then `q = 2^(12 - 11*r_native)` and `r_ggx = (2 / (q + 2))^(1/4)`. Here the microfacet slope parameter is `alpha = r_ggx²`. This first normalizes the inferred native power lobe, then matches its NDF peak to isotropic GGX. It retains the native width relationship, including palette broadening, without importing the native lookup's extra gain.

Peak matching is an explicit conversion convention. Matching median scattering angle or half-width produces different valid approximations; none recovers ground-truth surface statistics from artist parameters. GGX has longer tails than Blinn-Phong, so it can produce broader visible reflection energy even with matched peaks. Enhanced reflections retain those tails rather than narrowing them to imitate the old SSR. An exact physical calibration would require surface measurements or authored GGX material data.

Zero RGB reflectance disables the interface in this model. A qualified surface with no interface produces a valid black replacement, including zero fallback weight; it is not treated as missing material data. The native rate remains a separate artistic multiplier; it is not folded into F0, roughness, or metallicness. Native direct lighting keeps its original material response. Matching all lighting to one BSDF later requires changing those consumers too, including diffuse/specular allocation, transmission, and special material classes.

Single scattering uses GGX with height-correlated Smith masking and Schlick Fresnel. The integration table stores its two Schlick coefficients plus the cosine-weighted average of the unit-Fresnel directional response. Kulla/Conty compensation supplies a reciprocal multiple-scattering lobe. Each ray samples visible GGX normals; its weight evaluates the complete BRDF, including the broad scattering term, divided by that proposal's PDF. Trace normalization and resolve use the same integrated response. No valid above-one importance weight is clipped. The compensation corrects microsurface energy loss; it is not another scene bounce or diffuse GI pass.

The RGBA16F integration table is built once and shared by trace and resolve. The complete lobe adds a directional table lookup and ray-weight math. It reuses the prepared inputs and outputs without adding a separate transport pass or API handoff.

## Shader families and exceptions

The November 15 audit scanned 1,589 shader assembly dumps. It identified 220 material G-buffer writers. The 59 that sample a material cube have ports in `native/`; the other 161 run unmodified. It compared material data flow across those families and the 17 direct-specular permutations. These counts describe the available dump, not every possible shader in the game. Registry membership is checked separately from live execution and visual quality.

| Writer family | Dump variants | Material behavior that matters |
|---|---:|---|
| Ordinary, layered, dirty, reflector, and base variants | 141 | Technique-specific constants; SRM/layer blends; optional material cube; dirty roughness and incidence-color changes |
| Terrain | 14 | Selection/weight/index textures and packed or virtual material sampling; consumes the final blended outputs |
| Skin and eyes | 12 | Incidence tint and family-specific roughness/rate; later native scattering is separate from a physical skin BSDF |
| Foliage, waves, and translucent variants | 20 | Alpha-tested coverage, vertex/material controls, and native translucency; opaque screen depth does not describe leaf transmission |
| Metallic-bacteria variants excluding hair | 9 | Specialized incidence/specular color and layer operations; not a generic metallic mask |
| Hair including metallic-bacteria hair | 3 | Tangent/light/view-dependent anisotropic modulation can already be baked into material G |
| Cloth | 4 | Family-specific layer/material operations; no separate physical cloth sheen lobe is recovered |
| Micro-normal/roughness variants | 5 | Extra normal/roughness processing before the final G-buffer write |
| Decals in the three-target writer set | 12 | Blend into albedo, normal, and material without writing depth; a changed albedo over a cube material leaves its record unknown; other decal/overlay passes have different target layouts |

Hair is a particularly important limit: the native writer evaluates a tangent-shifted directional specular factor using view and main light, then stores its result in the rate channel. That channel is no longer intrinsic reflectance. The current reflection model preserves it as native modulation; it cannot undo it from the final G-buffer. A true hair BSDF needs separate strand/tangent and absorption parameters. Likewise, leaf transmission, physical skin, layered water, glass refraction, and cloth sheen need explicit providers rather than guesses from palette IDs. Forward ocean/glass/effects are not made into opaque GGX receivers merely because their color appears in the reflection source.

Rain's two G-buffer variants, `0x2C230875` and `0xC3538951`, adjust roughness and albedo using CPU-bound wetness and interrupt inputs. They preserve the remaining material fields. Dirt and rust families also blend roughness, rather than merely changing color. Decals can alter albedo, normal, material, or only a subset through blending. Use the final native images; never reapply these adjustments from guessed constants in reflection preparation.

## CPU and shader evidence

These are retail addresses in the inspected executable, not the development executable addresses printed inside some database decompiles. Available function prefixes were compared with installed executable bytes. Development names/bodies help interpret the retail code; prefix matching alone does not verify every instruction or every runtime path. No CPU hooks were added for this material checkpoint.

| Retail function | Role |
|---|---|
| `0x140200200` / `0x140297D30`, `GrTools::SetMaterialTexture` / `ShaderTextures::SetMaterialTexture` | Select and load the global palette texture asset |
| `0x140297C00` / `0x140297BD0`, material-file loader / `Shader::GetMaterialParameters` | Load and expose the separate 8 KiB CPU material table |
| `0x140269990`, preblend-reflection technique selection | Checks model material IDs against that CPU table to select a VR technique; the CPU table is not a replacement for the GPU palette binding |
| `0x140268A00` / `0x140268B90`, `ShaderTechnique::BuildPackedParameter` | Pack components according to the selected technique |
| `0x14027A1C0` / `0x14027B020`, parameter binding / packed parameter retrieval | Update mapped material components and return the packed values |
| `0x14026C750` / `0x1402D7920`, model material initialization / command-buffer material binding | Associate model parameters/textures, select shaders, and bind b4 plus textures |
| `0x1402853C0` / `0x1402DB910`, packed terrain texture / material configuration | Load terrain inputs and map material selections |
| `0x140728180`, rain G-buffer update | Select the rain technique and bind wetness constants and the interrupt texture |

Representative consumer shaders are `SunLight_DL` (`0xB067B01D`), its CS/ES variants, `SSLighting2_Sun_LargestCascade` (`0x0C708B4B`), `SSLighting2_Point` (`0x80F0A0BC`), and `SSLighting2_Spot` (`0x08292A5E`). Their specular-capable variants use the three-input power lookup. NoSpec variants intentionally omit it. `SunLight_ES` (`0x34B04FB1`) exports environment specular, `SSLighting2_SH_SkyLight_Encode` (`0x4FBDBB39`) supplies diffuse sky lighting, and the two late reflection compositors consume already shaded scene/reflection inputs. The dedicated floor trace is `LocalReflectionTPP` (`0xA621C684`); it traces a floor-plane reflection against screen depth, not a separately rendered camera.

Local audit scripts, complete shader inventories, original lookup bytes, byte comparisons, and numerical results are under `build/analysis/mgsv/mgsv-material-calibration-20261115/` and `build/analysis/mgsv/mgsv-material-reflections-20261107/`. These ignored files are investigation evidence, not runtime dependencies. Refresh the inventory when new dumps or shader families appear. A missing permutation must be reported rather than silently assigned a guessed material layout.

## Validation and future changes

Validate packing and conversion against native inputs before judging a screenshot. Exercise both roughness inputs independently, colored and zero F0, invalid inputs, and cube materials under rain and decals. Test the BRDF separately with constant illumination, angular quadrature, reciprocity, grazing views, and sharp/rough limits. Then compare fixed-camera gameplay for coverage, alignment, expected material response, and performance. A denoiser cannot repair an incorrect material interpretation or estimator.

Future raster, RT, and PT providers should consume one explicitly defined material record, with raw parameters kept separate from already shaded fallback light. Add material class, tangent/sidedness, transmission, and measured/authored parameters when their producers are established. Share a BSDF and its sampling/PDF/integrals across those providers rather than translating the same native inputs differently for each backend. NRD and optional Ray Reconstruction operate on the resulting lighting signals; they do not determine the material model.

## References

- [Walter et al., Microfacet Models for Refraction through Rough Surfaces (2007)](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf): normalized microfacet distributions and their different shapes; GGX and Phong do not have an exact one-parameter correspondence.
- [Heitz, Sampling the GGX Distribution of Visible Normals (2018)](https://jcgt.org/published/0007/04/01/): visible-normal sampling.
- [Filament rendering documentation](https://google.github.io/filament/main/filament.html): GGX/Smith response and the reciprocal Kulla/Conty multiple-scattering construction from 2017.
