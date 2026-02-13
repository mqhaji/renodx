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

float ComputeMaxChCompressionScale(float3 untonemapped) {
  float peak = renodx::math::Max(untonemapped);
  float mapped_peak = renodx::tonemap::Neutwo(peak);
  float scale = renodx::math::DivideSafe(mapped_peak, peak, 1.f);

  return scale;
}

float3 CorrectHueAndChrominanceOKLab(
    float3 incorrect_color_bt709,
    float3 reference_color_bt709,
    float hue_emulation_strength = 0.f,
    float chrominance_emulation_strength = 0.f,
    float hue_emulation_ramp_start = 0.f,
    float hue_emulation_ramp_end = 0.f) {
  if (hue_emulation_strength == 0.0 && chrominance_emulation_strength == 0.0) {
    return incorrect_color_bt709;
  }

  float3 perceptual_new = renodx::color::oklab::from::BT709(incorrect_color_bt709);
  float3 perceptual_reference = renodx::color::oklab::from::BT709(reference_color_bt709);

  float chrominance_current = length(perceptual_new.yz);
  float chrominance_ratio = 1.0;

  if (hue_emulation_strength != 0.0) {
    if (hue_emulation_ramp_end != 0.f) {
      float ramp_denom = hue_emulation_ramp_end - hue_emulation_ramp_start;
      float ramp_t = clamp(renodx::math::DivideSafe(perceptual_new.x - hue_emulation_ramp_start, ramp_denom, 0.0), 0.0, 1.0);
      hue_emulation_strength *= ramp_t;
    }

    float chrominance_pre = chrominance_current;
    perceptual_new.yz = lerp(perceptual_new.yz, perceptual_reference.yz, hue_emulation_strength);
    float chrominance_post = length(perceptual_new.yz);
    chrominance_ratio = renodx::math::DivideSafe(chrominance_pre, chrominance_post, 1.0);
    chrominance_current = chrominance_post;
  }

  if (chrominance_emulation_strength != 0.0) {
    float reference_chrominance = length(perceptual_reference.yz);
    float target_chrominance_ratio = renodx::math::DivideSafe(reference_chrominance, chrominance_current, 1.0);
    chrominance_ratio = lerp(chrominance_ratio, target_chrominance_ratio, chrominance_emulation_strength);
  }

  perceptual_new.yz *= chrominance_ratio;

  float3 corrected_color_bt709 = renodx::color::bt709::from::OkLab(perceptual_new);
  return corrected_color_bt709;
}

float4 main(float2 texcoord: TEXCOORD)
    : COLOR {
  float4 o;
  float4 working_color;

  float4 r0;
  float4 r1;
  float4 r2;
  working_color = tex2D(g_Sampler[0], texcoord);

  working_color.a = saturate(working_color.a);
  if (RENODX_TONE_MAP_TYPE == 0.f) {
    working_color = saturate(working_color);
  }

  if (RENODX_COLOR_GRADE_STRENGTH != 0.f) {
    float3 ungraded = working_color.rgb;

    float scale = 1.f;
    if (RENODX_TONE_MAP_TYPE != 0.f) {
      ungraded = renodx::color::gamma::DecodeSafe(ungraded);
      scale = ComputeMaxChCompressionScale(ungraded);
      ungraded *= scale;
      ungraded = renodx::color::gamma::EncodeSafe(ungraded);
    }

    float3 uv = ungraded;
    if (RENODX_COLOR_GRADE_FIX_LUT_SAMPLING != 0.f) {  // add lut offsets
      uv = uv * (255.0 / 256.0) + (0.5 / 256.0);
    }

    float3 graded = float3(
        tex2D(ColorTableTexture, float2(uv.r, 0)).x,
        tex2D(ColorTableTexture, float2(uv.g, 0)).x,
        tex2D(ColorTableTexture, float2(uv.b, 0)).x);

    if (RENODX_TONE_MAP_TYPE != 0.f) {
      graded = renodx::color::gamma::DecodeSafe(graded);
      graded /= scale;
      graded = renodx::color::gamma::EncodeSafe(graded);
    }

    working_color.rgb = lerp(ungraded, graded, RENODX_COLOR_GRADE_STRENGTH);
  }

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
      working_color.rgb = CorrectHueAndChrominanceOKLab(working_color.rgb, renodx::tonemap::neutwo::PerChannel(working_color.rgb, 3.f), 1.f, 1.f, 0.f, 0.f);
      working_color.rgb = renodx::tonemap::neutwo::MaxChannel(working_color.rgb, 1.f, 100.f);
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
