// SPDX-License-Identifier: MIT

// Fills the fog volume: for each screen column, the light scattered toward the
// camera (RGB, divided by the scene's fog scale) and the transmittance (alpha)
// from the camera to each of the 128 slices.
//
// The fog is the game's (VolFog_TppVolFog, VS 0x13EEB68B and PS 0x99BEE81F):
// daylight scattering along the view ray, with that draw's constants. The
// game's shader gives every cell the light of an unshadowed path. This pass
// adds the light slab by slab from the camera outward and scales the sun's
// share of each slab by how much of the cell the sun reaches, so fog in
// shadow stays dark and shafts form. With every cell lit, and the corrections
// described below switched off or out of reach, the two agree to half-float
// rounding, except that the game clamps the stored light to 1 and this pass
// does not: at a low sun the far haze toward it goes past 1 and the game's
// clamp flattened it. Every reader multiplies the stored light by the scale
// it was divided by, or reads only the transmittance.
//
// How much of a cell the sun reaches is the visibility pass's estimate for
// this frame, blended here with the value last frame's visibility image holds
// at the same place in the world. Only this visibility is carried over time;
// the light's color and direction are applied fresh, so they never trail.
//
// The game's fog is as thick on a mountain top as in the valley below it. Here
// its density falls off with height, as haze does: the game's density holds at
// the map's lowest ground (sea level on a map without terrain) and thins by a
// fixed share per unit of height above it. Each slab's optical thickness on
// the game's curve is scaled by the mean of that falloff along the slab.
//
// Thick fog and blowing dust are never even. Where the game's fog is dense
// (its range under a few thousand units) the density is varied by a drifting
// noise, around the game's value as the mean; clear-weather haze stays even.
//
// Local lights (spots and point lights) add their light to the same slabs,
// scattered as a medium of the fog's density scatters light. The game's fog
// has no such term. A light's falloff with distance is steep, so it is not
// sampled at a point: its inverse-square part and the scattering's dependence
// on the angle are integrated along the slab's stretch of the view ray in
// closed form, which stays steady when a lamp sits inside a cell.

#include "./fog_lights.hlsli"

// The fog draw's scene constants of the previous execution.
cbuffer PreviousFogDrawScene : register(b9) {
  FogScene previous_scene;
}

Texture2D<float> history : register(t2);
Texture2D<float> estimate : register(t3);
Texture2D<float4> rain_occlusion : register(t4);
Texture2D<uint> light_bins : register(t8);
Texture2D<float> spot_shadows : register(t10);
Texture3D<float> noise : register(t11);
SamplerState linear_clamp : register(s0);
SamplerState linear_wrap : register(s2);
RWTexture2D<float4> volume : register(u0);
RWTexture2D<float> visibility : register(u1);

// The share of the new frame in the result. Forty-odd frames of four points
// each make the estimate.
static const float NEW_WEIGHT = 0.05;

// Uneven fog. The fog's range (where the game's fog turns opaque) at which
// the unevenness starts and at which it is full: clear and rainy days have
// ranges over 6,000 units, fog under 800, a sandstorm under 100.
static const float UNEVEN_FROM_RANGE = 5000.0;
static const float UNEVEN_FULL_RANGE = 1000.0;
// The noise is read at two sizes: banks (one texel of the noise per this
// many units) and wisps. Each drifts along the wind at its own pace, the
// wisps faster, so the pattern changes as it moves.
static const float2 UNEVEN_TEXEL = float2(6.0, 1.5);
static const float3 UNEVEN_WIND = float3(0.94, 0.05, 0.34);
static const float2 UNEVEN_SPEED = float2(1.5, 2.5);

// The least and the greatest estimate among a slice's cells in and around a
// column.
float2 EstimateRange(uint slice, uint2 cells, uint2 column) {
  const uint2 tile = uint2(slice % uint(FOG_TILES.x), slice / uint(FOG_TILES.x)) * cells;
  float2 range = float2(1.0, 0.0);
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      const float value = estimate[tile + uint2(clamp(int2(column) + int2(x, y), 0, int2(cells) - 1))];
      range = float2(min(range.x, value), max(range.y, value));
    }
  }
  return range;
}

[numthreads(8, 8, 1)] void main(uint3 id : SV_DispatchThreadID) {
  uint width, height;
  volume.GetDimensions(width, height);
  const uint2 cells = uint2(width, height) / uint2(FOG_TILES);
  if (id.x >= cells.x || id.y >= cells.y) return;

  // 1. The column's ray.
  const float2 clip = FogClip((float2(id.xy) + 0.5) / float2(cells));
  const float3 ray = FogViewPosition(clip, 1.0);
  const float ray_length = length(ray);
  const float3 direction = ray / ray_length;
  const float cosine = dot(fog_object.m_localParam[1].xyz, normalize(mul(float4(ray, 0.0), fog_scene.m_view).xyz));

  // 2. Whether the camera stands under a rain shelter: one lookup at the
  // camera's place in the top-down occlusion map, which holds two layers of
  // shelter height (x, z) and opacity (y, w). A shelter weakens the forward lobe.
  const float3 map_position = float3(-fog_object.m_localParam[0].x, fog_object.m_localParam[0].z, -fog_object.m_localParam[0].y) + fog_materials[4].xyw;
  const float4 shelter = rain_occlusion.SampleLevel(
      linear_clamp, map_position.xy * 0.03 * float2(0.5, -0.5) + 0.5 - 0.5 * m_renderBuffer.zw, 0.0);
  const float2 below = saturate((0.001 + shelter.xz - (1.0 - saturate(0.02 * map_position.z))) / 0.001);
  const float2 open = below * shelter.yw + (1.0 - below);
  const float sharpness = fog_materials[2].w * open.x * open.y;

  // 3. The light a unit of optical distance scatters toward the camera: the
  // sun's by the forward and blue terms, the sky's by the third.
  const float forward_lobe = 0.0795774683 * (1.0 - sharpness * sharpness) / ((1.0 - sharpness * cosine) * (1.0 - sharpness * cosine));
  const float3 sunlit = (fog_materials[2].xyz * (fog_materials[0].y + (1.0 - fog_materials[0].y) * forward_lobe)
                         + fog_materials[3].xyz * (0.0596831031 * (1.0 + cosine * cosine)))
                        * fog_materials[0].w;
  const float3 skylit = fog_materials[3].w * fog_materials[1].xyz;
  const float3 scattering = fog_materials[3].xyz + fog_materials[2].xyz + fog_materials[3].w;
  // A local light has no part in the game's fog, so it is scattered as a
  // medium of the fog's density scatters light: by all three terms, each
  // sending on all the light it takes (its share over the whole sphere is 1).
  // The forward term's color is authored to tint the sun's white number, so a
  // lamp takes its largest channel; the term's constant share goes evenly in
  // every direction, as the third term's does, and the rest by the lobe.
  const float lamp_forward = max(fog_materials[2].x, max(fog_materials[2].y, fog_materials[2].z));
  const float lamp_even = 0.0795774683 * (lamp_forward * fog_materials[0].y + fog_materials[3].w);

  // 4. Front to back. With the source constant over a slab, the light it adds
  // is the source over the scattering coefficient times the drop in
  // transmittance across it.
  float3 light = 0.0;
  float3 local_light = 0.0;
  float3 reached = 1.0;
  float near_depth = 0.0;
  // The height above the reference a point of the ray is at: the eye's, plus
  // the ray's climb per unit of view depth.
  const float eye_height = fog_object.m_localParam[0].y - ((fog_flags & FOG_FLAG_TERRAIN_MAP) != 0u ? terrain_object.m_localParam[2].y : 0.0);
  const float climb = mul(float4(ray, 0.0), fog_scene.m_view).y;
  float optical_distance = 0.0;
  float game_optical_distance = 0.0;
  // How uneven the fog is: none in clear weather, full in thick fog.
  const float uneven = fog_unevenness * saturate(log2(UNEVEN_FROM_RANGE / exp2(1.0 / fog_scene.m_fogParam[1].x)) / log2(UNEVEN_FROM_RANGE / UNEVEN_FULL_RANGE));
  const float3 drift = UNEVEN_WIND * fog_time;
  const uint2 bin_count = FogBins(cells);
  // The radius of the disk as large as the cell's section across the ray,
  // per unit of distance along the ray.
  const float2 cell_sides = 2.0 * FOG_CLIP_SCALE / float2(cells) * fog_scene.m_projectionParam.xy;
  const float cell_radius_scale = sqrt(abs(cell_sides.x * cell_sides.y) / (3.14159265359 * ray_length * ray_length * ray_length));
  // The estimate's range in the slice before, this slice and the one after.
  float2 range_before = EstimateRange(0u, cells, id.xy);
  float2 range_here = range_before;
  for (uint slice = 0u; slice < uint(FOG_SLICES); ++slice) {
    const uint2 texel = uint2(slice % uint(FOG_TILES.x), slice / uint(FOG_TILES.x)) * cells + id.xy;
    const float far_depth = FogSliceDepth(float(slice));
    const float2 range_after = EstimateRange(min(slice + 1u, uint(FOG_SLICES) - 1u), cells, id.xy);

    // How much of the cell the light reaches: this frame's estimate, blended
    // with last frame's value at the cell's middle. The cell at slice s
    // covers slice coordinates s - 1 to s, so a depth's coordinate is its
    // slice plus a half. Depths before the first slice belong to the first
    // cell.
    float lit = estimate[texel];
    if ((fog_flags & FOG_FLAG_HISTORY) != 0u) {
      const float3 world = mul(float4(FogViewPosition(clip, slice == 0u ? 0.5 : sqrt(near_depth * far_depth)), 1.0), fog_scene.m_view).xyz;
      // m_view is a rotation and a translation, so its inverse is the
      // transpose applied to the offset from its translation row.
      const float3 view = mul((float3x3)previous_scene.m_view, world - previous_scene.m_view[3].xyz);
      const float2 fraction = view.xy / (max(view.z, 1.0e-4) * previous_scene.m_projectionParam.xy * FOG_CLIP_SCALE) * 0.5 + 0.5;
      const float coordinate = max(FOG_SLICES * previous_scene.m_fogParam[1].x * log2(max(view.z, 1.0e-4)) + 0.5, 0.0);
      if (view.z > 0.0 && all(fraction >= 0.0) && all(fraction <= 1.0) && coordinate <= FOG_SLICES - 1.0) {
        const float lower = floor(coordinate);
        const float upper = min(lower + 1.0, FOG_SLICES - 1.0);
        // Keep the lookup's footprint inside its tile.
        const float2 inside = clamp(fraction, 0.5 / float2(cells), 1.0 - 0.5 / float2(cells));
        const float previous = lerp(
            history.SampleLevel(linear_clamp, (float2(lower % FOG_TILES.x, floor(lower / FOG_TILES.x)) + inside) / FOG_TILES, 0.0),
            history.SampleLevel(linear_clamp, (float2(upper % FOG_TILES.x, floor(upper / FOG_TILES.x)) + inside) / FOG_TILES, 0.0),
            coordinate - lower);
        // The old value is kept within the range of this frame's estimates
        // in and around the cell. Where a shadow has just arrived or left,
        // those estimates agree and the old value gives way at once; where
        // they are noisy, the old value steadies them.
        lit = lerp(
            clamp(previous, min(range_before.x, min(range_here.x, range_after.x)), max(range_before.y, max(range_here.y, range_after.y))),
            lit,
            NEW_WEIGHT);
      }
    }
    visibility[texel] = lit;
    range_before = range_here;
    range_here = range_after;

    // The game's optical depth is a scattering coefficient times a power of
    // the distance past the start distance.
    // Its thickness falls off with height. Over a slab whose height changes
    // from a to b the mean of exp(-f h) is (exp(-f a) - exp(-f b)) / (f (b - a)).
    // Below the reference the density stays the game's, which makes the mean
    // approximate for the one slab that crosses it.
    const float far_game_optical_distance = pow(max(far_depth * ray_length - fog_materials[0].z, 0.0), fog_materials[1].w);
    const float low = fog_height_falloff * max(eye_height + climb * near_depth, 0.0);
    const float high = fog_height_falloff * max(eye_height + climb * far_depth, 0.0);
    float thickness = (far_game_optical_distance - game_optical_distance) * (abs(high - low) > 1.0e-3 ? (exp(-low) - exp(-high)) / (high - low) : exp(-0.5 * (low + high)));
    if (uneven > 0.0) {
      // The noise at the slab's middle, blurred to the slab's length so a
      // long slab takes its mean. The noise's mean is a half, so at full
      // unevenness the density runs from almost none to twice the game's
      // with the game's value as its mean. The floor keeps a slab from
      // emptying; it rarely acts.
      const float3 middle = mul(float4(ray * (0.5 * (near_depth + far_depth)), 1.0), fog_scene.m_view).xyz;
      const float2 blur = log2(max((far_depth - near_depth) * ray_length / UNEVEN_TEXEL, 1.0));
      const float banks = noise.SampleLevel(linear_wrap, (middle - drift * UNEVEN_SPEED.x) / (UNEVEN_TEXEL.x * 64.0), blur.x);
      const float wisps = noise.SampleLevel(linear_wrap, (middle - drift * UNEVEN_SPEED.y) / (UNEVEN_TEXEL.y * 64.0), blur.y);
      thickness *= max(1.0 + uneven * (1.3 * (banks - 0.5) + 0.7 * (wisps - 0.5)), 0.05);
    }
    optical_distance += thickness;
    game_optical_distance = far_game_optical_distance;
    const float3 passed = exp(-scattering * optical_distance);
    light += (sunlit * lit + skylit) / max(scattering, 1.0e-6) * (reached - passed);

    // Local lights over the slab's stretch of the ray, from its near
    // distance to its far one. The numbers of a light's constants are in
    // fog_lights.hlsli.
    if ((fog_flags & FOG_FLAG_LOCAL_LIGHTS) != 0u) {
      const float slab_start = near_depth * ray_length;
      const float slab_end = far_depth * ray_length;
      const uint row = ((slice / FOG_BIN_SLICES) * bin_count.y + id.y / FOG_BIN_CELLS) * bin_count.x + id.x / FOG_BIN_CELLS;
      float3 local_source = 0.0;
      // What the slab's fog takes from a lamp's light per unit of distance
      // on its way to the ray, as the volume's transmittance counts it.
      const float lamp_extinction = fog_materials[0].x * thickness / max(slab_end - slab_start, 1.0e-6);
      [loop] for (uint slot = 0u; slot < FOG_BIN_LIGHTS; ++slot) {
        const uint entry = light_bins[uint2(slot, row)];
        if (entry == FOG_BIN_END) break;
        const uint index = entry & FOG_BIN_LIGHT_MASK;
        const uint shadow_tile = (entry >> FOG_BIN_SHADOW_SHIFT) & FOG_BIN_SHADOW_MASK;
        const uint type = asuint(FogLightVector(index, FOG_LIGHT_KIND).x);
        const float3 position = FogLightVector(index, 3u).xyz;
        // Where along the ray the light is nearest, and how far off the ray.
        const float along = dot(position, direction);
        const float3 across = position - direction * along;
        const float offset_squared = dot(across, across);
        const float reach = f16tof32(entry >> 16u);
        if (offset_squared >= reach * reach) continue;
        // The part of the slab within the light's reach.
        const float half_chord = sqrt(reach * reach - offset_squared);
        const float from = max(slab_start, along - half_chord);
        const float to = min(slab_end, along + half_chord);
        if (from >= to) continue;

        // The falloff's inverse-square part, averaged over the slab: the
        // integral of 1 / (offset^2 + s^2) is atan(s / offset) / offset.
        // The cell's light stands for every ray through the cell, not only
        // its middle one. Near a lamp the falloff changes fast across the
        // cell, and the middle ray's value would swing as the camera turns
        // and the lamp slides between rays. So the offset is first replaced
        // by the one whose falloff is the mean over the cell's section where
        // the lamp is nearest: for a disk of radius R with its middle at
        // offset h, the mean of 1 / distance is (2 - k^2 (0.5806 - 0.2735
        // k^2 + 0.4197 k^4)) / R inside it (k = h / R) and (1 + q / 8 +
        // 3 q^2 / 64 + 0.1013 q^3) / h outside (q = 1 / k^2), fitted to the
        // elliptic integral within 2.2%.
        const float cell_radius = cell_radius_scale * clamp(along, from, to);
        const float cell_ratio = sqrt(offset_squared) / max(cell_radius, 1.0e-6);
        const float cell_ratio_squared = cell_ratio * cell_ratio;
        const float cell_share = 1.0 / max(cell_ratio_squared, 1.0e-12);
        float offset = cell_ratio <= 1.0
                           ? cell_radius / (2.0 - cell_ratio_squared * (0.5806 + cell_ratio_squared * (-0.2735 + 0.4197 * cell_ratio_squared)))
                           : sqrt(offset_squared) / (1.0 + cell_share * (0.125 + cell_share * (0.046875 + 0.1013 * cell_share)));
        // The game keeps a light from growing without bound near its
        // source: a least distance (limits.x, and for a point light an
        // added [6].x) and for a spot a greatest falloff (limits.w). Both
        // become a least offset from the ray, applied after the mean so no
        // cell is brighter than the game's limit allows.
        const float4 limits = FogLightVector(index, FOG_LIGHT_FALLOFF_LIMITS);
        const bool spot = (type & 1u) != 0u;
        offset = max(offset, max(limits.x, 0.05));
        if (spot && limits.w > 0.0) {
          offset = max(offset, rsqrt(limits.w));
        }
        if (!spot) {
          offset = max(offset + FogLightVector(index, 6u).x, 0.05);
        }
        // In terms of the angle between the view ray and the direction to
        // the light, the falloff's weight is the same at every angle: the
        // integral of 1 / (offset^2 + s^2) over the stretch is the angle it
        // spans over the offset. The angle at a point of the ray is that of
        // the right triangle with the offset opposite it and, beside it, the
        // distance along the ray from the point to where the light is
        // nearest: legs holds that distance at the stretch's two ends. Where
        // the light lies before the stretch's middle the angles are near 180
        // degrees and lose precision, so they are taken from the ray's other
        // direction instead and the lobe turned around.
        const bool behind = along < 0.5 * (from + to);
        const float2 legs = behind ? float2(to - along, from - along) : float2(along - from, along - to);
        const float2 hypotenuses = sqrt(offset * offset + legs * legs);
        const float2 sines = offset / hypotenuses;
        const float2 cosines = legs / hypotenuses;
        const float span = atan2(offset * (legs.x - legs.y), offset * offset + legs.x * legs.y);
        const float inverse_square = span / (offset * (slab_end - slab_start));

        // The scattering's dependence on that angle, averaged over it in
        // closed form. Near the light it changes fastest, and samples spread
        // along the ray would miss most of the angles there. The lobe's
        // integral over the angle x is k sin x / (1 - k cos x) + 2 / sqrt(1 -
        // k^2) atan(sqrt((1 + k) / (1 - k)) tan(x / 2)), over 4 pi, with the
        // two ends' arctangents taken as one of their difference; the blue
        // term's is 3 / (16 pi) (1.5 x + sin(2 x) / 4). Over a tiny span the
        // quotient loses its precision, and the ends' mean cosine stands in.
        const float lobe_sharpness = clamp(behind ? -sharpness : sharpness, -0.9999, 0.9999);
        const float2 half_tangents = sqrt((1.0 + lobe_sharpness) / (1.0 - lobe_sharpness)) * sines / max(1.0 + cosines, 1.0e-6);
        const float mean_cosine = 0.5 * (cosines.x + cosines.y);
        const float light_lobe = span > 1.0e-3
                                     ? 0.0795774683 / span
                                           * (lobe_sharpness * (sines.y / (1.0 - lobe_sharpness * cosines.y) - sines.x / (1.0 - lobe_sharpness * cosines.x))
                                              + 2.0 * rsqrt(1.0 - lobe_sharpness * lobe_sharpness)
                                                    * atan((half_tangents.y - half_tangents.x) / (1.0 + half_tangents.x * half_tangents.y)))
                                     : 0.0795774683 * (1.0 - lobe_sharpness * lobe_sharpness) / ((1.0 - lobe_sharpness * mean_cosine) * (1.0 - lobe_sharpness * mean_cosine));
        const float light_blue = span > 1.0e-3 ? 0.0596831031 * (1.5 + 0.5 * (sines.y * cosines.y - sines.x * cosines.x) / span)
                                               : 0.0596831031 * (1.0 + mean_cosine * mean_cosine);

        // The rest varies gently and is taken at four angles spread evenly
        // over the span, which the falloff weighs alike: the fade toward the
        // light's reach (the game's 1 - k d^4, or 1 - k d^16), a spot's cone,
        // its shadow, and what the fog between the light and the ray takes.
        // The angles' sines and cosines come from the first end's by turning:
        // an eighth of the span to the first, then a quarter at a time.
        const float3 axis = FogLightVector(index, 5u).xyz;
        const float3 cone = FogLightVector(index, 7u).xyz;
        float2 eighth;
        sincos(0.125 * span, eighth.y, eighth.x);
        const float2 quarter = float2(eighth.x * eighth.x - eighth.y * eighth.y, 2.0 * eighth.x * eighth.y);
        float2 turned = float2(cosines.x * eighth.x - sines.x * eighth.y, sines.x * eighth.x + cosines.x * eighth.y);
        float shares = 0.0;
        for (uint point_index = 0u; point_index < 4u; ++point_index) {
          const float place = along + (behind ? offset : -offset) * turned.x / turned.y;
          turned = float2(turned.x * quarter.x - turned.y * quarter.y, turned.y * quarter.x + turned.x * quarter.y);
          const float3 toward = position - direction * place;
          const float distance = max(length(toward), 1.0e-4);
          const float fade_base = distance / reach;
          const float fade_power = fade_base * fade_base * fade_base * fade_base;
          float share = saturate(1.0 - (type >= 3u ? fade_power * fade_power * fade_power * fade_power : fade_power));
          if (spot) {
            share *= pow(saturate((dot(axis, -toward / distance) - cone.x) * cone.y), cone.z);
          }
          if (shadow_tile != 0u && share > 0.0) {
            // The light's kept shadow map, as the light's draw reads the
            // game's: the point through the light's projection, then lit
            // where no stored depth is nearer the light. Larger stored
            // depths are nearer the light.
            const uint record = (shadow_tile - 1u) * FOG_SPOT_SHADOW_VECTORS;
            const float4 point_position = float4(direction * place, 1.0);
            const float4 projected = float4(dot(point_position, fog_spot_shadows.Load(record + 1u)), dot(point_position, fog_spot_shadows.Load(record + 2u)),
                                            dot(point_position, fog_spot_shadows.Load(record + 3u)), dot(point_position, fog_spot_shadows.Load(record + 4u)));
            const float2 map_fraction = projected.xy / projected.w;
            if (projected.w > 0.0 && all(map_fraction > 0.0) && all(map_fraction < 1.0)) {
              const float reference = projected.z / projected.w;
              share *= dot(step(spot_shadows.GatherRed(linear_clamp, (float2((shadow_tile - 1u) % FOG_SPOT_SHADOW_TILES, (shadow_tile - 1u) / FOG_SPOT_SHADOW_TILES) + map_fraction)
                                                                         / float(FOG_SPOT_SHADOW_TILES)),
                                reference + max(reference * 0.002, 2.0e-6)),
                           0.25);
            }
          }
          shares += share * exp(-lamp_extinction * distance);
        }
        // The light's constants give its light on a surface facing it as the
        // light shaders count it, which is the light arriving over pi.
        local_source += (lamp_even + lamp_forward * (1.0 - fog_materials[0].y) * light_lobe + fog_materials[3].xyz * light_blue)
                        * (FogLightVector(index, 4u).xyz * shares * (3.14159265359 * FogLightVector(index, 8u).y * fog_scene.m_exposure.z * inverse_square * 0.25));
      }
      local_light += local_source * fog_local_lights / max(scattering, 1.0e-6) * (reached - passed);
    }
    reached = passed;
    const float transmittance = exp(-optical_distance * fog_materials[0].x);
    float4 result = float4(max(light + local_light + fog_materials[7].xyz * (1.0 - transmittance), 0.0) / fog_scene.m_fogParam[1].y, saturate(transmittance));
    // The debug views, over a dimmed scene: each cell's visibility in place
    // of its light, or the local lights' share of the light alone.
    if (fog_debug_view == 1u) {
      result = float4(saturate(lit * 0.3 / fog_scene.m_fogParam[1].y) * float3(1.0, 1.0, 1.0), 0.25);
    } else if (fog_debug_view == 2u) {
      result = float4(local_light / fog_scene.m_fogParam[1].y, 0.25);
    }
    volume[texel] = result;
    near_depth = far_depth;
  }
}
