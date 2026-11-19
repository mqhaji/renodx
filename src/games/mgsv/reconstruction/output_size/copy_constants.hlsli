#ifndef RENODX_MGSV_POSTPROCESS_COPY_CONSTANTS_HLSLI_
#define RENODX_MGSV_POSTPROCESS_COPY_CONSTANTS_HLSLI_

// Matches the eight 32-bit values written by the output-resource code.
cbuffer CopyConstants : register(b0) {
  uint2 input_size;
  uint2 output_size;
  uint copy_reserved;
  float2 jitter_uv;
  uint alignment_padding;
}

#endif  // RENODX_MGSV_POSTPROCESS_COPY_CONSTANTS_HLSLI_
