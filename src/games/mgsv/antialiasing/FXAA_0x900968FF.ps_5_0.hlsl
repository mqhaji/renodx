#include "./antialiasing.hlsli"
// ---- Created with 3Dmigoto v1.4.1 on Tue Feb 17 09:33:29 2026

// clang-format off
cbuffer cPSSystem : register(b0)
{
  struct
  {
    float4 m_param;
    float4 m_renderInfo;
    float4 m_renderBuffer;
    float4 m_dominantLightDir;
  } g_psSystem : packoffset(c0);
}
// clang-format on

SamplerState g_samplerLinear_Clamp_s : register(s11);
Texture2D<float4> g_Image : register(t0);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 v0: SV_Position0,
    float2 v1: TEXCOORD0,
    out float4 o0: SV_Target0) {
  float4 r0, r1, r2, r3, r4, r5, r6;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = float2(-0.5, -0.5) + v0.xy;
  r0.xy = float2(0.5, 0.5) + r0.xy;
  r0.xy = g_psSystem.m_renderBuffer.zw * r0.xy;
  r0.zw = g_psSystem.m_renderBuffer.zw;
  r0.xy = r0.xy;
  r0.zw = r0.zw;
  r0.x = r0.x;
  r0.y = r0.y;
  r1.xyz = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r0.xy, 0).xyz;
  r2.xyz = renodx_game::antialiasing::GatherScene(g_Image, g_samplerLinear_Clamp_s, r0.xy).xyz;
  r2.xyz = r2.xyz;
  r3.xyz = renodx_game::antialiasing::GatherScene(g_Image, g_samplerLinear_Clamp_s, r0.xy, int2(-1, -1)).zxw;
  r3.xyz = r3.xyz;
  r1.w = max(r2.x, r1.y);
  r2.w = min(r2.x, r1.y);
  r1.w = max(r2.z, r1.w);
  r2.w = min(r2.z, r2.w);
  r3.w = max(r3.x, r3.y);
  r4.x = min(r3.x, r3.y);
  r1.w = max(r3.w, r1.w);
  r2.w = min(r4.x, r2.w);
  r3.w = 0.166659996 * r1.w;
  r2.w = -r2.w;
  r1.w = r2.w + r1.w;
  r2.w = max(0.083329998, r3.w);
  r2.w = cmp(r1.w < r2.w);
  if (r2.w != 0) {
    r4.xyz = r1.xyz;
  }
  if (r2.w == 0) {
    r1.x = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r0.xy, 0, int2(1, -1)).y;
    r1.x = r1.x;
    r1.x = r1.x;
    r1.z = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r0.xy, 0, int2(-1, 1)).y;
    r1.z = r1.z;
    r1.z = r1.z;
    r2.w = r3.x + r2.x;
    r3.w = r3.y + r2.z;
    r1.w = 1 / r1.w;
    r4.w = r3.w + r2.w;
    r5.x = -2 * r1.y;
    r2.w = r5.x + r2.w;
    r3.w = r5.x + r3.w;
    r5.x = r1.x + r2.y;
    r1.x = r3.z + r1.x;
    r5.y = -2 * r2.z;
    r5.y = r5.y + r5.x;
    r5.z = -2 * r3.x;
    r1.x = r5.z + r1.x;
    r3.z = r3.z + r1.z;
    r1.z = r1.z + r2.y;
    r2.y = -r2.w;
    r2.y = max(r2.w, r2.y);
    r2.y = 2 * r2.y;
    r2.w = -r5.y;
    r2.w = max(r5.y, r2.w);
    r2.y = r2.y + r2.w;
    r2.w = -r3.w;
    r2.w = max(r3.w, r2.w);
    r2.w = 2 * r2.w;
    r3.w = -r1.x;
    r1.x = max(r3.w, r1.x);
    r1.x = r2.w + r1.x;
    r2.w = -2 * r3.y;
    r2.w = r2.w + r3.z;
    r3.w = -2 * r2.x;
    r1.z = r3.w + r1.z;
    r3.w = -r2.w;
    r2.w = max(r3.w, r2.w);
    r2.y = r2.w + r2.y;
    r2.w = -r1.z;
    r1.z = max(r2.w, r1.z);
    r1.x = r1.z + r1.x;
    r1.z = r3.z + r5.x;
    r2.w = r0.z;
    r1.x = cmp(r2.y >= r1.x);
    r2.y = 2 * r4.w;
    r1.z = r2.y + r1.z;
    r2.y = ~(int)r1.x;
    if (r1.x == 0) {
      r3.x = r3.y;
    }
    if (r1.x == 0) {
      r2.x = r2.z;
    }
    if (r1.x != 0) {
      r2.w = r0.w;
    }
    r1.z = 0.0833333358 * r1.z;
    r2.z = -r1.y;
    r1.z = r2.z + r1.z;
    r3.y = r3.x + r2.z;
    r2.z = r2.x + r2.z;
    r3.x = r3.x + r1.y;
    r2.x = r2.x + r1.y;
    r3.z = -r3.y;
    r3.y = max(r3.y, r3.z);
    r3.z = -r2.z;
    r2.z = max(r3.z, r2.z);
    r3.z = cmp(r3.y >= r2.z);
    r2.z = max(r3.y, r2.z);
    if (r3.z != 0) {
      r2.w = -r2.w;
    }
    r3.y = -r1.z;
    r1.z = max(r3.y, r1.z);
    r1.z = r1.z * r1.w;
    r1.z = max(0, r1.z);
    r1.z = min(1, r1.z);
    r1.w = r0.x;
    r3.y = r0.y;
    r0.z = r2.y ? 0 : r0.z;
    r0.w = r1.x ? 0 : r0.w;
    if (r1.x == 0) {
      r2.y = 0.5 * r2.w;
      r1.w = r2.y + r1.w;
    }
    if (r1.x != 0) {
      r2.y = 0.5 * r2.w;
      r3.y = r3.y + r2.y;
    }
    r2.y = 1 * r0.z;
    r3.w = -r2.y;
    r5.x = r3.w + r1.w;
    r3.w = 1 * r0.w;
    r4.w = -r3.w;
    r5.y = r4.w + r3.y;
    r5.z = r2.y + r1.w;
    r5.w = r3.y + r3.w;
    r1.w = -2 * r1.z;
    r1.w = 3 + r1.w;
    r2.y = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.xy, 0).y;
    r2.y = r2.y;
    r2.y = r2.y;
    r2.y = r2.y;
    r1.z = r1.z * r1.z;
    r3.y = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.zw, 0).y;
    r3.y = r3.y;
    r3.y = r3.y;
    if (r3.z == 0) {
      r3.x = r2.x;
    }
    r2.x = 1 * r2.z;
    r2.x = r2.x / 4;
    r2.z = 0.5 * r3.x;
    r2.z = -r2.z;
    r1.y = r2.z + r1.y;
    r1.z = r1.w * r1.z;
    r1.y = cmp(r1.y < 0);
    r6.x = r2.y + r2.z;
    r6.y = r3.y + r2.z;
    r1.w = -r6.x;
    r1.w = max(r6.x, r1.w);
    r1.w = cmp(r1.w >= r2.x);
    r2.y = -r6.y;
    r2.y = max(r6.y, r2.y);
    r2.y = cmp(r2.y >= r2.x);
    r3.x = ~(int)r1.w;
    if (r1.w == 0) {
      r3.y = 1.5 * r0.z;
      r3.y = -r3.y;
      r5.x = r5.x + r3.y;
    }
    if (r1.w == 0) {
      r3.y = 1.5 * r0.w;
      r3.y = -r3.y;
      r5.y = r5.y + r3.y;
    }
    r3.y = ~(int)r2.y;
    r3.x = (int)r3.y | (int)r3.x;
    if (r2.y == 0) {
      r3.y = 1.5 * r0.z;
      r5.z = r5.z + r3.y;
    }
    if (r2.y == 0) {
      r3.y = 1.5 * r0.w;
      r5.w = r5.w + r3.y;
    }
    if (r3.x != 0) {
      if (r1.w == 0) {
        r6.x = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.xy, 0).y;
        r6.x = r6.x;
        r6.x = r6.x;
      }
      if (r2.y == 0) {
        r6.y = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.zw, 0).y;
        r6.y = r6.y;
        r6.y = r6.y;
      }
      if (r1.w == 0) {
        r6.x = r6.x + r2.z;
      }
      if (r2.y == 0) {
        r6.y = r6.y + r2.z;
      }
      r1.w = -r6.x;
      r1.w = max(r6.x, r1.w);
      r1.w = cmp(r1.w >= r2.x);
      r2.y = -r6.y;
      r2.y = max(r6.y, r2.y);
      r2.y = cmp(r2.y >= r2.x);
      r3.x = ~(int)r1.w;
      if (r1.w == 0) {
        r3.y = 2 * r0.z;
        r3.y = -r3.y;
        r5.x = r5.x + r3.y;
      }
      if (r1.w == 0) {
        r3.y = 2 * r0.w;
        r3.y = -r3.y;
        r5.y = r5.y + r3.y;
      }
      r3.y = ~(int)r2.y;
      r3.x = (int)r3.y | (int)r3.x;
      if (r2.y == 0) {
        r3.y = 2 * r0.z;
        r5.z = r5.z + r3.y;
      }
      if (r2.y == 0) {
        r3.y = 2 * r0.w;
        r5.w = r5.w + r3.y;
      }
      if (r3.x != 0) {
        if (r1.w == 0) {
          r6.x = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.xy, 0).y;
          r6.x = r6.x;
          r6.x = r6.x;
        }
        if (r2.y == 0) {
          r6.y = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.zw, 0).y;
          r6.y = r6.y;
          r6.y = r6.y;
        }
        if (r1.w == 0) {
          r6.x = r6.x + r2.z;
        }
        if (r2.y == 0) {
          r6.y = r6.y + r2.z;
        }
        r1.w = -r6.x;
        r1.w = max(r6.x, r1.w);
        r1.w = cmp(r1.w >= r2.x);
        r2.y = -r6.y;
        r2.y = max(r6.y, r2.y);
        r2.y = cmp(r2.y >= r2.x);
        r3.x = ~(int)r1.w;
        if (r1.w == 0) {
          r3.y = 4 * r0.z;
          r3.y = -r3.y;
          r5.x = r5.x + r3.y;
        }
        if (r1.w == 0) {
          r3.y = 4 * r0.w;
          r3.y = -r3.y;
          r5.y = r5.y + r3.y;
        }
        r3.y = ~(int)r2.y;
        r3.x = (int)r3.y | (int)r3.x;
        if (r2.y == 0) {
          r3.y = 4 * r0.z;
          r5.z = r5.z + r3.y;
        }
        if (r2.y == 0) {
          r3.y = 4 * r0.w;
          r5.w = r5.w + r3.y;
        }
        if (r3.x != 0) {
          if (r1.w == 0) {
            r6.x = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.xy, 0).y;
            r6.x = r6.x;
            r6.x = r6.x;
          }
          if (r2.y == 0) {
            r6.y = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r5.zw, 0).y;
            r6.y = r6.y;
            r6.y = r6.y;
          }
          if (r1.w == 0) {
            r6.x = r6.x + r2.z;
          }
          if (r2.y == 0) {
            r6.y = r6.y + r2.z;
          }
          r1.w = -r6.x;
          r1.w = max(r6.x, r1.w);
          r1.w = cmp(r1.w >= r2.x);
          r2.y = -r6.y;
          r2.y = max(r6.y, r2.y);
          r2.x = cmp(r2.y >= r2.x);
          if (r1.w == 0) {
            r2.y = 12 * r0.z;
            r2.y = -r2.y;
            r5.x = r5.x + r2.y;
          }
          if (r1.w == 0) {
            r1.w = 12 * r0.w;
            r1.w = -r1.w;
            r5.y = r5.y + r1.w;
          }
          if (r2.x == 0) {
            r0.z = 12 * r0.z;
            r5.z = r5.z + r0.z;
          }
          if (r2.x == 0) {
            r0.z = 12 * r0.w;
            r5.w = r5.w + r0.z;
          }
        }
      }
    }
    r0.z = -r5.x;
    r0.z = r0.x + r0.z;
    r0.w = -r0.x;
    r0.w = r5.z + r0.w;
    if (r1.x == 0) {
      r1.w = -r5.y;
      r0.z = r1.w + r0.y;
    }
    if (r1.x == 0) {
      r1.w = -r0.y;
      r0.w = r5.w + r1.w;
    }
    r1.w = cmp(r6.x < 0);
    r1.w = cmp((int)r1.w != (int)r1.y);
    r2.x = r0.w + r0.z;
    r2.y = cmp(r6.y < 0);
    r1.y = cmp((int)r2.y != (int)r1.y);
    r2.x = 1 / r2.x;
    r2.y = cmp(r0.z < r0.w);
    r0.z = min(r0.z, r0.w);
    r0.w = r2.y ? r1.w : r1.y;
    r1.y = r1.z * r1.z;
    r1.z = -r2.x;
    r0.z = r1.z * r0.z;
    r0.z = 0.5 + r0.z;
    r1.y = 0.75 * r1.y;
    r0.z = r0.w ? r0.z : 0;
    r0.z = max(r0.z, r1.y);
    if (r1.x == 0) {
      r0.w = r0.z * r2.w;
      r0.x = r0.x + r0.w;
    }
    if (r1.x != 0) {
      r0.z = r0.z * r2.w;
      r0.y = r0.y + r0.z;
    }
    r4.xyz = renodx_game::antialiasing::SampleLevelScene(g_Image, g_samplerLinear_Clamp_s, r0.xy, 0).xyz;
  }
  o0.xyz = renodx_game::antialiasing::ApplySceneScale(r4.xyz);
  o0.w = 0;
  return;
}
