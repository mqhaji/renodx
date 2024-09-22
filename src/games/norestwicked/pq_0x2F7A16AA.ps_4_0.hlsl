#include "./shared.h"
#include "./hueHelper.hlsl"

Texture2D<float4> _MainTex : register(t0);
SamplerState _MainSampler : register(s0);

cbuffer Constants : register(b0)
{
    float4 _ColorProperties[7];
}

void main(
    float4 position : SV_POSITION0,
    float2 texCoords : TEXCOORD0,
    out float4 outputColor : SV_Target0)
{
    float4 texColor = _MainTex.Sample(_MainSampler, texCoords.xy);

    if (injectedData.toneMapGammaCorrection) {  // fix gamma mismatch
      texColor.rgb = renodx::color::correct::GammaSafe(texColor.rgb);
    }

    if (injectedData.toneMapType) {    // apply DICE tonemapper
      const float paperWhiteNits = injectedData.toneMapGameNits / renodx::color::srgb::REFERENCE_WHITE;
      const float peakWhiteNits = injectedData.toneMapPeakNits / renodx::color::srgb::REFERENCE_WHITE;
      const float highlightsShoulderStart = paperWhiteNits;
      texColor.rgb *= paperWhiteNits;  // multiply paper white in
      texColor.rgb = renodx::tonemap::dice::BT709(texColor.rgb, peakWhiteNits, highlightsShoulderStart);
      texColor.rgb /= paperWhiteNits;  // normalize paper white
    }

    texColor.rgb = Hue(texColor.rgb, injectedData.toneMapHueCorrection);


    // Convert color from BT709 to BT2020 color space and encode in PQ
    float3 colorBT2020 = renodx::color::bt2020::from::BT709(texColor.rgb);
    float3 colorPQ = renodx::color::pq::from::BT2020(colorBT2020, injectedData.toneMapGameNits);

    outputColor.rgb = float4(colorPQ, texColor.a);
    return;
}
