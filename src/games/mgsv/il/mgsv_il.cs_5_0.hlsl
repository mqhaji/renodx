// MGSV screen-space diffuse gather (SSGI) - the addon dispatches this after the AO.
//
// A separate pass on purpose. The AO marches angular slices to measure occlusion; this
// gathers radiance from the neighbourhood to add a bounce. They share the depth, normal
// and light buffers and nothing else, so one can be tuned or disabled without touching
// the other.
//
// The gather reads `lightSource`, which is the light the retail scatter pass consumes
// (its t4), and writes `lightOutput`, the buffer that scatter pass just produced. They
// are different resources, so reading neighbours while writing this pixel is not a
// hazard; reading the output buffer's neighbours would be, because a dispatch does not
// order its own writes.
//
// No history and no spatial filter yet: the shape of the gather is judged first, then the
// noise gets a denoiser. The settings that shape it - count, radius, falloff - are exposed
// so the trade can be made without a rebuild.

#include "../shared.h"
#include "../gbuffer.hlsli"

Texture2D<float4> depthBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float4> lightSource : register(t2);
RWTexture2D<float4> lightOutput : register(u0);

#define IL_MAX_SAMPLES 16

// Integer hash for the rotation. Interleaved gradient noise is the cheaper choice and was
// the first one here, but it is a diagonal ramp: every pixel's pattern then rotated by a
// diagonally varying amount, and the residual read as screen-space stripes rather than as
// noise. A hash has no screen-space structure to leak.
uint Hash(uint x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

float Hash01(uint2 pixel, uint salt) {
  return (float)(Hash(pixel.x + Hash(pixel.y + Hash(salt))) & 0xFFFFFFu) * (1.0 / 16777216.0);
}

// Golden-angle spiral over the unit disk with a jittered radius. The jitter keeps every
// sample off its ring, which is what stops a fixed set of radii from banding on flat
// surfaces that all share one rotation.
float2 DiskSample(uint index, uint count, float rotation, float radius_jitter) {
  const float radius = sqrt((index + radius_jitter) / (float)count);
  const float angle = rotation + index * 2.39996323;
  return radius * float2(cos(angle), sin(angle));
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 pixel = tid.xy;
  if (pixel.x >= cBufferWidth || pixel.y >= cBufferHeight) return;

  const float4 centre = lightOutput.Load(int3(pixel, 0));
  const float z = LinearZ(depthBuffer.Load(int3(pixel, 0)).x);
  if (!(z > 0.0) || z > 1.0e6) return;

  const float2 pixel_f = float2(pixel);
  const float3 P = ViewPos(pixel_f, z);
  const float3 N = DecodeNormal(normalBuffer.Load(int3(pixel, 0)).xyz);

  // The radius is a fraction of screen height, with its world equivalent at this depth.
  // The falloff normalises against the world radius so the setting changes reach rather
  // than overall brightness.
  const float radius_fraction = max(CUSTOM_MGSV_IL_RADIUS, 0.001);
  const float radius_pixels = radius_fraction * (float)cBufferHeight;
  const float radius_world = radius_fraction * 2.0 * z * abs(cProjectionParam.y);

  const uint sample_count = (uint)clamp(CUSTOM_MGSV_IL_SAMPLES, 1.0, (float)IL_MAX_SAMPLES);
  const float falloff = max(CUSTOM_MGSV_IL_FALLOFF, 0.0);
  const bool difference = CUSTOM_MGSV_IL_MODE > 0.5;

  // The frame term only rotates the pattern when something is accumulating it; with
  // reconstruction off the pattern stays put and the hash keeps it from striping.
  const uint frame_salt = CUSTOM_TAA != 0.0 ? (uint)(CUSTOM_RANDOM * 65535.0) : 0u;
  const float rotation = Hash01(pixel, frame_salt) * 6.2831853;

  float3 sum = 0.0;
  float weight_sum = 0.0;

  [loop] for (uint index = 0; index < sample_count; ++index) {
    const float radius_jitter = Hash01(pixel, index * 977u + frame_salt + 1u);
    const int2 sample_pixel =
        (int2)round(pixel_f + DiskSample(index, sample_count, rotation, radius_jitter) * radius_pixels);
    if (sample_pixel.x < 0 || sample_pixel.y < 0
        || sample_pixel.x >= (int)cBufferWidth || sample_pixel.y >= (int)cBufferHeight) continue;

    const float sample_z = LinearZ(depthBuffer.Load(int3(sample_pixel, 0)).x);
    if (!(sample_z > 0.0) || sample_z > 1.0e6) continue;

    const float3 S = ViewPos(float2(sample_pixel), sample_z);
    const float3 delta = S - P;
    const float distance_sq = dot(delta, delta);
    if (distance_sq < 1.0e-8) continue;

    const float distance = sqrt(distance_sq);
    const float3 direction = delta / distance;

    const float elevation = dot(N, direction);

    // Additive mode must reject the same surface, or a flat plane feeds itself and the
    // gather reads as a halo. Difference mode does not: a uniformly lit plane cancels
    // against the centre, so coplanar samples are allowed - and they are where a
    // flashlight's light actually is, on the floor or wall being looked at.
    float plane_weight;
    if (difference) {
      if (elevation <= 0.0) continue;  // below this pixel's surface: occluded geometry
      plane_weight = elevation;
    } else {
      plane_weight = smoothstep(0.02, 0.35, elevation);
      if (plane_weight <= 0.0) continue;
    }

    const float3 sample_normal = DecodeNormal(normalBuffer.Load(int3(sample_pixel, 0)).xyz);
    const float source_facing = dot(sample_normal, -direction);
    if (source_facing <= 0.0) continue;

    const float weight = plane_weight * source_facing
                       * pow(1.0 + 4.0 * distance_sq / (radius_world * radius_world), -falloff);
    sum += lightSource.Load(int3(sample_pixel, 0)).rgb * weight;
    weight_sum += weight;
  }

  // A weighted mean, so the strength reads as a fraction of the neighbourhood's lighting
  // rather than as a sum that dims whenever the reach shortens. Difference mode reports
  // only how much brighter the neighbourhood is than this pixel, which is what keeps the
  // same-surface case from lifting the whole plane.
  const float3 mean = weight_sum > 0.0 ? sum / weight_sum : 0.0;
  const float3 gather = difference ? max(mean - centre.rgb, 0.0) : mean;
  const float3 bounce = gather * CUSTOM_MGSV_IL_AMOUNT;

  // Debug 1 writes the bounce alone into the light, so the term can be judged without the
  // scene's own light in the way. Debug 2 writes the gather's source at this pixel, which
  // answers whether a light - a flashlight, say - is in that buffer at all.
  float3 output = centre.rgb + bounce;
  if (CUSTOM_MGSV_IL_DEBUG > 1.5) {
    output = lightSource.Load(int3(pixel, 0)).rgb;
  } else if (CUSTOM_MGSV_IL_DEBUG > 0.5) {
    output = bounce;
  }
  lightOutput[pixel] = float4(output, centre.a);
}
