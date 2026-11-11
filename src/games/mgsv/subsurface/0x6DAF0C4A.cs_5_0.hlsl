// Horizontal skin-scattering port. Each group covers 128 columns of one row.
// The shader registry supplies this replacement after custom lighting runs.
// See ssss.hlsli for shared bindings and the limits of native-parity checks.

#include "ssss.hlsli"

RWTexture2D<float4> resultBuffer : register(u0);

[numthreads(128, 1, 1)]
void main(uint3 gid : SV_GroupID,
          uint3 tid : SV_DispatchThreadID,
          uint gi : SV_GroupIndex) {
  const uint2 pixel = tid.xy;
  const TileWindow window = CacheTile(pixel, gid, gi, false);
  GroupMemoryBarrierWithGroupSync();

  // Preserve the port's strict comparison. A thread exactly at the width reaches
  // an out-of-bounds UAV store, which D3D11 discards.
  if (pixel.x > cBufferWidth) return;

  const float4 center = gTileLight[window.centerSlot];
  if (center.w == 0.0) {
    // Unshaded pixel: the retail pass passes the cached light through untouched.
    resultBuffer[pixel] = center;
    return;
  }

  float ndotv;  // unused by this stage, but part of the shared sweep's signature
  const float3 diffused = SweepDiffusedLight(
      pixel, gTileDepth[window.centerSlot].y, window, false, ndotv);
  resultBuffer[pixel] = float4(diffused, 1.0);
}
