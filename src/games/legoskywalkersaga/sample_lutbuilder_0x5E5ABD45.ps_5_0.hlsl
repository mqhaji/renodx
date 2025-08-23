// ---- Created with 3Dmigoto v1.4.1 on Fri Aug 22 07:08:40 2025

// clang-format off
cbuffer g_FullscreenTextureSampling_CB : register(b0) {
  struct
  {
    float4 textureSize;
    float4 uvScaleBias;
    float4 pixelExtents;
    float4 uvExtents;
    float4 uvExtentsBilinear[10];
    float4 uvScaleBiasLastFrame;
    float4 pixelExtentsLastFrame;
    float4 uvExtentsLastFrame;
    float4 uvExtentsBilinearLastFrame[10];
    float4 uvScaleBiasCurrentRT;
    float4 uvExtentsCurrentRT;
  } g_FullscreenTextureSampling: packoffset(c0);
}

cbuffer g_MainFilterPS_CB : register(b1) {
  struct
  {
    float4 glareParams;
    float4 fxaaParams;
    float4 mainFilterToneMapping;
    float4 mainFilterDof;
    float4 pixel_size;
    float4 naughtyTonemapParams1;
    float4 naughtyTonemapParams2;
    float4 vignetteQrc;
    float4 vignetteViewport;
    float4 vignetteParams;
    float4 textureMips;
  } g_MainFilterPS: packoffset(c0);
}

cbuffer g_ColourProcessPS_CB : register(b2) {
  struct
  {
    float4 colorUVScale;
    float4 colorUVBias;
    float4 colorUVScaleB;
    float4 colorUVBiasB;
  } g_ColourProcessPS: packoffset(c0);
}
// clang-format on

SamplerState MinMagPointMipDisabledClampSampler_s : register(s0);
SamplerState MinMagLinearMipDisabledClampSampler_s : register(s1);
Texture2D<float4> fullColor_tex : register(t0);
Texture2D<float4> blur_tex : register(t1);
Texture3D<float4> cubeTexC : register(t2);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float4 v1: TEXCOORD0,
    float4 v2: TEXCOORD1,
    out float4 o0: SV_Target0) {
  float4 r0, r1;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = -g_MainFilterPS.vignetteViewport.xy + v1.xy;
  r0.xy = g_MainFilterPS.vignetteViewport.zw * r0.xy;
  r0.x = dot(r0.xy, r0.xy);
  r0.x = sqrt(r0.x);
  r0.x = saturate(-g_MainFilterPS.vignetteParams.x + r0.x);
  r0.y = 1 + -g_MainFilterPS.vignetteParams.x;
  r0.x = r0.x / r0.y;
  r0.y = r0.x * r0.x;
  r0.zw = r0.yy * r0.xy;
  r0.x = dot(g_MainFilterPS.vignetteQrc.xyzw, r0.xyzw);
  r0.x = saturate(-r0.x * g_MainFilterPS.vignetteParams.y + 1);
  r0.x = r0.x * r0.x;
  r0.y = ceil(g_MainFilterPS.textureMips.x);
  r0.y = (uint)r0.y;
  r0.zw = max(g_FullscreenTextureSampling.uvExtentsBilinear[r0.y].xy, v1.xy);
  r0.yz = min(g_FullscreenTextureSampling.uvExtentsBilinear[r0.y].zw, r0.zw);
  r0.yzw = blur_tex.Sample(MinMagLinearMipDisabledClampSampler_s, r0.yz).xyz;
  r1.xyz = fullColor_tex.Sample(MinMagPointMipDisabledClampSampler_s, v1.xy).xyz;
  r1.xyz = max(float3(0, 0, 0), r1.xyz);
  r0.yzw = r0.yzw * r0.yzw + r1.xyz;
  r0.xyz = r0.yzw * r0.xxx;
  r0.xyz = sqrt(r0.xyz);
  r0.xyz = r0.xyz * float3(32, 32, 32) + float3(1, 1, 1);
  r0.xyz = log2(r0.xyz);
  r0.xyz = g_ColourProcessPS.colorUVScale.xyz * r0.xyz;
  r0.xyz = r0.xyz * float3(0.130948052, 0.130948052, 0.130948052) + g_ColourProcessPS.colorUVBias.xyz;
  r0.xyz = cubeTexC.Sample(MinMagLinearMipDisabledClampSampler_s, r0.xyz).xyz;
  o0.w = dot(r0.xyz, float3(0.298999995, 0.587000012, 0.114));
  o0.xyz = r0.xyz;
  return;
}
