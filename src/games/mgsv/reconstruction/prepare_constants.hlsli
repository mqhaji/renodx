#ifndef RENODX_MGSV_PREPARE_CONSTANTS_HLSLI_
#define RENODX_MGSV_PREPARE_CONSTANTS_HLSLI_

// Matches PrepareConstants in frame_inputs.hpp.
cbuffer ReconstructionPrepareConstants : register(b0) {
  float2 current_jitter_uv;
  float velocity_projection_jitter_scale;
  float camera_reprojection_valid;
  uint2 render_size;
  float2 reciprocal_render_size;
  float4 current_to_previous_clip_row_0;
  float4 current_to_previous_clip_row_1;
  float4 current_to_previous_clip_row_2;
  float4 current_to_previous_clip_row_3;
  float2 previous_jitter_uv;
  float object_motion_mode;
  float analytical_taa;
};

#endif  // RENODX_MGSV_PREPARE_CONSTANTS_HLSLI_
