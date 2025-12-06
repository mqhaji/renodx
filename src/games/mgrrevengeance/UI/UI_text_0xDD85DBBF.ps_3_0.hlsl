#include "./ui.hlsli"

sampler2D g_Sampler0 : register(s13);
sampler2D g_Sampler1 : register(s14);

struct PS_IN {
  float2 texcoord : TEXCOORD;
  float2 texcoord1 : TEXCOORD1;
  float3 texcoord2 : TEXCOORD2;
  float4 color : COLOR;
};

float4 main(PS_IN i)
    : COLOR {
  float4 o;

  float4 r0;
  float4 r1;
  r0 = tex2D(g_Sampler0, i.texcoord);
  r1.xyz = r0.xyz * i.texcoord2.x + i.texcoord2.y;
  r0.w = dot(r0.wxw, i.texcoord2.xyz);
  r0.xyz = r1.xyz + i.texcoord2.z;
  r0 = r0 * i.color;
  r1 = tex2D(g_Sampler1, i.texcoord1);
  o.w = r0.w * r1.w;
  o.xyz = r0.xyz;

  o.rgb = ScaleUI(o.rgb);

  return o;
}
