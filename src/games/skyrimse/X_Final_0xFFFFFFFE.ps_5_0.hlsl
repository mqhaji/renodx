#include "./shared.h"

SamplerState sourceSampler_s : register(s0);
Texture2D<float4> sourceTexture : register(t0);

void main(
        float4 vpos : SV_Position,
        float2 texcoord : TEXCOORD,
    out float4 output : SV_Target0)
{
    float4 color = sourceTexture.Sample(sourceSampler_s, texcoord.xy);

    if (injectedData.toneMapGammaCorrection == 1) {
      color.rgb = renodx::color::gamma::DecodeSafe(color.rgb, 2.2);
    } else {
      color.rgb = renodx::color::srgb::DecodeSafe(color.rgb);
    }
    color.rgb *= injectedData.toneMapGameNits / 80.f;
    color.rgb = renodx::color::bt709::clamp::AP1(color.rgb);  // clamp to AP1
    
    output.rgba = color;
}