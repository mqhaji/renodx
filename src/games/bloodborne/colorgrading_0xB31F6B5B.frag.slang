#define USE_AUX_PUSHCONSTANTS

#include "./shared.h"
// ------------------------------------------------------------------
// Resources
// ------------------------------------------------------------------
[[vk::binding(0, 0)]]
Texture2D fs_img4;
[[vk::binding(1, 0)]]
Texture3D fs_img16;
[[vk::binding(2, 0)]]
SamplerState fs_sampsgpr_12;
[[vk::binding(3, 0)]]
SamplerState fs_sampsgpr_24;

// ------------------------------------------------------------------
// Push Constants
// ------------------------------------------------------------------
struct AuxData
{
    float xoffset;
    float yoffset;
    float xscale;
    float yscale;
    uint4 ud_regs0;
    uint4 ud_regs1;
    uint4 ud_regs2;
    uint4 ud_regs3;
    uint4 buf_offsets0;
    uint4 buf_offsets1;
    uint2 buf_offsets2;
};

[[vk::push_constant]]
ConstantBuffer<AuxData> push_data;

// ------------------------------------------------------------------
// Inputs / Outputs
// ------------------------------------------------------------------
struct PsInput {
  [[vk::location(0)]]
  float4 attr0 : TEXCOORD0;  // matches out_attr0 from vertex shader
};

struct PsOutput {
  float4 frag_color0 : SV_Target0;
};

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------
[shader("fragment")]
PsOutput main(PsInput IN) {
  PsOutput OUT;

  float2 uv = IN.attr0.xy;

  float4 s = fs_img4.SampleLevel(fs_sampsgpr_12, uv, 0.0);
  float3 untonemapped = s.rgb;

  float3 sdr = s.rgb;
  if (RENODX_TONE_MAP_TYPE) {
    sdr = renodx::tonemap::ReinhardPiecewise(sdr.rgb);
  }
  float3 lutInput = sdr.rgb;

  float3 coord = lutInput * 0.9375 + 0.03125;
  float4 t = fs_img16.Sample(fs_sampsgpr_24, coord);

  OUT.frag_color0 = float4(
      max(0.0, t.z),
      max(0.0, t.y),
      max(0.0, t.x),
      max(0.0, s.w));

  if (RENODX_TONE_MAP_TYPE) {
    OUT.frag_color0.rgb = renodx::tonemap::UpgradeToneMap(untonemapped, sdr, OUT.frag_color0.rgb, 1.f, 1.f);
  }

  // Ignore above
  OUT.frag_color0.rgb = untonemapped;
  return OUT;
}
