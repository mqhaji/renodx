#include "../../../../shaders/color/srgb.hlsl"
#include "../../materials/material_source.hlsli"
#include "common.hlsli"
#include "../../rendering/motion.hlsli"

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
  // Export the complete scene block even for views narrower than thirty pixels.
  if (all(group == 0u) && lane < 32u) scene_output[uint2(lane, 0)] = lane < 30u ? scene_rows[lane] : 0u;
  if (any(id.xy >= reflection_size)) return;
  const int2 pixel = id.xy;
  const float depth = native_depth.Load(int3(pixel, 0));
  const float4 packed_normal = native_normal.Load(int3(pixel, 0));
  const float4 packed_material = native_material.Load(int3(pixel, 0));
  const float3 normal = DecodeReflectionNormal(packed_normal.xyz);
  const bool normal_valid = all(isfinite(normal));
  const float4 color = LoadCleanMaterial(clean_material, material_owner, pixel, reflection_size,
                                         native_albedo.Load(int3(pixel, 0)), packed_normal, packed_material, depth);
  const SpecularMaterial material = LoadSpecularMaterial(palette, packed_material);
  const float4 captured = environment.Load(int3(pixel, 0));
  const bool valid = depth > 0.0 && color.a >= 0.0 && material.valid && normal_valid
                     && captured.a >= 0.5 && receiver_environment.Load(int3(pixel, 0)).a >= 0.0;

  // The native late source is extended sRGB in a float view. Decode exactly
  // once here, before tracing. Its sky, direct light, fog and forward light do
  // not depend on material-capture coverage.
  const float3 radiance = renodx::color::srgb::Decode(max(late_scene.Load(int3(pixel, 0)).rgb, 0.0));
  const bool source_valid = all(isfinite(radiance));
  source_output[pixel] = float4(source_valid ? radiance : 0.0, source_valid ? 1.0 : 0.0);
  // An unsupported receiver can still supply reflected light elsewhere. Keep
  // its native source normal; a zero normal rejects genuinely invalid geometry.
  normal_output[pixel] = float4(normal_valid ? normal : 0.0, valid ? material.roughness : -1.0);
  material_output[pixel] = float4(material.f0, material.rate);
  depth_output[pixel] = depth;
  // Write denoiser guides beside the existing source and material work. NRD
  // expects world normals, positive view Z and previous-minus-current UV motion.
  // Native camera pose is stored as rows, unlike the CPU world-to-view record.
  // Its explicit row-major declaration makes this inverse rotation unambiguous.
  const float orientation = dot(normal, GetReflectionPosition(float2(pixel) + 0.5, depth)) > 0.0 ? -1.0 : 1.0;
  const float3 world_normal = valid ? normalize(mul((float3x3)current_scene.view_to_world, normal)) * orientation : float3(0.0, 0.0, 1.0);
  nrd_normal_output[pixel] = NRD_FrontEnd_PackNormalAndRoughness(world_normal, valid ? material.roughness : 1.0, 0.0);
  view_depth_output[pixel] = valid ? GetReflectionDepth(depth) : 1e6;
  float2 velocity = 0.0;
  if ((reflection_flags & 1u) != 0u) {
    const float2 native_motion = DecodeNativeMotion(native_velocity.Load(int3(pixel, 0)).ba, rcp(float2(reflection_size)));
    velocity = native_motion;
    if (!GetCameraVelocity((float2(pixel) + 0.5) / float2(reflection_size), depth, current_jitter, current_to_previous_clip, velocity)) velocity = native_motion;
    if (object_velocity.Load(int3(pixel, 0)).r > 0.5)
      velocity = native_motion - current_jitter * velocity_projection_jitter_scale;
  }
  motion_output[pixel] = -velocity;
}
