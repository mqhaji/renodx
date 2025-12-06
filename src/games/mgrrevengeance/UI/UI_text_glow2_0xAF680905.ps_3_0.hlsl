#include "./ui.hlsli"

sampler2D g_Sampler0 : register(s13);
sampler2D g_Sampler1 : register(s14);
sampler2D g_Sampler2 : register(s15);

struct PS_IN {
  float2 texcoord : TEXCOORD;
  float2 texcoord1 : TEXCOORD1;
  float4 texcoord2 : TEXCOORD2;
  float4 texcoord3 : TEXCOORD3;
  float3 texcoord4 : TEXCOORD4;
};

float4 main(PS_IN i)
    : COLOR {
  float4 o;

  float4 r0;
  float4 r1;
  float3 r2;
  float3 r3;
  r0 = tex2D(g_Sampler0, i.texcoord);
  r1.x = r0.y + r0.x;
  r1.x = r0.z + r1.x;
  r1.x = r1.x * i.texcoord4.y;
  r1.y = r0.w * i.texcoord4.x;
  r1.x = r1.x * 0.33333334 + r1.y;
  r1.w = r0.w * i.texcoord4.z + r1.x;
  r0.xyz = r0.xyz * i.texcoord4.x + i.texcoord4.y;
  r1.xyz = r0.xyz + i.texcoord4.z;
  r0 = i.texcoord2;
  r0 = r1 * r0 + i.texcoord3;
  r1 = tex2D(g_Sampler1, i.texcoord1);
  o.w = r0.w * r1.w;
  r1 = tex2D(g_Sampler2, i.texcoord1);
  r0.w = (r1.x * r0.x) + (r1.x * r0.x);  // dp2add r0.w, r1.x, r0.x, 0
  r2.xyz = -r0.xyz + 1;
  r3.xyz = -r1.xyz + 1;
  r3.xyz = r3.xyz + r3.xyz;
  r2.xyz = r3.xyz * -r2.xyz + 1;
  r3.xyz = r1.xyz + -0.5;
  r0.x = (r1.y * r0.y) + (r1.y * r0.y);  // dp2add r0.x, r1.y, r0.y, 0
  r0.y = (r1.z * r0.z) + (r1.z * r0.z);  // dp2add r0.y, r1.z, r0.z, 0
  o.xyz = (r3.xyz >= 0) ? r2.xyz : float3(r0.w, r0.x, r0.y);

  o.rgb = ScaleUI(o.rgb);

  return o;
}
