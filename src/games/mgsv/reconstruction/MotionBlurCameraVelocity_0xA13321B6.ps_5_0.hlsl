// Port of MotionBlurCameraVelocity_ps, which writes the combined motion image
// that motion blur, reconstruction and reflections read: camera motion from
// depth, replaced by the object motion image where its mask is set. It runs
// only while temporal reconstruction is on, reads object motion in either
// form, and writes the combined image in precise form (rendering/motion.hlsli).
// Everything else follows the game's shader.
#include "../rendering/motion.hlsli"

cbuffer cPSScene : register(b2) {
  struct {
    float4x4 m_projectionView;
    float4x4 m_projection;
    float4x4 m_view;
    float4x4 m_shadowProjection;
    float4x4 m_shadowProjection2;
    float4 m_eyepos;
    float4 m_projectionParam;
    float4 m_viewportSize;
    float4 m_exposure;
    float4 m_fogParam[3];
    float4 m_fogColor;
    float4 m_cameraCenterOffset;
    float4 m_shadowMapResolutions;
  } g_psScene : packoffset(c0);
}

cbuffer cPSObject : register(b5) {
  struct {
    float4x4 m_viewWorld;
    float4x4 m_world;
    float4 m_useWeightCount;
    float4 m_localParam[4];
  } g_psObject : packoffset(c0);
}

cbuffer cPSSystem : register(b0) {
  struct {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}

SamplerState g_samplerPoint_Clamp_s : register(s9);
SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> inDepth : register(t2);
Texture2D<float4> inVelocity : register(t3);

void main(
    float4 v0 : SV_Position0,
    float4 v1 : TEXCOORD0,
    out float4 o0 : SV_Target0) {
  // v1.xy is the texture coordinate and v1.zw the pixel's clip-space position.
  const float2 render_size = g_psSystem.m_renderInfo.xy;
  const float2 velocity_uv = render_size * v1.xy * g_psSystem.m_renderBuffer.zw;
  const float2 depth_uv = (floor(g_psObject.m_localParam[0].zw * (v0.xy - 0.5)) + 0.5) * g_psObject.m_localParam[0].xy;

  // The object motion image: R is the mask, B and A the motion, in either form.
  const float4 object_motion = inVelocity.Sample(g_samplerLinear_Clamp_s, velocity_uv);

  // Camera motion: rebuild the view position from depth, move it to the
  // previous frame, and project it there.
  const float4 projection = g_psScene.m_projectionParam;
  const float2 clip_position = v1.zw - g_psScene.m_cameraCenterOffset.xy;
  const float depth = inDepth.SampleLevel(g_samplerPoint_Clamp_s, depth_uv, 0).x;
  float4 view_position;
  view_position.z = projection.z / (depth - projection.w);
  view_position.xy = clip_position * projection.xy * view_position.z;
  view_position.w = 1.0;
  float4 moved;
  moved.x = dot(view_position, g_psScene.m_shadowProjection2._m00_m10_m20_m30);
  moved.y = dot(view_position, g_psScene.m_shadowProjection2._m01_m11_m21_m31);
  moved.z = dot(view_position, g_psScene.m_shadowProjection2._m02_m12_m22_m32);
  moved.w = dot(view_position, g_psScene.m_shadowProjection2._m03_m13_m23_m33);
  float3 previous;
  previous.x = dot(moved, g_psScene.m_view._m00_m10_m20_m30);
  previous.y = dot(moved, g_psScene.m_view._m01_m11_m21_m31);
  previous.z = dot(moved, g_psScene.m_view._m03_m13_m23_m33);
  // The game limits this to 64 pixels and encodes it; here it stays in render
  // pixels, and the motion blur passes apply the limit when they read it.
  const float2 clip_motion = v1.zw - previous.xy / previous.z;
  const float2 camera = BoundPreciseMotion(float2(clip_motion.x, -clip_motion.y) * render_size * 0.5, render_size);

  o0 = float4(0.0, PRECISE_MOTION, camera + (MotionPixels(object_motion) - camera) * object_motion.r);
}
