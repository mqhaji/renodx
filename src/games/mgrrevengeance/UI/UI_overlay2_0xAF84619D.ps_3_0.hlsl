#include "./ui.hlsli"

sampler2D g_Sampler0 : register(s13);
float g_TexelWeights0 : register(c74);
float g_TexelWeights1 : register(c75);
float g_TexelWeights2 : register(c76);
float g_TexelWeights3 : register(c77);
float g_TexelWeights4 : register(c78);
float g_TexelWeights5 : register(c79);
float g_TexelWeights6 : register(c80);
float g_TexelWeights7 : register(c81);
float2 g_TotalOffset : register(c65);

struct PS_IN {
  float4 color : COLOR;
  float2 texcoord : TEXCOORD;
  float2 texcoord1 : TEXCOORD1;
  float2 texcoord2 : TEXCOORD2;
  float2 texcoord3 : TEXCOORD3;
  float2 texcoord4 : TEXCOORD4;
  float2 texcoord5 : TEXCOORD5;
  float2 texcoord6 : TEXCOORD6;
  float2 texcoord7 : TEXCOORD7;
};

float4 main(PS_IN i)
    : COLOR {
  float4 o;

  float4 r0;
  float4 r1;
  float4 r2;
  r0.xy = g_TotalOffset.xy + i.texcoord6.xy;
  r0 = tex2D(g_Sampler0, r0);
  r1 = tex2D(g_Sampler0, i.texcoord1);
  r0.xyz = r0.xyz + r1.xyz;
  r0.xyz = r0.xyz * g_TexelWeights1.x;
  r1.xy = g_TotalOffset.xy + i.texcoord7.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights0.x * r1.xyz + r0.xyz;
  r1.xy = g_TotalOffset.xy + i.texcoord5.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord2);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights2.x * r1.xyz + r0.xyz;
  r1.xy = g_TotalOffset.xy + i.texcoord4.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord3);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights3.x * r1.xyz + r0.xyz;
  r1.xy = g_TotalOffset.xy + i.texcoord3.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord4);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights4.x * r1.xyz + r0.xyz;
  r1.xy = g_TotalOffset.xy + i.texcoord2.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord5);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights5.x * r1.xyz + r0.xyz;
  r1.xy = g_TotalOffset.xy + i.texcoord1.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord6);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights6.x * r1.xyz + r0.xyz;
  r1.xy = g_TotalOffset.xy + i.texcoord.xy;
  r1 = tex2D(g_Sampler0, r1);
  r2 = tex2D(g_Sampler0, i.texcoord7);
  r1.xyz = r1.xyz + r2.xyz;
  r0.xyz = g_TexelWeights7.x * r1.xyz + r0.xyz;
  r0.w = 1;
  o = r0 * i.color;

  o.rgb = ScaleUI(o.rgb);

  return o;
}
