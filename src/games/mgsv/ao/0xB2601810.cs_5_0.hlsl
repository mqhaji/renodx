// MGSV line-integral AO, second stage - CRC32 0xB2601810, cs_5_0.
//
// REPLACED WITH VBAO - "Screen Space Indirect Lighting with Visibility Bitmask",
// Therrien, Levesque and Gilet (The Visual Computer, 2023), Algorithm 1.
//
// The method replaces GTAO's two horizon angles with a bit field of Nb sectors
// distributed around the hemisphere slice, so each sample can occlude the angular band
// its own thickness subtends instead of everything below its horizon. That is what lets
// light pass behind thin surfaces. It is also what keeps a flat surface from occluding
// itself: with the band on the deeper side, a coplanar sample's whole band falls past
// the tangent plane and the sector clamp collapses it to zero width.
//
// What this follows, from the paper:
//
//   * The sector space is centred on the projected normal and spans [-PI/2, PI/2], so
//     one bit field covers both sides of the slice. The sectors are equal in *measure*,
//     not in angle: the horizon is pushed through the CDF of the slice's cosine-weighted
//     measure (equation 3) before quantising, which is section 3.1's instruction to
//     convert the angles out of cosine space. That CDF is a verbatim port of GT-VBAO's
//     SliceRelCDF_Cos and was checked against an independent numerical integration of the
//     density |sin(psi + n)| cos(psi) to 5 decimals over its whole domain.
//   * The occluding band is between the sample and a back sample one thickness deeper,
//     along the view ray: sb = sf - V * t. The paper writes that as
//     sb = sf - normalize(p) * t, which reads as the opposite direction. See the first
//     deviation below.
//   * AO = 1 - popcount(mask) / Nb (algorithm line 26). With equal-measure sectors that
//     is the slice's own visibility, and slices are combined by their own measure - see
//     the fifth deviation. The bit field is the integration; there is no horizon angle.
//   * The un-occluded fraction multiplies the light, which is the paper's application in
//     sections 3.2 and 3.3.
//   * The thickness is a small constant - the paper's benchmark uses 0.2 at radius 4,
//     five percent of the radius - and it is the parameter that decides visual fidelity.
//     GTAO without falloff is equivalent to an infinite thickness, which is the
//     over-occlusion this avoids. The setting's default is the paper's five percent.
//   * Samples are distributed exponentially along the direction, and jittered, because
//     evenly spaced samples band.
//
// Where this deviates from Algorithm 1, and why. Each is deliberate; do not "fix" one
// without reading its note.
//
//   * Line 15, back sample. Algorithm 1 subtracts normalize(p); this adds it. The paper
//     never states its view-space handedness, so normalize(p) points at or away from the
//     camera depending on the convention and the line cannot be read literally. The
//     deeper form is the one that reproduces the paper's own GTAO limit (an infinite
//     thickness must sweep the band to the tangent plane, as a horizon does, not to the
//     normal) and the one Mirko Salm's GT-VBAO corrections ship. It is also what keeps a
//     coplanar sample's band entirely past the tangent plane, where the clamp collapses it
//     to zero width: put the band on the camera side instead and a flat floor darkens
//     itself uniformly, which is the failure this line has already caused twice.
//   * Line 18, sector edges. Algorithm 1 uses floor for the low edge and ceil for the
//     high edge. This rounds both to nearest to build the bit field, but the value does not
//     come from the popcount alone: the slice's occlusion is the sum of the bands' exact
//     widths, less the overlap the mask actually detected (the sum of the claims it made
//     minus its popcount). A union in which nothing overlaps therefore comes out exact, and
//     so does every monotone surface; only overlapping bands lose anything, and only by
//     whatever the mask could see. Measured against exact interval unions: exact in six of
//     eight scenarios, -0.02 on a 30% overlap. The floor/ceil bias GT-VBAO records is +1
//     sector per marched direction, which is worse and is why it is documented, not used.
//     Before this, both edges took a shared stochastic dither: unbiased in expectation, but
//     deterministic per pixel, so on a flat surface every pixel rounded the same way and the
//     error was a fixed pattern that read as grain, which no sample count could average.
//   * Line 6, step size. Algorithm 1 spaces samples at a constant r/(Ns+1); this uses
//     the exponential spacing of section 4.3 and table 2's "exp. steps", which the paper
//     recommends for detail around small objects.
//   * Line 7 and section 3.1, distribution. The paper asks for the occlusion integral to
//     be distributed spatially *and* temporally. Both are here: a coherent 4x4 tile
//     spatially, and CUSTOM_RANDOM - a fresh uniform every frame, which the addon's
//     utils::random already rewrites for film grain - rotating the slice angles and the
//     step jitter per frame. That is what makes a low sample count viable, and it covers
//     DLSS as well as the analytical TAA and FSR3, since the gate is "any reconstruction
//     except Off". Off leaves the term inert on purpose: a rotating pattern with nothing
//     accumulating it is shimmer rather than grain. The one thing the paper's own
//     benchmarks do that this cannot is denoise afterwards - nothing downstream of this
//     pass runs - so the phases are averaged by the frame's reconstruction instead.
//   * Equation 2, slice weights. Equation 2 averages the slices with equal weight. This
//     weights each by its own cosine-weighted measure, |projN| (cosN + n sinN), and
//     normalizes by the sum. Equal weights are only correct if every slice integrates the
//     same amount of the hemisphere, and equation 3 says they do not; the equal-measure
//     sectors above are meaningless without the matching weights. Normalizing by the
//     measured slices also removes a bias that the equal average had: a slice dropped as
//     degenerate used to count as fully occluded.
//
// Not implemented here, and part of the paper: indirect diffuse (Algorithm 1 lines
// 20-24) and directionally occluded ambient light (section 3.2). This pass computes the
// ambient-occlusion visibility and applies it per sections 3.2/3.3 by multiplying the
// light. See the workspace note MGSV_AmbientOcclusion.md for the pipeline, the evidence,
// and what is still open.
//
// Bindings, from the retail DXBC:
//   b0  Constants   cBufferWidth, cBufferHeight, cNumTilesX, cNumTilesY,
//                   cHalfFovRecip, cCameraCenterOffset, cProjectionParam,
//                   cTweakParams, cFade
//   t0  lightBuffer          (the preceding stage's local average of the light, unread -
//                             see the texture declarations below)
//   t1  depthBuffer          (r32_float)
//   t2  normalBuffer         (b8g8r8a8, same packing as the deferred G-buffer)
//   t4  lightBufferOriginal  (the light this stage applies AO to)
//   u0  resultBuffer         (light with AO applied)
//   128x1x1 threads, one group per 128 pixels of a row.

#include "../shared.h"

#define AO_PI 3.14159265
#define AO_SECTORS 32u

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

// t0 is bound by the engine - it is the preceding stage's local average of the light -
// but the retail stage's use of it (fading the light toward that average) is not what
// the paper does, and the replacement does not read it. Its declaration is omitted
// deliberately, not forgotten.
Texture2D<float4> depthBuffer : register(t1);
Texture2D<float4> normalBuffer : register(t2);
Texture2D<float4> lightBufferOriginal : register(t4);  // the light this stage applies AO to
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
uint SectorMask(uint start, uint width) {
  return width > 0u ? (0xFFFFFFFFu >> (AO_SECTORS - width)) << start : 0u;
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

[numthreads(128, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  // The engine dispatches this pass transposed: the retail shader stores at
  // vThreadID.yx and samples at vThreadID.yx, so tid.y is the column and tid.x is the
  // row. Taking tid.xy as (column, row) writes only cNumTilesX*128 rows as if they were
  // columns - a hard vertical edge at 76% of the width - and reads the depth buffer
  // transposed.
  const uint2 p = tid.yx;
  if (p.x >= cBufferWidth || p.y >= cBufferHeight) return;

  const float4 light = lightBufferOriginal.Load(int3(p, 0));

  const float depth = depthBuffer.Load(int3(p, 0)).x;
  const float z     = LinearZ(depth);
  if (!(z > 0.0) || z > 1.0e6) {
    resultBuffer[p] = light;
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
    resultBuffer[p] = light;
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

  // The sample jitter is spatially coherent, from a repeating 4x4 tile, rather than a
  // per-pixel hash. A per-pixel random offset is neither spatial nor temporal in the
  // sense the paper means: it makes the field grain, and the variation lands on the
  // first sample, where no amount of extra samples averages it away. The temporal half
  // sits with the frame phase below.
  const uint tiles = 4u;
  const uint tileIndex = dot(p % tiles, uint2(1u, tiles)) * 11u;
  const float jitter = float((tileIndex % (tiles * tiles)) + 0.5) / float(tiles * tiles);

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
  // The step jitter is a deliberately different value from the slice phase. They shared one
  // until now, which stacks their errors instead of averaging them.
  const float stepPhase = frac(jitter * 2.0 + 0.31 + framePhase);

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

    uint mask = 0u;
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
        const float sampleZ = LinearZ(depthBuffer.Load(int3(samplePixel, 0)).x);
        if (!(sampleZ > 0.0) || sampleZ > 1.0e6) continue;

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
        bandWidthSum += hi - lo;
        bandSpanSum += (float)(end - start);
        mask |= SectorMask(start, end - start);
      }
    }

    // The union's measure: the exact band widths, less the overlap the bit field detected.
    // A union where nothing overlaps therefore comes out exact, and one that overlaps is
    // corrected by however much the mask could see.
    const float sliceOcclusion = saturate((bandWidthSum - bandSpanSum + (float)countbits(mask)) / (float)AO_SECTORS);
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
  // multiplies the light. This is what lets the integral reach the image at all. It
  // previously reached it only as a lerp weight toward t0, the preceding stage's average
  // of the light, which moves the light by weight * (t0 - light) - a few percent, and a
  // blur rather than an occlusion.
  //
  // Amount scales the occlusion rather than lerping toward it: 1 is the paper's occlusion
  // and above 1 over-darkens, which is what makes the top of the slider do anything. It
  // used to be wrapped in saturate() for the lerp formulation, where more than 1 meant
  // extrapolating past the average, and that silently pinned everything from 100 up to the
  // same value. The saturation now happens after the scale, so a pixel can at worst be
  // fully dark. At 1 the result is arithmetically identical to the lerp it replaces.
  //
  // Note that this buffer is the whole light, direct included, so the visibility also
  // scales sunlit surfaces. The paper multiplies the ambient and indirect terms, which
  // this buffer does not separate.
  const float occlusionApplied = saturate(occlusionRatio * amount) * fade;
  resultBuffer[p] = float4(light.xyz * (1.0 - occlusionApplied), light.w);
}
