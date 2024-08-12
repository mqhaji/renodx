// ---- Created with 3Dmigoto v1.3.16 on Sun Jun 09 17:02:21 2024

SamplerState RenderMapPointSampler_s : register(s1);
Texture2D<float4> RenderMapPointSampler : register(t1);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : SV_Position0,
  float2 v1 : TEXCOORD0,
  float4 v2 : COLOR0,
  out float4 o0 : SV_Target0)
{
  float4 r0;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xyz = RenderMapPointSampler.Sample(RenderMapPointSampler_s, v1.xy).xyz;
  o0.xyz = v2.xyz * r0.xyz;
  o0.w = v2.w;

  o0.xyz = sign(o0.xyz) * pow(abs(o0.xyz), 2.2f);
  o0.xyz *= 203.f/80.f;
  return;
}