  // ---- Created with 3Dmigoto v1.3.16 on Mon Jun 03 19:56:42 2024
  groupshared struct { float val[36]; } g1[18];
  groupshared struct { float val[72]; } g0[18];

  struct postfx_luminance_autoexposure_t
  {
      float EngineLuminanceFactor;   // Offset:    0
      float LuminanceFactor;         // Offset:    4
      float MinLuminanceLDR;         // Offset:    8
      float MaxLuminanceLDR;         // Offset:   12
      float MiddleGreyLuminanceLDR;  // Offset:   16
      float EV;                      // Offset:   20
      float Fstop;                   // Offset:   24
      uint PeakHistogramValue;       // Offset:   28
  };

  cbuffer PerInstanceCB : register(b2)
  {
    float4 cb_positiontoviewtexture : packoffset(c0);
    float4 cb_taatexsize : packoffset(c1);
    float4 cb_taaditherandviewportsize : packoffset(c2);
    float4 cb_postfx_tonemapping_tonemappingparms : packoffset(c3);
    float4 cb_postfx_tonemapping_tonemappingcoeffsinverse1 : packoffset(c4);
    float4 cb_postfx_tonemapping_tonemappingcoeffsinverse0 : packoffset(c5);
    float4 cb_postfx_tonemapping_tonemappingcoeffs1 : packoffset(c6);
    float4 cb_postfx_tonemapping_tonemappingcoeffs0 : packoffset(c7);
    uint2 cb_postfx_luminance_exposureindex : packoffset(c8);
    float2 cb_prevresolutionscale : packoffset(c8.z);
    float cb_env_tonemapping_white_level : packoffset(c9);
    float cb_view_white_level : packoffset(c9.y);
    float cb_taaamount : packoffset(c9.z);
    float cb_postfx_luminance_customevbias : packoffset(c9.w);
  }

  cbuffer PerViewCB : register(b1)
  {
    float4 cb_alwaystweak : packoffset(c0);
    float4 cb_viewrandom : packoffset(c1);
    float4x4 cb_viewprojectionmatrix : packoffset(c2);
    float4x4 cb_viewmatrix : packoffset(c6);
    float4 cb_subpixeloffset : packoffset(c10);
    float4x4 cb_projectionmatrix : packoffset(c11);
    float4x4 cb_previousviewprojectionmatrix : packoffset(c15);
    float4x4 cb_previousviewmatrix : packoffset(c19);
    float4x4 cb_previousprojectionmatrix : packoffset(c23);
    float4 cb_mousecursorposition : packoffset(c27);
    float4 cb_mousebuttonsdown : packoffset(c28);
    float4 cb_jittervectors : packoffset(c29);
    float4x4 cb_inverseviewprojectionmatrix : packoffset(c30);
    float4x4 cb_inverseviewmatrix : packoffset(c34);
    float4x4 cb_inverseprojectionmatrix : packoffset(c38);
    float4 cb_globalviewinfos : packoffset(c42);
    float3 cb_wscamforwarddir : packoffset(c43);
    uint cb_alwaysone : packoffset(c43.w);
    float3 cb_wscamupdir : packoffset(c44);
    uint cb_usecompressedhdrbuffers : packoffset(c44.w);
    float3 cb_wscampos : packoffset(c45);
    float cb_time : packoffset(c45.w);
    float3 cb_wscamleftdir : packoffset(c46);
    float cb_systime : packoffset(c46.w);
    float2 cb_jitterrelativetopreviousframe : packoffset(c47);
    float2 cb_worldtime : packoffset(c47.z);
    float2 cb_shadowmapatlasslicedimensions : packoffset(c48);
    float2 cb_resolutionscale : packoffset(c48.z);
    float2 cb_parallelshadowmapslicedimensions : packoffset(c49);
    float cb_framenumber : packoffset(c49.z);
    uint cb_alwayszero : packoffset(c49.w);
  }

  SamplerState smp_linearclamp_s : register(s0);
  Texture2D<float3> ro_taahistory_read : register(t0);
  Texture2D<float4> ro_motionvectors : register(t1);
  Texture2D<float4> ro_viewcolormap : register(t2);
  StructuredBuffer<postfx_luminance_autoexposure_t> ro_postfx_luminance_buffautoexposure : register(t3);
  RWTexture2D<float3> rw_taahistory_write : register(u0);
  RWTexture2D<float3> rw_taaresult : register(u1);


  // 3Dmigoto declarations
  #define cmp -

  [numthreads(16, 16, 1)]
  void main(
    uint3 vThreadID : SV_DispatchThreadID,
    uint3 vGroupID : SV_GroupID,
    uint3 vThreadIDInGroup : SV_GroupThreadID)
  {
  // Needs manual fix for instruction:
  // unknown dcl_: dcl_uav_typed_texture2d (float,float,float,float) u0
  // Needs manual fix for instruction:
  // unknown dcl_: dcl_uav_typed_texture2d (float,float,float,float) u1
    float4 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13;
    uint4 bitmask, uiDest;
    float4 fDest;

  // Needs manual fix for instruction:
  // unknown dcl_: dcl_thread_group 16, 16, 1
    r0.x = mad((int)vThreadIDInGroup.y, 16, (int)vThreadIDInGroup.x);
    r0.x = (int)r0.x;
    r0.x = 0.5 + r0.x;
    r0.x = 0.055555556 * r0.x;
    r0.y = floor(r0.x);
    r0.x = frac(r0.x);
    r0.x = 18 * r0.x;
    r0.x = floor(r0.x);
    r1.xy = (int2)r0.xy;
    r0.xy = (int2)-vThreadIDInGroup.xy + (int2)vThreadID.xy;
    r0.xy = (int2)r0.xy + int2(-1,-1);
    r0.z = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EngineLuminanceFactor;
    r0.w = exp2(-cb_postfx_luminance_customevbias);
    r0.w = r0.z * r0.w;
    r1.w = cmp((int)r1.y < 14);
    if (r1.w != 0) {
      r2.xy = (int2)r1.xy + (int2)r0.xy;
      r2.zw = float2(0,0);
      r3.xyz = ro_viewcolormap.Load(r2.xyw).xyz;
      r1.w = cb_view_white_level * r0.z;
      r4.xyz = r3.xyz * r1.www;
      r3.xyz = cb_usecompressedhdrbuffers ? r4.xyz : r3.xyz;
      // r3.xyz = max(float3(0,0,0), r3.xyz);
      // r3.xyz = min(cb_env_tonemapping_white_level, r3.xyz);
      r3.xyz = r3.xyz * r0.www;
      r4.xyz = cmp(r3.xyz < cb_postfx_tonemapping_tonemappingparms.xxx);
      r5.xyzw = r4.xxxx ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
      r3.xw = r5.xy * r3.xx + r5.zw;
      r5.x = r3.x / r3.w;
      r6.xyzw = r4.yyyy ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
      r3.xy = r6.xy * r3.yy + r6.zw;
      r5.y = r3.x / r3.y;
      r4.xyzw = r4.zzzz ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
      r3.xy = r4.xy * r3.zz + r4.zw;
      r5.z = r3.x / r3.y;
      r2.xy = ro_motionvectors.Load(r2.xyz).xy;
      r5.w = dot(r2.xy, r2.xy);
      r2.zw = (uint2)r1.xx << int2(4,3);
      g0[r1.y].val[r2.z/4] = r5.x;
      g0[r1.y].val[r2.z/4+1] = r5.y;
      g0[r1.y].val[r2.z/4+2] = r5.z;
      g0[r1.y].val[r2.z/4+3] = r5.w;
      g1[r1.y].val[r2.w/4] = r2.x;
      g1[r1.y].val[r2.w/4+1] = r2.y;
    }
    r1.z = (int)r1.y + 14;
    r1.y = cmp((int)r1.z < 18);
    if (r1.y != 0) {
      r2.xy = (int2)r0.xy + (int2)r1.xz;
      r2.zw = float2(0,0);
      r3.xyz = ro_viewcolormap.Load(r2.xyw).xyz;
      r0.x = cb_view_white_level * r0.z;
      r4.xyz = r3.xyz * r0.xxx;
      r3.xyz = cb_usecompressedhdrbuffers ? r4.xyz : r3.xyz;
      // r3.xyz = max(float3(0,0,0), r3.xyz);
      // r3.xyz = min(cb_env_tonemapping_white_level, r3.xyz);
      r3.xyz = r3.xyz * r0.www;
      r4.xyz = cmp(r3.xyz < cb_postfx_tonemapping_tonemappingparms.xxx);
      r5.xyzw = r4.xxxx ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
      r0.xy = r5.xy * r3.xx + r5.zw;
      r5.x = r0.x / r0.y;
      r6.xyzw = r4.yyyy ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
      r0.xy = r6.xy * r3.yy + r6.zw;
      r5.y = r0.x / r0.y;
      r4.xyzw = r4.zzzz ? cb_postfx_tonemapping_tonemappingcoeffs0.xyzw : cb_postfx_tonemapping_tonemappingcoeffs1.xyzw;
      r0.xy = r4.xy * r3.zz + r4.zw;
      r5.z = r0.x / r0.y;
      r0.xy = ro_motionvectors.Load(r2.xyz).xy;
      r5.w = dot(r0.xy, r0.xy);
      r1.xy = (uint2)r1.xx << int2(4,3);
      g0[r1.z].val[r1.x/4] = r5.x;
      g0[r1.z].val[r1.x/4+1] = r5.y;
      g0[r1.z].val[r1.x/4+2] = r5.z;
      g0[r1.z].val[r1.x/4+3] = r5.w;
      g1[r1.z].val[r1.y/4] = r0.x;
      g1[r1.z].val[r1.y/4+1] = r0.y;
    }
    GroupMemoryBarrierWithGroupSync();
    r0.xy = (int2)vThreadID.yx;
    r0.xy = float2(0.5,0.5) + r0.xy;
    r1.xy = cb_taaditherandviewportsize.yx + r0.xy;
    r1.x = dot(r1.xy, float2(214013.156,2531011.75));
    r1.y = (int)r1.x * (int)r1.x;
    r1.y = mad((int)r1.y, 0x00003d73, 0x000c0ae5);
    r1.x = (int)r1.y * (int)r1.x;
    r1.x = (uint)r1.x >> 9;
    r1.x = (int)r1.x + 0x3f800000;
    r1.x = 2 + -r1.x;
    r1.x = r1.x * 0.600000024 + -0.300000012;
    r1.y = (uint)vThreadIDInGroup.x << 4;
    r2.x = g0[vThreadIDInGroup.y].val[r1.y/4];
    r2.y = g0[vThreadIDInGroup.y].val[r1.y/4+1];
    r2.z = g0[vThreadIDInGroup.y].val[r1.y/4+2];
    r2.w = g0[vThreadIDInGroup.y].val[r1.y/4+3];
    r1.z = cmp(-1 < r2.w);
    r3.x = r2.w;
    r3.yz = vThreadIDInGroup.xy;
    r3.xyz = r1.zzz ? r3.xyz : float3(-1,0,0);
    r4.xyzw = (int4)vThreadIDInGroup.xxyy + int4(1,2,0,0);
    r1.zw = (int2)r1.yy + int2(16,32);
    r5.x = g0[r4.w].val[r1.z/4+3];
    r5.y = g0[r4.w].val[r1.z/4];
    r5.z = g0[r4.w].val[r1.z/4+1];
    r5.w = g0[r4.w].val[r1.z/4+2];
    r6.xyz = r5.yzw + r2.xyz;
    r7.xyz = r5.yzw * r5.yzw;
    r2.xyz = r2.xyz * r2.xyz + r7.xyz;
    r2.w = cmp(r3.x < r5.x);
    r5.yz = r4.xw;
    r3.xyz = r2.www ? r5.xyz : r3.xyz;
    r5.x = g0[r4.z].val[r1.w/4];
    r5.y = g0[r4.z].val[r1.w/4+1];
    r5.z = g0[r4.z].val[r1.w/4+2];
    r5.w = g0[r4.z].val[r1.w/4+3];
    r6.xyz = r6.xyz + r5.xyz;
    r2.xyz = r5.xyz * r5.xyz + r2.xyz;
    r2.w = cmp(r3.x < r5.w);
    r4.x = r5.w;
    r3.xyz = r2.www ? r4.xyz : r3.xyz;
    r4.xyzw = (int4)vThreadIDInGroup.xxyy + int4(0,1,1,1);
    r5.x = g0[r4.w].val[r1.y/4+3];
    r5.y = g0[r4.w].val[r1.y/4];
    r5.z = g0[r4.w].val[r1.y/4+1];
    r5.w = g0[r4.w].val[r1.y/4+2];
    r6.xyz = r6.xyz + r5.yzw;
    r2.xyz = r5.yzw * r5.yzw + r2.xyz;
    r2.w = cmp(r3.x < r5.x);
    r5.yz = r4.xw;
    r3.xyz = r2.www ? r5.xyz : r3.xyz;
    r5.x = g0[r4.z].val[r1.z/4];
    r5.y = g0[r4.z].val[r1.z/4+1];
    r5.z = g0[r4.z].val[r1.z/4+2];
    r5.w = g0[r4.z].val[r1.z/4+3];
    r2.w = dot(r5.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r2.w = 1 + -r2.w;
    r1.x = r1.x * r2.w + 1;
    r7.xyz = r5.xyz * r1.xxx;
    r6.xyz = r6.xyz + r5.xyz;
    r2.xyz = r5.xyz * r5.xyz + r2.xyz;
    r2.w = cmp(r3.x < r5.w);
    r4.x = r5.w;
    r3.xyz = r2.www ? r4.xyz : r3.xyz;
    r4.xyzw = (int4)vThreadIDInGroup.xxyy + int4(2,0,2,1);
    r8.x = g0[r4.w].val[r1.w/4+3];
    r8.y = g0[r4.w].val[r1.w/4];
    r8.z = g0[r4.w].val[r1.w/4+1];
    r8.w = g0[r4.w].val[r1.w/4+2];
    r6.xyz = r8.yzw + r6.xyz;
    r2.xyz = r8.yzw * r8.yzw + r2.xyz;
    r2.w = cmp(r3.x < r8.x);
    r8.yz = r4.xw;
    r3.xyz = r2.www ? r8.xyz : r3.xyz;
    r8.x = g0[r4.z].val[r1.y/4];
    r8.y = g0[r4.z].val[r1.y/4+1];
    r8.z = g0[r4.z].val[r1.y/4+2];
    r8.w = g0[r4.z].val[r1.y/4+3];
    r6.xyz = r8.xyz + r6.xyz;
    r2.xyz = r8.xyz * r8.xyz + r2.xyz;
    r1.y = cmp(r3.x < r8.w);
    r4.x = r8.w;
    r3.xyz = r1.yyy ? r4.xyz : r3.xyz;
    r4.xyzw = (int4)vThreadIDInGroup.xyxy + int4(1,2,2,2);
    r8.x = g0[r4.y].val[r1.z/4+3];
    r8.y = g0[r4.y].val[r1.z/4];
    r8.z = g0[r4.y].val[r1.z/4+1];
    r8.w = g0[r4.y].val[r1.z/4+2];
    r6.xyz = r8.yzw + r6.xyz;
    r2.xyz = r8.yzw * r8.yzw + r2.xyz;
    r1.y = cmp(r3.x < r8.x);
    r8.yz = r4.xy;
    r3.xyz = r1.yyy ? r8.xyz : r3.xyz;
    r8.x = g0[r4.w].val[r1.w/4];
    r8.y = g0[r4.w].val[r1.w/4+1];
    r8.z = g0[r4.w].val[r1.w/4+2];
    r8.w = g0[r4.w].val[r1.w/4+3];
    r1.yzw = r8.xyz + r6.xyz;
    r2.xyz = r8.xyz * r8.xyz + r2.xyz;
    r2.w = cmp(r3.x < r8.w);
    r3.xy = r2.ww ? r4.zw : r3.yz;
    r4.xyz = float3(0.111111112,0.111111112,0.111111112) * r1.yzw;
    r6.xyz = r4.xyz * r4.xyz;
    r2.xyz = r2.xyz * float3(0.111111112,0.111111112,0.111111112) + -r6.xyz;
    r2.xyz = max(float3(0,0,0), r2.xyz);
    r2.xyz = sqrt(r2.xyz);
    r6.xyz = r1.yzw * float3(0.111111112,0.111111112,0.111111112) + -r2.xyz;
    r1.yzw = r1.yzw * float3(0.111111112,0.111111112,0.111111112) + r2.xyz;
    r2.xyz = min(r7.xyz, r6.xyz);
    r1.yzw = max(r7.xyz, r1.yzw);
    r2.w = (uint)r3.x << 3;
    r3.x = g1[r3.y].val[r2.w/4];
    r3.y = g1[r3.y].val[r2.w/4+1];
    r0.xy = -r3.yx * cb_taaditherandviewportsize.wz + r0.xy;
    r3.xy = cb_prevresolutionscale.yx / cb_resolutionscale.yx;
    r3.zw = r0.yx * r3.yx + float2(-0.5,-0.5);
    r3.zw = floor(r3.zw);
    r6.xyzw = float4(0.5,0.5,-0.5,-0.5) + r3.zwzw;
    r0.xy = r0.xy * r3.xy + -r6.yx;
    r3.xy = r0.yx * r0.yx;
    r7.xy = r3.xy * r0.yx;
    r7.zw = r3.yx * r0.xy + r0.xy;
    r7.zw = -r7.zw * float2(0.5,0.5) + r3.yx;
    r8.xy = float2(2.5,2.5) * r3.yx;
    r7.xy = r7.yx * float2(1.5,1.5) + -r8.xy;
    r7.xy = float2(1,1) + r7.xy;
    r0.xy = r3.xy * r0.yx + -r3.xy;
    r3.xy = float2(0.5,0.5) * r0.xy;
    r8.xy = float2(1,1) + -r7.wz;
    r8.xy = r8.xy + -r7.yx;
    r0.xy = -r0.xy * float2(0.5,0.5) + r8.xy;
    r7.xy = r7.xy + r0.yx;
    r0.xy = r0.xy / r7.yx;
    r0.xy = r6.xy + r0.xy;
    r3.zw = float2(2.5,2.5) + r3.zw;
    r6.xw = cb_taatexsize.zw * r6.zw;
    r6.yz = cb_taatexsize.wz * r0.yx;
    r8.xy = cb_taatexsize.zw * r3.zw;
    r0.xy = -cb_positiontoviewtexture.zw * float2(0.5,0.5) + cb_prevresolutionscale.xy;
    r9.xyzw = cb_prevresolutionscale.xyxy * r6.xwzw;
    r9.xyzw = max(float4(0,0,0,0), r9.xyzw);
    r9.xyzw = min(r9.xyzw, r0.xyxy);
    r10.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r9.xy, 0).xyz;
    r0.z = cb_view_white_level * r0.z;
    r11.xyz = r10.xyz * r0.zzz;
    r10.xyz = cb_usecompressedhdrbuffers ? r11.xyz : r10.xyz;
    r2.w = r7.w * r7.z;
    r3.z = dot(r10.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r3.w = min(1.00000003e+032, r3.z);
    r4.w = max(-1.00000003e+032, r3.z);
    r9.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r9.zw, 0).xyz;
    r11.xyz = r9.xyz * r0.zzz;
    r9.xyz = cb_usecompressedhdrbuffers ? r11.xyz : r9.xyz;
    r11.xy = r7.yx * r7.zw;
    r11.xzw = r11.xxx * r9.xyz;
    r10.xyz = r10.xyz * r2.www + r11.xzw;
    r2.w = dot(r9.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r3.z + r2.w;
    r3.w = min(r3.w, r2.w);
    r2.w = max(r4.w, r2.w);
    r8.zw = r6.wy;
    r9.xyzw = cb_prevresolutionscale.xyxy * r8.xzxw;
    r9.xyzw = max(float4(0,0,0,0), r9.xyzw);
    r9.xyzw = min(r9.xyzw, r0.xyxy);
    r11.xzw = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r9.xy, 0).xyz;
    r12.xyz = r11.xzw * r0.zzz;
    r11.xzw = cb_usecompressedhdrbuffers ? r12.xyz : r11.xzw;
    r7.zw = r3.xy * r7.zw;
    r10.xyz = r11.xzw * r7.zzz + r10.xyz;
    r4.w = dot(r11.xzw, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r4.w + r3.z;
    r3.w = min(r4.w, r3.w);
    r2.w = max(r4.w, r2.w);
    r12.xyzw = cb_prevresolutionscale.xyxy * r6.xyzy;
    r12.xyzw = max(float4(0,0,0,0), r12.xyzw);
    r12.xyzw = min(r12.xyzw, r0.xyxy);
    r11.xzw = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r12.xy, 0).xyz;
    r13.xyz = r11.xzw * r0.zzz;
    r11.xzw = cb_usecompressedhdrbuffers ? r13.xyz : r11.xzw;
    r10.xyz = r11.xzw * r11.yyy + r10.xyz;
    r4.w = dot(r11.xzw, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r4.w + r3.z;
    r3.w = min(r4.w, r3.w);
    r2.w = max(r4.w, r2.w);
    r11.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r12.zw, 0).xyz;
    r12.xyz = r11.xyz * r0.zzz;
    r11.xyz = cb_usecompressedhdrbuffers ? r12.xyz : r11.xyz;
    r4.w = r7.y * r7.x;
    r10.xyz = r11.xyz * r4.www + r10.xyz;
    r4.w = dot(r11.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r4.w + r3.z;
    r3.w = min(r4.w, r3.w);
    r2.w = max(r4.w, r2.w);
    r9.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r9.zw, 0).xyz;
    r11.xyz = r9.xyz * r0.zzz;
    r9.xyz = cb_usecompressedhdrbuffers ? r11.xyz : r9.xyz;
    r7.xy = r7.xy * r3.xy;
    r10.xyz = r9.xyz * r7.xxx + r10.xyz;
    r4.w = dot(r9.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r4.w + r3.z;
    r3.w = min(r4.w, r3.w);
    r2.w = max(r4.w, r2.w);
    r6.y = r8.y;
    r6.xyzw = cb_prevresolutionscale.xyxy * r6.xyzy;
    r6.xyzw = max(float4(0,0,0,0), r6.xyzw);
    r6.xyzw = min(r6.xyzw, r0.xyxy);
    r9.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r6.xy, 0).xyz;
    r11.xyz = r9.xyz * r0.zzz;
    r9.xyz = cb_usecompressedhdrbuffers ? r11.xyz : r9.xyz;
    r7.xzw = r9.xyz * r7.www + r10.xyz;
    r4.w = dot(r9.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r4.w + r3.z;
    r3.w = min(r4.w, r3.w);
    r2.w = max(r4.w, r2.w);
    r6.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r6.zw, 0).xyz;
    r9.xyz = r6.xyz * r0.zzz;
    r6.xyz = cb_usecompressedhdrbuffers ? r9.xyz : r6.xyz;
    r7.xyz = r6.xyz * r7.yyy + r7.xzw;
    r4.w = dot(r6.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r3.z = r4.w + r3.z;
    r3.w = min(r4.w, r3.w);
    r2.w = max(r4.w, r2.w);
    r6.xy = cb_prevresolutionscale.xy * r8.xy;
    r6.xy = max(float2(0,0), r6.xy);
    r0.xy = min(r6.xy, r0.xy);
    r6.xyz = ro_taahistory_read.SampleLevel(smp_linearclamp_s, r0.xy, 0).xyz;
    r8.xyz = r6.xyz * r0.zzz;
    r6.xyz = cb_usecompressedhdrbuffers ? r8.xyz : r6.xyz;
    r0.x = r3.x * r3.y;
    r7.xyz = r6.xyz * r0.xxx + r7.xyz;
    r0.x = dot(r6.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r0.y = r3.z + r0.x;
    r3.x = min(r3.w, r0.x);
    r0.x = max(r2.w, r0.x);
    r0.y = 0.111111112 * r0.y;
    r3.yzw = max(r7.xyz, r2.xyz);
    r3.yzw = min(r3.yzw, r1.yzw);
    r1.y = dot(r1.yzw, float3(0.212599993,0.715200007,0.0722000003));
    r1.z = dot(r2.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r1.y = r1.y + -r1.z;
    r1.y = max(0.100000001, r1.y);
    r0.y = max(0.100000001, r0.y);
    r0.y = r1.y * r0.y;
    r0.x = r0.x + -r3.x;
    r0.x = max(0.100000001, r0.x);
    r1.y = dot(r4.xyz, float3(0.212599993,0.715200007,0.0722000003));
    r1.y = max(0.100000001, r1.y);
    r0.x = r1.y * r0.x;
    r0.x = r0.y / r0.x;
    r0.x = saturate(-1 + r0.x);
    r0.y = r0.x * -2 + 3;
    r0.x = r0.x * r0.x;
    r0.x = -r0.y * r0.x + 1;
    r0.x = cb_taaamount * r0.x + 0.00999999978;
    r1.xyz = r5.xyz * r1.xxx + -r3.yzw;
    r1.xyz = r0.xxx * r1.xyz + r3.yzw;
    r2.xyzw = r1.xyzx / r0.zzzz;
    r2.xyzw = cb_usecompressedhdrbuffers ? r2.xyzw : r1.xyzx;


  // No code for instruction (needs manual fix):
    // store_uav_typed u0.xyzw, vThreadID.xyyy, r2.xyzw

    rw_taahistory_write[vThreadID.xy] = r2.xyzw;  // added

    r2.xyz = cmp(r1.xyz < cb_postfx_tonemapping_tonemappingparms.yyy);
    r3.xyzw = r2.xxxx ? cb_postfx_tonemapping_tonemappingcoeffsinverse0.xyzw : cb_postfx_tonemapping_tonemappingcoeffsinverse1.xyzw;
    r0.xy = r3.xy * r1.xx + r3.zw;
    r3.xw = r0.xx / r0.yy;
    r4.xyzw = r2.yyyy ? cb_postfx_tonemapping_tonemappingcoeffsinverse0.xyzw : cb_postfx_tonemapping_tonemappingcoeffsinverse1.xyzw;
    r0.xy = r4.xy * r1.yy + r4.zw;
    r3.y = r0.x / r0.y;
    r2.xyzw = r2.zzzz ? cb_postfx_tonemapping_tonemappingcoeffsinverse0.xyzw : cb_postfx_tonemapping_tonemappingcoeffsinverse1.xyzw;
    r0.xy = r2.xy * r1.zz + r2.zw;
    r3.z = r0.x / r0.y;
    r1.xyzw = r3.xyzw / r0.wwww;
    r0.xyzw = r1.wyzw / r0.zzzz;
    r0.xyzw = cb_usecompressedhdrbuffers ? r0.xyzw : r1.xyzw;


  // No code for instruction (needs manual fix):
    // store_uav_typed u1.xyzw, vThreadID.xyyy, r0.xyzw

    
    rw_taaresult[vThreadID.xy] = r0.xyzw; // added

    return;
  }