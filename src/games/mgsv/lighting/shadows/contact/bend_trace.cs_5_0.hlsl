// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// Adapted from Bend's shared-depth screen-space shadows. See ../BEND_LICENSE.md.

#include "bend_common.hlsli"

#include "../pcss.hlsli"

Texture2D<float4> bend_depth : register(t0);
SamplerState bend_border : register(s0);

// Casting depths along the group's ray, toward the light from lane 0's
// receiver (BendDepthSlot).
groupshared float shared_depth[BEND_DEPTH_SLOTS];
// The farthest pixel toward the light any receiver of the group tests.
groupshared uint group_reach;
// A local light's center sits inside its lamp, so the trace stops this many
// game units short of it: closer casters are taken to be the light's own
// fixture, which the engine's shadow maps leave out too.
static const float BEND_FIXTURE_DISTANCE = 0.25;
// The most a caster is taken to be thick behind the surface the camera sees,
// in game units. Depth shows only that front surface, so how far back a caster
// keeps blocking a light ray that passes behind it is a guess: it is taken to
// be as thick as it is wide along the ray (BendCasterSeparation), up to this
// much. The reference method's thickness is a share of the distance from the
// camera (0.5%): about 0.01 units in a close-up, where a chest beside an arm
// stopped casting on the pixels next to it and left a lit gap. One thickness
// of 0.1 for every caster closed that gap, but made each lock of hair cast on
// the face as a slab that deep (both seen in a Mother Base cutscene).
static const float BEND_MAX_CASTER_THICKNESS = 0.3;

// How far a casting depth is from blocking a receiver: 1 or more lets the
// light through, 0 blocks it. difference is the receiver's reference less the
// scaled casting depth: 1 where the caster lies on the light ray, falling to 0
// one unit of the reference thickness in front of it. From there the caster
// keeps blocking until it is solid_span units in front, then lets go over one
// more. A span of 1 is the reference method's symmetric test.
float BendSeparation(float difference, float solid_span) {
  return difference >= 0.0 ? difference : saturate(1.0 - solid_span - difference);
}

float4 BendSeparation(float4 difference, float solid_span) {
  return difference >= 0.0 ? difference : saturate(1.0 - solid_span - difference);
}

// The slot of shared_depth that holds a pixel of the group's ray, counted
// from lane 0's receiver toward the light, or the last slot before it. The
// reference method keeps every pixel, and its 320 depths reach 256 pixels: in
// a close-up a hand's width. Here the first 128 slots are every pixel, so
// each receiver's contacts (64 pixels or more) keep their detail; the next 64
// are every second pixel and the rest every fourth, which takes the same
// number of depths to 704 pixels. A shadow that far from its caster needs no
// finer steps. A receiver tests the slots after its own in turn.
uint BendDepthSlot(uint ray_pixel) {
  return ray_pixel < 128u ? ray_pixel : (ray_pixel < 256u ? 64u + (ray_pixel >> 1) : 128u + (ray_pixel >> 2));
}

// The ray's pixel a slot holds.
uint BendSlotPixel(uint slot) {
  return slot < 128u ? slot : (slot < 192u ? 2u * slot - 128u : 4u * slot - 512u);
}

// Spreads separations of four slots in a row, from slot on, over the four
// interleaved minima. In the reference method a caster darkens a receiver
// fully once it covers four pixels in a row, one in each minimum. A slot that
// stands for two pixels feeds two minima and one that stands for four feeds
// all four, as the trace does at reduced render scale, so a caster darkens as
// much in the sparser slots as in the dense ones.
float4 BendSlotMinima(float4 separation, uint slot) {
  return slot >= 192u ? min(min(separation.x, separation.y), min(separation.z, separation.w))
                      : (slot >= 128u ? min(separation, separation.zwxy) : separation);
}

// What a receiver's samples are tested against.
struct BendReceiver {
  // The receiver's place on the light ray and the scale of a stored casting
  // depth, both in units of the reference thickness (BendSeparation).
  float reference;
  float depth_scale;
  // BEND_MAX_CASTER_THICKNESS in those units, never less than 1.
  float solid_span;
  // Render pixels along the ray as long as one of those units.
  float unit_pixels;
  // The ray's pixels of the receiver and of the end of its reach, counted as
  // BendDepthSlot counts them.
  int pixel;
  int last_pixel;
};

// Whether the ray's pixel shows a surface at least one reference thickness in
// front of the receiver's ray. Pixels outside the receiver's reach do not
// count.
bool BendCovers(BendReceiver receiver, int ray_pixel) {
  if (ray_pixel <= receiver.pixel || ray_pixel > receiver.last_pixel) return false;
  const float stored = shared_depth[BendDepthSlot((uint)ray_pixel)];
  // 1e20 marks a pixel without a usable depth.
  return stored < 1e19 && receiver.reference - stored * receiver.depth_scale < 0.0;
}

// BendSeparation for the casting depth of a slot, with the caster taken to be
// as thick as it is wide: a ray that passes some distance behind the caster's
// front is blocked when the caster still covers the ray that distance to
// either side along it, up to BEND_MAX_CASTER_THICKNESS. A narrower caster
// keeps the reference thickness.
float BendCasterSeparation(BendReceiver receiver, float difference, uint slot) {
  const float separation = BendSeparation(difference, receiver.solid_span);
  if (difference >= 0.0 || separation >= 1.0) return separation;
  const int width = (int)ceil((1.0 - difference) * receiver.unit_pixels);
  const int ray_pixel = (int)BendSlotPixel(slot);
  return BendCovers(receiver, ray_pixel + width) || BendCovers(receiver, ray_pixel - width)
             ? separation
             : BendSeparation(difference, 1.0);
}

// The same for four slots in a row. Only a sample that would lower one of the
// current minima is worth the width test: current holds, for each slot, the
// largest of the minima it feeds.
float4 BendCasterSeparation(BendReceiver receiver, float4 difference, uint slot, float4 current) {
  float4 separation = BendSeparation(difference, receiver.solid_span);
  [branch]
  if (any(difference < 0.0 && separation < current)) {
    separation = float4(BendCasterSeparation(receiver, difference.x, slot), BendCasterSeparation(receiver, difference.y, slot + 1u),
                        BendCasterSeparation(receiver, difference.z, slot + 2u), BendCasterSeparation(receiver, difference.w, slot + 3u));
  }
  return separation;
}

// Each admitted light owns one packed rectangle of the atlas: a run of texels
// from its first, wrapped over the atlas's rows. The atlas's width is a power
// of two (CreateStorage in batch.hpp).
int2 BendMaskPixel(int2 pixel, int4 bounds, uint record) {
  const uint2 storage = bend_dispatch.Load2(record + BEND_ATLAS_OFFSET);
  const uint2 local = uint2(pixel - bounds.xy);
  const uint texel_index = storage.x + local.y * uint(bounds.z - bounds.x) + local.x;
  return int2(texel_index & (storage.y - 1u), texel_index >> firstbithigh(storage.y));
}

// Stores one read of the group's casting depths: slot read * 64 + lane,
// whose pixel is at sample_pixel, distance pixels from the light. Returns
// that pixel's depth, which for read 0 is the lane's receiver.
float StoreCastingDepth(uint read, uint lane, float2 sample_pixel, float distance, bool major_x, float2 inverse_size,
                        float render_scale, float light_depth, out bool depth_edge) {
  const float2 texel = floor(sample_pixel) + 0.5;
  const float minor = major_x ? sample_pixel.y : sample_pixel.x;
  const float interpolation = frac(minor) - 0.5;
  const float bias = interpolation > 0.0 ? 1.0 : -1.0;
  const float2 offset = major_x ? float2(0.0, bias) : float2(bias, 0.0);
  float first = bend_depth.SampleLevel(bend_border, texel * inverse_size, 0).x;
  float second = bend_depth.SampleLevel(bend_border, (texel + offset) * inverse_size, 0).x;
  first = isfinite(first) && first > 0.0 ? first : 0.0;
  second = isfinite(second) && second > 0.0 ? second : 0.0;
  // Bend's default 2% threshold avoids joining separate surfaces.
  depth_edge = abs(first - second) > abs(first) * 0.02;
  // Reverse-Z: moving the casting depth farther away means subtracting. The
  // neighbor is one render pixel away; bias by one output pixel's difference.
  const float casting_depth = depth_edge ? first : first - abs(first - second) * render_scale;
  float stored = (casting_depth - light_depth) / distance;
  if (abs(distance) < 1e-7 || (read != 0 && distance <= 0.0) || !isfinite(stored)) {
    stored = 1e20;
  }
  shared_depth[read * BEND_GROUP_SIZE + lane] = stored;
  return first;
}

[numthreads(BEND_GROUP_SIZE, 1, 1)]
void main(uint3 group: SV_GroupID, uint lane: SV_GroupIndex) {
  // One dispatch traces every admitted light; each group finds its light from
  // its tile index. A group's 64 lanes are receivers on one shared screen ray
  // from the light. Each lane tests its receiver against the depths of the
  // next pixels toward the light, up to the reach setting.
  uint tile = group.y + (group.z << BEND_TILE_ROW_BITS);
  const uint light_count = asuint(bend_lights.Load(0u).x);
  // Each light stores the running total of trace tiles. Skipped lights repeat
  // the preceding total, so binary search jumps over them and finds the light
  // that owns this tile.
  uint light_index = 0u;
  uint lights_end = light_count;
  [loop]
  while (light_index < lights_end) {
    const uint middle = light_index + ((lights_end - light_index) >> 1u);
    if (bend_dispatch.Load(middle * BEND_RECORD_STRIDE + BEND_TRACE_END_OFFSET) <= tile) {
      light_index = middle + 1u;
    } else {
      lights_end = middle;
    }
  }
  if (light_index == light_count) return;
  const uint record = light_index * BEND_RECORD_STRIDE;
  const uint first_light_tile = light_index == 0u ? 0u : bend_dispatch.Load(record - BEND_RECORD_STRIDE + BEND_TRACE_END_OFFSET);
  tile -= first_light_tile;
  const float light_type = float(ReadBendLightKind(light_index).x);
  const uint4 metadata = bend_dispatch.Load4(record + 32u);
  if (metadata.z != 1) return;
  const bool directional = light_type < 0.5;
  const bool spot = !IsBendPointLight(light_type);
  const bool receiver_culling = !directional
                                && all(isfinite(bend_projection_param)) && bend_projection_param.z > 0.0
                                && all(isfinite(bend_camera_center.xy));
  float local_range = -1.0;
  if (receiver_culling) {
    const bool distance_light = IsBendDistanceLight(light_type);
    const float attenuation = distance_light ? ReadBendLightParam(light_index, 6u).z : ReadBendLightParam(light_index, 6u).w;
    if (isfinite(attenuation) && attenuation > 0.0
        && (!distance_light || attenuation >= 1.17549435e-38)) {
      local_range = (distance_light ? rsqrt(sqrt(sqrt(sqrt(attenuation)))) : rsqrt(sqrt(attenuation)))
                    + (spot ? 0.0 : abs(ReadBendLightParam(light_index, 6u).x));
      if (!isfinite(local_range)) {
        local_range = -1.0;
      }
    }
  }
  const int4 bounds = asint(bend_dispatch.Load4(record + 16u));
  const float4 light = asfloat(bend_dispatch.Load4(record));
  // Recover the original sweep from the cumulative tile counts written by
  // setup. All sweeps share one indirect call, including very large viewports.
  uint first_tile = 0;
  uint sector = 0;
  [loop]
  for (; sector < metadata.w; ++sector) {
    const uint last_tile = bend_dispatch.Load(record + BEND_DISPATCH_HEADER + sector * BEND_DISPATCH_STRIDE + 20);
    if (tile < last_tile) break;
    first_tile = last_tile;
  }
  if (sector == metadata.w) return;
  tile -= first_tile;
  const uint address = record + BEND_DISPATCH_HEADER + sector * BEND_DISPATCH_STRIDE;
  const uint columns = bend_dispatch.Load(address + 4);
  group.yz = uint2(tile % columns, tile / columns);
  const int2 wave_offset = asint(bend_dispatch.Load2(address + 12));
  // Construct a shared ray through this square ring around the light. Neighbor
  // lanes advance one major-axis pixel; reversing lanes handles camera-side sign.
  int2 relative = int2(group.yz) * BEND_GROUP_SIZE + wave_offset;
  const int2 signs = sign(relative);
  const bool horizontal = abs(relative.x + signs.y) < abs(relative.y - signs.x);
  const int2 axis = horizontal ? int2(signs.y, 0) : int2(0, -signs.x);
  relative += axis * (int)group.x;
  const int2 light_grid = int2(floor(light.xy));
  const precise float2 light_fraction = frac(light.xy) - 0.5;
  const bool major_x = abs(relative.x) > abs(relative.y);
  const precise float major = major_x ? relative.x : relative.y;
  const precise float fraction = (major > 0.0 ? -1.0 : 1.0) * (major_x ? light_fraction.x : light_fraction.y);
  const precise float major_start = abs(major);
  const precise float start_distance = major_start + fraction;
  if (abs(start_distance) < 1e-7) return;
  // Cancel large light coordinates as integers before converting to pixels.
  // The equivalent lerp from the projected light loses pixels near the horizon.
  const precise float2 start = float2(relative + light_grid) + 0.5;
  const precise float2 pixel_step = (float2(relative) - light_fraction) / start_distance;
  if (local_range > 0.0) {
    // All receivers lie in this small bundle of camera rays. If the entire
    // light sphere is outside any side, this group cannot illuminate a pixel.
    // The decision is uniform and precedes all shared-memory/depth work.
    const float2 last = start - pixel_step * float(BEND_GROUP_SIZE - 1);
    if (BendSphereOutsideRays(ReadBendLightParam(light_index, 3u).xyz, local_range,
                              (min(start, last) - 2.0),
                              (max(start, last) + 2.0), float2(metadata.xy))) return;
  }
  const uint thread_step = lane ^ (light.w > 0.0 ? 0u : (BEND_GROUP_SIZE - 1u));
  const precise float2 sample_pixel = start - pixel_step * float(thread_step);
  const int2 output_pixel = int2(floor(sample_pixel));
  const precise float pixel_distance = major_start - thread_step + fraction;
  const float direction = -light.w;
  const float thickness = 0.005;
  // The trace's other distances are tuned in output pixels: an upscaled frame
  // crosses more of the screen per sample, so it uses proportionally fewer hard
  // and interleaved samples, a shorter end fade, and a smaller neighbor-depth
  // bias. Otherwise thin casters lose most of their shadow as the render
  // resolution drops.
  const float render_scale = isfinite(CUSTOM_CONTACT_SHADOW_RENDER_SCALE) && CUSTOM_CONTACT_SHADOW_RENDER_SCALE > 0.0
                                 ? min(CUSTOM_CONTACT_SHADOW_RENDER_SCALE, 1.0)
                                 : 1.0;
  // Whole divisors keep the four interleaved minima aligned. The thresholds are
  // the geometric midpoints between scales 1, 1/2, and 1/4.
  const uint sample_divisor = render_scale > 0.7071 ? 1u : (render_scale > 0.3536 ? 2u : 4u);
  const float2 inverse_size = rcp(float2(metadata.xy));
  if (lane == 0u) {
    // A group starts with no receiver to trace for. One of sky alone, or that
    // a local light does not reach, then reads no more depths.
    group_reach = 0u;
  }
  GroupMemoryBarrierWithGroupSync();

  // Each thread supplies a few depths for many neighboring receivers. Stream
  // them into group memory instead of keeping a large per-thread depth array.
  // The first read is this lane's receiver, whose depth sets its sample count.
  bool edge = false;
  const float receiver_depth = StoreCastingDepth(0u, lane, sample_pixel, pixel_distance, major_x, inverse_size, render_scale,
                                                 light.z, edge);
  // The receiver's view-space position, when the camera projection is usable.
  const float denominator = receiver_depth - bend_projection_param.w;
  const float view_depth = bend_projection_param.z / denominator;
  const bool projected = denominator > 0.0 && isfinite(view_depth)
                         && all(isfinite(bend_projection_param.xy)) && all(isfinite(bend_camera_center.xy));
  const float2 ray = ((float2(output_pixel) + 0.5) * inverse_size * float2(2.0, -2.0) + float2(-1.0, 1.0)
                      - bend_camera_center.xy)
                     * bend_projection_param.xy;
  const float3 receiver_position = float3(ray * view_depth, view_depth);
  // Reach is a world distance toward the light. The point that far along the
  // light's direction lies, by perspective division, this many render pixels
  // along the ray's main screen axis: the direction's part across the camera
  // ray through the receiver, over the point's view depth, in pixels per unit
  // of that axis (view_depth / footprint). So a light near the view axis
  // takes fewer pixels for the same length. Without a usable projection, 60
  // render pixels.
  uint reach = 60u;
  const float footprint = 2.0 * abs(major_x ? bend_projection_param.x * inverse_size.x : bend_projection_param.y * inverse_size.y) * view_depth;
  // The sun's direction toward it, or a local light's position.
  const float3 source = ReadBendLightParam(light_index, directional ? 0u : 3u).xyz;
  const float3 toward_source = directional ? source : source - receiver_position;
  const float source_distance = length(toward_source);
  if (projected && isfinite(footprint) && footprint > 0.0 && isfinite(source_distance) && source_distance > 0.0
      && isfinite(CUSTOM_CONTACT_SHADOW_REACH) && CUSTOM_CONTACT_SHADOW_REACH > 0.0) {
    const float3 toward = toward_source / source_distance;
    const float end_depth = view_depth + CUSTOM_CONTACT_SHADOW_REACH * toward.z;
    const float across = abs(major_x ? toward.x - ray.x * toward.z : toward.y - ray.y * toward.z);
    // An end behind the camera is past the screen's edge.
    const float pixels = end_depth > 0.0 ? across * CUSTOM_CONTACT_SHADOW_REACH * view_depth / (footprint * end_depth) : float(BEND_MAX_REACH);
    reach = (uint)clamp(isfinite(pixels) ? pixels : 16.0, 16.0, float(BEND_MAX_REACH));
  }
  if (projected && !directional) {
    // Stop BEND_FIXTURE_DISTANCE short of the light. On screen the ray covers
    // the fraction s of its pixels to the light, from the 3D fraction t by
    // perspective division.
    if (source.z > 0.0 && isfinite(source_distance) && source_distance > 0.0) {
      const float t = 1.0 - min(BEND_FIXTURE_DISTANCE / source_distance, 1.0);
      const float s = t * source.z / max((1.0 - t) * view_depth + t * source.z, 1e-20);
      reach = (uint)min(float(reach), max(floor(s * abs(pixel_distance)), 0.0));
    }
  }
  // Whether the native light reaches this receiver at all: within its range
  // and, for a spot, its cone, keeping uncertain pixels at the boundary. One
  // it does not reach needs no trace, and its pixel stays as the clear left
  // it, which the light shader takes as unshadowed.
  bool lit = true;
  if (receiver_culling && projected) {
    const float3 delta = receiver_position - ReadBendLightParam(light_index, 3u).xyz;
    const float distance = length(delta);
    const float margin = length(2.0 * bend_projection_param.xy * view_depth * inverse_size)
                         + view_depth * 0.00001 + distance * 0.00002;
    lit = local_range <= 0.0 || distance <= local_range + margin;
    const float3 axis = ReadBendLightParam(light_index, 5u).xyz;
    const float3 cone = ReadBendLightParam(light_index, 7u).xyz;
    if (spot && all(isfinite(axis)) && all(isfinite(cone))
        && cone.x > 0.0 && cone.y > 0.0 && cone.z > 0.0) {
      lit = lit && dot(axis, delta) + length(axis) * margin >= cone.x * max(0.0, distance - margin);
    }
  }
  if (all(output_pixel >= bounds.xy) && all(output_pixel < bounds.zw) && receiver_depth > 0.0 && lit) {
    InterlockedMax(group_reach, reach);
  }
  GroupMemoryBarrierWithGroupSync();
  // The group's last receiver to trace for reads through the ray's pixel
  // 63 + its reach. With none, no more depths are read.
  const uint reads = group_reach == 0u ? 1u : BendDepthSlot(group_reach + BEND_GROUP_SIZE - 1u) / BEND_GROUP_SIZE + 1u;
  [loop]
  for (uint read = 1u; read < reads; ++read) {
    // How far toward the light the pixel of this lane's slot is: read 1
    // holds the next 64 pixels, read 2 every second pixel from 128, and the
    // rest every fourth from 256.
    const precise float advance = direction * float(read == 1u ? 64u : (read == 2u ? 128u + lane : 256u * (read - 2u) + 3u * lane));
    const precise float2 read_pixel = sample_pixel + pixel_step * advance;
    bool unused_edge;
    StoreCastingDepth(read, lane, read_pixel, pixel_distance + advance, major_x, inverse_size, render_scale, light.z, unused_edge);
  }
  // Out-of-bounds threads still supply depths needed by in-bounds receivers.
  GroupMemoryBarrierWithGroupSync();
  if (any(output_pixel < bounds.xy) || any(output_pixel >= bounds.zw)) return;
  // Projected rays can land on the same pixel. Select one writer after every
  // thread has supplied its depths, so group execution order cannot change it.
  if (major_start == 0.0) return;
  const int2 center_grid = output_pixel - light_grid;
  const precise float2 center_direction = float2(center_grid) - light_fraction;
  const float2 extension = float2(int(major_start) - abs(center_grid)) / max(abs(center_direction), 1e-20);
  // Intersect the pixel-center ray with this group's square ring, then round
  // to its nearest shared ray. Subtract integers first to retain precision for
  // distant projected lights, including a sun on the camera's horizon.
  const precise float2 endpoint_delta = float2(center_grid - relative)
                                        + center_direction * min(extension.x, extension.y);
  if (any(floor(endpoint_delta + 0.5) != 0.0)) return;
  // Receivers too close to the light for a trace past its fixture stay lit.
  if (!(receiver_depth > 0.0) || abs(pixel_distance) < 1e-7 || reach < 16u) {
    bend_output[BendMaskPixel(output_pixel, bounds, record)] = float4(1.0, 0.25, 0.0, 0.0);
    return;
  }

  // Having supplied its depths for the other receivers, one the light does
  // not reach is done.
  if (!lit) return;

  // Units of the reference thickness per difference in nonlinear depth
  // relative to the depth, which is the same share of the view distance:
  // 1 / thickness, or fewer within 200 pixels of the light on screen.
  const float depth_units = min(pixel_distance + direction, rcp(thickness));
  const float depth_scale = depth_units * pixel_distance / receiver_depth;
  const float reference = (receiver_depth - light.z) / pixel_distance * depth_scale + 1.0;
  // One unit is then view_depth / depth_units game units for a caster as
  // far from the light on screen as the receiver, and fewer for one nearer
  // it. Without a usable projection the span of 1 leaves the reference
  // method's test.
  const BendReceiver receiver = {reference, depth_scale,
                                 projected ? max(BEND_MAX_CASTER_THICKNESS * depth_units / view_depth, 1.0) : 1.0,
                                 view_depth / (max(depth_units, 1.0) * max(footprint, 1e-20)), (int)lane, (int)(lane + reach)};
  // The slots between this receiver and its reach.
  const uint first_sample = lane + 1u;
  const uint samples = BendDepthSlot(lane + reach) - lane;
  // Four hard samples and eight fade samples at native scale. The fade keeps
  // at least one whole group of four.
  const uint hard_samples = 4u / sample_divisor;
  const uint fade_samples = sample_divisor == 1u ? 8u : 4u;
  const float contrast = 4.0;
  const float solid_threshold = 1.0 - rcp(contrast);
  float hard = 1.0;
  // The first solid caster's distance from the receiver, in pixels.
  uint blocker_pixel = 0u;
  // Keep the nearest contacts sharp. Farther samples use interleaved visibility
  // minima, followed by the reference method's end fade.
  [unroll(4)]
  for (uint hard_index = 0; hard_index < 4u; ++hard_index) {
    if (hard_index < hard_samples) {
      const float separation = BendCasterSeparation(receiver, reference - shared_depth[first_sample + hard_index] * depth_scale, first_sample + hard_index);
      if (separation < hard) {
        hard = separation;
      }
      if (blocker_pixel == 0u && separation <= solid_threshold) {
        blocker_pixel = BendSlotPixel(first_sample + hard_index) - lane;
      }
    }
  }
  const float hard_visibility = saturate(hard * contrast + 1.0 - contrast);
  float soft_visibility = 1.0;
  // Shared depth reads are complete. Fully blocked contacts need no farther reduction.
  [branch]
  if (hard_visibility > 0.0) {
    float4 soft = 1.0;
    // Four independent minima permit four contiguous loads per iteration, with
    // no dynamic vector indexing or four-way mask work for every single sample.
    uint soft_index = hard_samples;
    [loop]
    for (; soft_index + 3u < samples - fade_samples; soft_index += 4u) {
      const uint address = first_sample + soft_index;
      const float4 depths = float4(shared_depth[address], shared_depth[address + 1u],
                                   shared_depth[address + 2u], shared_depth[address + 3u]);
      const float4 separation = BendCasterSeparation(receiver, reference - depths * depth_scale, address, -BendSlotMinima(-soft, address));
      soft = min(soft, BendSlotMinima(separation, address));
      // The first solid caster controls contact hardening. Once found, there
      // is no need to track four changing winner indices through the sweep.
      if (blocker_pixel == 0u && any(separation <= solid_threshold)) {
        const uint component = separation.x <= solid_threshold ? 0u : (separation.y <= solid_threshold ? 1u : (separation.z <= solid_threshold ? 2u : 3u));
        blocker_pixel = BendSlotPixel(first_sample + soft_index + component) - lane;
      }
    }
    // The reach setting need not be a multiple of four samples.
    [loop]
    for (; soft_index < samples - fade_samples; ++soft_index) {
      const float separation = BendCasterSeparation(receiver, reference - shared_depth[first_sample + soft_index] * depth_scale, first_sample + soft_index);
      if (blocker_pixel == 0u && separation <= solid_threshold) {
        blocker_pixel = BendSlotPixel(first_sample + soft_index) - lane;
      }
      soft = min(soft, BendSlotMinima(uint4(0u, 1u, 2u, 3u) == ((soft_index - hard_samples) & 3u) ? separation : 1.0,
                                      first_sample + soft_index));
    }
    // The fade samples form groups of four. Align their minima once with the
    // existing groups instead of selecting a component for every sample.
    float4 fade = 1.0;
    [unroll(2)]
    for (uint fade_block = 0; fade_block < 2u; ++fade_block) {
      if (fade_block * 4u < fade_samples) {
        const uint address = first_sample + samples - fade_samples + fade_block * 4u;
        const float4 depths = float4(shared_depth[address], shared_depth[address + 1u],
                                     shared_depth[address + 2u], shared_depth[address + 3u]);
        const float4 fade_offset = (float4(1, 2, 3, 4) + fade_block * 4u) / float(fade_samples + 1u) * 0.75;
        fade = min(fade, BendSlotMinima(BendCasterSeparation(receiver, reference - depths * depth_scale, address, 1.0) + fade_offset, address));
      }
    }
    // Match the fade lanes to the same four minima as the preceding samples,
    // which count from the first soft sample.
    switch ((samples - fade_samples - hard_samples) & 3u) {
      case 1u: fade = fade.wxyz; break;
      case 2u: fade = fade.zwxy; break;
      case 3u: fade = fade.yzwx; break;
    }
    soft = min(soft, fade);
    // A lower render scale interleaves fewer minima: lanes two apart share
    // one, and at the lowest scales all four do.
    if (sample_divisor >= 2u) {
      soft = min(soft, soft.zwxy);
    }
    if (sample_divisor == 4u) {
      soft = min(soft, soft.yxwz);
    }
    soft_visibility = dot(saturate(soft * contrast + 1.0 - contrast), 0.25);
  }
  float visibility = min(hard_visibility, soft_visibility);
  float radius = 0.0;
  if (visibility < 1.0 && blocker_pixel > 0u && CUSTOM_SHADOW_SOFTNESS > 0.0) {
    // Size the soft edge from the first solid caster and the same light size
    // as PCSS. Receiver depth cancels out of the radius in screen pixels; only
    // the world-space cap (PCSS_MAX_RADIUS) needs inverse depth.
    const float2 viewport = bend_viewport.xy;
    const float2 projected_span = (source.xy - ray * source.z) * (0.5 * viewport / bend_projection_param.xy);
    const float remaining_span = max(abs(projected_span.x), abs(projected_span.y)) - float(blocker_pixel) * source.z;
    const float pixel_scale = 0.5 * max(viewport.x / abs(bend_projection_param.x), viewport.y / abs(bend_projection_param.y));
    const float inverse_depth = (receiver_depth - bend_projection_param.w) / bend_projection_param.z;
    const float source_radius = directional ? tan(CUSTOM_SHADOW_SOFTNESS * SHADOW_DIRECTIONAL_ANGULAR_RADIUS) * source_distance
                                            : CUSTOM_SHADOW_SOFTNESS * SHADOW_LOCAL_SOURCE_RADIUS;
    if (remaining_span > 1e-6 && inverse_depth > 0.0 && all(isfinite(float3(remaining_span, inverse_depth, pixel_scale)))) {
      const float cone_radius = source_radius * float(blocker_pixel) * pixel_scale / remaining_span;
      radius = min(SHADOW_CONTACT_WIDE_RADIUS, min(cone_radius, PCSS_MAX_RADIUS * pixel_scale * inverse_depth));
    }
  }
  // r/(1+r) keeps a useful fraction of a pixel in RG8. G still marks support.
  float diagnostic = radius / (1.0 + radius);
  const bool debug_selected = uint(CUSTOM_SHADOW_DEBUG_LIGHT) == (directional ? 0u : (IsBendPointLight(light_type) ? 2u : 1u));
  if (debug_selected && CUSTOM_CONTACT_SHADOW_DEBUG_VIEW == 2.0) {
    diagnostic = edge ? 1.0 : 0.0;
  }
  if (debug_selected && CUSTOM_CONTACT_SHADOW_DEBUG_VIEW == 3.0) {
    diagnostic = frac(float(group.x) / BEND_GROUP_SIZE);
  }
  if (debug_selected && CUSTOM_CONTACT_SHADOW_DEBUG_VIEW == 5.0) {
    diagnostic = 1.0;
  }
  // G below 0.25 means the clear ran but no sweep wrote this pixel. The reader
  // keeps native lighting there; supported pixels carry radius or debug data.
  bend_output[BendMaskPixel(output_pixel, bounds, record)] = float4(isfinite(visibility) ? visibility : 1.0, 0.25 + 0.75 * diagnostic,
                                                                    0.0, 0.0);
}
