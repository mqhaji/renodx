#define USE_SETTINGS_PUSHCONSTANTS

#include "./shared.h"
// --------------------------------------------------------------
// Shared definitions
// --------------------------------------------------------------

// Push constants
struct Settings
{
    float gamma;
    uint  hdr;
};

[[vk::push_constant]]
ConstantBuffer<Settings> pp;

// Texture + sampler binding
[[vk::binding(0, 0)]]
Texture2D texSampler;

[[vk::binding(1, 0)]]
SamplerState texSampler_sampler;

// --------------------------------------------------------------
// Gamma correction function
// --------------------------------------------------------------
float3 gamma_fn(float3 rgb) {
  float inv = 1.0 / (3.400000095367431640625 - pp.gamma);

  float3 power_term = pow(rgb, float3(inv)) * 1.05499994754791259765625
                      - float3(0.054999999701976776123046875);

  float3 linear_term = (rgb * 12.9200000762939453125) / float3(pp.gamma);

  // Equivalent of GLSL's mix(..., lessThan(...))
  bool3 cond = rgb < float3(0.003130800090730190277099609375);
  return select(power_term, linear_term, cond);
}

// --------------------------------------------------------------
// Main shader
// --------------------------------------------------------------
float4 main(float2 uv: TEXCOORD0) : SV_Target0 {
  float4 color_linear = texSampler.Sample(texSampler_sampler, uv);

  float4 color;

  color = color_linear;

  // Optional tone map hook

  color = renodx::draw::SwapChainPass(color);
  return color;
  
  if (pp.hdr != 0u) {
    color = color_linear;
  } else {
    color = float4(gamma_fn(color_linear.xyz), color_linear.w);
  }

  return color;
}
