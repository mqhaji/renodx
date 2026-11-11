// SPDX-License-Identifier: MIT
// Copyright and license notices: ../README.md#credits-and-license.

// MGSV GT-VBGI - full-resolution diffuse lighting and specular visibility.
//
// Reconstruct ambient and shared lighting before skin scattering. AO changes
// ambient diffuse only; direct light and visibility-integrated GI stay separate.
// Fog, tone curves, and display encoding still happen in the game's later passes.

#include "../../../rendering/geometry.hlsli"
#include "../../../materials/material_source.hlsli"
#include "../../../shared.h"
#include "../indirect_diffuse.hlsli"
#include "../../../materials/brdf.hlsli"
#include "../trace/specular_occlusion.hlsli"

Texture2D<float4> lighting_buffer : register(t0);
Texture2D<float4> depth_buffer : register(t1);
Texture2D<float4> diagnostic_buffer : register(t2);
Texture2D<float4> normal_buffer : register(t4);
Texture2D<float4> environment_specular : register(t5);
Texture2D<float4> native_material : register(t6);
Texture2D<float4> raw_specular : register(t7);
Texture2D<float4> temporal_specular : register(t8);
Texture2D<float4> filtered_specular : register(t9);
Texture2D<float4> ambient_diffuse : register(t10);
Texture2D<float4> ambient_weights : register(t11);
Texture2D<float4> transport_buffer : register(t3);
Texture2D<float4> material_palette : register(t12);
Texture2D<float4> brdf_table : register(t13);
Texture2D<float4> response_buffer : register(t14);
Texture2D<float4> fresnel_table : register(t15);
Texture2D<float4> clean_material : register(t16);
Texture2D<uint4> material_owner : register(t17);
Texture2D<float4> source_albedo : register(t18);
Texture2D<float4> current_material : register(t19);
SamplerState ambient_point_wrap : register(s0);
cbuffer AmbientSampling : register(b3) {
  float4 ambient_object_unused[9];
  float4 ambient_local_params[4];
}
RWTexture2D<float4> light_output : register(u0);
RWTexture2D<float2> specular_output : register(u1);
RWTexture2D<float4> scattering_copy : register(u2);
RWTexture2D<float4> resolved_transport : register(u3);
RWTexture2D<float4> resolved_response : register(u4);
RWTexture2D<float> resolved_readiness : register(u5);

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
  // 1. Reconstruct full-resolution lighting from four low-resolution taps.
  // Depth weights prevent foreground bounce from spreading into the background;
  // specular additionally checks normal, roughness, reflected direction, and side.
  const uint2 pixel = thread_id.xy;
  if (pixel.x >= buffer_width || pixel.y >= buffer_height) return;
  // Only the small readiness image needs resetting. Deferred composition
  // ignores HDR results until this invocation writes both halves below.
  resolved_readiness[pixel] = 0.0;

  const float scale = max(CUSTOM_MGSV_LIGHTING_RESOLUTION_DIVISOR, 1.0);
  const float2 position = (float2(pixel) + 0.5) / scale - 0.5;
  const int2 base = (int2)floor(position);
  const float2 fraction = frac(position);

  const float depth = depth_buffer.Load(int3(pixel, 0)).x;
  const float z = LinearZ(depth);
  if (!(z > 0.0) || z > 1.0e6) {
    specular_output[pixel] = 0.0;
    return;
  }
  const bool gi_receiver_valid = depth > 0.0 && z < 1.0e6;
  const float inv_z = 1.0 / z;
  uint gi_width, gi_height;
  lighting_buffer.GetDimensions(gi_width, gi_height);

  float3 radiance = 0.0;
  float weight_sum = 0.0;
  float visibility = 0.0;
  float visibility_weight = 0.0;
  const int diagnostic = (int)clamp(CUSTOM_MGSV_LIGHTING_DEBUG_VIEW, 0.0, 37.0);
  const bool specular_active = CUSTOM_MGSV_SPECULAR_OCCLUSION_ACTIVE >= 0.5;
  float3 center_normal = float3(0.0, 0.0, -1.0);
  float3 center_reflection = 0.0;
  float3 center_lobe = 0.0;
  float center_side = 1.0;
  bool specular_receiver = false;
  float2 specular_mass_sum = 0.0;
  float specular_weight_sum = 0.0;
  float specular_support_sum = 0.0;
  if (specular_active) {
    center_normal = DecodeNormal(normal_buffer.Load(int3(pixel, 0)).xyz);
  }
  if (specular_active && all(isfinite(center_normal))) {
    const float4 captured = environment_specular.Load(int3(pixel, 0));
    const float center_roughness = native_material.Load(int3(pixel, 0)).r;
    if (captured.a > 0.5 && all(isfinite(captured)) && isfinite(center_roughness)
        && center_roughness >= 0.0 && center_roughness <= 1.0) {
      const float3 view = normalize(-ViewPos(float2(pixel) + 0.5, z));
      center_reflection = reflect(-view, center_normal);
      center_lobe = GetSpecularLobeParameters(center_roughness);
      center_side = GetSpecularSurfaceSide(center_normal, view);
      specular_receiver = true;
    }
  }
  const bool transport_active = CUSTOM_MGSV_SPECULAR_GI_ACTIVE >= 0.5;
  uint material_width, material_height;
  current_material.GetDimensions(material_width, material_height);
  const bool material_guide_available = material_width == gi_width && material_height == gi_height;
  SpecularMaterial material = (SpecularMaterial)0;
  float3 transport_lobe = 0.0;
  float3 integrated_response = 0.0;
  if (transport_active && specular_receiver) {
    material = LoadSpecularMaterial(material_palette, fresnel_table, native_material.Load(int3(pixel, 0)));
    transport_lobe = GetSpecularLobeParameters(material.roughness);
    const float3 view = normalize(-ViewPos(float2(pixel) + 0.5, z));
    const float4 integral = GetSpecularBRDFIntegral(brdf_table, abs(dot(center_normal, view)), material.roughness);
    // Material response is removed during denoising and restored only here.
    // The native rate stays distinct from F0 and is applied exactly once.
    integrated_response = GetSpecularEnergy(material.f0, integral.xy) * material.rate;
  }
  float3 transport_sum = 0.0;
  float3 response_sum = 0.0;
  float transport_support = 0.0;
  float transport_weight_sum = 0.0;
  float3 diagnostic_sum = 0.0;
  float diagnostic_weight = 0.0;
  [unroll]
  for (int y = 0; y < 2; ++y) {
    [unroll]
    for (int x = 0; x < 2; ++x) {
      const int2 q = clamp(base + int2(x, y), int2(0, 0), int2(gi_width, gi_height) - 1);
      const float bilinear = (x != 0 ? fraction.x : 1.0 - fraction.x)
                             * (y != 0 ? fraction.y : 1.0 - fraction.y);

      const int2 full = clamp((int2)floor((float2(q) + 0.5) * scale), int2(0, 0),
                              int2((int)buffer_width - 1, (int)buffer_height - 1));
      const float qdepth = depth_buffer.Load(int3(full, 0)).x;
      const float qz = LinearZ(qdepth);
      const float depth_match = qz > 0.0 ? 1.0 / (1.0 + 32.0 * abs(qz - z) * inv_z) : 1.0;

      const float weight = bilinear * depth_match;
      const float4 tap = lighting_buffer.Load(int3(q, 0));
      visibility += (HasAmbientVisibility(tap) ? saturate(tap.a) : 1.0) * weight;
      visibility_weight += weight;
      // Preserve the AO resolve's neutral background taps. GI has stricter
      // geometry validity and must keep a separate normalization sum.
      if (!gi_receiver_valid || !(qdepth > 0.0 && qz > 0.0 && qz < 1.0e6)) continue;
      // Valid black remains in the weight sum; missing/invalid results do not.
      if (HasDiffuseGI(tap)) {
        radiance += tap.rgb * weight;
        weight_sum += weight;
      }
      // Invalid GI must remain visible in the validity view. Diagnostic RGB
      // therefore has its own weights and never feeds the radiance result.
      if ((diagnostic >= 2 && diagnostic <= 11)
          || (diagnostic == 17 && CUSTOM_MGSV_SPECULAR_OCCLUSION_HISTORY_VALID >= 0.5)
          || (diagnostic == 31 && transport_active)) {
        diagnostic_sum += diagnostic_buffer.Load(int3(q, 0)).rgb * weight;
        diagnostic_weight += weight;
      }
      // Both specular signals use this tap's geometry. Reconstruct it once;
      // their roughness tests and accepted weight sums remain independent.
      bool specular_tap_valid = false;
      float normal_weight = 0.0;
      float3 sample_reflection = 0.0;
      if (specular_receiver) {
        const float3 sample_normal = DecodeNormal(normal_buffer.Load(int3(full, 0)).xyz);
        if (all(isfinite(sample_normal))) {
          const float3 view = normalize(-ViewPos(float2(full) + 0.5, qz));
          specular_tap_valid = GetSpecularSurfaceSide(sample_normal, view) == center_side;
          sample_reflection = reflect(-view, sample_normal);
          normal_weight = saturate(dot(center_normal, sample_normal));
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
          normal_weight *= normal_weight;
        }
      }
      if (transport_active && material.valid && specular_tap_valid) {
        const float4 sample = transport_buffer.Load(int3(q, 0));
        const float4 response = response_buffer.Load(int3(q, 0));
        // Neighbors use the lighting grid's current material guide. Keep the
        // receiver's full-resolution lookup above: it also supplies native rate.
        float4 sample_material = float4(0.0, 0.0, 0.0, -1.0);
        if (material_guide_available) {
          sample_material = LoadSpecularMaterialGuide(current_material, q);
        } else {
          const SpecularMaterial native = LoadSpecularMaterial(material_palette, fresnel_table, native_material.Load(int3(full, 0)));
          if (native.valid) sample_material = float4(native.f0, native.roughness);
        }
        if (IsSpecularGIValid(sample, response) && sample_material.a >= 0.0) {
          const float accepted = weight * normal_weight
                                 * GetSpecularLobeWeight(center_reflection, transport_lobe, sample_reflection, sample.a)
                                 * GetSpecularMaterialWeight(material.f0, sample_material.rgb);
          transport_sum += sample.rgb * accepted;
          response_sum += response.rgb * accepted;
          transport_support += response.a * accepted;
          transport_weight_sum += accepted;
        }
      }
      if (specular_tap_valid) {
        const float4 sample = (diagnostic == 14   ? raw_specular.Load(int3(q, 0))
                               : diagnostic == 15 ? temporal_specular.Load(int3(q, 0))
                                                  : filtered_specular.Load(int3(q, 0)));
        if (IsSpecularSampleValid(sample)) {
          const float lobe_weight = GetSpecularLobeWeight(center_reflection, center_lobe, sample_reflection, sample.z);
          const float specular_weight = weight * normal_weight * lobe_weight;
          specular_mass_sum += sample.xy * specular_weight;
          specular_support_sum += sample.a * specular_weight;
          specular_weight_sum += specular_weight;
        }
      }
    }
  }

  // 2. Recover only the native ambient contribution. Our lighting upsample above
  // cannot stand in for the game's own phase/depth-weighted ambient upsample:
  // subtracting a different result would incorrectly change direct light.
  const float3 bounce = radiance / max(weight_sum, 1.0e-4) * CUSTOM_MGSV_DIFFUSE_GI_STRENGTH;
  const float4 center_light = light_output.Load(pixel);
  const float ao = visibility_weight > 1.0e-4 ? visibility / visibility_weight : 1.0;
  float3 ambient = 0.0;
  bool ambient_valid = false;
  uint ambient_width, ambient_height;
  ambient_diffuse.GetDimensions(ambient_width, ambient_height);
  if (ambient_width == buffer_width / 2u && ambient_height == buffer_height / 2u
      && ambient_width > 0u && ambient_height > 0u) {
    // Match BilateralUpscale2x2: point-wrap sampling, four pixel phases, and
    // raw-depth weights. The GT resolve above uses a different reconstruction.
    const float2 full_size = float2(buffer_width, buffer_height);
    const float2 size = ambient_local_params[0].xy;
    const float2 inverse_size = ambient_local_params[0].zw;
    const float2 vertex_position = (float2(pixel) + 0.5) / full_size * size;
    const float center_depth = depth_buffer.SampleLevel(ambient_point_wrap, vertex_position * inverse_size, 0).r;
    float4 weights = ambient_weights.SampleLevel(ambient_point_wrap, vertex_position * 0.25, 0);
    const float2 block = floor(float2(pixel) * 0.5) * 2.0;
    const float2 lower = (block + 0.5) * inverse_size;
    const float2 upper = (block + 2.5) * inverse_size;
    const float4 depths = float4(
        depth_buffer.SampleLevel(ambient_point_wrap, lower, 0).r,
        depth_buffer.SampleLevel(ambient_point_wrap, float2(upper.x, lower.y), 0).r,
        depth_buffer.SampleLevel(ambient_point_wrap, float2(lower.x, upper.y), 0).r,
        depth_buffer.SampleLevel(ambient_point_wrap, upper, 0).r);
    weights /= abs(depths - center_depth) + 1e-6;
    weights /= max(dot(weights, 1.0.xxxx), 1e-20);
    const float4 a = ambient_diffuse.SampleLevel(ambient_point_wrap, lower, 0);
    const float4 b = ambient_diffuse.SampleLevel(ambient_point_wrap, float2(upper.x, lower.y), 0);
    const float4 c = ambient_diffuse.SampleLevel(ambient_point_wrap, float2(lower.x, upper.y), 0);
    const float4 d = ambient_diffuse.SampleLevel(ambient_point_wrap, upper, 0);
    ambient = a.rgb * weights.x + b.rgb * weights.y + c.rgb * weights.z + d.rgb * weights.w;
    // Native ambient was rounded to FP16 before direct lighting was added.
    // Reject larger mismatches; absorb only the small rounding uncertainty.
    const float3 rounding = max(abs(ambient), abs(center_light.rgb)) * (2.0 / 1024.0) + 1.2e-7;
    ambient_valid = all(float4(a.a, b.a, c.a, d.a) > 0.5)
                    && all(isfinite(ambient)) && all(ambient >= 0.0)
                    && all(ambient <= center_light.rgb + rounding)
                    && all(abs(size - full_size) < 0.5);
    ambient = ambient_valid ? min(ambient, max(center_light.rgb, 0.0)) : 0.0;
  }
  // 3. AO attenuates ambient alone. GI already integrates sampled visibility,
  // so multiplying it by AO again would double-count the same obstruction.
  // Receiving albedo is applied once later by native deferred composition.
  const float3 direct = center_light.rgb - ambient;
  const float3 shaded_diffuse = direct + ambient * saturate(ao);
  if (!gi_receiver_valid) {
    light_output[pixel] = float4(shaded_diffuse, center_light.a);
    specular_output[pixel] = 0.0;
    return;
  }
  // Debug views replace diffuse only; surface color, native specular, fog,
  // and later image processing still affect how they look.
  // Both native destinations receive the same RGB before scattering, with
  // their own alpha preserved: accumulation alpha and the skin-material mask.
  float3 output = (diagnostic == 1) ? bounce : shaded_diffuse + bounce;
  if (diagnostic >= 2 && diagnostic <= 11) output = diagnostic_sum / max(diagnostic_weight, 1.0e-4);
  if (diagnostic == 6) output *= CUSTOM_MGSV_DIFFUSE_GI_STRENGTH;
  // 4. Divide filtered specular areas into visibility, then estimate confidence.
  // A small or poorly sampled cone retains native AO through zero confidence.
  // This produces a factor for deferred composition, not reflected color.
  float2 specular = 0.0;
  if (diagnostic >= 14 && diagnostic <= 18) output = float3(1.0, 0.0, 1.0);
  if (specular_receiver && specular_weight_sum > 1e-4) {
    const float2 masses = specular_mass_sum / specular_weight_sum;
    const float support = saturate(specular_support_sum / specular_weight_sum)
                          * saturate(specular_weight_sum / max(visibility_weight, 1e-4));
    const bool has_mass = masses.y > 1e-7;
    if (has_mass) {
      const float mass_support = 1.0 - smoothstep(0.35, 0.75, abs(log2(masses.y)));
      // Deferred composition keeps native AO wherever our estimate lacks
      // coverage. A valid black estimate still has positive confidence.
      specular = float2(saturate(masses.x / masses.y), support * mass_support);
    }
    if (diagnostic >= 14 && diagnostic <= 16) output = (has_mass ? specular.xxx : float3(0.0, 1.0, 1.0));
    if (diagnostic == 18) output = specular.yyy;
  }
  if (diagnostic == 17 && specular_active) output = diagnostic_sum / max(diagnostic_weight, 1e-4);
  if (diagnostic == 12 || diagnostic == 13) {
    // Missing capture must be distinct from a valid black environment result.
    output = float3(1.0, 0.0, 1.0);
    const float4 captured = environment_specular.Load(int3(pixel, 0));
    if (captured.a > 0.5 && all(isfinite(captured))) {
      output = (diagnostic == 12 ? native_material.Load(int3(pixel, 0)).rrr : captured.rgb);
    }
  }
  if (diagnostic == 19) output = ambient_valid ? ambient : float3(1.0, 0.0, 1.0);
  if (diagnostic == 20) output = ambient_valid ? direct : float3(1.0, 0.0, 1.0);
  if (diagnostic == 21) output = shaded_diffuse + bounce;
  if (diagnostic == 22) output = ambient_valid ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 1.0);
  // Stage views show the same paired estimate used by normal composition.
  // Empty native SSR hits preserve this estimate after deferred composition.
  if (diagnostic >= 23) {
    output = float3(1.0, 0.0, 1.0);
    if (transport_active && material.valid) {
      if (diagnostic == 23) output = material.f0;
      if (diagnostic == 24) output = material.roughness.xxx;
      if (diagnostic == 29) output = integrated_response;
      if (diagnostic == 31) output = diagnostic_sum / max(diagnostic_weight, 1e-4);
      if (transport_weight_sum > 1e-4) {
        if (diagnostic >= 25 && diagnostic <= 27) output = transport_sum / transport_weight_sum * integrated_response;
        if (diagnostic == 28) output = response_sum / transport_weight_sum;
        if (diagnostic == 30) output = (transport_support / transport_weight_sum).xxx;
      }
    }
  }
  // Publish a complete paired result only for a receiver whose material is
  // still owned by the captured producer. Missing data keeps native reflections.
  const float4 albedo = source_albedo.Load(int3(pixel, 0));
  const float4 clean = LoadCleanMaterial(clean_material, material_owner, pixel,
                                         uint2(buffer_width, buffer_height), albedo, normal_buffer.Load(int3(pixel, 0)),
                                         native_material.Load(int3(pixel, 0)), depth);
  const bool transport_ready = transport_active && material.valid && clean.a >= 0.0
                               && transport_weight_sum > 1e-4;
  const float3 surface_light = transport_sum / max(transport_weight_sum, 1e-4) * integrated_response;
  const float3 remaining_environment = response_sum / max(transport_weight_sum, 1e-4);
  if (transport_ready && all(isfinite(surface_light)) && all(surface_light >= 0.0)) {
    resolved_transport[pixel] = float4(min(surface_light, 65504.0), 1.0);
    resolved_response[pixel] = float4(saturate(remaining_environment), saturate(transport_support / transport_weight_sum));
    resolved_readiness[pixel] = 1.0;
  }
  if (diagnostic >= 32 && diagnostic <= 34) {
    output = float3(1.0, 0.0, 1.0);
    if (clean.a >= 0.0) {
      if (diagnostic == 32) output = clean.rgb;
      if (diagnostic == 33) output = max(albedo.rgb - clean.rgb, 0.0);
      if (diagnostic == 34) output = float3(0.0, 1.0, 0.0);
    }
  }
  if (diagnostic == 35) output = transport_ready ? surface_light : float3(1.0, 0.0, 1.0);
  if (diagnostic == 36) output = transport_ready ? remaining_environment : float3(1.0, 0.0, 1.0);
  if (diagnostic == 37) output = transport_ready ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 1.0);
  // 5. Update both diffuse consumers before skin scattering. The copy's alpha
  // selects scattering materials; it is not a general lighting-validity flag.
  // Preserve finite values in the native RGBA16F destination at high strengths.
  light_output[pixel] = float4(min(output, 65504.0), center_light.a);
  uint copy_width, copy_height;
  scattering_copy.GetDimensions(copy_width, copy_height);
  if (copy_width == buffer_width && copy_height == buffer_height) {
    const float4 original = scattering_copy.Load(pixel);
    if (original.a > 0.0) scattering_copy[pixel] = float4(min(output, 65504.0), original.a);
  }
  specular_output[pixel] = specular;
}
