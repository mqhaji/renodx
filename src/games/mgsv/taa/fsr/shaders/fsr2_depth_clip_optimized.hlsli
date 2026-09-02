#ifndef MGSV_FSR2_DEPTH_CLIP_OPTIMIZED_HLSLI
#define MGSV_FSR2_DEPTH_CLIP_OPTIMIZED_HLSLI

#ifndef MGSV_FSR2_ZERO_INPUT_MASKS
#define MGSV_FSR2_ZERO_INPUT_MASKS 0
#endif

FfxFloat32 ComputeDepthClipPrecomputed(FfxFloat32x2 uv_sample, FfxFloat32 current_depth_sample) {
  const FfxFloat32 current_depth_view = GetViewSpaceDepth(current_depth_sample);
  const BilinearSamplingData bilinear = GetBilinearSamplingData(uv_sample, RenderSize());

  FfxFloat32 depth = 0.f;
  FfxFloat32 weight_sum = 0.f;
  for (FfxInt32 sample_index = 0; sample_index < 4; ++sample_index) {
    const FfxInt32x2 sample_pos = bilinear.iBasePos + bilinear.iOffsets[sample_index];
    if (!IsOnScreen(sample_pos, RenderSize())) continue;

    const FfxFloat32 weight = bilinear.fWeights[sample_index];
    if (weight <= fReconstructedDepthBilinearWeightThreshold) continue;

    const FfxFloat32 previous_depth_view = GetViewSpaceDepth(LoadReconstructedPrevDepth(sample_pos));
    const FfxFloat32 depth_difference = current_depth_view - previous_depth_view;
    if (depth_difference <= 0.f) continue;

    const FfxFloat32 depth_threshold = ffxMax(current_depth_view, previous_depth_view);
    const FfxFloat32 normalized_separation = ffxSaturate(
        mgsv_depth_separation_scale * depth_threshold / depth_difference);
    const FfxFloat32 depth_weight = mgsv_depth_clip_power_is_three > 0.f
                                        ? normalized_separation * normalized_separation * normalized_separation
                                        : ffxPow(normalized_separation, mgsv_depth_clip_power);
    depth += depth_weight * weight;
    weight_sum += weight;
  }

  return weight_sum > 0.f ? ffxSaturate(1.f - depth / weight_sum) : 0.f;
}

FfxFloat32 ComputeDepthDivergencePrecomputed(FfxInt32x2 pixel) {
  FfxFloat32 depth_max = 0.f;
  FfxFloat32 depth_min = FSR2_FLT_MAX;
  FfxInt32 max_distance_found = 0;

  for (FfxInt32 y = -1; y <= 1; ++y) {
    for (FfxInt32 x = -1; x <= 1; ++x) {
      const FfxInt32x2 sample_pos = pixel + FfxInt32x2(x, y);
      const FfxFloat32 on_screen = IsOnScreen(sample_pos, RenderSize()) ? 1.f : 0.f;
      const FfxFloat32 device_depth = LoadDilatedDepth(sample_pos);
      const FfxFloat32 depth = GetViewSpaceDepthInMeters(device_depth) * on_screen;

      max_distance_found |= FfxInt32(on_screen > 0.f && device_depth == 0.f);
      depth_min = ffxMin(depth_min, depth);
      depth_max = ffxMax(depth_max, depth);
    }
  }

  return (1.f - depth_min / depth_max) * (FfxBoolean(max_distance_found) ? 0.f : 1.f);
}

void DepthClipOptimized(FfxInt32x2 pixel) {
  const FfxFloat32x2 depth_uv = (pixel + 0.5f) / RenderSize();
  FfxFloat32x2 motion_vector = LoadDilatedMotionVector(pixel);
  motion_vector *= FfxFloat32(length(motion_vector * DisplaySize()) > 0.01f);

  const FfxFloat32x2 dilated_uv = depth_uv + motion_vector;
  const FfxFloat32 dilated_depth = LoadDilatedDepth(pixel);
  const FfxFloat32 depth_clip = ComputeDepthClipPrecomputed(dilated_uv, dilated_depth)
                                * EvaluateSurface(pixel, motion_vector);
  StorePreparedInputColor(
      pixel,
      FfxFloat32x4(ComputePreparedInputColor(pixel), depth_clip));

  const FfxFloat32 motion_divergence = ComputeMotionDivergence(pixel, RenderSize());
  const FfxFloat32 temporal_motion_difference = ffxSaturate(
      ComputeTemporalMotionDivergence(pixel) - ComputeDepthDivergencePrecomputed(pixel));
  const FfxFloat32 accumulation_mask = ffxMax(temporal_motion_difference, motion_divergence);

#if MGSV_FSR2_ZERO_INPUT_MASKS
  StoreDilatedReactiveMasks(pixel, FfxFloat32x2(0.f, accumulation_mask));
#else
  PreProcessReactiveMasks(pixel, accumulation_mask);
#endif
}

#endif  // MGSV_FSR2_DEPTH_CLIP_OPTIMIZED_HLSLI