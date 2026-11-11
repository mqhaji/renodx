// Shared implementation for the MGSV skin-scattering ports: horizontal
// 0x6DAF0C4A and vertical 0xB2601810. Seven depth-weighted bands spread light
// along each axis; the vertical port also blends with the original light.
// The horizontal port is active. The registry keeps original vertical bytecode.
// Exact native parity, including this port's normal decoder, is not established.
//
// The vertical dispatch transposes the thread coordinates used by the horizontal
// pass. Bindings are b0 constants, t0 light, t1 depth, t2 normal, u0 result, and
// t4 original light for the vertical port only.

#ifndef MGSV_SSSS_HLSLI
#define MGSV_SSSS_HLSLI

cbuffer SubsurfaceConstants : register(b0)
{
  uint cBufferWidth : packoffset(c0);
  uint cBufferHeight : packoffset(c0.y);
  uint cNumTilesX : packoffset(c0.z);
  uint cNumTilesY : packoffset(c0.w);
  float2 cHalfFovRecip : packoffset(c1);
  float2 cCameraCenterOffset : packoffset(c1.z);
  float4 cProjectionParam : packoffset(c2);
  float2 cTweakParams : packoffset(c3);
  float cFade : packoffset(c3.z);
  float padding : packoffset(c3.w);
}

// Stage two reads the original light from t4; stage one has no t4.
Texture2D<float4> lightBuffer : register(t0);
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);

// One tile of the sweep axis is cached per group: 128 threads hold up to 192
// pixels, which is the 128-pixel tile plus the 32-pixel reach either side. The
// retail shader also caches the raw depth next to the light, and only for pixels
// whose light alpha is non-zero - that alpha is the "pixel was shaded" flag.
#define SSSS_TILE 192
groupshared float4 gTileLight[SSSS_TILE];
groupshared float2 gTileDepth[SSSS_TILE];  // raw depth, linear depth

// Seven fractions of the sample step, with less blue at larger offsets.
static const float kBandDistance[7] = {0.0, 0.1667, 0.3333, 0.5, 0.6667, 0.8333, 1.0};
static const float3 kBandTint[7] = {
    float3(1.000000, 1.000000, 1.000000),
    float3(0.986717, 0.964554, 0.943217),
    float3(0.947919, 0.865578, 0.791492),
    float3(0.886615, 0.722668, 0.590886),
    float3(0.807391, 0.561334, 0.392446),
    float3(0.715847, 0.405661, 0.231894),
    float3(0.617933, 0.272743, 0.121903),
};

// Per-channel depth falloff. Red tolerates larger depth differences than blue.
static const float3 kDepthFalloff = float3(313629.34375, 515248.21875, 655770.5);

// Convert native raw depth to view depth.
float LinearDepth(float raw) {
  return cProjectionParam.z / (raw - cProjectionParam.w);
}

// Cache position and bounds for one sweep tile. Both ports use these bounds
// while filling the cache and selecting samples.
struct TileWindow {
  uint centerSlot;  // cache slot of the pixel this thread owns
  uint slotBias;    // 32 for every group but the first
  uint span;        // 128, or the remainder in the partial tile
  bool partial;     // group id >= cNumTilesX
  bool firstGroup;  // group id zero
};

// Fills the group cache. `pixel` is (column, row); `sweep_y` says the cache index
// is the row coordinate (stage two) instead of the column (stage one).
TileWindow CacheTile(uint2 pixel, uint3 gid, uint gi, bool sweep_y) {
  TileWindow window;
  window.firstGroup = gid.x == 0;
  window.partial = gid.x >= cNumTilesX;
  window.slotBias = window.firstGroup ? 0u : 32u;

  // Retail: the group's first thread reads the tile origin minus the 32-pixel
  // reach the halo needs; group zero starts at column/row zero.
  const int coord_bias = window.firstGroup ? 0 : -32;
  const uint axis_extent = sweep_y ? cBufferHeight : cBufferWidth;
  window.span = window.partial ? axis_extent - gid.x * 128u : 128u;

  // Group zero caches 160 pixels (tile plus forward halo); every later group
  // caches the full 192 (halo on both sides). This thread owns slots gi and
  // gi + 128, so clearing them here covers the whole cache with no barrier.
  const uint count = window.span + window.slotBias + (window.partial ? 0u : 32u);
  gTileLight[gi] = 0.0;
  gTileDepth[gi] = 0.0;
  if (gi + 128u < SSSS_TILE) {
    gTileLight[gi + 128u] = 0.0;
    gTileDepth[gi + 128u] = 0.0;
  }

  const int first_coord = (int)(sweep_y ? pixel.y : pixel.x) + coord_bias;

  for (uint index = gi; index < count; index += 128u) {
    const int coord = (gi < index) ? first_coord + 128 : first_coord;
    const int2 sample_pixel = sweep_y ? int2(pixel.x, coord) : int2(coord, pixel.y);
    const float4 light = lightBuffer.Load(int3(sample_pixel, 0));
    gTileLight[index] = light;
    // Retail only caches depth where the light alpha says the pixel was shaded.
    if (light.w != 0.0) {
      const float raw_depth = depthBuffer.Load(int3(sample_pixel, 0)).x;
      gTileDepth[index] = float2(raw_depth, LinearDepth(raw_depth));
    }
  }

  window.centerSlot = window.slotBias + gi;
  return window;
}

// Runs the seven-band sweep for this pixel and returns the diffused light.
// `n_dot_v` is the surface's cosine to the view ray, which stage two's blend needs.
float3 SweepDiffusedLight(uint2 pixel, float linear_z, TileWindow window, bool sweep_y,
                          out float n_dot_v) {
  const float2 uv = float2(pixel) / float2(cBufferWidth, cBufferHeight);
  const float2 ndc = uv * 2.0 - 1.0;

  // The TAA jitter is removed through cCameraCenterOffset, and NDC y is flipped
  // because it runs down the screen while the projection runs up.
  const float2 offset = float2(ndc.x, -ndc.y) - cCameraCenterOffset.xy;

  // Preserve this port's existing packed-normal calculation. It is not the
  // verified native decoder in rendering/geometry.hlsli; changing it requires
  // a separate scattering comparison rather than a cleanup-only edit.
  const float3 encoded = normalBuffer.Load(int3(pixel, 0)).xyz;
  const float2 nxy = encoded.xy * 2.0 - 1.0;
  const float nz = encoded.z * encoded.z - 1.0;
  const float tangent = 1.0 - nz * nz;
  float3 normal = float3(nxy * tangent, nz) * rsqrt(tangent * dot(nxy, nxy) + 1.0e-7);
  normal *= rsqrt(dot(normal, normal));

  // View position and view ray. The pass works in a 2D plane of (sweep axis,
  // depth): that is the pair the angle and the step are measured in.
  const float3 view_pos = float3(offset * cProjectionParam.xy * linear_z, linear_z);
  const float inv_length = rsqrt(dot(view_pos, view_pos));
  const float3 view_ray = -view_pos * inv_length;
  n_dot_v = dot(normal, view_ray);
  const float2 view_dir = -float2(sweep_y ? view_pos.y : view_pos.x, view_pos.z) * inv_length;
  const float2 normal_dir = normalize(float2(sweep_y ? normal.y : normal.x, normal.z));

  // Step in pixels. The |V.z|^3 (|P|^2 + 1) term is the retail range compensation:
  // the step shrinks as the pixel moves away from the camera axis.
  const float cos_theta = dot(normal_dir, view_dir);
  const float view_z = view_pos.z * inv_length;
  const float axis_extent = (float)(sweep_y ? cBufferHeight : cBufferWidth);
  const float range = abs(view_z) * view_z * view_z * (dot(view_pos, view_pos) + 1.0);
  const float step_pixels = 0.5 * axis_extent * abs(cos_theta)
                          * (sweep_y ? cHalfFovRecip.y : cHalfFovRecip.x)
                          * sqrt(1.6e-5 / range);

  float3 sum = 0.0;
  float3 weight_sum = 0.0;

  // Retail walks -6..6 and folds the sign into the offset, so both directions of
  // a band are visited. Negative indices walk positive, which is symmetric.
  [loop]
  for (int tap = -6; tap <= 6; ++tap) {
    const uint band = (uint)abs(tap);
    const float distance = min(kBandDistance[band] * step_pixels * cTweakParams.x, 32.0);
    const float direction = tap < 0 ? 1.0 : (tap > 0 ? -1.0 : 0.0);
    int slot = (int)window.centerSlot + (int)(direction * distance);

    // The first group has no negative halo, so it clamps at zero; the partial
    // tile clamps at its span. Later groups rely on the cache being fully filled.
    if (window.firstGroup) slot = max(slot, 0);
    if (window.partial) slot = min(slot, (int)window.span);
    if (slot < 0 || slot >= SSSS_TILE) continue;

    const float4 tap_light = gTileLight[slot];
    if (tap_light.w == 0.0) continue;  // outside the shaded region

    const float delta = gTileDepth[slot].y - linear_z;
    const float3 weight = exp2(-delta * delta * kDepthFalloff);
    sum += kBandTint[band] * weight * tap_light.rgb;
    weight_sum += kBandTint[band] * weight * tap_light.w;
  }

  // Per-channel division, with zero where a channel gathered nothing (retail
  // uses a select on each channel's own weight sum).
  return float3(weight_sum.x > 0.0 ? sum.x / weight_sum.x : 0.0,
                weight_sum.y > 0.0 ? sum.y / weight_sum.y : 0.0,
                weight_sum.z > 0.0 ? sum.z / weight_sum.z : 0.0);
}

#endif  // MGSV_SSSS_HLSLI
