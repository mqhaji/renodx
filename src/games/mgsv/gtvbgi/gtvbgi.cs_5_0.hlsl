// MGSV bidirectional, cosine-weighted GT-VBGI with AO visibility output.
// Based on Screen Space Indirect Lighting with Visibility Bitmask, Therrien,
// Levesque and Gilet (2023), with GT-VBAO/GT-VBGI corrections by Mirko Salm
// (TinyTexel): https://www.shadertoy.com/view/lfdBWn, Buffer C.
// The reference core is dual CC0/MIT; the adapted math here uses its MIT license.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Runs after native second subsurface scatter, producing separate AO visibility.
// One geometry search writes AO visibility to u0 and bounce/validity to u1.
// The host applies AO to existing diffuse before adding this visibility-integrated GI.

#include "../gbuffer.hlsli"
#include "../shared.h"

#define AO_PI               3.14159265358979323846
#define AO_SECTORS          32u
#define AO_DEPTH_MIP_LEVELS 4

Texture2D<float4> depthBuffer : register(t0);
Texture2D<float4> normalBuffer : register(t1);
Texture2D<float4> depthMips : register(t2);
Texture2DArray<float2> fastNoise : register(t6);
RWTexture2D<float2> aoOutput : register(u0);

Texture2D<float4> sourceDiffuse : register(t3);
Texture2D<float4> sourceAlbedo : register(t4);
Texture2D<float4> sourceSpecular : register(t5);
RWTexture2D<float4> giOutput : register(u1);
RWTexture2D<float4> diagnosticOutput : register(u2);

// Follow the same floor-sized, odd-edge-inclusive max reductions as AO depth
// preparation. Prefer the sampled child on ties, otherwise the first maximum.
// This matches radiance/normal to the depth owner without changing the existing
// conservative assignment of that depth to the continuous slice position.
bool ResolveDepthOwner(int2 samplePixel, int mipLevel, float sampledDepth, out int2 owner) {
  owner = samplePixel;
  if (mipLevel == 0) return true;
  owner = clamp(samplePixel >> mipLevel, int2(0, 0),
                max(int2(cBufferWidth, cBufferHeight) >> mipLevel, 1) - 1);
  float ownerDepth = sampledDepth;
  [loop]
  for (int level = mipLevel; level > 0; --level) {
    const int2 parentSize = max(int2(cBufferWidth, cBufferHeight) >> level, 1);
    const int2 childSize = max(int2(cBufferWidth, cBufferHeight) >> (level - 1), 1);
    const int2 base = owner * 2;
    const int2 end = int2(owner.x + 1 == parentSize.x ? childSize.x : min(base.x + 2, childSize.x),
                          owner.y + 1 == parentSize.y ? childSize.y : min(base.y + 2, childSize.y));
    int2 selected = clamp(samplePixel >> (level - 1), base, end - 1);
    [branch]
    if (level == 1) {
      ownerDepth = depthBuffer.Load(int3(selected, 0)).x;
    }
    else {
      ownerDepth = depthMips.Load(int3(selected, level - 2)).x;
    }
    [loop]
    for (int y = base.y; y < end.y; ++y) {
      [loop]
      for (int x = base.x; x < end.x; ++x) {
        float candidate;
        [branch]
        if (level == 1) {
          candidate = depthBuffer.Load(int3(x, y, 0)).x;
        }
        else {
          candidate = depthMips.Load(int3(x, y, level - 2)).x;
        }
        if (candidate > ownerDepth) {
          ownerDepth = candidate;
          selected = int2(x, y);
        }
      }
    }
    owner = selected;
  }
  return ownerDepth == sampledDepth;
}

// Sector ownership is [first, end). Guard empty/full intervals before shifting.
uint SectorMask(uint first, uint end) {
  if (end <= first || first >= AO_SECTORS) return 0u;
  uint high = 0xFFFFFFFFu;
  if (end < AO_SECTORS) high = (1u << end) - 1u;
  return high & ~((1u << first) - 1u);
}

// Reference mode 3, using its HQ angular mapping. The asin form is equivalent
// to (1 - acos(x * sin(stretch)) * 2 / PI) / s without cancellation near s=0.
float2 SampleCosineSliceDir(float2 normalXY, float phase) {
  const float angle = phase * AO_PI;
  float2 direction = float2(cos(angle), sin(angle));
  const float normalLength = length(normalXY);
  if (normalLength == 0.0) return direction;
  direction *= dot(direction, normalXY) < 0.0 ? -1.0 : 1.0;
  const float2 n = normalXY / normalLength;
  const float x = direction.x * n.y - direction.y * n.x;
  float s = saturate(normalLength);
  s += (s - s * s) * 0.15;
  const float stretch = s * (AO_PI * 0.5);
  const float y = asin(clamp(x * sin(stretch), -1.0, 1.0)) / stretch;
  const float alignedX = sqrt(saturate(1.0 - y * y));
  return float2(alignedX * n.x - y * n.y, y * n.x + alignedX * n.y);
}

// Cumulative cosine-weighted measure along a slice. x spans the receiver's
// hemisphere; endpoints must be handled before evaluating the interior formula.
float SliceRelCDFCos(float x, float normalAngle, float cosN, bool directionIsRight) {
  if (x <= 0.0 || x >= 1.0) return x;
  const float phi = x * AO_PI - AO_PI * 0.5;
  const float n0 = directionIsRight ? 3.0 : 1.0;
  const float n1 = directionIsRight ? -1.0 : 1.0;
  const float n2 = directionIsRight ? 4.0 : 0.0;
  const float sinN = sin(normalAngle);
  return (n0 * cosN + n1 * cos(normalAngle - 2.0 * phi)
          + (n2 * normalAngle + n1 * 2.0 * phi + AO_PI) * sinN)
         / (4.0 * (cosN + normalAngle * sinN));
}

float LoadTraceDepth(int2 samplePixel, int mipLevel) {
  float sampledDepth = 0.0;
  [branch]
  if (mipLevel == 0) {
    sampledDepth = depthBuffer.Load(int3(samplePixel, 0)).x;
  }
  else {
    const int2 coarseSize = max(int2(cBufferWidth, cBufferHeight) >> mipLevel, 1);
    const int2 coarsePixel = clamp(samplePixel >> mipLevel, int2(0, 0), coarseSize - 1);
    sampledDepth = depthMips.Load(int3(coarsePixel, mipLevel - 1)).x;
  }
  return sampledDepth;
}

uint CoverageMask(float2 samplePosition, float sampleDepth, float thickness,
                  float3 P, float3 V, float side, float normalAngle, float cosN,
                  float sectorOffset) {
  const float sampleZ = LinearZ(sampleDepth);
  // Native reverse-Z zero is background, not a finite surface slab.
  if (!(sampleDepth > 0.0 && sampleZ > 0.0) || sampleZ > 1.0e6) return 0u;
  // Keep both endpoints in the perspective slice. A coarse depth remains a
  // conservative proxy even when its source radiance is read from the owner.
  const float3 front = ViewPos(samplePosition, sampleZ) - P;
  const float3 back = ViewPos(samplePosition, sampleZ + thickness) - P;
  const float2 lengthsSquared = float2(dot(front, front), dot(back, back));
  if (any(lengthsSquared < 1e-12)) return 0u;
  const float2 horizonCos = clamp(float2(dot(front, V), dot(back, V)) * rsqrt(lengthsSquared), -1.0, 1.0);
  float2 horizon = saturate(acos(horizonCos) * (side / AO_PI) + normalAngle / AO_PI + 0.5);
  if (side < 0.0) horizon = horizon.yx;
  horizon.x = SliceRelCDFCos(horizon.x, normalAngle, cosN, side > 0.0);
  horizon.y = SliceRelCDFCos(horizon.y, normalAngle, cosN, side > 0.0);
  const uint2 sectors = (uint2)floor(saturate(horizon + sectorOffset) * float(AO_SECTORS));
  return SectorMask(sectors.x, sectors.y);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  const uint2 ao_pixel = tid.xy;
  uint ao_width, ao_height;
  aoOutput.GetDimensions(ao_width, ao_height);
  if (ao_pixel.x >= ao_width || ao_pixel.y >= ao_height) return;
  giOutput[ao_pixel] = 0.0;
  const int diagnostic = (int)clamp(CUSTOM_MGSV_GI_DEBUG_VIEW, 0.0, 12.0);
  if (diagnostic >= 2) {
    diagnosticOutput[ao_pixel] = diagnostic == 4 ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
    if (diagnostic >= 9 && diagnostic <= 11) diagnosticOutput[ao_pixel] = 1.0;
  }

  const float scale = max(CUSTOM_MGSV_GT_RESOLUTION_DIVISOR, 1.0);
  const int2 p = min((int2)floor((float2(ao_pixel) + 0.5) * scale),
                     int2(cBufferWidth, cBufferHeight) - 1);
  const float depth = depthBuffer.Load(int3(p, 0)).x;
  const float z = LinearZ(depth);
  if (!(depth > 0.0 && z > 0.0) || z > 1.0e6) {
    aoOutput[ao_pixel] = float2(1.0, 1.0);
    return;
  }

  const float2 pixel = float2(p);
  const float3 N = DecodeNormal(normalBuffer.Load(int3(p, 0)).xyz);
  // Match the reference receiver offset and project that same point back to
  // the screen. MGSV uses native view units and signed projection factors.
  const float3 P = ViewPos(pixel, z) + N * max(CUSTOM_MGSV_GT_SURFACE_OFFSET, 0.0);
  const float3 V = normalize(-P);
  // The reference CDF assumes a view-facing receiver. Do not extrapolate its
  // measure to backfacing shading normals; use neutral visibility instead.
  if (any(!isfinite(N)) || !(P.z > 0.0) || dot(N, V) < 0.0) {
    aoOutput[ao_pixel] = float2(1.0, 1.0);
    return;
  }

  const float2 rayStart = ((P.xy / (cProjectionParam.xy * P.z) + cCameraCenterOffset)
                               * float2(1.0, -1.0)
                           + 1.0)
                          * 0.5 * float2(cBufferWidth, cBufferHeight);
  const int slices = (int)max(CUSTOM_MGSV_GT_SLICE_COUNT, 1.0);
  const int steps = (int)max(CUSTOM_MGSV_GT_SAMPLES_PER_SIDE, 1.0);
  const float radiusPx = max(CUSTOM_MGSV_GT_SEARCH_RADIUS_PIXELS, 1.0);
  const float thickness = max(CUSTOM_MGSV_GT_SURFACE_THICKNESS, 0.0);
  const float stepMultiplier = pow(radiusPx, 1.0 / float(steps));
  const float specularContribution = max(CUSTOM_MGSV_GI_SOURCE_SPECULAR_WEIGHT, 0.0);

  // Rotated X/Y axes of the reference quaternion from -Z to V. Positive native
  // view depth guarantees V.z<0, avoiding the antipodal singularity.
  const float basisScale = rcp(1.0 - V.z);
  const float3 axisX = float3(1.0 - V.x * V.x * basisScale, -V.x * V.y * basisScale, V.x);
  const float3 axisY = float3(-V.x * V.y * basisScale, 1.0 - V.y * V.y * basisScale, V.y);
  const float2 normalXY = float2(dot(N, axisX), dot(N, axisY));

  float visibility = 0.0;
  float3 indirect = 0.0;
  float acceptedCoverage = 0.0;
  bool giValid = true;

  [loop]
  for (int slice = 0; slice < slices; ++slice) {
    // Use the stored uniform channels directly.
    // A separated R2 offset supplies the third variate, following EA's guidance.
    // Address the trace lattice, avoiding decimation of noise at reduced scale.
    const uint frame = (CUSTOM_TAA != 0.0 || CUSTOM_MGSV_GI_TEMPORAL_ACTIVE > 0.5 || CUSTOM_MGSV_AO_TEMPORAL_ACTIVE > 0.5)
                          ? (uint)CUSTOM_MGSV_GT_NOISE_FRAME : 0u;
    const uint sampleIndex = (frame * (uint)slices + (uint)slice) & 31u;
    const uint2 q = ao_pixel & 127u;
    float3 phases;
    phases.xy = fastNoise.Load(int4(q, sampleIndex, 0));
    phases.z = fastNoise.Load(int4((q + uint2(96u, 72u)) & 127u, sampleIndex, 0)).x;
    // Map the 256 stored ranks to bin centers, excluding exact zero/one.
    phases = (phases * 255.0 + 0.5) / 256.0;
    const float2 sliceDirection = SampleCosineSliceDir(normalXY, (float(slice) + phases.x) / float(slices));
    const float3 D = axisX * sliceDirection.x + axisY * sliceDirection.y;
    // Derivative of native ViewPos's inverse projection, including signed X and
    // the screen-Y flip. Unlike uniform screen angles, this preserves slice area.
    const float2 screenDirection = normalize(float2(
        float(cBufferWidth) / cProjectionParam.x * (D.x - P.x / P.z * D.z),
        -float(cBufferHeight) / cProjectionParam.y * (D.y - P.y / P.z * D.z)));
    const float3 sliceNormal = cross(V, D);
    const float3 projectedNormal = N - sliceNormal * dot(N, sliceNormal);
    const float projectedLengthSquared = dot(projectedNormal, projectedNormal);
    if (projectedLengthSquared < 1e-12) {
      visibility += 1.0;
      continue;
    }
    const float cosN = clamp(dot(projectedNormal, V) * rsqrt(projectedLengthSquared), 0.0, 1.0);
    const float normalSign = dot(V, cross(sliceNormal, projectedNormal)) < 0.0 ? -1.0 : 1.0;
    const float normalAngle = normalSign * acos(cosN);
    const float sectorOffset = phases.z / float(AO_SECTORS);
    const float radialPhase = phases.y;
    uint occupied = 0u;

    [loop]
    for (int side = 0; side < 2; ++side) {
      const float d = side == 0 ? -1.0 : 1.0;
      const float2 rayDirection = screenDirection * d;
      const float sidePhase = side == 0 ? radialPhase : 1.0 - radialPhase;
      float distancePx = pow(stepMultiplier, sidePhase);
      [loop]
      for (int step = 0; step < steps; ++step) {
        const float sampleDistance = distancePx;
        distancePx *= stepMultiplier;
        // One shared geometry sample supplies both visibility and bounce.
        const float2 samplePosition = rayStart + rayDirection * sampleDistance;
        if (any(samplePosition < 0.0) || any(samplePosition >= float2(cBufferWidth, cBufferHeight))) break;

        const int2 samplePixel = min((int2)floor(samplePosition + 0.5), int2(cBufferWidth, cBufferHeight) - 1);
        int mipLevel = 0;
        [branch]
        if (CUSTOM_MGSV_GT_DEPTH_MIPS_ENABLED > 0.5) {
          mipLevel = clamp((int)floor(log2(max(sampleDistance, 1.0)) - 3.3), 0, AO_DEPTH_MIP_LEVELS);
        }
        const float sampleDepth = LoadTraceDepth(samplePixel, mipLevel);
        const uint covered = CoverageMask(samplePosition, sampleDepth, thickness, P, V, d, normalAngle, cosN, sectorOffset);
        uint fresh = covered & ~occupied;
        if (fresh != 0u) {
          int2 owner;
          if (!ResolveDepthOwner(samplePixel, mipLevel, sampleDepth, owner)) {
            giValid = false;
          } else {
            if (CUSTOM_MGSV_GI_REJECT_BACKFACES > 0.5) {
              const float3 sourceNormal = DecodeNormal(normalBuffer.Load(int3(owner, 0)).xyz);
              if (any(!isfinite(sourceNormal))) {
                giValid = false;
                fresh = 0u;
              }
              // A backfacing surface still occludes. Only its outgoing radiance
              // is masked, using the reference's projected source hemisphere.
              const float3 sourceProjected = sourceNormal - sliceNormal * dot(sourceNormal, sliceNormal);
              const float sourceLengthSquared = dot(sourceProjected, sourceProjected);
              if (sourceLengthSquared > 1e-12) {
                const bool flipT = dot(cross(sliceNormal, projectedNormal), sourceNormal) < 0.0;
                const float sourceCos = clamp(dot(projectedNormal, sourceProjected)
                                                  * rsqrt(projectedLengthSquared) * rsqrt(sourceLengthSquared),
                                              -1.0, 1.0);
                float cutoff = acos(sourceCos) / AO_PI;
                if (flipT) cutoff = 1.0 - cutoff;
                cutoff = SliceRelCDFCos(cutoff, normalAngle, cosN,
                                        cutoff * AO_PI - AO_PI * 0.5 > normalAngle);
                const uint first = (uint)floor(saturate(cutoff + sectorOffset) * float(AO_SECTORS));
                uint sourceMask = SectorMask(first, AO_SECTORS);
                if (!flipT) sourceMask = ~sourceMask;
                fresh &= sourceMask;
              }
            }
            if (fresh != 0u) {
              // Albedo is already decoded by its sRGB SRV. Diffuse/specular
              // are native linear pre-exposed light; source alpha is an SSS
              // mask and must not reject non-skin or valid black samples.
              const float3 albedo = sourceAlbedo.Load(int3(owner, 0)).rgb;
              const float3 diffuse = sourceDiffuse.Load(int3(owner, 0)).rgb;
              float3 radiance = albedo * diffuse;
              if (specularContribution > 0.0) radiance += specularContribution * sourceSpecular.Load(int3(owner, 0)).rgb;
              if (any(!isfinite(radiance))) {
                giValid = false;
              } else {
                indirect += max(radiance, 0.0) * (float(countbits(fresh)) / float(AO_SECTORS));
                acceptedCoverage += float(countbits(fresh)) / float(AO_SECTORS);
              }
            }
          }
        }
        // Rejected or invalid source radiance must not make geometry transparent.
        occupied |= covered;
        if (occupied == 0xFFFFFFFFu) break;
      }
    }
    // Importance sampling already accounts for slice weight. Fractional band
    // compensation would no longer describe which surface owns each sector.
    visibility += 1.0 - float(countbits(occupied)) / float(AO_SECTORS);
  }

  const float occlusion = saturate(1.0 - visibility / float(slices));
  const float applied = saturate(occlusion * CUSTOM_MGSV_AO_STRENGTH);
  aoOutput[ao_pixel] = float2(1.0 - applied, 1.0);
  if (diagnostic >= 9 && diagnostic <= 11) {
    diagnosticOutput[ao_pixel] = float4((1.0 - applied).xxx, 1.0);
  }
  // No-hit and black-source results remain valid zero. Missing qualification
  // is separate from AO visibility. Bound only to the RGBA16F storage range.
  const float3 bounce = indirect / float(slices);
  giValid = giValid && all(isfinite(bounce));
  giOutput[ao_pixel] = giValid ? float4(clamp(bounce, 0.0, 65504.0), 1.0) : 0.0;
  if (diagnostic >= 2 && diagnostic <= 5) {
    float3 value = occlusion.xxx;
    if (diagnostic == 3) value = (acceptedCoverage / float(slices)).xxx;
    if (diagnostic == 4) value = giValid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    if (diagnostic == 5) {
      value = sourceAlbedo.Load(int3(p, 0)).rgb * sourceDiffuse.Load(int3(p, 0)).rgb;
      if (specularContribution > 0.0) value += specularContribution * sourceSpecular.Load(int3(p, 0)).rgb;
      value = all(isfinite(value)) ? clamp(value, 0.0, 65504.0) : float3(1.0, 0.0, 0.0);
    }
    diagnosticOutput[ao_pixel] = float4(value, 1.0);
  }
}
