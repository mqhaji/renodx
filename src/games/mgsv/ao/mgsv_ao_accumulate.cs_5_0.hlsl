// MGSV VBAO - the temporal half.
//
// The AO pass rotates its sample pattern every frame (CUSTOM_RANDOM), so the residual it leaves
// is zero-mean noise. Averaging that across frames is what lets the per-frame tap count drop
// without losing the contact shadows the extra taps were buying.
//
// The signal is an un-occluded fraction in [0, 1], not radiance: no value here can explode, and a
// bad sample cannot compound into black the way an HDR history can. That makes this far more
// forgiving than the indirect lighting history, and the same depth/normal rejection is enough to
// keep silhouettes sharp.
//
// With AO Temporal off the blend is zero and this pass writes the frame through unchanged, so the
// toggle needs no second code path and no different bindings downstream.

#include "../shared.h"
#include "../gbuffer.hlsli"

#define AO_HISTORY_MAX_LENGTH 32.0

Texture2D<float4> currentBuffer : register(t0);  // this frame's factor, alpha unused
Texture2D<float4> historyBuffer : register(t1);  // previous result, alpha = length
Texture2D<float4> velocityBuffer : register(t2);
Texture2D<float4> depthBuffer : register(t3);
Texture2D<float4> normalBuffer : register(t4);
Texture2D<float4> historyAuxBuffer : register(t5);  // z, normal.xy
RWTexture2D<float4> historyOutput : register(u0);
RWTexture2D<float4> accumulatedOutput : register(u1);  // what the resolve reads
RWTexture2D<float4> historyAuxOutput : register(u2);

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  const float scale = max(CUSTOM_MGSV_AO_SCALE, 1.0);
  const int2 ao_size = int2(float2(cBufferWidth, cBufferHeight) / scale);
  if (pixel.x >= (uint)ao_size.x || pixel.y >= (uint)ao_size.y) return;

  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float current = currentBuffer.Load(int3(pixel, 0)).r;
  const float z = LinearZ(depthBuffer.Load(int3(centre, 0)).x);
  const float3 N = DecodeNormal(normalBuffer.Load(int3(centre, 0)).xyz);

  const float4 raw = velocityBuffer.Load(int3(centre, 0));
  const float2 velocity = (raw.ba * 2.0 - 1.0) * 64.0 / float2(cBufferWidth, cBufferHeight);

  const float2 uv = (float2(centre) + 0.5) / float2(cBufferWidth, cBufferHeight);
  const float2 previous = (uv - velocity) * float2(ao_size) - 0.5;
  const int2 base = (int2)floor(previous);
  const float2 fraction = frac(previous);

  float history = 0.0;
  float history_length = 0.0;
  [unroll] for (int y = 0; y <= 1; ++y) {
    [unroll] for (int x = 0; x <= 1; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), ao_size - 1);
      const float4 tap = historyBuffer.Load(int3(q, 0));
      const float weight = (x != 0 ? fraction.x : 1.0 - fraction.x)
                         * (y != 0 ? fraction.y : 1.0 - fraction.y);
      history += tap.r * weight;
      history_length += tap.a * AO_HISTORY_MAX_LENGTH * weight;
    }
  }

  const int2 aux_pixel = clamp((int2)round(previous), int2(0, 0), ao_size - 1);
  const float4 previous_aux = historyAuxBuffer.Load(int3(aux_pixel, 0));
  const float previous_z = previous_aux.x;
  const float2 previous_normal_xy = previous_aux.yz;
  // The packed normal's z is negative by construction (nz = enc.z^2 - 1), so reconstruct it with
  // this frame's sign instead of a blind +sqrt - otherwise the test rejects every pixel.
  const float previous_normal_z =
      (N.z < 0.0 ? -1.0 : 1.0) * sqrt(saturate(1.0 - dot(previous_normal_xy, previous_normal_xy)));
  const float3 previous_normal = float3(previous_normal_xy, previous_normal_z);

  const bool inside = previous.x >= 0.0 && previous.y >= 0.0
                      && previous.x <= (float)(ao_size.x - 1)
                      && previous.y <= (float)(ao_size.y - 1);
  const bool depth_matches = previous_z > 0.0 && abs(z - previous_z) <= 0.02 * max(z, 1.0e-6);
  const bool normal_matches = dot(N, previous_normal) >= 0.9;
  const bool has_history = inside && depth_matches && normal_matches && history_length > 0.5;

  const float adaptive = 1.0 - 1.0 / max(history_length, 1.0);
  const float blend = (CUSTOM_MGSV_AO_TEMPORAL > 0.5 && has_history)
                          ? min(CUSTOM_MGSV_AO_HISTORY, adaptive)
                                * saturate(CUSTOM_MGSV_AO_HISTORY_VALID)
                          : 0.0;
  const float result = lerp(current, saturate(history), blend);
  const float length = has_history ? min(history_length + 1.0, AO_HISTORY_MAX_LENGTH) : 1.0;

  const float4 value = float4(saturate(result), saturate(result), saturate(result),
                              length / AO_HISTORY_MAX_LENGTH);
  historyOutput[pixel] = value;
  accumulatedOutput[pixel] = value;
  historyAuxOutput[pixel] = float4(z, N.x, N.y, length / AO_HISTORY_MAX_LENGTH);
}
