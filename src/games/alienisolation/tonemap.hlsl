#include "./common.hlsl"
#include "./include/CBuffer_DefaultPSC.hlsl"
#include "./include/CBuffer_DefaultXSC.hlsl"
#include "./include/CBuffer_UbershaderXSC.hlsl"

float3 applyFilmGrain(float3 input_color, Texture2D<float4> SamplerNoise_TEX,
                      SamplerState SamplerNoise_SMP_s, float4 v1) {
  float3 grained_color;
  if (injectedData.fxFilmGrainType == 0.f) {  // Noise
    float4 r0, r2;
    float3 r1, r3;
    r0.rgb = input_color;

    r1.xyz = SamplerNoise_TEX.Sample(SamplerNoise_SMP_s, v1.xy).xyz;
    r1.xyz = float3(-0.5, -0.5, -0.5) + r1.xyz;
    r0.w = dot(float3(0.298999995, 0.587000012, 0.114), r0.xyz);
    r2.xyz = float3(0, 0.5, 1) + -r0.www;
    r2.xyz = saturate(rp_parameter_ps[7].xyz + -abs(r2.xyz));
    r2.xyz = r2.xyz / rp_parameter_ps[7].xyz;
    r2.xyz = rp_parameter_ps[8].xyz * r2.xyz;
    r3.xyz = r2.yyy * r1.xyz;
    r2.xyw = r1.xyz * r2.xxx + r3.xyz;
    r1.xyz = r1.xyz * r2.zzz + r2.xyw;
    r0.xyz = injectedData.fxFilmGrain * r1.xyz + r0.xyz;

    grained_color = r0.rgb;
  } else {  // Film Grain, applied in linear space
    float3 linear_color = renodx::color::gamma::DecodeSafe(input_color, 2.2f);
    if (injectedData.fxFilmGrainType == 1.f) {  // B&W
      float random_seed = SamplerNoise_TEX.Sample(SamplerNoise_SMP_s, v1.xy).y;
      grained_color = renodx::effects::ApplyFilmGrain(
          linear_color,
          v1.xy,        // Screen-space coordinates
          random_seed,  // Sample noise tex for random seed
          injectedData.fxFilmGrain * .02f);
    } else {  // Colored
      float3 random_seed = SamplerNoise_TEX.Sample(SamplerNoise_SMP_s, v1.xy).rgb;
      grained_color = renodx::effects::ApplyFilmGrainColored(
          linear_color,
          v1.xy,        // Screen-space coordinates
          random_seed,  // Sample noise tex for random seed
          injectedData.fxFilmGrain * .02f);
    }
    grained_color = renodx::color::gamma::EncodeSafe(grained_color, 2.2f);
  }
  return grained_color;
}
