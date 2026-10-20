// The deferred G-buffer reads shared by the screen-space passes: the engine's constants,
// the depth linearisation, the view-space position and the packed-normal decode.
//
// Both the AO pass and the diffuse gather read the same three resources with the same
// projection, so a change to either convention has to reach both at once.

#pragma once

cbuffer Constants : register(b0)
{
  uint cBufferWidth : packoffset(c0);
  uint cBufferHeight : packoffset(c0.y);
  uint cNumTilesX : packoffset(c0.z);
  uint cNumTilesY : packoffset(c0.w);
  float2 cHalfFovRecip : packoffset(c1);
  float2 cCameraCenterOffset : packoffset(c1.z);
  float4 cProjectionParam : packoffset(c2);
  float2 cTweakParams : packoffset(c3);
  float cFade : packoffset(c3.z);
  float padding : packoffset(c3.w);
}

// Linear view depth, the same expression the retail passes use.
float LinearZ(float depth) {
  return cProjectionParam.z / max(depth - cProjectionParam.w, 1e-6);
}

// View position of a pixel at a given linear depth. Matches the retail passes: the
// TAA jitter is subtracted from the ndc, and y is flipped because ndc runs down
// the screen while view space runs up.
float3 ViewPos(float2 pixel, float z) {
  const float2 uv = (pixel + 0.5) / float2(cBufferWidth, cBufferHeight);
  const float2 ndc = uv * 2.0 - 1.0 - cCameraCenterOffset.xy;
  return float3(ndc.x * z * cProjectionParam.x,
                -ndc.y * z * cProjectionParam.y,
                z);
}

// 8-bit packed normal: xy doubled and biased, z recovered as enc.z^2 - 1, xy
// scaled by 1 - z^2.
float3 DecodeNormal(float3 enc) {
  const float2 xy = enc.xy * 2.0 - 1.0;
  const float nz = enc.z * enc.z - 1.0;
  const float w = 1.0 - nz * nz;
  return normalize(float3(xy * w, nz));
}
