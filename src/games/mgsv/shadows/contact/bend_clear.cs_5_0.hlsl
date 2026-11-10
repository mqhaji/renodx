// Copyright 2023 Sony Interactive Entertainment.
// SPDX-License-Identifier: Apache-2.0
// MGSV dispatch preparation for Bend shadows. See ../BEND_LICENSE.md.

#include "bend_common.hlsli"

[numthreads(BEND_GROUP_SIZE, 1, 1)]
void main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex) {
  if (bend_dispatch.Load(40) != 1) return;
  const int4 bounds = asint(bend_dispatch.Load4(16));
  const int2 pixel = bounds.xy + int2(group.x * BEND_GROUP_SIZE + lane, group.y);
  if (any(pixel >= bounds.zw)) return;
  const int2 light_pixel = int2(floor(asfloat(bend_dispatch.Load2(0))));
  // A ray is undefined at its projected source pixel. Keep it neutral.
  bend_output[pixel] = float4(1.0, all(pixel == light_pixel) ? 0.25 : 0.0, 0.0, 0.0);
}
