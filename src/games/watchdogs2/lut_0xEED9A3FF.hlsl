// ---- Created with 3Dmigoto v1.3.16 on Sat Jun 01 19:04:04 2024

cbuffer HDRColorGradingLUTs : register(b0)
{
  float4 ColorCorrectColor : packoffset(c0);
  float4 ColorCorrectGain : packoffset(c1);
  float4 ColorCorrectGamma : packoffset(c2);
  float4 ColorCorrectLift : packoffset(c3);
  float4 ColorCorrectOffset : packoffset(c4);
  float ColorCorrectBrightness : packoffset(c5);
  float ColorCorrectContrast : packoffset(c5.y);
  float ColorCorrectSaturation : packoffset(c5.z);
  float HDRColorGradingLUTsWeights[16] : packoffset(c6);
}

Texture3D<float4> HDRColorGradingLUTs__HDRColorGradingLUTsTextures00__TexObj__ : register(t0);
RWTexture3D<float4> HDRColorGradingLUTs__HDRColorGradingLUTsBlendedTexture : register(u0);


// 3Dmigoto declarations
#define cmp -


void main)
{
// Needs manual fix for instruction:
// unknown dcl_: dcl_uav_typed_texture3d (float,float,float,float) u0
  float4 r0,r1,r2;
  uint4 bitmask, uiDest;
  float4 fDest;

// Needs manual fix for instruction:
// unknown dcl_: dcl_thread_group 8, 8, 1
  r0.xyz = vThreadID.xyz;
  r0.w = 0;
  r0.xyz = HDRColorGradingLUTs__HDRColorGradingLUTsTextures00__TexObj__.Load(r0.xyzw).xyz;
  r1.xyz = HDRColorGradingLUTsWeights[0] * r0.xyz;
  r0.xyz = -r0.xyz * HDRColorGradingLUTsWeights[0] + float3(1,1,1);
  r0.xyz = ColorCorrectLift.xyz * r0.xyz + r1.xyz;
  r0.xyz = ColorCorrectGain.xyz * r0.xyz;
  r0.xyz = max(float3(0,0,0), r0.xyz);
  r0.xyz = log2(r0.xyz);
  r1.xyz = max(float3(9.99999997e-007,9.99999997e-007,9.99999997e-007), ColorCorrectGamma.xyz);
  r1.xyz = float3(1,1,1) / r1.xyz;
  r0.xyz = r1.xyz * r0.xyz;
  r0.xyz = exp2(r0.xyz);
  r0.xyz = ColorCorrectOffset.xyz + r0.xyz;
  r0.xyz = ColorCorrectBrightness + r0.xyz;
  r0.w = dot(r0.xyz, float3(0.212639004,0.715168655,0.0721923187));
  r1.xyz = r0.www + -r0.xyz;
  r0.xyz = -ColorCorrectSaturation * r1.xyz + r0.xyz;
  r0.w = 1 + -ColorCorrectContrast;
  r0.w = 0.180000007 * r0.w;
  r0.xyz = ColorCorrectContrast * r0.xyz + r0.www;
  r1.xyz = saturate(ColorCorrectColor.xyz);
  r0.xyz = r1.xyz * r0.xyz;
  r1.xyz = log2(abs(r0.xyz));
  r1.xyz = float3(0.416666657,0.416666657,0.416666657) * r1.xyz;
  r1.xyz = exp2(r1.xyz);
  r1.xyz = r1.xyz * float3(1.05499995,1.05499995,1.05499995) + float3(-0.0549999997,-0.0549999997,-0.0549999997);
  r2.xyz = cmp(float3(0.00313080009,0.00313080009,0.00313080009) >= r0.xyz);
  r0.xyz = float3(12.9200001,12.9200001,12.9200001) * r0.xyz;
  r0.xyz = r2.xyz ? r0.xyz : r1.xyz;
  r0.w = 0;
// No code for instruction (needs manual fix):
store_uav_typed u0.xyzw, vThreadID.xyzz, r0.xyzw
  return;
}