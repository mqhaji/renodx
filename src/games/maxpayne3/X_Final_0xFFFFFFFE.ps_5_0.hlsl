#include "./shared.h"

SamplerState sourceSampler_s : register(s0);
Texture2D<float4> sourceTexture : register(t0);

/**
 * Calculates the average luminance of several points sampled from the texture.
 * The luminance is based on linearized color values.
 *
 * @param tex - The texture to sample luminance values from.
 * @param sampler - The sampler state to use for texture sampling.
 * @return The average luminance of the sampled points.
 */
float SampleAverageLuminance(Texture2D<float4> sourceTexture, SamplerState sourceSampler_s) {
    const int sampleCount = 4;
    float sumLuminance = 0.0f;
    
    float2 samplePoints[4] = {
        float2(0.25, 0.25),
        float2(0.75, 0.25),
        float2(0.25, 0.75),
        float2(0.75, 0.75)
    };

    // Loop over sample points and accumulate luminance from linearized colors
    for (int i = 0; i < sampleCount; i++) {
        float4 sampledColor = sourceTexture.Sample(sourceSampler_s, samplePoints[i]);
        // Linearize the sampled color (gamma to linear)
        float3 linearColor = renodx::math::SafePow(sampledColor.rgb, 2.2f);
        // Calculate luminance from the linearized color
        sumLuminance += renodx::color::y::from::BT709(linearColor);
    }

    // Return the average luminance
    return sumLuminance / sampleCount;
}

void main(
        float4 vpos : SV_Position,
        float2 texcoord : TEXCOORD,
    out float4 output : SV_Target0)
{
    // Sample the texture and linearize the color (gamma to linear space)
    float4 color = sourceTexture.Sample(sourceSampler_s, texcoord.xy);
    color.rgb = renodx::math::SafePow(color.rgb, 2.2f);

    if (injectedData.adaptiveTonemap) {
        // Sample average luminance from the linearized source texture
        float avgLuminance = SampleAverageLuminance(sourceTexture, sourceSampler_s);
        
        // Set max brightness limit based on paper white or any other target value
        float maxAllowedLuminance = 0.75f;
        
        // Scale down luminance if it exceeds 75% of paper white
        if (avgLuminance > maxAllowedLuminance) {
            float scaleFactor = maxAllowedLuminance / avgLuminance;
            color.rgb *= scaleFactor; // Directly scale the color
        }

        #if 0   // Optional: draw the sampled luminance in the bottom right corner
            float grayscale = avgLuminance / maxAllowedLuminance;
            color.rgb = float3(grayscale, grayscale, grayscale);
        #endif
    }

    // Apply paper white scaling
    color.rgb *= injectedData.toneMapGameNits / 80.0f;

    // Output the final color
    output.rgba = color;
}
