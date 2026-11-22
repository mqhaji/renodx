// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// MGSV mask clear for Bend shadows. See ../BEND_LICENSE.md.

#include "bend_common.hlsli"

[numthreads(BEND_GROUP_SIZE, 1, 1)]
void main(uint3 group: SV_GroupID, uint lane: SV_GroupIndex) {
  // One dispatch clears every admitted light's packed receiver rectangle to
  // visibility 1 with G = 0 (no data), so pixels the trace skips stay unread.
  // Setup packs the rectangles end to end from the atlas's first texel, so
  // they are the atlas's first texels up to the last light's running total,
  // whichever light each belongs to. The atlas's width is a power of two.
  const float4 batch = bend_lights.Load(0u);
  const uint texel = (group.x + (group.y << BEND_TILE_ROW_BITS)) * BEND_GROUP_SIZE + lane;
  if (asuint(batch.x) == 0u || texel >= bend_dispatch.Load((asuint(batch.x) - 1u) * BEND_RECORD_STRIDE + BEND_CLEAR_END_OFFSET)) return;
  bend_output[uint2(texel & (asuint(batch.y) - 1u), texel >> firstbithigh(asuint(batch.y)))] = float4(1.0, 0.0, 0.0, 0.0);
}
