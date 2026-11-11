// Vertical skin-scattering port, with transposed dispatch coordinates and a
// translucency blend against the original light. The registry retains native
// bytecode instead of using this port. See ssss.hlsli for shared implementation
// and the limits of native-parity checks.

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
    const float4 center = gTileLight[window.centerSlot];
    if (center.w == 0.0) return;

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
