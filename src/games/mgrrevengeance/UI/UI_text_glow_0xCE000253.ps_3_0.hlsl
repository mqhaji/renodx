#include "./ui.hlsli"

float2 g_GridScale : register(c69);
sampler2D g_Sampler0 : register(s13);
sampler2D g_Sampler1 : register(s14);
sampler2D g_Sampler2 : register(s15);

struct PS_IN {
  float2 texcoord : TEXCOORD;
  float2 texcoord1 : TEXCOORD1;
  float3 texcoord2 : TEXCOORD2;
  float3 texcoord3 : TEXCOORD3;
  float2 texcoord4 : TEXCOORD4;
  float4 color : COLOR;
};

float4 main(PS_IN i)
    : COLOR {
  float4 o;

  float4 r0;
  float4 r1;
  float4 r2;
  r0 = tex2D(g_Sampler0, i.texcoord);
  r0.xy = r0.yz * i.texcoord2.x + i.texcoord2.y;
  r0.xy = r0.xy + i.texcoord2.z;
  r0.z = r0.w * i.color.w;
  r1.yz = r0.z * r0.xxyw;  //  mul r1.yz, r0.z, r0.xxyw (not verified)
  r2 = tex2D(g_Sampler0, i.texcoord4);
  r0.x = r2.x * i.texcoord2.x + i.texcoord2.y;
  r0.x = r0.x + i.texcoord2.z;
  r2.x = r2.w * i.color.w;
  r0.w = r0.w + r2.w;
  r0.w = r0.w * 0.5;
  r2.y = r0.x * r2.x + r1.y;
  r1.x = r0.x * r2.x;
  r0.x = r0.y * r0.z + r2.y;
  r0.xy = r0.xw * i.texcoord2.yx;
  r0.x = r0.x * 0.33333334 + r0.y;
  r1.w = r0.w * i.texcoord2.z + r0.x;
  r0 = r1 * i.color;
  r1 = tex2D(g_Sampler1, i.texcoord1);
  r0.w = r0.w * r1.w;
  o.xyz = r0.xyz;
  r0.x = 1 / i.texcoord3.z;
  r0.xy = i.texcoord3.xy * r0.x + 1;
  r0.xy = r0.xy * g_GridScale.xy;
  r0.xy = r0.xy * 0.5;
  r1 = tex2D(g_Sampler2, r0);
  o.w = r0.w * r1.w;

  o.rgb = ScaleUI(o.rgb);

  return o;
}
