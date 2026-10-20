// MGSV VBAO - visibility-bitmask ambient occlusion, applied to the light buffer.
//
// Screen Space Indirect Lighting with Visibility Bitmask, Therrien, Levesque and
// Gilet, The Visual Computer 2023 - docs/vbao.pdf, Algorithm 1. This file keeps
// the measure, the application and the quantiser from that work, with the
// deviations listed in MGSV_AmbientOcclusion.md.
//
// Where it runs, and why it is not a replacement: the retail passes at
// 0x6DAF0C4A / 0xB2601810 are the game's screen-space *subsurface scatter*
// chain, not AO. Replacing either of them removes the scattering that makes skin
// read as skin. This pass therefore runs on its own dispatch, after stage 2 has
// written the scattered light, and multiplies that result. The scatter has
// already happened when this reads the buffer, so the occlusion cannot remove it.
//
// The light is read and written through the same UAV: the resource this pass
// overwrites is the one the retail stage just wrote, and no engine pass is
// touched. Alpha is written through unchanged - the retail chain reads it as the
// "pixel was shaded" flag.
//
// Bindings (own layout, dispatched by the addon):
//   b0   Constants - inherited from the engine's retail AO constants
//   t1   depth       (r32_float)
//   t2   normal      (b8g8r8a8, same packing as the deferred G-buffer)
//   t6   depth mips  (addon-built nearest-reduced chain)
//   u0   light       (read-modify-write, rgb scaled, alpha preserved)
//
// The engine's t0 of the retail chain - the horizontal stage's average of the
// light - is not bound and not read.

#include "../shared.h"

#define AO_PI 3.14159265
#define AO_SECTORS 128u
// Levels in the addon's depth chain. The chain starts at half resolution, so level k here is
// texture level k - 1 and covers W >> (k + 1).
#define AO_DEPTH_MIP_LEVELS 4

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

// The light is both input and output: this pass is the last writer of the buffer
// the retail scatter stage just produced, so a typed UAV load returns the
// scattered value and the store puts the occluded value back.
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
// The addon's depth mip chain: each texel is the nearest of the four below it. Only read
// when the injection says the chain exists for this dispatch.
Texture2D<float4> depthMips : register(t6);
RWTexture2D<float4> resultBuffer : register(u0);

// Linear view depth, the same expression the retail passes use.
float LinearZ(float depth) {
  return cProjectionParam.z / max(depth - cProjectionParam.w, 1e-6);
}

// View position of a pixel at a given linear depth. Matches the retail passes: the
// TAA jitter is subtracted from the ndc, and y is flipped because ndc runs down
// the screen while view space runs up.
float3 ViewPos(float2 pixel, float z) {
  const float2 uv  = (pixel + 0.5) / float2(cBufferWidth, cBufferHeight);
  const float2 ndc = uv * 2.0 - 1.0 - cCameraCenterOffset.xy;
  return float3(ndc.x * z * cProjectionParam.x,
               -ndc.y * z * cProjectionParam.y,
                z);
}

// 8-bit packed normal: xy doubled and biased, z recovered as enc.z^2 - 1, xy
// scaled by 1 - z^2.
float3 DecodeNormal(float3 enc) {
  const float2 xy = enc.xy * 2.0 - 1.0;
  const float  nz = enc.z * enc.z - 1.0;
  const float  w  = 1.0 - nz * nz;
  return normalize(float3(xy * w, nz));
}

// Sectors [start, start + width) of the bit field. A zero width must produce an empty
// mask, and HLSL shifts are undefined at the width of the type, so the shift is
// guarded rather than relying on a masked result.
// Sectors [start, start + width) of the 128-bit field, one 32-bit limb at a time. The paper
// uses 32 because they fit one limb; 128 is four, and the cost the paper measures for that is
// 5-10%, which buys back the thing that was crushing both the occlusion and the bounce: with
// 32 sectors a sample's band covers a small fraction of the field, so a surface's occluded
// measure - and therefore the GI's per-sample weight - came out far below what the geometry
// actually subtends. Shifts are undefined at the width of the type, so both ends are guarded,
// and the arithmetic is signed because a start past a limb's base underflows unsigned.
uint4 SectorMask(uint start, uint width) {
  uint4 mask = uint4(0u, 0u, 0u, 0u);
  if (width == 0u) return mask;
  const int end = (int)(start + width);
  [unroll] for (int limb = 0; limb < 4; ++limb) {
    const int base = limb * 32;
    const int first = min(max((int)start - base, 0), 32);
    const int last = min(max(end - base, 0), 32);
    if (last > first) {
      const uint high = last >= 32 ? 0xFFFFFFFFu : (1u << (uint)last) - 1u;
      const uint low = first >= 32 ? 0xFFFFFFFFu : (first == 0 ? 0u : ((1u << (uint)first) - 1u));
      mask[limb] = high & ~low;
    }
  }
  return mask;
}

// Population count over the four limbs: HLSL has no 128-bit integer, so the field is four of
// them and this is the only place the count is assembled.
uint CountBits(uint4 mask) {
  return countbits(mask.x) + countbits(mask.y) + countbits(mask.z) + countbits(mask.w);
}

// Interleaved gradient noise: one value per pixel, spatially decorrelated at the pixel
// level. Used for the sample jitter, so that a temporal reconstruction's residual is noise
// rather than structure.
float Ign(float2 p) {
  return frac(52.9829189 * frac(dot(p, float2(0.06711056, 0.00583715))));
}

// Slice-relative CDF of the cosine-weighted measure inside a hemisphere slice.
//
// Sectors are equal in angle, but a slice's measure is not: solid angle carries a |sin|
// Jacobian around the view vector, which every slice shares as a pole, and cosine-weighted
// AO adds another cos(angle to the normal). Neither can be attached to a bit, so the
// horizon angle is pushed through the CDF of the measure instead, which makes each
// equal-width sector carry equal weight. This is the paper's "we must convert the angles
// from cosine space to angular space" (section 3.1), and it is what makes
// 1 - countbits/Nb the mean of equation 3 rather than of equation 4.
//
// The returned x in [0, 1] spans the half-slice. psi = x * PI - PI/2 = -(sigma + n),
// where sigma is the sample's signed angle from the view vector and n the projected
// normal's, so the return is the sector coordinate mirrored about the normal. The mirror
// is global - every sample takes it, including its back edge - and reflecting the bit
// assignments leaves popcount unchanged, so it costs nothing.
//
// GT-VBAO's SliceRelCDF_Cos: sideSign supplies its directionIsRight, and the normal's
// sign is already folded into normalAngle and sinN.
float RemapHorizonCdfCos(float horizonCos, float sideSign, float normalAngle, float cosN,
                         float sinN, float invNormalization) {
  const float d = sideSign;
  // The cosine CDF is not monotonic past the hemisphere edge, so hold the horizon at the
  // edge first: a sample below the tangent plane must collapse to zero width instead of
  // wrapping to the other side of the slice.
  const float h = max(clamp(horizonCos, -1.0, 1.0), sinN * d);
  const float sinTheta = sqrt(saturate(1.0 - h * h));
  const float theta = acos(h);
  const float cosTwoPsiPlusN = (2.0 * h * h - 1.0) * cosN - 2.0 * d * h * sinTheta * sinN;
  const float n0 = d > 0.0 ? 1.0 : 3.0;
  const float n1 = d > 0.0 ? 1.0 : -1.0;
  const float n2 = d > 0.0 ? 0.0 : 4.0;
  const float t0 = cosN * n0
                 + cosTwoPsiPlusN * n1
                 + (theta * (2.0 * n1 * d) + normalAngle * (n2 + 2.0 * n1) - AO_PI) * sinN;
  return saturate(t0 * invNormalization);
}

[numthreads(8, 8, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // This pass owns its dispatch, so the thread id is the pixel directly: no
  // transposed retail tiling to reproduce here.
  const uint2 p = tid.xy;
  if (p.x >= cBufferWidth || p.y >= cBufferHeight) return;

  // Typed UAV load: the scattered light the retail stage just wrote.
  const float4 light = resultBuffer.Load(int3(p, 0));

  const float depth = depthBuffer.Load(int3(p, 0)).x;
  const float z     = LinearZ(depth);
  if (!(z > 0.0) || z > 1.0e6) {
    return;
  }

  // If the injection does not reach this stage every field reads zero. That is
  // detected once and the defaults are used as a set. A per-knob "zero means default"
  // fallback is wrong: it makes a slider genuinely set to zero a no-op, so the bottom
  // of every range does nothing. These match the shipped settings, so a missing
  // injection degrades to the intended look rather than to the paper's quiet one.
  const bool injected = (CUSTOM_MGSV_AO_STRENGTH + CUSTOM_MGSV_AO_RADIUS
                         + CUSTOM_MGSV_AO_FADE + CUSTOM_MGSV_AO_SLICES
                         + CUSTOM_MGSV_AO_STEPS + CUSTOM_MGSV_AO_THICKNESS) > 0.0;
  const float amount    = injected ? CUSTOM_MGSV_AO_STRENGTH : 1.5;
  const float radius    = injected ? CUSTOM_MGSV_AO_RADIUS : 2.5;
  const float fadeDepth = injected ? CUSTOM_MGSV_AO_FADE : 0.25;
  const float thicknessScale = injected ? CUSTOM_MGSV_AO_THICKNESS : 4.0;
  const int   slices    = (int)clamp(injected ? CUSTOM_MGSV_AO_SLICES : 2.0, 1.0, 8.0);
  const int   steps     = (int)clamp(injected ? CUSTOM_MGSV_AO_STEPS : 6.0, 1.0, 24.0);

  // Fade the effect out with distance: screen-space occlusion has nothing to say about
  // the far field. An absolute falloff that reaches zero at the far end of the depth
  // range, overlaid with an exponential one tuned by the fade distance.
  const float fade = saturate(1.0 - depth * depth)
                   * saturate(exp2(-(depth / max(fadeDepth, 1e-4)) * (depth / max(fadeDepth, 1e-4))));
  if (fade < 0.001) {
    return;
  }

  const float2 pixel = float2(p);
  const float3 P = ViewPos(pixel, z);
  const float3 N = DecodeNormal(normalBuffer.Load(int3(p, 0)).xyz);

  // The view vector, from the surface toward the camera.
  const float3 V = normalize(-P);

  // Screen-space radius, so no world scale is involved. The radius is a fraction of
  // the screen, and the world radius is recovered from the depth so that the thickness
  // and the step spacing stay consistent with it.
  const float2 texelSize = 1.0 / float2(cBufferWidth, cBufferHeight);
  const float screenRadius = max(radius, 0.01) * 0.03;
  const float worldRadius  = screenRadius * P.z * 2.0 * abs(cProjectionParam.y);
  const float radiusPx     = max(1.0, screenRadius * float(cBufferWidth));

  // The paper's thickness: a small constant fraction of the radius, their benchmark
  // being 0.2 at radius 4. It is the parameter they call decisive for visual fidelity -
  // too large and a surface occludes itself, too small and every band falls below one
  // sector of the 32 and the occlusion disappears. It is scaled by a setting because
  // the radius here is reconstructed through the projection, and the world scale that
  // reconstruction assumes has never been verified.
  const float thickness = thicknessScale * 0.05 * worldRadius;

  // The sample jitter is per pixel, from interleaved gradient noise. It was a repeating 4x4
  // tile, which was wrong in a way that only shows once there is a temporal term: the tile
  // gave sixteen values per frame and the frame phase is one scalar for the whole image, so
  // every pixel of a 4x4 block shifted in lockstep and shared its entire phase history. A
  // reconstruction averages over a finite window, so the block's residual stayed correlated
  // across its own pixels - a 4x4 grid the reconstruction cannot remove, because it is
  // structure rather than noise. Per-pixel noise is what a reconstruction is for: its
  // residual is high frequency, which the neighbourhood clamp resolves.
  const float jitter = Ign(pixel);
  // A second, decorrelated value for the step jitter: it used to be a linear function of the
  // slice jitter, which stacks the two errors instead of averaging them.
  const float stepJitter = Ign(pixel + float2(47.0, 71.0));

  // The temporal term. CUSTOM_RANDOM is a fresh uniform in [0, 1) every frame: the addon
  // binds it to utils::random, whose present handler rewrites it unconditionally, and film
  // grain is its only other consumer. That is enough to rotate the sample pattern per frame
  // and let the frame's temporal reconstruction accumulate the phases - the paper's
  // "distributed spatially and temporally" (section 3.1), and what makes a low sample count
  // viable: eight phases turn two directions into sixteen.
  //
  // It covers every reconstruction path this addon offers, DLSS included - anything but
  // Off, which is the point. DLSS is a temporal upscaler with an enforced eight-phase
  // jitter, so a rotating AO converges under it the same way it does under the analytical
  // TAA and FSR3, and at DLSS render resolutions the pass also shades fewer pixels to begin
  // with. With reconstruction off the term is deliberately inert: a pattern that rotates
  // with nothing accumulating it is shimmer, not grain.
  const float framePhase = CUSTOM_TAA != 0.0 ? CUSTOM_RANDOM : 0.0;
  const float slicePhase = frac(jitter + framePhase);
  const float stepPhase = frac(stepJitter + framePhase);

  // The visibility integral. `occlusion` accumulates each slice's own occluded fraction
  // and `sliceWeightSum` the measure of the slices that contributed, so the average below
  // is normalized by the slices that were actually evaluated.
  float occlusion = 0.0;
  float sliceWeightSum = 0.0;

  [loop] for (int slice = 0; slice < slices; ++slice) {
    const float phi = ((float)slice + slicePhase) * AO_PI / (float)slices;
    const float2 dir2d = float2(cos(phi), sin(phi));

    // The slice plane contains the view vector and the screen direction. The projected
    // normal is the centre of the sector space, the tangent is the in-plane axis the
    // signed angle is measured against, and the view vector is the pole every horizon
    // angle is measured from.
    const float3 sliceDir = normalize(float3(dir2d.x * cProjectionParam.x,
                                            -dir2d.y * cProjectionParam.y,
                                             0.0));
    const float3 planeNormal = normalize(cross(sliceDir, V));
    const float3 tangent = cross(V, planeNormal);
    const float3 projectedNormal = N - planeNormal * dot(N, planeNormal);
    const float projectedLength = length(projectedNormal);
    // A slice whose plane contains the normal sees it straight down the view vector, so
    // its measure is zero. It adds nothing to either sum, so dropping it cannot bias the
    // average the way it would if the divisor were the slice count.
    if (projectedLength < 1e-4) continue;

    const float cosN = clamp(dot(projectedNormal / projectedLength, V), -1.0, 1.0);
    const float normalSign = -sign(dot(projectedNormal, tangent));
    const float normalAngle = normalSign * acos(cosN);
    const float sinN = normalSign * sqrt(saturate(1.0 - cosN * cosN));
    const float invNormalization = 0.25 / max(cosN + normalAngle * sinN, 1e-4);

    uint4 mask = uint4(0u, 0u, 0u, 0u);
    // The bands' exact widths and the spans the bit field claims for them, in sectors. See
    // the quantiser at the bottom of the step loop.
    float bandWidthSum = 0.0;
    float bandSpanSum = 0.0;

    [loop] for (int side = 0; side < 2; ++side) {
      const float sideSign = side == 0 ? 1.0 : -1.0;

      [loop] for (int step = 1; step <= steps; ++step) {
        // Exponential distribution: nearby surfaces influence the result more, and the
        // paper notes low sample density near the pixel loses detail around small
        // objects. The jitter on the step masks the banding that even spacing causes, and
        // it carries the frame phase with it.
        const float t = ((float)step - 0.5 + stepPhase) / (float)steps;
        const float span = (exp(t * 1.5) - 1.0) / (exp(1.5) - 1.0);
        const float2 offset = dir2d * sideSign * span * radiusPx;
        const float2 sampleUv = (pixel + 0.5) * texelSize + offset * texelSize;
        if (sampleUv.x <= 0.0 || sampleUv.y <= 0.0 || sampleUv.x >= 1.0 || sampleUv.y >= 1.0) break;

        const int2 samplePixel = (int2)floor(sampleUv * float2(cBufferWidth, cBufferHeight));

        // A distant step is looking for a large occluder but reads a single depth texel, so
        // an occluder narrower than the step spacing falls between two steps and is never
        // seen - the measured reason the effect wanted a high step count. The reference
        // implementation answers that by fetching the step from a coarse level of a
        // nearest-reduced depth chain, so one texel already covers the footprint the step
        // skips over (XeGTAO's depth mip sampling). The level follows the step's distance
        // with their 3.3 offset: steps within eight texels read full resolution, and a step
        // a hundred and twenty-eight texels out reads a level whose texel is eight wide.
        const int mipLevel =
            CUSTOM_MGSV_AO_DEPTH_MIPS > 0.5
                ? clamp((int)floor(log2(max(span * radiusPx, 1.0)) - 3.3), 0, AO_DEPTH_MIP_LEVELS)
                : 0;
        float sampleZ;
        if (mipLevel == 0) {
          sampleZ = LinearZ(depthBuffer.Load(int3(samplePixel, 0)).x);
        } else {
          const int2 coarseSize = max(int2(cBufferWidth, cBufferHeight) >> mipLevel, 1);
          const int2 coarsePixel = clamp(samplePixel >> mipLevel, int2(0, 0), coarseSize - 1);
          sampleZ = LinearZ(depthMips.Load(int3(coarsePixel, mipLevel - 1)).x);
        }
        if (!(sampleZ > 0.0) || sampleZ > 1.0e6) continue;

        // The sample keeps its exact position and takes only its *depth* from the coarse
        // level. Reconstructing it at the reduced texel's centre - which is what the
        // reference implementation does, and what this did at first - moves it off the slice
        // line by up to half a coarse texel, and the horizon math below assumes the sample
        // lies in the slice's plane. At level 4 that is eight pixels of sideways error at a
        // hundred-pixel step: several sectors' worth, which showed up as bands collapsing
        // into thin, grid-aligned lines instead of covering what they should.
        const float3 sf = ViewPos(float2(samplePixel), sampleZ);
        // The back sample, one thickness deeper along the view ray: the depth buffer
        // stores the *front* face, so the occluder's body extends away from the camera.
        // V points from the surface toward the camera, so deeper is a subtraction.
        //
        // The sign is the first deviation listed in the header. Put the band on the
        // camera side instead and coplanar geometry - most of the screen - stops
        // collapsing to zero and marks a band above its own horizon, which is uniform
        // self-occlusion on open ground.
        const float3 sb = sf - V * thickness;

        // Horizon cosines, measured from the view vector because that is the pole of the
        // slice and so the axis the sector space is anchored to. The CDF below converts
        // them into the equal-measure sector coordinate.
        const float3 df = sf - P;
        const float3 db = sb - P;
        const float lenSqF = dot(df, df);
        const float lenSqB = dot(db, db);
        if (lenSqF < 1e-10 || lenSqB < 1e-10) continue;
        const float hF = dot(df, V) * rsqrt(lenSqF);
        const float hB = dot(db, V) * rsqrt(lenSqB);

        // Which side of the pole the sample sits on, which is which branch of the CDF
        // applies. Both edges of one sample share it: they are a thickness apart, not a
        // sign apart.
        const float sampleSide = dot(df, tangent) >= 0.0 ? 1.0 : -1.0;

        const float xF = RemapHorizonCdfCos(hF, sampleSide, normalAngle, cosN, sinN, invNormalization);
        const float xB = RemapHorizonCdfCos(hB, sampleSide, normalAngle, cosN, sinN, invNormalization);

        // The band's true width and the span the bit field claims for it. The slice's
        // occlusion below is the sum of the widths less the overlap the mask actually
        // detected - the sum of the claims minus its popcount - so a band contributes its
        // true measure rather than a whole sector or nothing. Where bands do not overlap,
        // which is the common case and every monotone surface, that is exact and there is
        // no quantisation left to show as grain.
        //
        // The dither this replaces was unbiased in expectation but deterministic per pixel,
        // so its error was the same at every pixel of a flat surface - a fixed pattern that
        // reads as grain and that no sample count averages away, since every band in a slice
        // shares the phase. Carrying the widths removes the randomness from the decision
        // instead of sampling it, and what remains is smooth.
        const float lo = min(xF, xB) * (float)AO_SECTORS;
        const float hi = max(xF, xB) * (float)AO_SECTORS;
        const uint start = (uint)clamp(floor(lo + 0.5), 0.0, (float)AO_SECTORS);
        const uint end   = (uint)clamp(floor(hi + 0.5), 0.0, (float)AO_SECTORS);
        const uint4 band = SectorMask(start, end - start);
        bandWidthSum += hi - lo;
        bandSpanSum += (float)(end - start);

        mask |= band;
      }
    }

    // The union's measure: the exact band widths, less the overlap the bit field detected.
    // A union where nothing overlaps therefore comes out exact, and one that overlaps is
    // corrected by however much the mask could see.
    const float sliceOcclusion = saturate((bandWidthSum - bandSpanSum + (float)CountBits(mask)) / (float)AO_SECTORS);
    // Slices do not integrate equal amounts of the hemisphere, so averaging them equally
    // weights them as if they did. Each is weighted here by its own measure,
    // |projN| (cosN + n sinN), and the average is normalized by the sum. Equation 2 says
    // equal weights; equation 3's measure contradicts it, and this follows equation 3.
    const float sliceWeight = projectedLength * (cosN + normalAngle * sinN);
    occlusion += sliceOcclusion * sliceWeight;
    sliceWeightSum += sliceWeight;
  }

  const float occlusionRatio = saturate(occlusion / max(sliceWeightSum, 1e-4));

  // The application, per the paper (sections 3.2 and 3.3): the un-occluded fraction
  // multiplies the light.
  //
  // Amount scales the occlusion rather than lerping toward it: 1 is the paper's occlusion
  // and above 1 over-darkens, which is what makes the top of the slider do anything. The
  // saturation happens after the scale, so a pixel can at worst be fully dark.
  //
  // Note that this buffer is the whole light, direct included, so the visibility also
  // scales sunlit surfaces. The paper multiplies the ambient and indirect terms, which
  // this buffer does not separate.
  const float occlusionApplied = saturate(occlusionRatio * amount) * fade;

  // Alpha is written through from the light, not set to 1. The retail chain reads the
  // light's alpha (0.04 at the shading stage) as its "this pixel was shaded" flag, and
  // overwriting it is what broke skin scattering when the scatter pass was replaced.
  // This pass only scales rgb, so the flag survives untouched.
  resultBuffer[p] = float4(light.rgb * (1.0 - occlusionApplied), light.a);
}
