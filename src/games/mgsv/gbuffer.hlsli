// The deferred G-buffer reads shared by the screen-space passes: the engine's constants,
// the depth linearization, the view-space position and the packed-normal decode.
//
// Both the AO pass and the diffuse gather read the same three resources with the same
// projection, so a change to either convention has to reach both at once.

#pragma once

cbuffer MGSVGeometryConstants : register(b0) {
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

// View position from the integer coordinates used to load a full-resolution texel.
// Native scatter2 (0xB2601810) divides those coordinates by the buffer size without
// adding a half pixel. Flip screen Y before subtracting the camera-center offset;
// keep the signed projection factors, including the native negative X scale.
float3 ViewPos(float2 pixel, float z) {
  const float2 ndc = pixel / float2(cBufferWidth, cBufferHeight) * 2.0 - 1.0;
  return float3((float2(ndc.x, -ndc.y) - cCameraCenterOffset.xy)
                    * cProjectionParam.xy * z,
                z);
}

// Native UNORM view-space packing: enc.xy = normal.xy * 0.5 + 0.5 and
// enc.z = sqrt(normal.z * 0.5 + 0.5). Reconstruct XY with the retail decoder's
// epsilon, then normalize as native scatter2 does for directional comparisons.
float3 DecodeNormal(float3 enc) {
  const float2 xy = enc.xy * 2.0 - 1.0;
  const float nz = 2.0 * enc.z * enc.z - 1.0;
  const float w = 1.0 - nz * nz;
  return normalize(float3(xy * w * rsqrt(w * dot(xy, xy) + 1e-7), nz));
}
