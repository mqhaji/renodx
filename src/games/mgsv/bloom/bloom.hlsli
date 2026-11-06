/*
 * Copyright (C) 2026 Musa Haji
 * SPDX-License-Identifier: MIT
 */

#ifndef RENODX_GAMES_MGSV_BLOOM_HLSLI_
#define RENODX_GAMES_MGSV_BLOOM_HLSLI_

static const uint2 BLOOM_WORK_SIZE = uint2(246u, 138u);

// Integral of a cubic B-spline. Its spread stays consistent between grid cells.
float BloomCubicIntegral(float position) {
  const float distance = abs(position);
  const float inner = saturate(distance);
  const float inner_cubed = inner * inner * inner;
  const float inner_area = 0.5f + inner * (2.f / 3.f) + inner_cubed * (inner * 0.125f - 1.f / 3.f);
  const float tail = saturate(2.f - distance);
  const float tail_squared = tail * tail;
  const float outer_area = 1.f - tail_squared * tail_squared / 24.f;
  const float positive_area = (distance < 1.f ? inner_area : outer_area);
  return (position < 0.f ? 1.f - positive_area : positive_area);
}

float3 SampleEnhancedBloom(Texture2D<float4> source, SamplerState linear_sampler, float2 uv) {
  const float2 work_size = float2(BLOOM_WORK_SIZE);
  // The native final copy maps working texel i to output UV (i + 1) / work_size.
  const float2 position = uv * work_size - 1.f;
  const float2 base = floor(position);
  const float2 fraction = position - base;
  const float2 inverse_fraction = 1.f - fraction;
  const float2 squared = fraction * fraction;
  const float2 cubed = squared * fraction;
  const float2 w0 = inverse_fraction * inverse_fraction * inverse_fraction / 6.f;
  const float2 w1 = (3.f * cubed - 6.f * squared + 4.f) / 6.f;
  const float2 w2 = (-3.f * cubed + 3.f * squared + 3.f * fraction + 1.f) / 6.f;
  const float2 w3 = cubed / 6.f;
  const float2 lower_weight = w0 + w1;
  const float2 upper_weight = w2 + w3;
  const float2 lower_uv = (base + w1 / lower_weight) / work_size;
  const float2 upper_uv = (base + 2.f + w3 / upper_weight) / work_size;
  // Positive cubic B-spline weights smooth the coarse grid without ringing.
  const float3 a = source.SampleLevel(linear_sampler, lower_uv, 0.f).rgb;
  const float3 b = source.SampleLevel(linear_sampler, float2(upper_uv.x, lower_uv.y), 0.f).rgb;
  const float3 c = source.SampleLevel(linear_sampler, float2(lower_uv.x, upper_uv.y), 0.f).rgb;
  const float3 d = source.SampleLevel(linear_sampler, upper_uv, 0.f).rgb;
  return lerp(lerp(a, b, upper_weight.x), lerp(c, d, upper_weight.x), upper_weight.y);
}

#endif
