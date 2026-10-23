# MGSV pixel shader name map (retail 1.0.15.4, DX11)

Produced from a live `renodx-devkit` snapshot of a running open-world frame: 88 tracked `ps_5_0`
pixel shaders, each resolved to its original source filename.

Method: the DXBC embeds `#line` directives pointing at the original build path, e.g.

```
#line 101 "d:\work\master\git_clone\_intermediate\win64\GrModelShaders_dx11_win64\Release\SunLight_DL_CS_ES_ps.hlsl"
```

`devkit_get_shader` with `view: "disassembly"` returns the DXBC disassembly including the buffer
definitions, resource binding table with real names, input/output signatures, and 3Dmigoto-style
source variable annotations.

## Naming conventions observed

| Token | Meaning |
|---|---|
| `DL` | directional light |
| `CS` | cascade shadow |
| `ES` | environment / sky specular term |
| `VF` | vertex-fog variant |
| `NS` | no specular |
| `TA` | texture array? (paired with `LitDP1`) |
| `blin_` | blended / material-layer family |
| `NShdw` | no shadow |
| `NoSpec` | no specular |
| `Mask` | light mask |
| `NA` | (local light family, see engine notes) |
| `SS` | screen-space |
| `SH` | spherical harmonics |

## Pixel shaders

### Deferred shading / lighting

| CRC32 | Source |
|---|---|
| `0x5AFD2E17` | `SunLight_DL_CS_ES_ps.hlsl` — **sun light with cascade shadow (PCSS target)** |
| `0x712F7222` | `SunLight_DL_ES_ps.hlsl` — sun light, no cascade shadow |
| `0xB7F05D55` | `TPPSunLightReflectMap_ps.hlsl` |
| `0x4C4D58DA` | `SSLighting2_SH_MultiBlend_ps.hlsl` |
| `0x4FBDBB39` | `SSLighting2_SH_SkyLight_Encode_ps.hlsl` |
| `0xC82BC086` | `SSLighting2_Stencil_ps.hlsl` |
| `0x08292A5E` | `SSLighting2_Spot_ps.hlsl` — **spot light with shadow (PCSS port target)** |
| `0xBF83EEDD` | `NewTerrain_PackedMaterial_Near_ps.hlsl` |
| `0x907CC675` | `NewTerrain_Decal_ps.hlsl` |
| `0x4710870E` | `NewTerrain_Depth_ps.hlsl` |

### Shadow map producers (depth-only, zero resource bindings)

| CRC32 | Source |
|---|---|
| `0xAD5F619E` | `ModelForward_ShadowOpaqueRigid_ps.hlsl` |
| `0xD2FBC7B0` | `ModelForward_ShadowOpaque_ps.hlsl` |
| `0x996CF0BB` | `ModelForward_Shadow_ps.hlsl` |
| `0xD68FF97A` | `NewTerrain_Shadow_ps.hlsl` |

### Forward-lit primitives

| CRC32 | Source |
|---|---|
| `0x07EFBA51` | `Prim_Poly_FastDraw_DL_VF_ps.hlsl` |
| `0x94C2F79B` | `Prim_Poly_Softblend_DL_VF_ps.hlsl` |
| `0xA21162DF` | `Prim_Poly_LitDP1_NS_VF_ps.hlsl` |
| `0xB170995D` | `Prim_Poly_LitDP1TA_NS_VF_ps.hlsl` |

### Character / model materials (`fox3ddf_*`)

| CRC32 | Source |
|---|---|
| `0x003B5D88` | `fox3ddf_blin_3mt_vr_ps.hlsl` |
| `0x01B475B6` | `fox3ddf_skin_tension_dirty_ps.hlsl` |
| `0x138B22C7` | `fox3ddf_blin_layer_sbl_mu_ps.hlsl` |
| `0x235FDC90` | `fox3ddf_blin_dirty_ps.hlsl` |
| `0x28A98126` | `fox3ddf_blin_lym_subnm_nu_ps.hlsl` |
| `0x37E0E714` | `fox3ddf_eye_vr_ps.hlsl` |
| `0x3DF253C3` | `fox3ddf_blin_ly_sbl_2snm_nu_ps.hlsl` |
| `0x44C6EED8` | `fox3ddf_skin_dirty_ps.hlsl` |
| `0x44D87FFC` | `fox3ddf_hair_ps.hlsl` |
| `0x48164FE1` | `fox3ddf_translucent_nc_ps.hlsl` |
| `0x49078681` | `fox3ddf_blin_decal_ps.hlsl` |
| `0x4E86D24D` | `fox3ddf_blin_subnorm_ps.hlsl` |
| `0x5333C257` | `tpp3dfw_constant_sky_ps.hlsl` |
| `0x558D8C92` | `fox3ddf_fast_wave2_clone_ps.hlsl` |
| `0x56463CB6` | `fox3ddf_blin_nc_ps.hlsl` |
| `0x6D7FFA5B` | `fox3ddf_blin_clone_ps.hlsl` |
| `0x778852F2` | `fox3ddf_blin_4mt_ps.hlsl` |
| `0x87B1F3BA` | `fox3ddc_blin_layer_mul_nu_ps.hlsl` |
| `0x91C206BD` | `fox3ddf_blin_subnorm_nu_ps.hlsl` |
| `0x9719E840` | `fox3ddf_blin_ps.hlsl` |
| `0x993E9F08` | `fox3ddf_skin_ps.hlsl` |
| `0x9C55B6E3` | `tpp3ddf_MetalicBacteria_vr_ps.hlsl` |
| `0xADE574B1` | `fox3ddf_skin_vr_ps.hlsl` |
| `0xB2607488` | `fox3ddf_blin_2mt_dirty_ps.hlsl` |
| `0xBA5B88A7` | `fox3ddf_blin_incidence_ps.hlsl` |
| `0x22DC672A` | `tpp3dfw_generativeClouds_ps.hlsl` |

### G-buffer / velocity

| CRC32 | Source |
|---|---|
| `0x9815404F` | `GBufferVelocity_ps.hlsl` |
| `0x58C10658` | `GBufferMaskedVelocity_ps.hlsl` |

### Sky / reflections

| CRC32 | Source |
|---|---|
| `0x9EC71292` | `Sky_Draw_TppBaked_ps.hlsl` |
| `0xD6714838` | `LocalReflection_ps.hlsl` |
| `0xDC3A84C3` | `LocalReflectionAddBuffer_ps.hlsl` |
| `0x77E6EA19` | `LocalReflectionMask_ps.hlsl` |
| `0x0E691525` | `SH_SphereMap_ps.hlsl` |
| `0x1521085A` | `SH_FresnelMap_ps.hlsl` |

### Post-processing

| CRC32 | Source |
|---|---|
| `0xE04D1471` | `Tonemap_ps.hlsl` |
| `0x2EA8F13F` | `DR_VolFog_TppTonemap_ps.hlsl` |
| `0x463195C2` | `TonemapBrightpass_ps.hlsl` |
| `0x96BE4659` | `Tonemap_Lum_Lacc_ps.hlsl` |
| `0x1FF96E86` | `GammaControl_ps.hlsl` |
| `0x900968FF` | `Fxaa.hlsl` |
| `0xCA7F0E3D` | `ColorCorrection_BlndLut_Tex_Tex_ps.hlsl` |
| `0xBFC7D3C2` | `MotionBlurMcGuire_ps.hlsl` |
| `0x260C371F` | `MotionBlurTile_ps.hlsl` |
| `0xF05DCBFD` | `MotionBlurTileNeighbour_ps.hlsl` |
| `0x512E2B48` | `MotionBlurBakeVelocityDepth_ps.hlsl` |
| `0xA13321B6` | `MotionBlurCameraVelocity_ps.hlsl` |
| `0xE26983EE` | `DOF_ScatterGather1_ps.hlsl` |
| `0xE2D609B1` | `DOF_ScatterCompositeNear_ps.hlsl` |
| `0x7C017264` | `DOF_ScatterCompositeFar_ps.hlsl` |
| `0xFC5542BB` | `DOF_ScatterCompositeFinal_ps.hlsl` |
| `0xD05F5ABC` | `DOF_ScatterSprite_ps.hlsl` |
| `0x490E22EB` | `DOF_ScatterBakeSecond_ps.hlsl` |
| `0xFE1DC3F8` | `DOF_ScatterBakeFirst_ps.hlsl` |
| `0x83CAA522` | `DownSampleDepth_ps.hlsl` |
| `0x051711F2` | `HalfDownSample_ps.hlsl` |
| `0x72C1D157` | `CopyReductionX4_ps.hlsl` |
| `0x83272BCB` | `CopyRenderBuffer_ps.hlsl` |
| `0x4B3EDCC3` | `CopyDepthBuffer_ps.hlsl` |
| `0xD71E843E` | `CopyBuffer_ps.hlsl` |
| `0x844CE327` | `BilateralUpscale2x2_ps.hlsl` |
| `0x7B69DE2B` | `blur_Gaussian1D_X_ps.hlsl` |
| `0xE135BBD1` | `blur_Gaussian1D_Y_ps.hlsl` |
| `0x9BE51873` | `blur_kawase_ps.hlsl` |
| `0x6966F759` | `blur_box2D_ps.hlsl` |
| `0x1AC1A5EF` | `SSLensFlarePass1_ps.hlsl` |
| `0x265053A6` | `SSLensFlarePass2_ps.hlsl` |

### UI / 2D / utility

| CRC32 | Source |
|---|---|
| `0x2DD30911` | `Draw2D_Ui2_BL_ps.hlsl` |
| `0xEF836D9D` | `Draw2D_ps.hlsl` |
| `0x64B81A22` | `Stencil_Dummy_ps.hlsl` |

### Unresolved

| CRC32 | Note |
|---|---|
| `0x967994F1` | `ps_4_0`, 660 bytes, no `#line` directive — trivial blit/clear shader |

## Compute shaders tracked

| CRC32 | Size | Binds | Role |
|---|---|---|---|
| `0x6DAF0C4A` | 5868 | 5 | Subsurface scatter, horizontal sweep |
| `0xB2601810` | 6344 | 6 | Subsurface scatter, vertical sweep + translucency blend |

Both are the screen-space subsurface scatter chain, not ambient occlusion; see
`ao/MGSV_AmbientOcclusion.md`.

## Notes for future work

- Local-light shadow receivers are **not** in this map because no shadowed local light was visible in the
  captured scene. Engine code (`DgPluginLocalLightDx11.cpp:1456-1504`) enumerates 24 named variants:
  `SSLighting2_Point_NoShadow`, `SSLighting2_Spot_NoShadow`, `SSLighting2_Spot_Mask_NShdw`,
  `SSLighting2_Point_NoSpec_NoShadow`, `SSLighting2_Spot_NoSpec_NoShadow`,
  `SSLighting2_Spot_Mask_NSpec_NShdw`, `SSLighting2_Point`, `SSLighting2_Spot`, `SSLighting2_Spot_Mask`,
  `SSLighting2_Point_NoSpec`, `SSLighting2_Spot_NoSpec`, `SSLighting2_Spot_Mask_NSpec`,
  `Lighting_Point_NA_NShdw`, `Lighting_Spot_NA_NShdw`, `Lighting_Spot_NA_Mask_NShdw`,
  `Lighting_Point_NA_NoSpec_NShdw`, `Lighting_Spot_NA_NoSpec_NShdw`, `Lighting_Spot_NA_Mask_NSpec_NShdw`,
  `Lighting_Point_NA`, `Lighting_Spot_NA`, `Lighting_Spot_NA_Mask`, `Lighting_Point_NA_NoSpec`,
  `Lighting_Spot_NA_NoSpec`, `Lighting_Spot_NA_Mask_NSpec`, plus `SSLighting2_ComplexityView`.
- Vertex shaders and the two compute shaders were not name-mapped; the same method works on them.
