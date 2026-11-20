#include "../../../../shaders/color/srgb.hlsl"
#include "../../shared.h"
#include "../../materials/material_source.hlsli"
#include "common.hlsli"
#include "../../rendering/motion.hlsli"

// First enhanced reflection pass, run at the game's late reflection compositor.
// Reads the native G-buffer, the late scene color, cube-material records, the
// material palette, motion, the environment light captured from the light
// draws (t9), and the environment deferred composition exported (t10).
// Writes, per pixel:
//   u0 linear scene radiance that reflection rays gather (alpha 1 where valid)
//   u1 view-space normal and roughness (roughness -1: not a reflection receiver)
//   u2 F0 and native reflection rate
//   u3 native reverse-Z depth
//   u5-u7 NRD guides: motion, packed world normal and roughness, view Z
// and once per dispatch, u4: the native scene constants for later passes.

Texture2D<float4> native_albedo : register(t0);
Texture2D<float4> late_scene : register(t1);
Texture2D<float4> native_normal : register(t2);
Texture2D<float4> native_material : register(t3);
Texture2D<float> native_depth : register(t4);
Texture2D<float4> clean_material : register(t5);
Texture2D<uint4> material_owner : register(t6);
Texture2D<float4> palette : register(t7);
Texture2D<float4> native_velocity : register(t8);
Texture2D<float4> environment : register(t9);
Texture2D<float4> receiver_environment : register(t10);
Texture2D<float4> object_velocity : register(t11);

RWTexture2D<float4> source_output : register(u0);
RWTexture2D<float4> normal_output : register(u1);
RWTexture2D<float4> material_output : register(u2);
RWTexture2D<float> depth_output : register(u3);
RWTexture2D<uint4> scene_output : register(u4);
RWTexture2D<float2> motion_output : register(u5);
RWTexture2D<float4> nrd_normal_output : register(u6);
RWTexture2D<float> view_depth_output : register(u7);
cbuffer NativeSceneBytes : register(b3) { uint4 scene_rows[30]; }

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint lane : SV_GroupIndex) {
  // Group 0 exports all 30 scene-constant rows (padded to 32) before the bounds
  // check, so views narrower than 30 pixels still export every row.
  if (all(group == 0u) && lane < 32u) scene_output[uint2(lane, 0)] = lane < 30u ? scene_rows[lane] : 0u;
  if (any(id.xy >= reflection_size)) return;
  const int2 pixel = id.xy;
  const float depth = native_depth.Load(int3(pixel, 0));
  const float4 packed_normal = native_normal.Load(int3(pixel, 0));
  const float4 packed_material = native_material.Load(int3(pixel, 0));
  const float3 normal = DecodeReflectionNormal(packed_normal.xyz);
  const bool normal_valid = all(isfinite(normal));
  const float4 color = LoadCleanMaterial(clean_material, material_owner, pixel, reflection_size,
                                         native_albedo.Load(int3(pixel, 0)).rgb, packed_normal, packed_material, depth);
  const SpecularMaterial material = LoadSpecularMaterial(palette, packed_material);
  const float4 captured = environment.Load(int3(pixel, 0));
  const bool valid = depth > 0.0 && color.a >= 0.0 && material.valid && normal_valid
                     && captured.a >= 0.5 && receiver_environment.Load(int3(pixel, 0)).a >= 0.0;

  // The late scene is an FP16 target holding extended sRGB. Decode it once here
  // so tracing works in linear light. Its validity does not depend on material
  // capture: it already contains sky, direct light, fog and forward-drawn light.
  float3 radiance = late_scene.Load(int3(pixel, 0)).rgb;
#if FIX_UNORM_SRGB
  radiance = renodx::color::srgb::Decode(max(radiance, 0.0));
#endif
  const bool source_valid = all(isfinite(radiance));
  source_output[pixel] = float4(source_valid ? radiance : 0.0, source_valid ? 1.0 : 0.0);
  // A pixel that cannot receive reflections can still be hit by other rays, so
  // keep its normal for their backface test. A zero normal marks invalid
  // geometry, which that test rejects.
  normal_output[pixel] = float4(normal_valid ? normal : 0.0, valid ? material.roughness : -1.0);
  material_output[pixel] = float4(material.f0, material.rate);
  depth_output[pixel] = depth;
  // NRD guides. NRD expects world-space normals, positive view Z and
  // previous-minus-current UV motion. The GPU scene block stores the camera
  // pose as rows (unlike the CPU world-to-view record), hence the row_major
  // view_to_world. Normals facing away from the camera are flipped first.
  const float orientation = dot(normal, GetReflectionPosition(float2(pixel) + 0.5, depth)) > 0.0 ? -1.0 : 1.0;
  const float3 world_normal = valid ? normalize(mul((float3x3)current_scene.view_to_world, normal)) * orientation : float3(0.0, 0.0, 1.0);
  nrd_normal_output[pixel] = NRD_FrontEnd_PackNormalAndRoughness(world_normal, valid ? material.roughness : 1.0, 0.0);
  view_depth_output[pixel] = valid ? GetReflectionDepth(depth) : 1e6;
  // Motion is needed only when history continues. Static pixels use camera
  // motion from the matched transforms; pixels in the object mask use the
  // object image's motion with its projection jitter removed. velocity is
  // current-minus-previous, so it is negated for NRD.
  float2 velocity = 0.0;
  if ((reflection_flags & 1u) != 0u) {
    const float2 native_motion = MotionPixels(native_velocity.Load(int3(pixel, 0))) * rcp(float2(reflection_size));
    velocity = native_motion;
    if (!GetCameraVelocity((float2(pixel) + 0.5) / float2(reflection_size), depth, current_jitter, current_to_previous_clip, velocity)) velocity = native_motion;
    const float4 object_motion = object_velocity.Load(int3(pixel, 0));
    if (object_motion.r > 0.5)
      velocity = MotionPixels(object_motion) * rcp(float2(reflection_size)) - current_jitter * velocity_projection_jitter_scale;
  }
  motion_output[pixel] = -velocity;
}
