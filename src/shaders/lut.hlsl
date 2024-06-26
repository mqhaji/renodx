#ifndef SRC_SHADERS_LUT_HLSL_
#define SRC_SHADERS_LUT_HLSL_

#include "./color.hlsl"

struct LUTParams {
  SamplerState lutSampler;
  float strength;
  float scaling;
  uint inputType;
  uint outputType;
  float size;
  float3 precompute;
};

#define LUT_TYPE__LINEAR            0u
#define LUT_TYPE__SRGB              1u
#define LUT_TYPE__2_4               2u
#define LUT_TYPE__2_2               3u
#define LUT_TYPE__2_0               4u
#define LUT_TYPE__ARRI_C800         5u
#define LUT_TYPE__ARRI_C1000        6u
#define LUT_TYPE__ARRI_C800_NO_CUT  7u
#define LUT_TYPE__ARRI_C1000_NO_CUT 8u
#define LUT_TYPE__PQ                9u

LUTParams buildLUTParams(SamplerState lutSampler, float strength, float scaling, uint inputType, uint outputType, float size = 0) {
  LUTParams params = {lutSampler, strength, scaling, inputType, outputType, size, float(0).xxx};
  return params;
}

LUTParams buildLUTParams(SamplerState lutSampler, float strength, float scaling, uint inputType, uint outputType, float3 precompute) {
  LUTParams params = {lutSampler, strength, scaling, inputType, outputType, 0, precompute};
  return params;
}

// https://www.glowybits.com/blog/2016/12/21/ifl_iss_hdr_1/
float ColorGradeSmoothClamp(float x) {
  const float u = 0.525;
  float q = (2.0 - u - 1.0 / u + x * (2.0 + 2.0 / u - x / u)) / 4.0;
  return (abs(1.0 - x) < u) ? q : saturate(x);
}

float3 centerLUTTexel(float3 color, float size) {
  float scale = (size - 1.f) / size;
  float offset = 1.f / (2.f * size);
  return scale * color + offset;
}

#define sampleLUTTexture3DFunctionGenerator(textureType)                                       \
  float3 sampleLUT(textureType lut, SamplerState samplerState, float3 color, float size = 0) { \
    if (size == 0) {                                                                           \
      /* Removed by compiler if specified */                                                   \
      float width;                                                                             \
      float height;                                                                            \
      float depth;                                                                             \
      lut.GetDimensions(width, height, depth);                                                 \
      size = height;                                                                           \
    }                                                                                          \
                                                                                               \
    float3 position = centerLUTTexel(color, size);                                             \
                                                                                               \
    return lut.SampleLevel(samplerState, position, 0.0f).rgb;                                  \
  }

#define sampleLUTTexture2DPrecomputedFunctionGenerator(textureType)                               \
  float3 sampleLUT(textureType lut, SamplerState samplerState, float3 color, float3 precompute) { \
    float texelSize = precompute.x;                                                               \
    float slice = precompute.y;                                                                   \
    float maxIndex = precompute.z;                                                                \
                                                                                                  \
    float zPosition = color.z * maxIndex;                                                         \
    float zInteger = floor(zPosition);                                                            \
    float zFraction = zPosition - zInteger;                                                       \
    float zOffset = zInteger * slice;                                                             \
                                                                                                  \
    float xOffset = (color.r * maxIndex * texelSize) + (texelSize * 0.5f);                        \
                                                                                                  \
    float yOffset = (color.g * maxIndex * slice) + (slice * 0.5f);                                \
                                                                                                  \
    float2 uv = float2(                                                                           \
      zOffset + xOffset,                                                                          \
      yOffset                                                                                     \
    );                                                                                            \
                                                                                                  \
    float3 color0 = lut.SampleLevel(samplerState, uv, 0).rgb;                                     \
    uv.x += slice;                                                                                \
    float3 color1 = lut.SampleLevel(samplerState, uv, 0).rgb;                                     \
                                                                                                  \
    return lerp(color0, color1, zFraction);                                                       \
  }

#define sampleLUTTexture2DFunctionGenerator(textureType)                                       \
  float3 sampleLUT(textureType lut, SamplerState samplerState, float3 color, float size = 0) { \
    if (size == 0) {                                                                           \
      /* Removed by compiler if specified */                                                   \
      float width;                                                                             \
      float height;                                                                            \
      lut.GetDimensions(width, height);                                                        \
      size = min(width, height);                                                               \
    }                                                                                          \
                                                                                               \
    float maxIndex = size - 1.f;                                                               \
    float slice = 1.f / size;                                                                  \
    float texelSize = slice * slice;                                                           \
                                                                                               \
    return sampleLUT(lut, samplerState, color, float3(texelSize, slice, maxIndex));            \
  }

#define sampleLUT3DColorFunctionGenerator(textureType)                               \
  float3 sampleLUTColor(float3 color, LUTParams lutParams, textureType lutTexture) { \
    return sampleLUT(                                                                \
      lutTexture,                                                                    \
      lutParams.lutSampler,                                                          \
      color.rgb,                                                                     \
      lutParams.size                                                                 \
    );                                                                               \
  }

#define sampleLUT2DColorFunctionGenerator(textureType)                               \
  float3 sampleLUTColor(float3 color, LUTParams lutParams, textureType lutTexture) { \
    if (lutParams.precompute.x) {                                                    \
      return sampleLUT(                                                              \
        lutTexture,                                                                  \
        lutParams.lutSampler,                                                        \
        color.rgb,                                                                   \
        lutParams.precompute.xyz                                                     \
      );                                                                             \
    }                                                                                \
    return sampleLUT(                                                                \
      lutTexture,                                                                    \
      lutParams.lutSampler,                                                          \
      color.rgb,                                                                     \
      lutParams.size                                                                 \
    );                                                                               \
  }

sampleLUTTexture3DFunctionGenerator(Texture3D<float4>);
sampleLUTTexture3DFunctionGenerator(Texture3D<float3>);
sampleLUTTexture2DPrecomputedFunctionGenerator(Texture2D<float4>);
sampleLUTTexture2DPrecomputedFunctionGenerator(Texture2D<float3>);
sampleLUTTexture2DFunctionGenerator(Texture2D<float4>);
sampleLUTTexture2DFunctionGenerator(Texture2D<float3>);
sampleLUT3DColorFunctionGenerator(Texture3D<float4>);
sampleLUT3DColorFunctionGenerator(Texture3D<float3>);
sampleLUT2DColorFunctionGenerator(Texture2D<float4>);
sampleLUT2DColorFunctionGenerator(Texture2D<float3>);

float3 sampleLUTUnreal(Texture2D lut, SamplerState samplerState, float3 color, float size = 0) {
  if (size == 0) {
    // Removed by compiler if specified
    float width;
    float height;
    lut.GetDimensions(width, height);
    size = min(width, height);
  }
  float slice = 1.f / size;

  float zPosition = color.z * size - 0.5;
  float zInteger = floor(zPosition);
  half fraction = zPosition - zInteger;

  float2 uv = float2(
    (color.r + zInteger) * slice,
    color.g
  );

  float3 color0 = lut.SampleLevel(samplerState, uv, 0).rgb;
  uv.x += slice;
  float3 color1 = lut.SampleLevel(samplerState, uv, 0).rgb;

  return lerp(color0, color1, fraction);
}

float3 lutCorrectionBlack(float3 inputColor, float3 lutColor, float lutBlackY, float strength) {
  const float inputY = yFromBT709(inputColor);
  const float colorY = yFromBT709(lutColor);
  const float a = lutBlackY;
  const float b = lerp(0, lutBlackY, strength);
  const float g = inputY;
  const float h = colorY;
  const float newY = h - pow(lutBlackY, pow(1.f + g, b / a));
  lutColor *= (colorY > 0) ? min(colorY, newY) / colorY : 1.f;
  return lutColor;
}

float3 lutCorrectionWhite(float3 inputColor, float3 lutColor, float lutWhiteY, float targetWhiteY, float strength) {
  const float inputY = min(targetWhiteY, yFromBT709(inputColor));
  const float colorY = yFromBT709(lutColor);
  const float a = lutWhiteY / targetWhiteY;
  const float b = lerp(1.f, 0.f, strength);
  const float g = inputY;
  const float h = colorY;
  const float newY = h * pow((1.f / a), pow(g / targetWhiteY, b / a));
  lutColor *= (colorY > 0) ? max(colorY, newY) / colorY : 1.f;
  return lutColor;
}

float3 unclampSDRLUT(
  float3 originalGamma,
  float3 blackGamma,
  float3 midGrayGamma,
  float3 whiteGamma,
  float3 neutralGamma
) {
  float3 addedGamma = blackGamma;
  float3 removedGamma = 1.f - min(1.f, whiteGamma);

  float midGrayAvg = (midGrayGamma.r + midGrayGamma.g + midGrayGamma.b) / 3.f;

  // Remove relative to distance to inverse midgray
  float shadowLength = 1.f - midGrayAvg;
  float shadowStop = max(neutralGamma.r, max(neutralGamma.g, neutralGamma.b));
  float3 removeFog = addedGamma * max(0, shadowLength - shadowStop) / shadowLength;

  // Add back relative to distance from midgray
  float highlightsLength = midGrayAvg;
  float highlightsStop = 1.f - min(neutralGamma.r, min(neutralGamma.g, neutralGamma.b));
  float3 liftHighlights = removedGamma * (max(0, highlightsLength - highlightsStop) / highlightsLength);

  float3 unclampedInGamma = max(0, originalGamma - removeFog) + liftHighlights;
  return unclampedInGamma;
}

float3 recolorUnclampedLUT(float3 originalLinear, float3 unclampedLinear) {
  const float3 originalLab = okLabFromBT709(originalLinear);

  float3 retintedLab = okLabFromBT709(unclampedLinear);
  retintedLab[0] = max(0, retintedLab[0]);
  retintedLab[1] = originalLab[1];
  retintedLab[2] = originalLab[2];

  float3 outputLinear = bt709FromOKLab(retintedLab);

  outputLinear = mul(BT709_2_AP1_MAT, outputLinear);  // Convert to AP1
  outputLinear = max(0, outputLinear);                // Clamp to AP1
  outputLinear = mul(AP1_2_BT709_MAT, outputLinear);  // Convert BT709
  return outputLinear;
}

float3 convertLUTInput(float3 color, LUTParams lutParams) {
  if (lutParams.inputType == LUT_TYPE__SRGB) {
    color = srgbFromLinear(saturate(color));
  } else if (lutParams.inputType == LUT_TYPE__2_4) {
    color = pow(saturate(color), 1.f / 2.4f);
  } else if (lutParams.inputType == LUT_TYPE__2_2) {
    color = pow(saturate(color), 1.f / 2.2f);
  } else if (lutParams.inputType == LUT_TYPE__2_0) {
    color = sqrt(saturate(color));
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C800) {
    color = arriC800FromLinear(max(0, color));
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C1000) {
    color = arriC1000FromLinear(max(0, color));
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C800_NO_CUT) {
    color = arriC800FromLinear(max(0, color), 0);
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C1000_NO_CUT) {
    color = arriC1000FromLinear(max(0, color), 0);
  } else if (lutParams.inputType == LUT_TYPE__PQ) {
    float3 rec2020 = bt2020FromBT709(color);
    color = pqFromLinear((rec2020 * 100.f) / 10000.f);
  }
  return color;
}

float3 gammaLUTOutput(float3 color, LUTParams lutParams) {
  if (lutParams.outputType == LUT_TYPE__LINEAR) {
    color = srgbFromLinear(max(0, color));
  }
  return color;
}

float3 linearLUTOutput(float3 color, LUTParams lutParams) {
  if (lutParams.outputType == LUT_TYPE__SRGB) {
    color = sign(color) * linearFromSRGB(abs(color));
  } else if (lutParams.outputType == LUT_TYPE__2_4) {
    color = sign(color) * pow(abs(color), 2.4f);
  } else if (lutParams.outputType == LUT_TYPE__2_2) {
    color = sign(color) * pow(abs(color), 2.2f);
  } else if (lutParams.outputType == LUT_TYPE__2_0) {
    color = sign(color) * color * color;
  }
  return color;
}

float3 gammaLUTInput(float3 inputColor, float3 convertedInputColor, LUTParams lutParams) {
  if (
    lutParams.inputType == LUT_TYPE__SRGB
    || lutParams.inputType == LUT_TYPE__2_4
    || lutParams.inputType == LUT_TYPE__2_2
    || lutParams.inputType == LUT_TYPE__2_0
  ) {
    return convertedInputColor;
  } else {
    return srgbFromLinear(max(0, inputColor));
  }
}

float3 linearUnclampedLUTOutput(float3 color, LUTParams lutParams) {
  if (lutParams.outputType == LUT_TYPE__2_4) {
    color = sign(color) * pow(abs(color), 2.4f);
  } else if (lutParams.outputType == LUT_TYPE__2_2) {
    color = sign(color) * pow(abs(color), 2.2f);
  } else {
    color = sign(color) * linearFromSRGB(abs(color));
  }
  return color;
}

float3 restoreLUTSaturationLoss(float3 inputColor, float3 outputColor, LUTParams lutParams) {
  // Saturation (distance from grayscale)
  float yIn = yFromBT709(abs(inputColor));
  float3 satIn = inputColor - yIn;

  float3 clamped = inputColor;
  if (lutParams.inputType == LUT_TYPE__SRGB) {
    clamped = saturate(clamped);
  } else if (lutParams.inputType == LUT_TYPE__2_4) {
    clamped = saturate(clamped);
  } else if (lutParams.inputType == LUT_TYPE__2_2) {
    clamped = saturate(clamped);
  } else if (lutParams.inputType == LUT_TYPE__2_0) {
    clamped = saturate(clamped);
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C800) {
    clamped = max(0, clamped);
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C1000) {
    clamped = max(0, clamped);
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C800_NO_CUT) {
    clamped = max(0, clamped);
  } else if (lutParams.inputType == LUT_TYPE__ARRI_C1000_NO_CUT) {
    clamped = max(0, clamped);
  } else if (lutParams.inputType == LUT_TYPE__PQ) {
    clamped = max(0, bt2020FromBT709(clamped));
  }

  float yClamped = yFromBT709(abs(yClamped));
  float3 satClamped = clamped - yIn;

  float yOut = yFromBT709(abs(outputColor));
  float3 satOut = outputColor - yOut;
  float3 newSat = float3(
    satOut.r * (satClamped.r ? (satIn.r / satClamped.r) : 1.f),
    satOut.g * (satClamped.g ? (satIn.g / satClamped.g) : 1.f),
    satOut.b * (satClamped.b ? (satIn.b / satClamped.b) : 1.f)
  );
  return (yOut + newSat);
}

#define sampleLUTFunctionGenerator(textureType)                                                 \
  float3 sampleLUT(textureType lutTexture, LUTParams lutParams, float3 inputColor) {            \
    float3 lutInputColor = convertLUTInput(inputColor, lutParams);                              \
    float3 lutOutputColor = sampleLUTColor(lutInputColor, lutParams, lutTexture);               \
    float3 outputColor = linearLUTOutput(lutOutputColor, lutParams);                            \
    if (lutParams.scaling) {                                                                    \
      float3 lutBlack = sampleLUTColor(convertLUTInput(0, lutParams), lutParams, lutTexture);   \
      float3 lutMid = sampleLUTColor(convertLUTInput(0.18f, lutParams), lutParams, lutTexture); \
      float3 lutWhite = sampleLUTColor(convertLUTInput(1.f, lutParams), lutParams, lutTexture); \
      float3 unclamped = unclampSDRLUT(                                                         \
        gammaLUTOutput(lutOutputColor, lutParams),                                              \
        gammaLUTOutput(lutBlack, lutParams),                                                    \
        gammaLUTOutput(lutMid, lutParams),                                                      \
        gammaLUTOutput(lutWhite, lutParams),                                                    \
        gammaLUTInput(inputColor, lutInputColor, lutParams)                                     \
      );                                                                                        \
      float3 recolored = recolorUnclampedLUT(                                                   \
        outputColor,                                                                            \
        linearUnclampedLUTOutput(unclamped, lutParams)                                          \
      );                                                                                        \
      outputColor = lerp(outputColor, recolored, lutParams.scaling);                            \
    }                                                                                           \
    outputColor = restoreLUTSaturationLoss(inputColor, outputColor, lutParams);                 \
    return outputColor;                                                                         \
  }

// Deprecated
#define sampleLUTDeprecatedFunctionGenerator(textureType)                            \
  float3 sampleLUT(float3 inputColor, LUTParams lutParams, textureType lutTexture) { \
    return sampleLUT(lutTexture, lutParams, inputColor);                             \
  }

sampleLUTFunctionGenerator(Texture3D<float4>);
sampleLUTFunctionGenerator(Texture3D<float3>);
sampleLUTFunctionGenerator(Texture2D<float4>);
sampleLUTFunctionGenerator(Texture2D<float3>);

sampleLUTDeprecatedFunctionGenerator(Texture3D<float4>);
sampleLUTDeprecatedFunctionGenerator(Texture3D<float3>);
sampleLUTDeprecatedFunctionGenerator(Texture2D<float4>);
sampleLUTDeprecatedFunctionGenerator(Texture2D<float3>);

#endif  // SRC_SHADERS_LUT_HLSL_
