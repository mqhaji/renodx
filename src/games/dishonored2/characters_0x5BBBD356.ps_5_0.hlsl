#include "../../shaders/color.hlsl"
#include "./shared.h"

// ---- Created with 3Dmigoto v1.3.16 on Mon Jun 03 21:41:19 2024

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
  uint4 cb_indirectlighting_globalparms1 : packoffset(c0);
  float4 cb_positiontoviewtexture : packoffset(c1);
  float4 cb_mvpmatrixz : packoffset(c2);
  float4 cb_mvpmatrixy : packoffset(c3);
  float4 cb_mvpmatrixx : packoffset(c4);
  float4 cb_mvpmatrixw : packoffset(c5);
  uint4 cb_mtxoffset : packoffset(c6);
  float4x4 cb_modelviewmatrix : packoffset(c7);
  float4 cb_layer0_scalebias : packoffset(c11);
  float3 cb_emissivecolorconstant : packoffset(c12);
  uint cb_blackwhitemoderuntime : packoffset(c12.w);
  uint2 cb_postfx_luminance_exposureindex : packoffset(c13);
  float cb_view_white_level : packoffset(c13.z);
  float cb_sss_scale : packoffset(c13.w);
  uint cb_sss_blurquality : packoffset(c14);
  float cb_modeldepthhack : packoffset(c14.y);
  float cb_emissiveintensity : packoffset(c14.z);
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

SamplerState smp_bilinearclamp_s : register(s0);
SamplerState smp_pointsampler_s : register(s1);
SamplerState smp_anisotropicsamplerhq_s : register(s2);
Texture2D<float4> ro_texturemap : register(t0);
Texture2D<float4> ro_diffusemap : register(t1);
StructuredBuffer<postfx_luminance_autoexposure_t> ro_postfx_luminance_buffautoexposure : register(t2);


// 3Dmigoto declarations
#define cmp -


void main(
  float4 v0 : INTERP0,
  float4 v1 : INTERP1,
  float4 v2 : SV_POSITION0,
  out float4 o0 : SV_TARGET0)
{
  const float4 icb[] = { { 0, 0, 0, 0},
                              { 0.261544, -0.214057, 0.203516, 0.979019},
                              { 0.189276, 0.359203, -0.573551, -0.816369},
                              { -0.424879, 0.103702, 0.774372, -0.624775},
                              { -0.249214, -0.458083, -0.910972, 0.062549},
                              { -0.126163, 0.507767, 0.824860, 0.398556},
                              { 0.495071, 0.262032, -0.544196, 0.799576},
                              { 0.556352, -0.066713, 0.201379, -0.970482},
                              { -0.554277, -0.239570, 0.594248, -0.120721},
                              { 0.183783, -0.576739, -0.405308, -0.314276},
                              { 0.165838, 0.702719, -0.084916, 0.495581},
                              { 0.638386, -0.379589, 0.282918, -0.470724},
                              { -0.633869, 0.465529, -0.876101, -0.479263},
                              { -0.082461, -0.808313, 0.312257, 0.329286},
                              { 0.473953, -0.729209, -0.480392, 0.294665},
                              { 0.679281, 0.573687, -0.844959, 0.504639},
                              { -0.225166, 0.865322, -0.157169, 0.891863},
                              { -0.888882, 0.245308, -0.096641, -0.652121},
                              { -0.953093, -0.160100, 0.561738, 0.777860},
                              { 0.966643, 0.027039, 0.942426, 0.027156},
                              { -0.748131, -0.691977, -0.580005, -0.008042},
                              { 0.166936, -1.013520, 0.927360, -0.292484},
                              { 0.975921, 0.401562, -0.014385, -0.328475},
                              { -0.558306, 0.897183, 0.304831, 0.013269},
                              { 0.582598, 0.893521, -0.237669, -0.963739},
                              { 0.959807, -0.475906, 0.578900, 0.164567},
                              { -0.322581, -1.037080, 0.249293, 0.607113},
                              { 0.601642, -1.040498, 0.488160, -0.853603},
                              { 0.270943, 1.193335, -0.793537, -0.202170},
                              { -1.049409, 0.637654, -0.248217, 0.107184},
                              { 0.891079, -0.874966, 0.581960, 0.482066},
                              { -1.255226, -0.025452, -0.730991, 0.277543},
                              { -1.077364, -0.750694, -0.497359, -0.555789},
                              { -0.755211, -1.095675, -0.371377, 0.570196},
                              { 0.144352, -1.341533, 0.536028, -0.442197},
                              { -0.940153, 0.979217, 0.040745, 0.235643},
                              { -0.172307, 1.361064, 0.274689, -0.738347},
                              { -0.513016, 1.280129, -0.611833, 0.491174},
                              { 1.347636, -0.463820, 0.233718, -0.262406},
                              { 1.379254, 0.365307, 0.063664, 0.766826},
                              { -0.505936, -1.336283, -0.225748, 0.339386},
                              { -1.395612, -0.349071, -0.627762, -0.375558},
                              { 1.205542, -0.804163, -0.217231, -0.427728},
                              { 1.004364, 1.045869, -0.023220, -0.889440},
                              { -1.433699, 0.241035, -0.251040, -0.143716},
                              { 1.235084, 0.775719, 0.735672, -0.377227},
                              { 1.503525, -0.101138, 0, 0},
                              { 0.850917, -1.258400, 0.148316, 0.985186},
                              { 0.612018, 1.401593, 0.317916, -0.947081},
                              { -0.191717, -1.562365, -0.990148, -0.039443},
                              { -1.477279, 0.643879, 0.966769, -0.213494},
                              { -1.250587, -1.034272, -0.590150, -0.767088},
                              { -1.463851, -0.729820, -0.629034, 0.648048},
                              { 1.178198, -1.146947, 0.749578, 0.530213},
                              { 0.329417, 1.614002, -0.500613, 0.086952},
                              { -1.024873, -1.292581, -0.065061, -0.519965},
                              { 0.001892, 1.653798, 0.154750, 0.484460},
                              { 0.580401, -1.580187, 0.525807, -0.466283},
                              { 1.681753, -0.388989, 0.531709, -0.002859},
                              { -1.419660, 0.998749, -0.094240, -0.985693},
                              { 1.592029, 0.722495, -0.215248, 0.735804},
                              { -1.693594, -0.474197, -0.924056, 0.354231},
                              { -0.643635, 1.640736, -0.689596, -0.361485},
                              { 0.913541, 1.509506, 0.502202, 0.836021},
                              { -1.085543, 1.395367, 0.955955, 0.220772},
                              { 1.236915, 1.285134, 0.249681, -0.638899},
                              { -1.791986, 0.102237, -0.333113, 0.395288},
                              { -0.699545, -1.688101, 0.500957, 0.313053},
                              { 1.028779, -1.555773, -0.386949, -0.320305},
                              { -0.400342, -1.855708, 0, 0},
                              { 0.128605, -1.908445, -0.921892, 0.383776},
                              { 1.929563, 0.110782, 0.615124, -0.777556},
                              { -0.428785, 1.895505, -0.733556, -0.675458},
                              { 0.697714, 1.819086, 0.764137, 0.609827},
                              { 1.899777, 0.441847, -0.149440, 0.956036},
                              { -1.868038, 0.586505, -0.102636, -0.910027},
                              { -1.426252, -1.348491, 0.946465, -0.099615},
                              { -0.979949, 1.702139, -0.969984, -0.192197},
                              { 1.350200, -1.432722, 0.442961, -0.239245},
                              { 0.456130, -1.915281, -0.337835, 0.401963},
                              { 0.372631, 1.939207, 0.302445, 0.423627},
                              { -1.125462, -1.634022, -0.260242, -0.437158},
                              { -1.737297, -0.964568, -0.421322, -0.020392},
                              { -0.004089, 1.987426, 0.209220, -0.592766},
                              { -1.441023, 1.368755, 0.402556, 0.867385},
                              { 1.791620, -0.866787, 0, 0},
                              { 0.814051, -1.817499, 0, 0},
                              { 1.599353, -1.186621, 0, 0},
                              { 1.630604, 1.145238, 0, 0},
                              { -1.745476, 0.964568, 0, 0},
                              { -1.992553, -0.156682, 0, 0},
                              { 1.981567, -0.264107, 0, 0},
                              { 4.292275, 8.099651, 10.208315, 0},
                              { 0.884708, 1.110535, 0.740042, 0},
                              { 0.787022, 0.848465, 0.503994, 0},
                              { 0.747689, 0.745830, 0.416359, 0},
                              { 0.655842, 0.520428, 0.234269, 0},
                              { 0.654168, 0.516560, 0.231312, 0},
                              { 0.620095, 0.440157, 0.174569, 0},
                              { 0.619921, 0.439779, 0.174298, 0},
                              { 0.584093, 0.364954, 0.122639, 0},
                              { 0.582950, 0.362673, 0.121141, 0},
                              { 0.505486, 0.225580, 0.043627, 0},
                              { 0.493907, 0.208311, 0.036235, 0},
                              { 0.471036, 0.176735, 0.024630, 0},
                              { 0.458316, 0.160587, 0.019744, 0},
                              { 0.432374, 0.130593, 0.012675, 0},
                              { 0.424106, 0.121810, 0.011097, 0},
                              { 0.422012, 0.119642, 0.010741, 0},
                              { 0.410618, 0.108234, 0.009070, 0},
                              { 0.393495, 0.092270, 0.007265, 0},
                              { 0.393280, 0.092079, 0.007246, 0},
                              { 0.374511, 0.076147, 0.005962, 0},
                              { 0.371713, 0.073906, 0.005814, 0},
                              { 0.362221, 0.066564, 0.005375, 0},
                              { 0.361757, 0.066215, 0.005355, 0},
                              { 0.358486, 0.063785, 0.005224, 0},
                              { 0.356979, 0.062680, 0.005166, 0},
                              { 0.352238, 0.059274, 0.004993, 0},
                              { 0.318099, 0.037755, 0.004023, 0},
                              { 0.312226, 0.034603, 0.003880, 0},
                              { 0.311100, 0.034018, 0.003853, 0},
                              { 0.305648, 0.031269, 0.003723, 0},
                              { 0.303941, 0.030439, 0.003683, 0},
                              { 0.289711, 0.024065, 0.003349, 0},
                              { 0.285549, 0.022388, 0.003251, 0},
                              { 0.281260, 0.020748, 0.003151, 0},
                              { 0.279391, 0.020061, 0.003107, 0},
                              { 0.276141, 0.018907, 0.003031, 0},
                              { 0.274547, 0.018359, 0.002994, 0},
                              { 0.264589, 0.015207, 0.002761, 0},
                              { 0.264254, 0.015109, 0.002753, 0},
                              { 0.263824, 0.014984, 0.002743, 0},
                              { 0.261789, 0.014403, 0.002696, 0},
                              { 0.259611, 0.013802, 0.002645, 0},
                              { 0.259429, 0.013752, 0.002641, 0},
                              { 0.258652, 0.013544, 0.002623, 0},
                              { 0.257701, 0.013292, 0.002600, 0},
                              { 0.248073, 0.010961, 0.002377, 0},
                              { 0.245725, 0.010451, 0.002324, 0},
                              { 0.243759, 0.010041, 0.002278, 0},
                              { 0.235439, 0.008468, 0.002090, 0},
                              { 0.228725, 0.007379, 0.001940, 0},
                              { 0.226729, 0.007084, 0.001896, 0},
                              { 0.224498, 0.006770, 0.001847, 0},
                              { 0.223020, 0.006569, 0.001815, 0},
                              { 0.222505, 0.006501, 0.001804, 0},
                              { 0.222110, 0.006449, 0.001795, 0},
                              { 0.221391, 0.006356, 0.001780, 0},
                              { 0.216408, 0.005751, 0.001673, 0},
                              { 0.209419, 0.005013, 0.001528, 0},
                              { 0.207877, 0.004865, 0.001497, 0},
                              { 0.205889, 0.004684, 0.001456, 0},
                              { 0.204247, 0.004540, 0.001424, 0},
                              { 0.203662, 0.004490, 0.001412, 0},
                              { 0.203357, 0.004464, 0.001406, 0},
                              { 0.202815, 0.004419, 0.001395, 0},
                              { 0.200371, 0.004222, 0.001347, 0},
                              { 0.198652, 0.004091, 0.001314, 0},
                              { 0.193766, 0.003746, 0.001221, 0},
                              { 0.188204, 0.003401, 0.001119, 0},
                              { 0.183442, 0.003140, 0.001035, 0},
                              { 0.181417, 0.003038, 0.001000, 0},
                              { 0.178639, 0.002905, 0.000954, 0},
                              { 0.177172, 0.002839, 0.000929, 0},
                              { 0.176501, 0.002809, 0.000918, 0},
                              { 0.176203, 0.002796, 0.000914, 0},
                              { 0.175187, 0.002752, 0.000897, 0},
                              { 0.174528, 0.002725, 0.000886, 0},
                              { 0.174357, 0.002717, 0.000884, 0},
                              { 0.173734, 0.002692, 0.000874, 0},
                              { 0.173713, 0.002691, 0.000873, 0},
                              { 0.172928, 0.002659, 0.000861, 0},
                              { 0.171667, 0.002608, 0.000841, 0},
                              { 0.171268, 0.002593, 0.000835, 0},
                              { 0.171225, 0.002591, 0.000834, 0},
                              { 0.171220, 0.002591, 0.000834, 0},
                              { 0.170846, 0.002576, 0.000828, 0},
                              { 0.170687, 0.002570, 0.000826, 0},
                              { 0.170687, 0.002570, 0.000826, 0},
                              { 0.170539, 0.002564, 0.000824, 0},
                              { 0.170318, 0.002556, 0.000820, 0},
                              { 0.169731, 0.002534, 0.000811, 0},
                              { 0.169680, 0.002532, 0.000810, 0} };
  float4 r0,r1,r2,r3,r4;
  uint4 bitmask, uiDest;
  float4 fDest;

  r0.xy = v0.xy * cb_layer0_scalebias.xy + cb_layer0_scalebias.zw;
  r0.xyz = ro_diffusemap.Sample(smp_anisotropicsamplerhq_s, r0.xy).xyz;
  r0.w = cmp(asint(cb_alwaysone) != 0);
  r1.x = asint(cb_indirectlighting_globalparms1.z) & 2;
  r1.x = cmp((int)r1.x != 0);
  r0.w = r0.w ? r1.x : 0;
  r0.xyz = max(float3(9.99999975e-006,9.99999975e-006,9.99999975e-006), r0.xyz);
  r0.xyz = r0.www ? float3(0.5,0.5,0.5) : r0.xyz;
  r1.xy = cb_subpixeloffset.xy + v2.xy;
  r1.xy = cb_positiontoviewtexture.zw * r1.xy;
  r1.zw = cb_resolutionscale.xy * cb_sss_scale;
  r2.xy = cb_globalviewinfos.xy * v1.xx;
  r1.zw = r1.zw / r2.xy;
  if (cb_sss_blurquality == 0) {
    r2.xyz = float3(0,0,0);
    r0.w = 0;
    while (true) {
      r2.w = cmp((int)r0.w >= 92);
      if (r2.w != 0) break;
      r3.xy = icb[r0.w+0].xy * r1.zw + r1.xy;
      r3.xyz = ro_texturemap.Sample(smp_pointsampler_s, r3.xy).xyz;
      r2.xyz = r3.xyz * icb[r0.w+92].xyz + r2.xyz;
      r0.w = (int)r0.w + 1;
    }
    r3.xyz = float3(31.671093,15.9629145,13.2655134);
  } else {
    r0.w = cmp(asint(cb_sss_blurquality) == 1);
    if (r0.w != 0) {
      r2.xyz = float3(0,0,0);
      r0.w = 0;
      while (true) {
        r2.w = cmp((int)r0.w >= 46);
        if (r2.w != 0) break;
        r4.xy = icb[r0.w+0].zw * r1.zw + r1.xy;
        r4.xyz = ro_texturemap.Sample(smp_bilinearclamp_s, r4.xy).xyz;
        r2.xyz = r4.xyz * icb[r0.w+92].xyz + r2.xyz;
        r0.w = (int)r0.w + 1;
      }
      r3.xyz = float3(22.6987591,15.760149,13.2066622);
    } else {
      r0.w = cmp(asint(cb_sss_blurquality) == 2);
      if (r0.w != 0) {
        r2.xyz = float3(0,0,0);
        r0.w = 0;
        while (true) {
          r2.w = cmp((int)r0.w >= 23);
          if (r2.w != 0) break;
          r4.xy = icb[r0.w+46].zw * r1.zw + r1.xy;
          r4.xyz = ro_texturemap.Sample(smp_bilinearclamp_s, r4.xy).xyz;
          r2.xyz = r4.xyz * icb[r0.w+92].xyz + r2.xyz;
          r0.w = (int)r0.w + 1;
        }
        r3.xyz = float3(15.9418354,15.10149,13.1264191);
      } else {
        r0.w = cmp(asint(cb_sss_blurquality) == 3);
        if (r0.w != 0) {
          r2.xyz = float3(0,0,0);
          r0.w = 0;
          while (true) {
            r2.w = cmp((int)r0.w >= 16);
            if (r2.w != 0) break;
            r4.xy = icb[r0.w+69].zw * r1.zw + r1.xy;
            r4.xyz = ro_texturemap.Sample(smp_bilinearclamp_s, r4.xy).xyz;
            r2.xyz = r4.xyz * icb[r0.w+92].xyz + r2.xyz;
            r0.w = (int)r0.w + 1;
          }
          r3.xyz = float3(13.2139864,14.4726477,13.0749454);
        } else {
          r2.xyz = float3(0,0,0);
          r3.xyz = float3(0,0,0);
        }
      }
    }
  }
  r1.xyz = r2.xyz / r3.xyz;
  r0.w = ro_postfx_luminance_buffautoexposure[cb_postfx_luminance_exposureindex.y].EngineLuminanceFactor;
  r0.w = cb_view_white_level * r0.w;
  r2.xyz = r1.xyz * r0.www;
  r1.xyz = cb_usecompressedhdrbuffers ? r2.xyz : r1.xyz;
  r2.xyz = cb_emissiveintensity * cb_emissivecolorconstant.xyz;
  r0.xyz = r1.xyz * r0.xyz + r2.xyz;
  r1.x = dot(r0.xyz, float3(0.212599993,0.715200007,0.0722000003));
  r0.xyz = cb_blackwhitemoderuntime ? r1.xxx : r0.xyz;
  r1.xyz = r0.xyz / r0.www;
  o0.xyz = cb_usecompressedhdrbuffers ? r1.xyz : r0.xyz;
  o0.w = 1;

  if (injectedData.toneMapGammaCorrection) { // fix srgb 2.2 mismatch
    o0.xyz = srgbFromLinear(o0.xyz);
    o0.xyz = sign(o0.xyz) * pow(abs(o0.xyz), 2.2f);
  }
  o0.rgb *= injectedData.toneMapGameNits / 80.f;

  return;
}