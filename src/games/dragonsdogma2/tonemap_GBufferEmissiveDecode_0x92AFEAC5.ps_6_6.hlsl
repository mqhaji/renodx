/*

{
  struct RangeCompressInfo
  {
      float rangeCompress;                          ; Offset:    0
      float rangeDecompress;                        ; Offset:    4
      float prevRangeCompress;                      ; Offset:    8
      float prevRangeDecompress;                    ; Offset:   12
  } RangeCompressInfo;                              ; Offset:    0 Size:    16
}
*/
cbuffer RangeCompressInfoUBO : register(b0, space0) {
  float4 RangeCompressInfo_m0[1] : packoffset(c0);
};
/*
cbuffer Tonemap
{
  struct Tonemap
  {
      float exposureAdjustment;                     ; Offset:    0
      float tonemapRange;                           ; Offset:    4
      float specularSuppression;                    ; Offset:    8
      float sharpness;                              ; Offset:   12
      float preTonemapRange;                        ; Offset:   16
      int useAutoExposure;                          ; Offset:   20
      float echoBlend;                              ; Offset:   24
      float AABlend;                                ; Offset:   28
      float AASubPixel;                             ; Offset:   32
      float ResponsiveAARate;                       ; Offset:   36
      float VelocityWeightRate;                     ; Offset:   40
      float DepthRejectionRate;                     ; Offset:   44
      float ContrastTrackingRate;                   ; Offset:   48
      float ContrastTrackingThreshold;              ; Offset:   52
  } Tonemap;                                        ; Offset:    0 Size:    56
}
*/
cbuffer TonemapUBO : register(b1, space0) {
  float4 Tonemap_m0[4] : packoffset(c0);
};

Buffer<uint4> WhitePtSrv : register(t0, space0);
Texture2D<float4> ModifiedGBufferSRV : register(t1, space0);

static float4 gl_FragCoord;
static float4 SV_Target;

struct SPIRV_Cross_Input {
  float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output {
  float4 SV_Target : SV_Target0;
};

float max3(float a, float b, float c) { return max(a, max(b, c)); }

float max3(float3 v) { return max3(v.x, v.y, v.z); }

void PS_GBufferEmissiveDecode() {
  float4 inputColor = ModifiedGBufferSRV.Load(
      int3(uint2(uint(int(gl_FragCoord.x)), uint(int(gl_FragCoord.y))), 0u));

  float3 outputColor;
  if (floor(inputColor.w * 3.099999904632568359375f) == 1.0f) {
    float _79;
    if (asuint(Tonemap_m0[1u]).y == 0u) {
      _79 = 1.0f;
    } else {
      _79 = asfloat(WhitePtSrv.Load(0u).x);
    }
    float _88 =
        1.0f / max(1.52587890625e-05f,
                   ((RangeCompressInfo_m0[0u].y * Tonemap_m0[0u].x) * _79) *
                       (1.0f - max3(inputColor.rgb)));
    outputColor.rgb =
        RangeCompressInfo_m0[0u].y * max(0.0f, _88 * inputColor.rgb);
  } else {
    outputColor.rgb = 0.f;
  }
  SV_Target = float4(outputColor, 0.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input) {
  gl_FragCoord = stage_input.gl_FragCoord;
  gl_FragCoord.w = 1.0 / gl_FragCoord.w;
  PS_GBufferEmissiveDecode();
  SPIRV_Cross_Output stage_output;
  stage_output.SV_Target = SV_Target;
  return stage_output;
}
