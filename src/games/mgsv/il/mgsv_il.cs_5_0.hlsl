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
// The albedo G-buffer. The light buffer is irradiance, so on its own it carries no colour:
// a red wall lit by white light has a white light value and a red albedo. Multiplying the
// source's albedo into the gathered radiance is the term that turns the gather into colour
// bleeding instead of a neutral lift.
Texture2D<float4> albedoBuffer : register(t3);
// The specular term the composite adds on top of albedo * diffuse. Emissive content rides
// here, so leaving it out is what made a bright fixture contribute no more than a dim wall.
Texture2D<float4> specularBuffer : register(t4);
RWTexture2D<float4> bounceOutput : register(u0);

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

  // The gather may run below full resolution. Its threads map onto full-resolution pixels,
  // which is where every G-buffer read and every sample offset lives.
  const float scale = max(CUSTOM_MGSV_IL_SCALE, 1.0);
  const int2 centre = (int2)floor((float2(pixel) + 0.5) * scale);
  if (centre.x >= (int)cBufferWidth || centre.y >= (int)cBufferHeight) return;

  const float z = LinearZ(depthBuffer.Load(int3(centre, 0)).x);
  if (!(z > 0.0) || z > 1.0e6) return;

  const float2 pixel_f = float2(centre);
  const float3 P = ViewPos(pixel_f, z);
  const float3 N = DecodeNormal(normalBuffer.Load(int3(centre, 0)).xyz);

  // The radius is a fraction of screen height. A world-space radius is the right model for
  // bounce, but it needs a known world scale: LinearZ is not in metres, so converting a
  // world distance here collapses the radius to a pixel and the gather finds nothing.
  const float radius_fraction = max(CUSTOM_MGSV_IL_RADIUS, 0.001);
  const float radius_pixels = radius_fraction * (float)cBufferHeight;
  const float radius_world = radius_fraction * 2.0 * z * abs(cProjectionParam.y);

  const uint sample_count = (uint)clamp(CUSTOM_MGSV_IL_SAMPLES, 1.0, (float)IL_MAX_SAMPLES);
  // How many of those samples must survive the rejection gates before the pixel is allowed
  // to contribute at all. One bright neighbour passing alone is not an estimate, but the
  // floor has to stay low or the term disappears where geometry rejects most of the disk.
  const uint min_samples = (uint)clamp(CUSTOM_MGSV_IL_MIN_SAMPLES, 1.0, (float)IL_MAX_SAMPLES);

  // The frame term only rotates the pattern when something is accumulating it; with
  // reconstruction off the pattern stays put and the hash keeps it from striping.
  const uint frame_salt = CUSTOM_TAA != 0.0 ? (uint)(CUSTOM_RANDOM * 65535.0) : 0u;
  const float rotation = Hash01(pixel, frame_salt) * 6.2831853;

  float3 sum = 0.0;
  float weight_sum = 0.0;
  uint valid_samples = 0;

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

    // A sample on the same surface is the pixel's own light: adding it back is what makes a
    // gather read as a halo rather than as bounce. The soft ramp is the cosine term too - a
    // hard cut on the same value bands along geometry.
    const float plane_weight = smoothstep(0.02, 0.35, elevation);
    if (plane_weight <= 0.0) continue;

    const float3 sample_normal = DecodeNormal(normalBuffer.Load(int3(sample_pixel, 0)).xyz);
    const float source_facing = dot(sample_normal, -direction);
    if (source_facing <= 0.0) continue;

    // Distance weighting is a fixed inverse-square against the radius. It is deliberately
    // not a setting: the gather returns a normalized mean, so the kernel's shape cancels
    // wherever the neighbourhood is uniform and only shows up as an unreadable shift in
    // contrast. Reach is the radius; density is the sample count.
    const float range = 1.0 + 4.0 * distance_sq / (radius_world * radius_world);
    const float weight = plane_weight * source_facing / range;

    // Source radiance, the same combination the composite builds: albedo times diffuse light,
    // plus specular. Without the albedo the gather can only add neutral luminance; without
    // the specular it cannot see emissive or bright highlights, which is why a light fixture
    // counted for no more than a dim wall.
    const float3 radiance = albedoBuffer.Load(int3(sample_pixel, 0)).rgb
                                * lightSource.Load(int3(sample_pixel, 0)).rgb
                          + specularBuffer.Load(int3(sample_pixel, 0)).rgb;
    sum += radiance * weight;
    weight_sum += weight;
    ++valid_samples;
  }

  // The mean radiance, before the amount is applied: the composite scales it after the
  // denoiser, so the filter runs in radiance units and the amount stays a display knob.
  // Alpha carries validity - a starved pixel stays out of the filter rather than being
  // filled by its neighbours.
  const bool valid = valid_samples >= min_samples;
  bounceOutput[pixel] = float4(valid ? sum / max(weight_sum, 1.0e-6) : 0.0,
                               valid ? 1.0 : 0.0);
}
