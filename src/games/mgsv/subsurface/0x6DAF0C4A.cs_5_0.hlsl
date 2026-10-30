// MGSV screen-space subsurface scatter, stage 1 - CRC 0x6DAF0C4A, cs_5_0.
//
// Horizontal sweep of the separable depth-aware light diffusion. The retail
// shader is dispatched with groups on the column axis and threads on rows, so
// one group owns 128 columns of one row.
//
// This port is active through generated shader registration. See ssss.hlsli for
// its bindings and implementation; exact parity with retail has not been established.

#include "ssss.hlsli"

RWTexture2D<float4> resultBuffer : register(u0);

[numthreads(128, 1, 1)]
void main(uint3 gid : SV_GroupID,
          uint3 tid : SV_DispatchThreadID,
          uint gi : SV_GroupIndex) {
  const uint2 pixel = tid.xy;
  const TileWindow window = CacheTile(pixel, gid, gi, false);
  GroupMemoryBarrierWithGroupSync();

  // Retail drops threads past the buffer width. The thread sitting exactly on the
  // width is not dropped and stores out of bounds, which the UAV bounds check
  // discards; that is reproduced rather than "fixed".
  if (pixel.x > cBufferWidth) return;

  const float4 centre = gTileLight[window.centerSlot];
  if (centre.w == 0.0) {
    // Unshaded pixel: the retail pass passes the cached light through untouched.
    resultBuffer[pixel] = centre;
    return;
  }

  float ndotv;  // unused by this stage, but part of the shared sweep's signature
  const float3 diffused = SweepDiffusedLight(
      pixel, gTileDepth[window.centerSlot].y, window, false, ndotv);
  resultBuffer[pixel] = float4(diffused, 1.0);
}
