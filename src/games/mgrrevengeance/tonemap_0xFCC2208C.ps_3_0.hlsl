#include "./shared.h"

sampler2D ColorTableTexture : register(s2);
sampler2D NoiseTexture : register(s3);
float4 g_AddColor : register(c185);
float4 g_BloomColor : register(c187);
float4 g_Brightness : register(c188);
float4 g_MulColor : register(c184);
float4 g_NoiseColor : register(c190);
float4 g_NoiseOffset : register(c189);
float4 g_Saido : register(c186);
sampler2D g_Sampler[2] : register(s0);

float4 main(float2 texcoord: TEXCOORD)
    : COLOR {
  float4 o;
  float4 working_color;

  float4 r0;
  float4 r1;
  float4 r2;
  working_color = tex2D(g_Sampler[0], texcoord);

#if 0
  float max_channel = max(renodx::math::Max(r0.rgb), 1.f);
  r0.rgb /= max_channel;
#endif

  if (RENODX_COLOR_GRADE_STRENGTH != 0.f) {
    float3 ungraded = working_color.rgb;

    float3 uv = working_color.rgb;
    if (RENODX_COLOR_GRADE_FIX_LUT_SAMPLING != 0.f) {  // add lut offsets
      uv *= (255.0 / 256.0) + (0.5 / 256.0);
    }

    float3 graded = float3(
        tex2D(ColorTableTexture, float2(uv.r, 0)).x,
        tex2D(ColorTableTexture, float2(uv.g, 0)).x,
        tex2D(ColorTableTexture, float2(uv.b, 0)).x);

    working_color.rgb = lerp(ungraded, graded, RENODX_COLOR_GRADE_STRENGTH);
  }

#if 0
  r0.rgb *= max_channel;
#endif

  float grayscale = renodx::color::luma::from::BT601(working_color.rgb);
  working_color.rgb = lerp(grayscale, working_color.rgb, 1.0 + g_Saido.rgb);

  r1.xz = float2(1, -1);  // c0.xz
  r0.w = texcoord.y * -g_AddColor.w + r1.x;
  r2.xyz = saturate(r0.w * g_AddColor.xyz);
  working_color.rgb = working_color.rgb * g_MulColor.xyz + r2.xyz;

  float4 bloom = tex2D(g_Sampler[1], texcoord);
  working_color = bloom * g_BloomColor * CUSTOM_BLOOM + float4(working_color.rgb, 0);
  working_color *= g_Brightness;

  if (CUSTOM_GRAIN_TYPE == 0.f) {
    r1.yw = float2(texcoord.x * g_NoiseOffset.x + g_NoiseOffset.z,
                   texcoord.y * g_NoiseOffset.y + g_NoiseOffset.w);
    r2 = tex2D(NoiseTexture, r1.ywzw);
    r2 = r2 * g_NoiseColor + r1.z;
    r1 = g_NoiseColor.w * r2 + r1.x;
    working_color = working_color * r1;
  }

#if 1
  if (RENODX_TONE_MAP_TYPE != 0.f || CUSTOM_GRAIN_TYPE != 0.f) {
    working_color.rgb = renodx::color::gamma::DecodeSafe(working_color.rgb, 2.2f);

    if (RENODX_TONE_MAP_TYPE == 2.f) {
      working_color.rgb = renodx::color::correct::Chrominance(working_color.rgb, renodx::tonemap::ExponentialRollOff(working_color.rgb, 1.f, 3.f));
      working_color.rgb = renodx::tonemap::HermiteSplineLuminanceRolloff(working_color.rgb, 1.f, 10.f);
    }

    if (CUSTOM_GRAIN_TYPE != 0.f) {
      working_color.rgb = renodx::effects::ApplyFilmGrain(
          working_color.rgb,
          texcoord.xy,
          CUSTOM_RANDOM,
          CUSTOM_GRAIN_STRENGTH * 0.03f);
    }

    working_color.rgb = renodx::color::gamma::EncodeSafe(working_color.rgb, 2.2f);
  }
#endif

  return working_color;
}
