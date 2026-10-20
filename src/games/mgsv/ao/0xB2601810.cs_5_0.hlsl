// MGSV screen-space subsurface scatter, stage 2 - CRC 0xB2601810, cs_5_0.
//
// Vertical sweep, then the translucency blend. The retail pass is dispatched
// transposed relative to stage 1: groups on the row axis, threads on columns, so
// this shader's vThreadID.y is the column and vThreadID.x is the row.
//
// This is a 1:1 port of the retail DXBC; see ssss.hlsli for the algorithm and
// the bindings. Nothing is injected and nothing is tuned.
//
// The tail is the retail translucency term:
//
//   NdotV        = dot(N, V)
//   k            = 2 + 1 / (NdotV + 1e-9)
//   translucency = saturate(cTweakParams.y * (1 - k * (0.15, 0.18, 0.20)))
//   mixed        = translucency * diffused + (1 - translucency) * original
//   output       = lerp(original, mixed, cFade)
//
// so the diffusion is strongest where the surface faces the camera and falls to
// nothing at the silhouette. The per-channel constants make the term slightly
// stronger in blue than red, matching the warmer band tint of the sweep.

#include "ssss.hlsli"

Texture2D<float4> lightOriginal : register(t4);
RWTexture2D<float4> resultBuffer : register(u0);

[numthreads(128, 1, 1)]
void main(uint3 gid : SV_GroupID,
          uint3 tid : SV_DispatchThreadID,
          uint gi : SV_GroupIndex) {
  const uint2 pixel = tid.yx;
  const TileWindow window = CacheTile(pixel, gid, gi, true);
  GroupMemoryBarrierWithGroupSync();

  // Retail drops threads past the buffer height (the row axis for this pass).
  if (pixel.y > cBufferHeight) return;

  // Unshaded pixel: the retail pass returns without storing anything.
  const float4 centre = gTileLight[window.centerSlot];
  if (centre.w == 0.0) return;

  float ndotv;
  const float3 diffused = SweepDiffusedLight(
      pixel, gTileDepth[window.centerSlot].y, window, true, ndotv);

  const float3 original = lightOriginal.Load(int3(pixel, 0)).xyz;
  const float3 translucency =
      saturate(cTweakParams.y * (1.0 - (2.0 + 1.0 / (ndotv + 1.0e-9)) * float3(0.15, 0.18, 0.20)));
  const float3 mixed = translucency * diffused + (1.0 - translucency) * original;
  const float3 result = cFade * mixed + (1.0 - cFade) * original;

  resultBuffer[pixel] = float4(result, 1.0);
}
