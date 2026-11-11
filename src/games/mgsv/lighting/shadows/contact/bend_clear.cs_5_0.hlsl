// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// MGSV dispatch preparation for Bend shadows. See ../BEND_LICENSE.md.

#include "bend_common.hlsli"

[numthreads(BEND_GROUP_SIZE, 1, 1)]
void main(uint3 group: SV_GroupID, uint lane: SV_GroupIndex) {
  uint record = 0u;
#ifdef MGSV_BEND_BATCH
  const uint light_count = asuint(bend_lights.Load(0u).x);
  const uint work_pixel = (group.x + (group.y << BEND_TILE_ROW_BITS)) * BEND_GROUP_SIZE + lane;
  const uint light_index = FindBendBatchLight(work_pixel, light_count, BEND_CLEAR_END_OFFSET);
  if (light_index == light_count) return;
  record = light_index * BEND_BATCH_RECORD_STRIDE;
  const uint first_pixel = light_index == 0u ? 0u : bend_dispatch.Load(record - BEND_BATCH_RECORD_STRIDE + BEND_CLEAR_END_OFFSET);
  const int4 bounds = asint(bend_dispatch.Load4(record + 16u));
  const uint local = work_pixel - first_pixel;
  const uint width = uint(bounds.z - bounds.x);
  const int2 pixel = bounds.xy + int2(local % width, local / width);
#else
  const int4 bounds = asint(bend_dispatch.Load4(16u));
  const int2 pixel = bounds.xy + int2(group.x * BEND_GROUP_SIZE + lane, group.y);
#endif
  if (bend_dispatch.Load(record + 40u) != 1u) return;
  if (any(pixel >= bounds.zw)) return;
  const int2 light_pixel = int2(floor(asfloat(bend_dispatch.Load2(record))));
  // A ray is undefined at its projected source pixel. Keep it neutral.
  bend_output[BendMaskPixel(pixel, bounds, record)] = float4(1.0, all(pixel == light_pixel) ? 0.25 : 0.0, 0.0, 0.0);
}
