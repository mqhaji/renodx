#ifndef RENODX_MGSV_PREPARE_CONSTANTS_HLSLI_
#define RENODX_MGSV_PREPARE_CONSTANTS_HLSLI_

// Matches PrepareConstants in frame_inputs.hpp. Read by input preparation on
// D3D11 and by output encoding on D3D12.
cbuffer ReconstructionPrepareConstants : register(b0) {
  float2 current_jitter_uv;
  float camera_reprojection_valid;
  float analytical_taa;
  uint2 render_size;
  float2 reciprocal_render_size;
  float4 current_to_previous_clip_row_0;
  float4 current_to_previous_clip_row_1;
  float4 current_to_previous_clip_row_2;
  float4 current_to_previous_clip_row_3;
};

#endif  // RENODX_MGSV_PREPARE_CONSTANTS_HLSLI_
