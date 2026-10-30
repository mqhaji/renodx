#include "../../../shared.h"

#define MGSV_PREPARE_CONSTANTS Fsr3PrepareConstants
#include "../../prepare_inputs.hlsli"
#undef MGSV_PREPARE_CONSTANTS

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
	if (any(dispatch_thread_id.xy >= render_size)) return;

	const uint2 pixel = dispatch_thread_id.xy;
	const float2 uv = (float2(pixel) + 0.5f.xx) * reciprocal_render_size;
	const float4 encoded_color = encoded_color_texture.Load(int3(pixel, 0));
	// MGSV supplies sRGB scene RGB here; FSR3 temporal math requires linear RGB.
	linear_color_output[pixel] = float4(
			renodx::color::srgb::Decode(max(0.f.xxx, encoded_color.rgb)),
			encoded_color.a);

	const float2 native_velocity = DecodeNativeVelocity(pixel);
	float2 camera_velocity = native_velocity;
	if (camera_reprojection_valid > 0.f) {
		if (!ComputeMatrixCameraVelocity(uv, depth_texture.Load(int3(pixel, 0)), camera_velocity)) {
			camera_velocity = native_velocity;
		}
	}

	const float object_mask = object_velocity_texture.Load(int3(pixel, 0)).r;
	const float2 object_velocity = native_velocity - current_jitter_uv * velocity_projection_jitter_scale;
	const float2 current_minus_previous = object_mask > 0.5f ? object_velocity : camera_velocity;

	// FSR3 reprojects as current_uv + motion, while MGSV's native signal and
	// analytical resolve use current_uv - velocity.
	motion_vector_output[pixel] = -current_minus_previous;
}