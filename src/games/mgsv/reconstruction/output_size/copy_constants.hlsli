#ifndef RENODX_MGSV_POSTPROCESS_COPY_CONSTANTS_HLSLI_
#define RENODX_MGSV_POSTPROCESS_COPY_CONSTANTS_HLSLI_

// Shared by the output-size depth, stencil, and fill shaders. Matches the eight
// 32-bit values Output::PrepareDepth writes in resources.hpp. jitter_uv is the
// current projection jitter, so each output pixel reads the render-size pixel
// that saw the same point.
cbuffer CopyConstants : register(b0) {
  uint2 input_size;
  uint2 output_size;
  uint copy_reserved;
  float2 jitter_uv;
  uint alignment_padding;
}

#endif  // RENODX_MGSV_POSTPROCESS_COPY_CONSTANTS_HLSLI_
