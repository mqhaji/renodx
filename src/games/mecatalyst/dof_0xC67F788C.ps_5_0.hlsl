// ---- Created with 3Dmigoto v1.3.16 on Wed Sep 18 20:05:14 2024

cbuffer _Globals : register(b0) {
  float2 invPixelSize : packoffset(c0);
  float preBlendAmount : packoffset(c0.z);
  float postAddAmount : packoffset(c0.w);
  float4 parametricTonemapParams : packoffset(c1);
  float4 parametricTonemapToeCoeffs : packoffset(c2);
  float4 parametricTonemapShoulderCoeffs : packoffset(c3);
  float3 filmGrainColorScale : packoffset(c4);
  float4 filmGrainTextureScaleAndOffset : packoffset(c5);
  float4 color : packoffset(c6);
  float4 colorMatrix0 : packoffset(c7);
  float4 colorMatrix1 : packoffset(c8);
  float4 colorMatrix2 : packoffset(c9);
  float4 ironsightsDofParams : packoffset(c10);
  float4 filmicLensDistortParams : packoffset(c11);
  float4 colorScale : packoffset(c12);
  float4 runnersVisionColor : packoffset(c13);
  float3 depthScaleFactors : packoffset(c14);
  float4 dofParams : packoffset(c15);
  float4 dofParams2 : packoffset(c16);
  float4 dofDebugParams : packoffset(c17);
  float3 bloomScale : packoffset(c18);
  float3 lensDirtExponent : packoffset(c19);
  float3 lensDirtFactor : packoffset(c20);
  float3 lensDirtBias : packoffset(c21);
  float4 tonemapCoeffA : packoffset(c22);
  float4 tonemapCoeffB : packoffset(c23);
  float3 luminanceVector : packoffset(c24);
  float3 vignetteParams : packoffset(c25);
  float4 vignetteColor : packoffset(c26);
  float4 chromostereopsisParams : packoffset(c27);
  float4 distortionScaleOffset : packoffset(c28);
  float3 maxClampColor : packoffset(c29);
  float fftBloomSpikeDampingScale : packoffset(c29.w);
  float4 fftKernelSampleScales : packoffset(c30);
}

SamplerState depthTextureSampler_s : register(s0);
SamplerState dofBlurTextureSampler_s : register(s1);
Texture2D<float4> depthTexture : register(t0);
Texture2D<float4> dofBlurTexture : register(t1);

// 3Dmigoto declarations
#define cmp -

void main(
    float4 screenPos: SV_Position0,      // Screen-space position
    float4 texCoord: TEXCOORD0,          // Texture coordinates
    float2 uv: TEXCOORD1,                // 2D texture coordinates
    out float4 outputColor: SV_Target0)  // Output color
{
  // Sample the depth texture to get the depth value at the current pixel
  float depth = depthTexture.Sample(depthTextureSampler_s, uv.xy).x;

  // Calculate the difference between the current depth and the DOF focal depth
  float2 depthDiff = depth - dofParams.xy;

  // Ensure the DOF range parameters are not too small
  float2 clampedDOFParams = max(0.0001, dofParams.zw);

  // Normalize depth difference by multiplying with the clamped DOF parameters and saturate the result
  depthDiff = saturate(depthDiff * clampedDOFParams);

  // Adjust the normalized depth difference to get the blur factor
  float blurFactor = depthDiff.x - depthDiff.y;
  outputColor.a = 1.0 - blurFactor;

  // Sample the blurred texture at the current pixel position
  float3 blurredColor = dofBlurTexture.Sample(dofBlurTextureSampler_s, uv.xy).xyz;
  outputColor.rgb = blurredColor;
}
