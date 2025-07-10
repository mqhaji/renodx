#define M_PI 3.14159265358979323846

float3x3 Inverse3x3(float3x3 m) {
  float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2];
  float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2];
  float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2];

  float det = a00 * (a11 * a22 - a12 * a21)
              - a01 * (a10 * a22 - a12 * a20)
              + a02 * (a10 * a21 - a11 * a20);

  float invDet = 1.0 / det;

  float3x3 inv;
  inv[0][0] = (a11 * a22 - a12 * a21) * invDet;
  inv[0][1] = -(a01 * a22 - a02 * a21) * invDet;
  inv[0][2] = (a01 * a12 - a02 * a11) * invDet;
  inv[1][0] = -(a10 * a22 - a12 * a20) * invDet;
  inv[1][1] = (a00 * a22 - a02 * a20) * invDet;
  inv[1][2] = -(a00 * a12 - a02 * a10) * invDet;
  inv[2][0] = (a10 * a21 - a11 * a20) * invDet;
  inv[2][1] = -(a00 * a21 - a01 * a20) * invDet;
  inv[2][2] = (a00 * a11 - a01 * a10) * invDet;

  return inv;
}

float copysign(float x, float y) {
  return abs(x) * sign(y);
}

// <ACEStransformID>urn:ampas:aces:transformId:v2.0:Lib.Academy.Utilities.a2.v1</ACEStransformID>
// <ACESuserName>Utilities</ACESuserName>

//
// Generic functions that may be useful for writing CTL programs
//

// C[0] = red.xy
// C[1] = green.xy
// C[2] = blue.xy
// C[3] = white.xy
float3x3 RGBtoXYZ_f33(float4x2 C, float Y) {
  float2 red = C[0];
  float2 green = C[1];
  float2 blue = C[2];
  float2 white = C[3];

  float X = white.x * Y / white.y;
  float Z = (1.0 - white.x - white.y) * Y / white.y;

  float d = red.x * (blue.y - green.y) + blue.x * (green.y - red.y) + green.x * (red.y - blue.y);

  float Sr = (X * (blue.y - green.y) - green.x * (Y * (blue.y - 1.0) + blue.y * (X + Z)) + blue.x * (Y * (green.y - 1.0) + green.y * (X + Z))) / d;

  float Sg = (X * (red.y - blue.y) + red.x * (Y * (blue.y - 1.0) + blue.y * (X + Z)) - blue.x * (Y * (red.y - 1.0) + red.y * (X + Z))) / d;

  float Sb = (X * (green.y - red.y) - red.x * (Y * (green.y - 1.0) + green.y * (X + Z)) + green.x * (Y * (red.y - 1.0) + red.y * (X + Z))) / d;

  float3x3 M;
  M[0] = Sr * float3(red.x, red.y, 1.0 - red.x - red.y);
  M[1] = Sg * float3(green.x, green.y, 1.0 - green.x - green.y);
  M[2] = Sb * float3(blue.x, blue.y, 1.0 - blue.x - blue.y);

  return M;
}

float3x3 XYZtoRGB_f33(float4x2 C,
                      float Y) {
  return Inverse3x3(RGBtoXYZ_f33(C, Y));
}

float radians_to_degrees(float radians) {
  return radians * 180.0 / M_PI;
}

float degrees_to_radians(float degrees) {
  return degrees / 180.0 * M_PI;
}

float3x3 scale_matrix_diagonal_f33_f3(float3x3 A,
                                      float3 v) {
  float3x3 B = A;
  B[0][0] = A[0][0] * v[0];
  B[1][1] = A[1][1] * v[1];
  B[2][2] = A[2][2] * v[2];
  return B;
}

// <ACEStransformID>urn:ampas:aces:transformId:v2.0:Lib.Academy.Tonescale.a2.v1</ACEStransformID>
// <ACESuserName>Tonescale Functions</ACESuserName>

//
// Library File with functions used for pre-calculating the tonescale parameters and
// applying the fwd/inv tonescale function
//

struct TSParams {
  float n;
  float n_r;
  float g;
  float t_1;
  float c_t;
  float s_2;
  float u_2;
  float m_2;
  float forward_limit;
  float inverse_limit;
  float log_peak;
};

// Tonescale pre-calculations
TSParams init_TSParams(float peakLuminance) {
  // Preset constants that set the desired behavior for the curve
  const float n = peakLuminance;

  const float n_r = 100.0;       // normalized white in nits (what 1.0 should be)
  const float g = 1.15;          // surround / contrast
  const float c = 0.18;          // anchor for 18% grey
  const float c_d = 10.013;      // output luminance of 18% grey (in nits)
  const float w_g = 0.14;        // change in grey between different peak luminance
  const float t_1 = 0.04;        // shadow toe or flare/glare compensation
  const float r_hit_min = 128.;  // scene-referred value "hitting the roof" for SDR (e.g. when n = 100 nits)
  const float r_hit_max = 896.;  // scene-referred value "hitting the roof" for when n = 10000 nits

  // Calculate output constants
  const float r_hit = r_hit_min + (r_hit_max - r_hit_min) * (log(n / n_r) / log(10000. / 100.));
  const float m_0 = (n / n_r);
  const float m_1 = 0.5 * (m_0 + sqrt(m_0 * (m_0 + 4. * t_1)));
  const float u = pow((r_hit / m_1) / ((r_hit / m_1) + 1), g);
  const float m = m_1 / u;
  const float w_i = log(n / 100.) / log(2.);
  const float c_t = c_d / n_r * (1. + w_i * w_g);
  const float g_ip = 0.5 * (c_t + sqrt(c_t * (c_t + 4. * t_1)));
  const float g_ipp2 = -(m_1 * pow((g_ip / m), (1. / g))) / (pow(g_ip / m, 1. / g) - 1.);
  const float w_2 = c / g_ipp2;
  const float s_2 = w_2 * m_1;
  const float u_2 = pow((r_hit / m_1) / ((r_hit / m_1) + w_2), g);
  const float m_2 = m_1 / u_2;

  TSParams p;
  p.n = n;
  p.n_r = n_r;
  p.g = g;
  p.t_1 = t_1;
  p.c_t = c_t;
  p.s_2 = s_2;
  p.u_2 = u_2;
  p.m_2 = m_2;
  p.forward_limit = 8.0 * r_hit;
  p.inverse_limit = n / (u_2 * n_r);
  p.log_peak = log10(n / n_r);

  return p;
}

/* --- Tone scale math --- */
float tonescale_fwd(float x,         // scene-referred input (i.e. linear c-1)
                    TSParams params  // struct of type TSParams
) {
  // forward MM tone scale
  float f = params.m_2 * pow(max(0.0, x) / (x + params.s_2), params.g);
  float h = max(0., f * f / (f + params.t_1));  // forward flare

  return h * params.n_r;  // output is luminance in cd/m^2
}

float tonescale_inv(float Y,         // luminance in cd/m^2
                    TSParams params  // struct of type TSParams
) {
  float Z = max(0., min(params.n / (params.u_2 * params.n_r), Y));
  float h = (Z + sqrt(Z * (4. * params.t_1 + Z))) / 2.;
  float f = params.s_2 / (pow((params.m_2 / h), (1. / params.g)) - 1.);

  return f;  // output is scene-referred input
}

// <ACEStransformID>urn:ampas:aces:transformId:v2.0:Lib.Academy.OutputTransform.a2.v1</ACEStransformID>
// <ACESuserName>Output Transform Functions</ACESuserName>

//
// Library File with functions and presets used for the forward and inverse output
// transform
//

// Chromaticities & Conversion matrices
// Academy Primaries 0 (i.e. "ACES" Primaries from SMPTE ST2065-1)
const float4x2 Chromaticities_AP0 = float4x2(
    0.73470, 0.26530,
    0.00000, 1.00000,
    0.00010, -0.07700,
    0.32168, 0.33767);

static const float3x3 AP0_XYZ_TO_RGB = XYZtoRGB_f33(Chromaticities_AP0, 1.0);
static const float3x3 AP0_RGB_TO_XYZ = RGBtoXYZ_f33(Chromaticities_AP0, 1.0);

// Academy Primaries 1
const float4x2 Chromaticities_AP1 = float4x2(
    0.713, 0.293,
    0.165, 0.830,
    0.128, 0.044,
    0.32168, 0.33767);

static const float3x3 AP1_XYZ_TO_RGB = XYZtoRGB_f33(Chromaticities_AP1, 1.0);
static const float3x3 AP1_RGB_TO_XYZ = RGBtoXYZ_f33(Chromaticities_AP1, 1.0);

static const float3x3 AP0_TO_AP1 = mul(AP0_RGB_TO_XYZ, AP1_XYZ_TO_RGB);
static const float3x3 AP1_TO_AP0 = mul(AP1_RGB_TO_XYZ, AP0_XYZ_TO_RGB);

// "Reach" Primaries - equal to ACES "AP1" primaries
static const float4x2 REACH_PRI = Chromaticities_AP1;

// Table generation
const int tableSize = 360;
const int additionalTableEntries = 2;  // allots for extra entries to wrap the hues
static const int totalTableSize = tableSize + additionalTableEntries;
const int baseIndex = 1;  // array index for smallest filled entry of padded table

const float hue_limit = 360.;

const int cuspCornerCount = 6;
static const int totalCornerCount = cuspCornerCount + 2;
static const int max_sorted_corners = 2 * cuspCornerCount;
const float reach_cusp_tolerance = 1e-3;
const float display_cusp_tolerance = 1e-7;

// Wrappers for fixed-size float3 array return since HLSL does not allow array return types
struct JMhCornerTable {
  float3 values[8];  // totalCornerCount = 8
};
struct SortedHues {
  float values[12];  // max_sorted_corners = 12
};
struct HueTable {
  float values[362];  // totalTableSize = 362
};
struct CuspTable {
  float3 values[362];  // totalTableSize = 362
};
struct ReachMTable {
  float values[362];  // totalTableSize = 362
};

const float gamma_minimum = 0.0;
const float gamma_maximum = 5.0;
const float gamma_search_step = 0.4;
const float gamma_accuracy = 1e-5;

// CAM Parameters
const float ref_luminance = 100.;
const float L_A = 100.;
const float Y_b = 20.;
const float3 surround = float3(0.9, 0.59, 0.9);  // Dim surround

const float J_scale = 100.0;
const float cam_nl_Y_reference = 100.0;
static const float cam_nl_offset = 0.2713 * cam_nl_Y_reference;
static const float cam_nl_scale = 4.0 * cam_nl_Y_reference;

static const float model_gamma = surround[1] * (1.48 + sqrt(Y_b / ref_luminance));

// Chroma compression
const float chroma_compress = 2.4;
const float chroma_compress_fact = 3.3;
const float chroma_expand = 1.3;
const float chroma_expand_fact = 0.69;
const float chroma_expand_thr = 0.5;

// Gamut compression
const float smooth_cusps = 0.12;
const float smooth_m = 0.27;
const float cusp_mid_blend = 1.3;

const float focus_gain_blend = 0.3;
const float focus_adjust_gain = 0.55;
const float focus_distance = 1.35;
const float focus_distance_scaling = 1.75;

const float compression_threshold = 0.75;

const float3x3 MATRIX_IDENTITY = float3x3(
    1, 0, 0,
    0, 1, 0,
    0, 0, 1);

struct JMhParams {
  // Pre-computed conversion matrices and constants for conversions to/from JMh
  float3x3 MATRIX_RGB_to_CAM16_c;
  float3x3 MATRIX_CAM16_c_to_RGB;
  float3x3 MATRIX_cone_response_to_Aab;
  float3x3 MATRIX_Aab_to_cone_response;
  float F_L_n;  // F_L normalised
  float cz;
  float inv_cz;  // 1/cz
  float A_w_J;
  float inv_A_w_J;  // 1/A_w_J
};

struct HueDependentGamutParams {
  // Hue-dependent gamut parameters
  float2 JMcusp;
  float gamma_bottom_inv;
  float gamma_top_inv;
  float focus_J;
  float analytical_threshold;
};

struct ODTParams {
  float peakLuminance;

  // JMh parameters
  JMhParams input_params;
  JMhParams reach_params;
  JMhParams limit_params;

  // Tonescale parameters
  TSParams ts;

  // Shared compression parameters
  float limit_J_max;
  float model_gamma_inv;
  float TABLE_reach_M[362];  // totalTableSize

  // Chroma compression parameters
  float sat;
  float sat_thr;
  float compr;
  float chroma_compress_scale;

  // Gamut compression parameters
  float mid_J;
  float focus_dist;
  float lower_hull_gamma_inv;
  float TABLE_hues[362];              // totalTableSize
  float3 TABLE_gamut_cusps[362];      // totalTableSize
  float TABLE_upper_hull_gamma[362];  // totalTableSize
  int2 hue_linearity_search_range;
};

float wrap_to_360(float hue) {
  float y = fmod(hue, 360.);
  if (y < 0.) {
    y = y + 360.;
  }
  return y;
}

int hue_position_in_uniform_table(float hue, int table_size) {
  const float wrapped_hue = wrap_to_360(hue);
  int result = (wrapped_hue / hue_limit * table_size);
  return result;
}

int next_position_in_table(int entry, int table_size) {
  int result = (entry + 1) % table_size;
  return result;
}

float base_hue_for_position(int i_lo, int table_size) {
  float result = i_lo * hue_limit / table_size;
  return result;
}

// CAM Functions
float _post_adaptation_cone_response_compression_fwd(float Rc) {
  const float F_L_Y = pow(Rc, 0.42);
  const float Ra = (F_L_Y) / (cam_nl_offset + F_L_Y);
  return Ra;
}

float _post_adaptation_cone_response_compression_inv(float Ra) {
  const float Ra_lim = min(Ra, 0.99);
  const float F_L_Y = (cam_nl_offset * Ra_lim) / (1. - Ra_lim);
  const float Rc = pow(F_L_Y, 1. / 0.42);
  return Rc;
}

float post_adaptation_cone_response_compression_fwd(float v) {
  const float abs_v = abs(v);
  const float Ra = _post_adaptation_cone_response_compression_fwd(abs_v);
  return copysign(Ra, v);
}

float post_adaptation_cone_response_compression_inv(float v) {
  const float abs_v = abs(v);
  const float Rc = _post_adaptation_cone_response_compression_inv(abs_v);
  return copysign(Rc, v);
}

float Achromatic_n_to_J(float A,
                        float cz) {
  return J_scale * pow(A, cz);
}

float J_to_Achromatic_n(float J,
                        float inv_cz) {
  return pow(J * (1. / J_scale), inv_cz);
}

// Optimization for achromatic values
float _A_to_Y(float A,
              JMhParams p) {
  float Ra = p.A_w_J * A;
  float Y = _post_adaptation_cone_response_compression_inv(Ra) / p.F_L_n;

  return Y;
}

float J_to_Y(float J,
             JMhParams p) {
  float abs_J = abs(J);

  return _A_to_Y(J_to_Achromatic_n(abs_J, p.inv_cz), p);
}

float Y_to_J(float Y,
             JMhParams p) {
  float abs_Y = abs(Y);
  float Ra = _post_adaptation_cone_response_compression_fwd(abs_Y * p.F_L_n);
  float J = Achromatic_n_to_J(Ra * p.inv_A_w_J, p.cz);

  return copysign(J, Y);
}

float3 RGB_to_Aab(float3 RGB,
                  JMhParams p) {
  float3 rgb_m = mul(RGB, p.MATRIX_RGB_to_CAM16_c);

  float3 rgb_a = {
    post_adaptation_cone_response_compression_fwd(rgb_m[0]),
    post_adaptation_cone_response_compression_fwd(rgb_m[1]),
    post_adaptation_cone_response_compression_fwd(rgb_m[2])
  };

  float3 Aab = mul(rgb_a, p.MATRIX_cone_response_to_Aab);

  return Aab;
}

float3 Aab_to_JMh(float3 Aab,
                  JMhParams p) {
  float3 JMh = { 0., 0., 0. };
  if (Aab[0] <= 0.) {
    return JMh;
  }
  float J = Achromatic_n_to_J(Aab[0], p.cz);
  float M = sqrt(Aab[1] * Aab[1] + Aab[2] * Aab[2]);
  float h_rad = atan2(Aab[2], Aab[1]);
  float h = wrap_to_360(radians_to_degrees(h_rad));

  JMh[0] = J;
  JMh[1] = M;
  JMh[2] = h;

  return JMh;
}

float3 RGB_to_JMh(float3 RGB,
                  JMhParams p) {
  float3 Aab = RGB_to_Aab(RGB, p);
  float3 JMh = Aab_to_JMh(Aab, p);

  return JMh;
}

float3 JMh_to_Aab(float3 JMh,
                  JMhParams p) {
  float J = JMh[0];
  float M = JMh[1];
  float h = JMh[2];
  float h_rad = degrees_to_radians(h);
  float cos_hr = cos(h_rad);
  float sin_hr = sin(h_rad);

  float A = J_to_Achromatic_n(J, p.inv_cz);
  float a = M * cos_hr;
  float b = M * sin_hr;
  float3 Aab = { A, a, b };

  return Aab;
}

float3 Aab_to_RGB(float3 Aab,
                  JMhParams p) {
  float3 rgb_a = mul(Aab, p.MATRIX_Aab_to_cone_response);

  float3 rgb_m = float3(
      post_adaptation_cone_response_compression_inv(rgb_a[0]),
      post_adaptation_cone_response_compression_inv(rgb_a[1]),
      post_adaptation_cone_response_compression_inv(rgb_a[2]));

  float3 rgb = mul(rgb_m, p.MATRIX_CAM16_c_to_RGB);

  return rgb;
}

float3 JMh_to_RGB(float3 JMh,
                  JMhParams p) {
  float3 Aab = JMh_to_Aab(JMh, p);
  float3 rgb = Aab_to_RGB(Aab, p);
  return rgb;
}

float3 clamp_AP0_to_AP1(float3 aces,
                        float clamp_lower_limit,
                        float clamp_upper_limit) {
  float3 AP1 = mul(aces, AP0_TO_AP1);
  float3 AP1_clamped = clamp(AP1, clamp_lower_limit, clamp_upper_limit);
  float3 AP0_clamped = mul(AP1_clamped, AP1_TO_AP0);

  return AP0_clamped;
}

float reach_M_from_table(float h,
                         float table[362]) {  // float TABLE_reach_M[362];
  int base = hue_position_in_uniform_table(h, tableSize);
  float t = h - base;
  int i_lo = base + baseIndex;
  int i_hi = i_lo + 1;

  return lerp(table[i_lo], table[i_hi], t);
}

float reinhard_remap(float scale,
                     float nd,
                     bool invert = false) {
  if (invert) {
    if (nd >= 1.0) {
      return scale;
    } else {
      return scale * -(nd / (nd - 1.));
    }
  }
  return scale * nd / (1. + nd);
}

float midpoint(float low, float high) {
  return (low + high) / 2.;
}

// A "toe" function that remaps the given value x between 0 and limit.
// The k1 and k2 parameters change the size and shape of the toe.
// https://www.desmos.com/calculator/6vplvw14ti
float toe(float x,
          float limit,
          float k1_in,
          float k2_in,
          bool invert = false) {
  if (x > limit)
    return x;

  float k2 = max(k2_in, 0.001);
  float k1 = sqrt(k1_in * k1_in + k2 * k2);
  float k3 = (limit + k1) / (limit + k2);

  if (invert) {
    return (x * x + k1 * x) / (k3 * (x + k2));
  } else {
    const float minus_b = k3 * x - k1;
    const float minus_c = k2 * k3 * x;
    return 0.5 * (minus_b + sqrt(minus_b * minus_b + 4. * minus_c));
  }
}

float chroma_compress_norm(float h,
                           float chroma_compress_scale) {
  float hr = degrees_to_radians(h);

  float a = cos(hr);
  float b = sin(hr);
  float cos_hr2 = a * a - b * b;
  float sin_hr2 = 2.0 * a * b;
  float cos_hr3 = 4.0 * a * a * a - 3.0 * a;
  float sin_hr3 = 3.0 * b - 4.0 * b * b * b;

  float M = 11.34072 * a + 16.46899 * cos_hr2 + 7.88380 * cos_hr3 + 14.66441 * b + -6.37224 * sin_hr2 + 9.19364 * sin_hr3 + 77.12896;

  return M * chroma_compress_scale;
}

// In-gamut chroma compression
//
// Compresses colors inside the gamut with the aim for colorfulness to have an
// appropriate rate of change from display black to display white, and from
// achromatic outward to purer colors.
float3 chroma_compress_fwd(float3 JMh,
                           float tonemapped_J,
                           ODTParams p,
                           bool invert = false) {
  float J = JMh[0];
  float M = JMh[1];
  float h = JMh[2];

  float M_compr = M;

  if (M != 0.0) {
    const float nJ = tonemapped_J / p.limit_J_max;
    const float snJ = max(0., 1. - nJ);
    float Mnorm = chroma_compress_norm(h, p.chroma_compress_scale);
    float limit = pow(nJ, p.model_gamma_inv) * reach_M_from_table(h, p.TABLE_reach_M) / Mnorm;

    float toe_limit = limit - 0.001;
    float toe_snJ_sat = snJ * p.sat;
    float toe_sqrt_nJ_sat_thr = sqrt(nJ * nJ + p.sat_thr);
    float toe_nJ_compr = nJ * p.compr;

    // Rescaling of M with the tonescaled J to get the M to the same range as
    // J after the tonescale.  The rescaling uses the Hellwig2022 model gamma to
    // keep the M/J ratio correct (keeping the chromaticities constant).
    M_compr = M * pow(tonemapped_J / J, p.model_gamma_inv);

    // Normalize M with the rendering space cusp M
    M_compr = M_compr / Mnorm;

    // Expand the colorfulness by running the toe function in reverse.  The goal is to
    // expand less saturated colors less and more saturated colors more.  The expansion
    // increases saturation in the shadows and mid-tones but not in the highlights.
    // The 0.001 offset starts the expansions slightly above zero.  The sat_thr makes
    // the toe less aggressive near black to reduce the expansion of noise.
    M_compr = limit - toe(limit - M_compr, toe_limit, toe_snJ_sat, toe_sqrt_nJ_sat_thr, false);

    // Compress the colorfulness.  The goal is to compress less saturated colors more and
    // more saturated colors less, especially in the highlights.  This step creates the
    // saturation roll-off in the highlights, but attemps to preserve pure colors.  This
    // mostly affects highlights and mid-tones, and does not compress shadows.
    M_compr = toe(M_compr, limit, toe_nJ_compr, snJ, false);

    // Denormalize
    M_compr = M_compr * Mnorm;
  }

  float3 output = float3(tonemapped_J, M_compr, h);
  return output;
}

float3 chroma_compress_inv(float3 JMh,
                           float J,
                           ODTParams p,
                           bool invert = false) {
  float tonemapped_J = JMh[0];
  float M_compr = JMh[1];
  float h = JMh[2];

  float M = M_compr;

  if (M_compr != 0.0) {
    const float nJ = tonemapped_J / p.limit_J_max;
    const float snJ = max(0., 1. - nJ);
    float Mnorm = chroma_compress_norm(h, p.chroma_compress_scale);
    float limit = pow(nJ, p.model_gamma_inv) * reach_M_from_table(h, p.TABLE_reach_M) / Mnorm;

    float toe_limit = limit - 0.001;
    float toe_snJ_sat = snJ * p.sat;
    float toe_sqrt_nJ_sat_thr = sqrt(nJ * nJ + p.sat_thr);
    float toe_nJ_compr = nJ * p.compr;

    M = M_compr / Mnorm;
    M = toe(M, limit, toe_nJ_compr, snJ, true);
    M = limit - toe(limit - M, toe_limit, toe_snJ_sat, toe_sqrt_nJ_sat_thr, true);
    M = M * Mnorm;
    M = M * pow(tonemapped_J / J, -p.model_gamma_inv);
  }

  float3 output = float3(J, M, h);
  return output;
}

float3 tonemap_and_compress_fwd(float3 JMh,
                                ODTParams p) {
  // Applies the forward tonescale, then compresses M based on J and tonemapped J

  // Tonemap
  float linear_Y = J_to_Y(JMh[0], p.input_params) / ref_luminance;

  float tonemapped_Y = tonescale_fwd(linear_Y, p.ts);

  float J_ts = Y_to_J(tonemapped_Y, p.input_params);

  // Compress M; function returns { tonemapped J, compressed M, h }
  float3 JMh_tc = chroma_compress_fwd(JMh, J_ts, p, false);

  return JMh_tc;
}

float3 tonemap_and_compress_inv(float3 JMh_tc,
                                ODTParams p) {
  // Applies the inverse tonescale, then uncompresses M based on tonemapped J and J

  // Un-tonemap
  float luminance = J_to_Y(JMh_tc[0], p.input_params);

  float linear_Y = tonescale_inv(luminance / ref_luminance, p.ts);

  float J = Y_to_J(linear_Y * ref_luminance, p.input_params);

  // Un-compress M; function returns { J, M, h }
  float3 JMh = chroma_compress_inv(JMh_tc, J, p, true);

  return JMh;
}

float compute_compression_vector_slope(float intersect_J,
                                       float focus_J,
                                       float limit_J_max,
                                       float slope_gain) {
  float direction_scalar;
  if (intersect_J < focus_J) {
    direction_scalar = intersect_J;
  } else {
    direction_scalar = limit_J_max - intersect_J;
  }
  return direction_scalar * (intersect_J - focus_J) / (focus_J * slope_gain);
}

float solve_J_intersect(float J,
                        float M,
                        float focusJ,
                        float maxJ,
                        float slope_gain) {
  const float M_scaled = M / slope_gain;
  const float a = M_scaled / focusJ;

  if (J < focusJ) {
    const float b = 1. - M_scaled;
    const float c = -J;
    const float det = b * b - 4. * a * c;
    const float root = sqrt(det);
    return -2. * c / (b + root);
  } else {
    const float b = -(1. + M_scaled + maxJ * a);
    const float c = maxJ * M_scaled + J;
    const float det = b * b - 4. * a * c;
    const float root = sqrt(det);
    return -2. * c / (b - root);
  }
}

// Smooth minimum about the scaled reference, based upon a cubic polynomial
float smin_scaled(float a,
                  float b,
                  float scale_reference) {
  const float s_scaled = smooth_cusps * scale_reference;
  const float h = max(s_scaled - abs(a - b), 0.0) / s_scaled;
  return min(a, b) - h * h * h * s_scaled * (1. / 6.);
}

float estimate_line_and_boundary_intersection_M(float J_axis_intersect,
                                                float slope,
                                                float inv_gamma,
                                                float J_max,
                                                float M_max,
                                                float J_intersection_reference) {
  // Line defined by     J = slope * x + J_axis_intersect
  // Boundary defined by J = J_max * (x / M_max) ^ (1/inv_gamma)
  // Approximate as we do not want to iteratively solve intersection of a
  // straight line and an exponential

  // We calculate a shifted intersection from the original intersection using
  // the inverse of the exponential and the provided reference
  const float normalised_J = J_axis_intersect / J_intersection_reference;
  const float shifted_intersection = J_intersection_reference * pow(normalised_J, inv_gamma);

  // Now we find the M intersection of two lines
  // line from origin to J,M Max       l1(x) = J/M * x
  // line from J Intersect' with slope l2(x) = slope * x + Intersect'

  // return shifted_intersection / ((J_max / M_max) - slope);
  return shifted_intersection * M_max / (J_max - slope * M_max);
}

float find_gamut_boundary_intersection(float2 JM_cusp,
                                       float J_max,
                                       float gamma_top_inv,
                                       float gamma_bottom_inv,
                                       float J_intersect_source,
                                       float slope,
                                       float J_intersect_cusp) {
  const float M_boundary_lower = estimate_line_and_boundary_intersection_M(J_intersect_source,
                                                                           slope,
                                                                           gamma_bottom_inv,
                                                                           JM_cusp[0],
                                                                           JM_cusp[1],
                                                                           J_intersect_cusp);

  // The upper hull is flipped and thus 'zeroed' at J_max
  // Also note we negate the slope
  const float f_J_intersect_cusp = J_max - J_intersect_cusp;
  const float f_J_intersect_source = J_max - J_intersect_source;
  const float f_JM_cusp_J = J_max - JM_cusp[0];
  const float M_boundary_upper = estimate_line_and_boundary_intersection_M(f_J_intersect_source,
                                                                           -slope,
                                                                           gamma_top_inv,
                                                                           f_JM_cusp_J,
                                                                           JM_cusp[1],
                                                                           f_J_intersect_cusp);

  // Smooth minimum between the two calculated values for the M component
  float M_boundary = smin_scaled(M_boundary_lower, M_boundary_upper, JM_cusp[1]);
  return M_boundary;
}

float hueDependentUpperHullGamma(float h,
                                 float gamma_table[362]) {  // totalTableSize
  const int i_lo = hue_position_in_uniform_table(h, tableSize) + baseIndex;
  const int i_hi = next_position_in_table(i_lo, tableSize);  // gamma_table.size not supported in hlsl

  const float base_hue = base_hue_for_position(i_lo - baseIndex, tableSize);

  const float t = wrap_to_360(h) - base_hue;

  return lerp(gamma_table[i_lo], gamma_table[i_hi], t);
}

float get_focus_gain(float J,
                     float analytical_threshold,
                     float limit_J_max,
                     float focus_dist) {
  float gain = limit_J_max * focus_dist;

  if (J > analytical_threshold) {
    // Approximate inverse required above threshold due to the introduction of J in the calculation
    float gain_adjustment = log10((limit_J_max - analytical_threshold) / max(0.0001, limit_J_max - J));
    gain_adjustment = gain_adjustment * gain_adjustment + 1.;
    gain = gain * gain_adjustment;
  }

  return gain;
}

float remap_M(float M,
              float gamut_boundary_M,
              float reach_boundary_M,
              bool invert = false) {
  const float boundary_ratio = gamut_boundary_M / reach_boundary_M;
  const float proportion = max(boundary_ratio, compression_threshold);
  const float threshold = proportion * gamut_boundary_M;

  if (M <= threshold || proportion >= 1.)
    return M;

  // Translate to place threshold at zero
  const float m_offset = M - threshold;
  const float gamut_offset = gamut_boundary_M - threshold;
  const float reach_offset = reach_boundary_M - threshold;

  const float scale = reach_offset / ((reach_offset / gamut_offset) - 1.);
  const float nd = m_offset / scale;

  // Shift result back to absolute
  return threshold + reinhard_remap(scale, nd, invert);
}

float3 compress_gamut(float3 JMh,
                      float Jx,
                      ODTParams p,
                      HueDependentGamutParams hdp,
                      bool invert = false) {
  const float J = JMh[0];
  const float M = JMh[1];
  const float h = JMh[2];

  const float slope_gain = get_focus_gain(Jx, hdp.analytical_threshold, p.limit_J_max, p.focus_dist);
  const float J_intersect_source = solve_J_intersect(J, M, hdp.focus_J, p.limit_J_max, slope_gain);
  const float gamut_slope = compute_compression_vector_slope(J_intersect_source, hdp.focus_J, p.limit_J_max, slope_gain);

  const float J_intersect_cusp = solve_J_intersect(hdp.JMcusp[0], hdp.JMcusp[1], hdp.focus_J, p.limit_J_max, slope_gain);

  const float gamut_boundary_M = find_gamut_boundary_intersection(hdp.JMcusp,
                                                                  p.limit_J_max,
                                                                  hdp.gamma_top_inv,
                                                                  hdp.gamma_bottom_inv,
                                                                  J_intersect_source,
                                                                  gamut_slope,
                                                                  J_intersect_cusp);

  if (gamut_boundary_M <= 0.) {
    float3 returnJMh = float3(J, 0., h);
    return returnJMh;
  }

  float reach_max_M = reach_M_from_table(h, p.TABLE_reach_M);

  const float reach_boundary_M = estimate_line_and_boundary_intersection_M(J_intersect_source,
                                                                           gamut_slope,
                                                                           p.model_gamma_inv,
                                                                           p.limit_J_max,
                                                                           reach_max_M,
                                                                           p.limit_J_max);

  const float remapped_M = remap_M(M, gamut_boundary_M, reach_boundary_M, invert);

  float3 JMhcompressed = float3(J_intersect_source + remapped_M * gamut_slope,
                                remapped_M,
                                h);

  return JMhcompressed;
}

float2 cusp_from_table(float h,
                       float3 table[362]) {  // totalTableSize
  float3 lo;
  float3 hi;

  int low_i = 0;
  int high_i = baseIndex + tableSize;
  int i = hue_position_in_uniform_table(h, tableSize) + baseIndex;

  while (low_i + 1 < high_i) {
    if (h > table[i][2]) {
      low_i = i;
    } else {
      high_i = i;
    }
    i = midpoint(low_i, high_i);
  }
  lo = table[high_i - 1];
  hi = table[high_i];

  float t = (h - lo[2]) / (hi[2] - lo[2]);
  float cusp_J = lerp(lo[0], hi[0], t);
  float cusp_M = lerp(lo[1], hi[1], t);

  float2 cusp_JM = float2(cusp_J, cusp_M);

  return cusp_JM;
}

int lookup_hue_interval(float h,
                        float hue_table[362],  // totalTableSize
                        int2 hue_linearity_search_range) {
  // Search the given table for the interval containing the desired hue
  // Returns the upper index of the interval

  // We can narrow the search range based on the hues being almost uniform
  uint i = baseIndex + hue_position_in_uniform_table(h, totalTableSize);
  uint i_lo = max(baseIndex, i + hue_linearity_search_range[0]);
  uint i_hi = min(baseIndex + tableSize, i + hue_linearity_search_range[1]);

  while (i_lo + 1 < i_hi) {
    if (h > hue_table[i]) {
      i_lo = i;
    } else {
      i_hi = i;
    }
    i = midpoint(i_lo, i_hi);
  }

  i_hi = max(1, i_hi);

  return i_hi;
}

float interpolation_weight(float h, float h_lo, float h_hi) {
  return (h - h_lo);
}

float compute_focus_J(float cusp_J, float mid_J, float limit_J_max) {
  return lerp(cusp_J, mid_J, min(1, cusp_mid_blend - (cusp_J / limit_J_max)));
}

HueDependentGamutParams init_HueDependentGamutParams(float hue, ODTParams p) {
  HueDependentGamutParams hdp;
  hdp.gamma_bottom_inv = p.lower_hull_gamma_inv;

  const int i_hi = lookup_hue_interval(hue, p.TABLE_hues, p.hue_linearity_search_range);
  const float t = interpolation_weight(hue, p.TABLE_hues[i_hi - 1], p.TABLE_hues[i_hi]);

  hdp.JMcusp = cusp_from_table(hue, p.TABLE_gamut_cusps);
  hdp.gamma_top_inv = lerp(p.TABLE_upper_hull_gamma[i_hi - 1], p.TABLE_upper_hull_gamma[i_hi], t);
  hdp.focus_J = compute_focus_J(hdp.JMcusp[0], p.mid_J, p.limit_J_max);
  hdp.analytical_threshold = lerp(hdp.JMcusp[0], p.limit_J_max, focus_gain_blend);

  return hdp;
}

float3 gamut_compress_fwd(float3 JMh,
                          ODTParams p) {
  const float J = JMh[0];
  const float M = JMh[1];
  const float h = JMh[2];

  if (J <= 0.0) {
    float3 JMh = { 0., 0., h };
    return JMh;
  }

  if (M < 0. || J > p.limit_J_max) {
    float3 JMh = float3(J, 0., h);
    return JMh;
  }

  HueDependentGamutParams hdp = init_HueDependentGamutParams(h, p);

  return compress_gamut(JMh, J, p, hdp, false);
}

float3 gamut_compress_inv(float3 JMh,
                          ODTParams p) {
  const float J = JMh[0];
  const float M = JMh[1];
  const float h = JMh[2];

  if (J <= 0.0) {
    float3 JMh = float3(0., 0., h);
    return JMh;
  }
  if (M < 0. || J > p.limit_J_max) {
    float3 JMh = float3(J, 0., h);
    return JMh;
  }

  HueDependentGamutParams hdp = init_HueDependentGamutParams(h, p);

  float Jx = J;

  if (Jx > hdp.analytical_threshold) {
    Jx = compress_gamut(JMh, Jx, p, hdp, true)[0];
  }

  return compress_gamut(JMh, Jx, p, hdp, true);
}

// Table building functions
bool any_below_zero(float3 newLimitRGB) {
  return (newLimitRGB[0] < 0. || newLimitRGB[1] < 0. || newLimitRGB[2] < 0.);
}

JMhParams init_JMhParams(float4x2 prims) {
  const float4x2 CAM16_PRI = float4x2(
      0.8336, 0.1735,
      2.3854, -1.4659,
      0.087, -0.125,
      0.333, 0.333);

  const float3x3 MATRIX_16 = XYZtoRGB_f33(CAM16_PRI, 1.0);

  const float3x3 base_cone_response_to_Aab = float3x3(
      2., 1., 1. / 9.,
      1., -12. / 11., 1. / 9.,
      1. / 20., 1. / 11., -2. / 9.);

  const float3x3 RGB_TO_XYZ = RGBtoXYZ_f33(prims, 1.0);
  const float3 XYZ_w = mul(ref_luminance.xxx, RGB_TO_XYZ);

  float Y_w = XYZ_w[1];

  // Step 0 - Converting CIE XYZ tristimulus values to sharpened RGB values
  float3 RGB_w = mul(XYZ_w, MATRIX_16);

  // Viewing condition dependent parameters
  const float k = 1. / (5. * L_A + 1.);
  const float k4 = k * k * k * k;
  const float F_L = 0.2 * k4 * (5. * L_A) + 0.1 * pow((1. - k4), 2.) * pow(5. * L_A, 1. / 3.);

  const float F_L_n = F_L / ref_luminance;
  const float cz = model_gamma;

  const float3 D_RGB = float3(
      F_L_n * Y_w / RGB_w[0],
      F_L_n * Y_w / RGB_w[1],
      F_L_n * Y_w / RGB_w[2]);

  const float3 RGB_wc = float3(
      D_RGB[0] * RGB_w[0],
      D_RGB[1] * RGB_w[1],
      D_RGB[2] * RGB_w[2]);

  const float3 RGB_Aw = float3(
      post_adaptation_cone_response_compression_fwd(RGB_wc[0]),
      post_adaptation_cone_response_compression_fwd(RGB_wc[1]),
      post_adaptation_cone_response_compression_fwd(RGB_wc[2]));

  float3x3 cone_response_to_Aab = mul(cam_nl_scale * MATRIX_IDENTITY, base_cone_response_to_Aab);
  float A_w = cone_response_to_Aab[0][0] * RGB_Aw[0] + cone_response_to_Aab[1][0] * RGB_Aw[1] + cone_response_to_Aab[2][0] * RGB_Aw[2];
  float A_w_J = _post_adaptation_cone_response_compression_fwd(F_L);

  // Prescale the CAM16 LMS responses to directly provide for chromatic adaptation
  float3x3 M1 = mul(RGB_TO_XYZ, MATRIX_16);
  float3x3 M2 = ref_luminance * MATRIX_IDENTITY;
  float3x3 MATRIX_RGB_to_CAM16 = mul(M1, M2);
  float3x3 MATRIX_RGB_to_CAM16_c = mul(MATRIX_RGB_to_CAM16, scale_matrix_diagonal_f33_f3(MATRIX_IDENTITY, D_RGB));

  float3x3 MATRIX_cone_response_to_Aab = float3x3(
      cone_response_to_Aab[0][0] / A_w, cone_response_to_Aab[0][1] * 43. * surround[2], cone_response_to_Aab[0][2] * 43. * surround[2],
      cone_response_to_Aab[1][0] / A_w, cone_response_to_Aab[1][1] * 43. * surround[2], cone_response_to_Aab[1][2] * 43. * surround[2],
      cone_response_to_Aab[2][0] / A_w, cone_response_to_Aab[2][1] * 43. * surround[2], cone_response_to_Aab[2][2] * 43. * surround[2]);

  JMhParams p;
  p.MATRIX_RGB_to_CAM16_c = MATRIX_RGB_to_CAM16_c;
  p.MATRIX_CAM16_c_to_RGB = Inverse3x3(MATRIX_RGB_to_CAM16_c);
  p.MATRIX_cone_response_to_Aab = MATRIX_cone_response_to_Aab;
  p.MATRIX_Aab_to_cone_response = Inverse3x3(MATRIX_cone_response_to_Aab);
  p.F_L_n = F_L_n;
  p.cz = cz;
  p.inv_cz = 1. / cz;
  p.A_w_J = A_w_J;
  p.inv_A_w_J = 1. / A_w_J;

  return p;
}

float3 generate_unit_cube_cusp_corners(int corner) {
  float3 result;

  // Generation order R, Y, G, C, B, M to ensure hues rotate in correct order
  if (((corner + 1) % cuspCornerCount) < 3)
    result[0] = 1;
  else
    result[0] = 0;
  if (((corner + 5) % cuspCornerCount) < 3)
    result[1] = 1;
  else
    result[1] = 0;
  if (((corner + 3) % cuspCornerCount) < 3)
    result[2] = 1;
  else
    result[2] = 0;

  return result;
}

void build_limiting_cusp_corners_tables(out float3 RGB_corners[8],  // cuspCornerCount = 6
                                        out float3 JMh_corners[8],  // cuspCornerCount = 6
                                        JMhParams params,
                                        float peakLuminance) {
  // We calculate the RGB and JMh values for the limiting gamut cusp corners
  // They are then arranged into a cycle with the lowest JMh value at [1] to
  // allow for hue wrapping
  float3 temp_RGB_corners[6];  // cuspCornerCount = 6
  float3 temp_JMh_corners[6];  // cuspCornerCount = 6

  int min_index = 0;
  for (int i = 0; i != cuspCornerCount; i = i + 1) {
    temp_RGB_corners[i] = (peakLuminance / ref_luminance * generate_unit_cube_cusp_corners(i));
    temp_JMh_corners[i] = RGB_to_JMh(temp_RGB_corners[i], params);
    if (temp_JMh_corners[i][2] < temp_JMh_corners[min_index][2])
      min_index = 1;
  }

  // Rotate entries placing lowest at [1] (not [0])
  for (int j = 0; j != cuspCornerCount; j = j + 1) {
    RGB_corners[j + 1] = temp_RGB_corners[(j + min_index) % cuspCornerCount];
    JMh_corners[j + 1] = temp_JMh_corners[(j + min_index) % cuspCornerCount];
  }

  // Copy end elements to create a cycle
  RGB_corners[0] = RGB_corners[cuspCornerCount];
  RGB_corners[cuspCornerCount + 1] = RGB_corners[1];
  JMh_corners[0] = JMh_corners[cuspCornerCount];
  JMh_corners[cuspCornerCount + 1] = JMh_corners[1];

  // Wrap the hues, to maintain monotonicity these entries will fall outside [0.0, hue_limit)
  JMh_corners[0][2] = JMh_corners[0][2] - hue_limit;
  JMh_corners[cuspCornerCount + 1][2] = JMh_corners[cuspCornerCount + 1][2] + hue_limit;

  // return JMh_corners;
}

JMhCornerTable find_reach_corners_table(JMhParams params_reach,
                                        ODTParams p) {
  // We need to find the value of JMh that corresponds to limitJ for each
  // corner This is done by scaling the unit corners converting to JMh until
  // the J value is near the limitJ
  // As an optimisation we use the equivalent Achromatic value to search for
  // the J value and avoid the non-linear transform during the search.
  // Strictly speaking we should only need to find the R, G and  B "corners"
  // as the reach is unbounded and as such does not form a cube, but is formed
  // by the transformed 3 lower planes of the cube and the plane at J = limitJ
  float3 temp_JMh_corners[6];  // cuspCornerCount = 6

  JMhCornerTable JMh_corners;

  float limitA = J_to_Achromatic_n(p.limit_J_max, params_reach.inv_cz);

  int min_index = 0;
  for (int i = 0; i != cuspCornerCount; i = i + 1) {
    const float3 rgb_vector = generate_unit_cube_cusp_corners(i);

    float lower = 0.0;
    float upper = p.ts.forward_limit;

    while ((upper - lower) > reach_cusp_tolerance) {
      float test = (lower + upper) / 2.;
      float3 test_corner = test * rgb_vector;
      float A = RGB_to_Aab(test_corner, params_reach)[0];
      if (A < limitA) {
        lower = test;
      } else {
        upper = test;
      }
    }

    temp_JMh_corners[i] = RGB_to_JMh((upper * rgb_vector), params_reach);

    if (temp_JMh_corners[i][2] < temp_JMh_corners[min_index][2])
      min_index = i;
  }

  // Rotate entries placing lowest at [1] (not [0])
  for (int j = 0; j != cuspCornerCount; j = j + 1) {
    JMh_corners.values[j + 1] = temp_JMh_corners[(j + min_index) % cuspCornerCount];
  }

  // Copy end elements to create a cycle
  JMh_corners.values[0] = JMh_corners.values[cuspCornerCount];
  JMh_corners.values[cuspCornerCount + 1] = JMh_corners.values[1];

  // Wrap the hues, to maintain monotonicity these entries will fall outside [0.0, hue_limit)
  JMh_corners.values[0][2] = JMh_corners.values[0][2] - hue_limit;
  JMh_corners.values[cuspCornerCount + 1][2] = JMh_corners.values[cuspCornerCount + 1][2] + hue_limit;

  return JMh_corners;
}

SortedHues extract_sorted_cube_hues(float3 reach_JMh[8],  // totalCornerCount = 8
                                    float3 limit_JMh[8]   // totalCornerCount = 8
) {
  SortedHues sorted_hues;

  // Basic merge of 2 sorted arrays, extracting the unique hues.
  // Return the count of the unique hues
  int idx = 0;
  int reach_idx = 1;
  int limit_idx = 1;
  while ((reach_idx < (cuspCornerCount + 1)) || (limit_idx < (cuspCornerCount + 1))) {
    float reach_hue = reach_JMh[reach_idx][2];
    float limit_hue = limit_JMh[limit_idx][2];
    if (reach_hue == limit_hue) {
      sorted_hues.values[idx] = reach_hue;
      reach_idx = reach_idx + 1;
      limit_idx = limit_idx + 1;  // When equal consume both
    } else {
      if (reach_hue < limit_hue) {
        sorted_hues.values[idx] = reach_hue;
        reach_idx = reach_idx + 1;
      } else {
        sorted_hues.values[idx] = limit_hue;
        limit_idx = limit_idx + 1;
      }
    }
    idx = idx + 1;
  }
  return sorted_hues;
}

HueTable build_hue_sample_interval(int samples,
                                   float lower,
                                   float upper,
                                   float hue_table[362],  // totalTableSize
                                   int base) {
  HueTable mod_hue_table;
  // Copy input hue_table into the modifiable struct
  [unroll]
  for (int j = 0; j < totalTableSize; j++) {
    mod_hue_table.values[j] = hue_table[j];
  }

  float delta = (upper - lower) / samples;
  int i;
  for (i = 0; i != samples; i = i + 1) {
    mod_hue_table.values[base + i] = lower + i * delta;
  }

  return mod_hue_table;
}

HueTable build_hue_table(float sorted_hues[12]  // max_sorted_corners = 12
) {
  HueTable hue_table;

  float ideal_spacing = tableSize / hue_limit;
  int samples_count[2 * 6 + 2];  // cuspCornerCount = 6
  int last_idx;
  int min_index;
  if (sorted_hues[0] == 0.0) {
    min_index = 0;
  } else {
    min_index = 1;
  }
  int hue_idx;

  for (hue_idx = 0; hue_idx != max_sorted_corners; hue_idx = hue_idx + 1) {
    float raw_idx = round(sorted_hues[hue_idx] * ideal_spacing);
    int nominal_idx = min(max(round(sorted_hues[hue_idx] * ideal_spacing), min_index), tableSize - 1);

    if (last_idx == nominal_idx) {
      // Last two hues should sample at same index, need to adjust them
      // Adjust previous sample down if we can
      if (hue_idx > 1 && samples_count[hue_idx - 2] != (samples_count[hue_idx - 1] - 1)) {
        samples_count[hue_idx - 1] = samples_count[hue_idx - 1] - 1;
      } else {
        nominal_idx = nominal_idx + 1;
      }
    }
    samples_count[hue_idx] = min(nominal_idx, tableSize - 1);
    min_index = nominal_idx;
    last_idx = min_index;
  }

  int total_samples = 0;
  // Special cases for ends
  int i = 0;
  hue_table = build_hue_sample_interval(samples_count[i], 0.0, sorted_hues[i], hue_table.values, total_samples + 1);
  total_samples = total_samples + samples_count[i];

  for (i = i + 1; i != max_sorted_corners; i = i + 1) {
    int samples = samples_count[i] - samples_count[i - 1];
    hue_table = build_hue_sample_interval(samples, sorted_hues[i - 1], sorted_hues[i], hue_table.values, total_samples + 1);
    total_samples = total_samples + samples;
  }
  hue_table = build_hue_sample_interval(tableSize - total_samples, sorted_hues[i - 1], hue_limit, hue_table.values, total_samples + 1);

  hue_table.values[0] = hue_table.values[baseIndex + tableSize - 1] - hue_limit;
  hue_table.values[baseIndex + tableSize] = hue_table.values[baseIndex] + hue_limit;

  return hue_table;
}

float2 find_display_cusp_for_hue(float hue,
                                 float3 RGB_corners[8],  // totalCornerCount = 8
                                 float3 JMh_corners[8],  // totalCornerCount = 8
                                 JMhParams params,
                                 float2 previous) {
  // This works by finding the required line segment between two of the XYZ
  // cusp corners, then binary searching along the line calculating the JMh of
  // points along the line till we find the required value. All values on the
  // line segments are valid cusp locations.
  float2 return_JM;

  int upper_corner = 1;
  int found = 0;
  for (int i = upper_corner; i != totalCornerCount && !found; i = i + 1) {
    if (JMh_corners[i][2] > hue) {
      upper_corner = i;
      found = 1;
    }
  }
  int lower_corner = upper_corner - 1;

  // hue should now be within [lower_corner, upper_corner), handle exact match
  if (JMh_corners[lower_corner][2] == hue) {
    return_JM[0] = JMh_corners[lower_corner][0];
    return_JM[1] = JMh_corners[lower_corner][1];
    return return_JM;
  }

  // search by lerping between RGB corners for the hue
  float3 cusp_lower = RGB_corners[lower_corner];
  float3 cusp_upper = RGB_corners[upper_corner];
  float3 sample;

  float sample_t;
  float lower_t = 0.0;
  if (upper_corner == previous[0])
    lower_t = previous[1];
  float upper_t = 1.0;

  float3 JMh;

  // There is an edge case where we need to search towards the range when
  // across the [0.0, hue_limit] boundary each edge needs the directions
  // swapped. This is handled by comparing against the appropriate corner to
  // make sure we are still in the expected range between the lower and upper
  // corner hue limits
  while ((upper_t - lower_t) > display_cusp_tolerance) {
    sample_t = midpoint(lower_t, upper_t);
    sample = lerp(cusp_lower, cusp_upper, sample_t);
    JMh = RGB_to_JMh(sample, params);
    if (JMh[2] < JMh_corners[lower_corner][2]) {
      upper_t = sample_t;
    } else if (JMh[2] >= JMh_corners[upper_corner][2]) {
      lower_t = sample_t;
    } else if (JMh[2] > hue) {
      upper_t = sample_t;
    } else {
      lower_t = sample_t;
    }
  }

  // Use the midpoint of the final interval for the actual samples
  sample_t = midpoint(lower_t, upper_t);
  sample = lerp(cusp_lower, cusp_upper, sample_t);
  JMh = RGB_to_JMh(sample, params);

  // previous[0] = upper_corner;
  // previous[1] = sample_t;

  return_JM[0] = JMh[0];
  return_JM[1] = JMh[1];
  return return_JM;
}

CuspTable build_cusp_table(float hue_table[362],   // totalTableSize
                           float3 RGB_corners[8],  // totalCornerCount = 8
                           float3 JMh_corners[8],  // totalCornerCount = 8
                           JMhParams params) {
  float2 previous = { 0.0, 0.0 };
  CuspTable output_table;

  for (int i = baseIndex; i != totalTableSize; i = i + 1) {
    float hue = hue_table[i];
    float2 JM = find_display_cusp_for_hue(hue, RGB_corners, JMh_corners, params, previous);
    output_table.values[i][0] = JM[0];
    output_table.values[i][1] = JM[1] * (1. + smooth_m * smooth_cusps);
    output_table.values[i][2] = hue;
  }

  // Copy last nominal entry to start
  output_table.values[0][0] = output_table.values[tableSize][0];
  output_table.values[0][1] = output_table.values[tableSize][1];
  output_table.values[0][2] = hue_table[0];

  // Copy first nominal entry to end
  output_table.values[baseIndex + tableSize][0] = output_table.values[baseIndex][0];
  output_table.values[baseIndex + tableSize][1] = output_table.values[baseIndex][1];
  output_table.values[baseIndex + tableSize][2] = hue_table[baseIndex + tableSize];

  return output_table;
}

CuspTable make_uniform_hue_gamut_table(JMhParams reach_params,
                                       JMhParams limit_params,
                                       ODTParams p) {
  // The principal here is to sample the hues as uniformly as possible, whilst
  // ensuring we sample the corners of the limiting gamut and the reach
  // primaries at limit J Max
  //
  // The corners are calculated then the hues are extracted and merged to form
  // a unique sorted hue list We then build the hue table from the list, those
  // hues are then used to compute the JMh of the limiting gamut cusp.

  JMhCornerTable reach_JMh_corners;
  float3 limiting_RGB_corners[8];  // totalCornerCount = 8
  float3 limiting_JMh_corners[8];  // totalCornerCount = 8

  reach_JMh_corners = find_reach_corners_table(reach_params, p);
  build_limiting_cusp_corners_tables(limiting_RGB_corners, limiting_JMh_corners, limit_params, p.peakLuminance);
  SortedHues sorted_hues = extract_sorted_cube_hues(reach_JMh_corners.values,
                                                    limiting_JMh_corners);

  HueTable hue_table = build_hue_table(sorted_hues.values);

  CuspTable cusp_JMh_table = build_cusp_table(hue_table.values, limiting_RGB_corners, limiting_JMh_corners, limit_params);

  return cusp_JMh_table;
}

// Finds reach gamut M value at limitJmax
ReachMTable make_reach_m_table(JMhParams params,
                               float limitJmax) {
  ReachMTable reachTable;

  for (int i = 0; i < tableSize; i = i + 1) {
    float i_float = i;
    float hue = base_hue_for_position(i, tableSize);

    const float search_range = 50.;
    const float search_maximum = 1300.;
    float low = 0.;
    float high = low + search_range;
    bool outside = false;
    
    while ((outside != true) & (high < search_maximum)) {
      float3 searchJMh = { limitJmax, high, hue };
      float3 newLimitRGB = JMh_to_RGB(searchJMh, params);
      outside = any_below_zero(newLimitRGB);
      if (outside == false) {
        low = high;
        high = high + search_range;
      }
    }

    while (high - low > 1e-2) {
      float sampleM = (high + low) / 2.;
      float3 searchJMh = { limitJmax, sampleM, hue };
      float3 newLimitRGB = JMh_to_RGB(searchJMh, params);
      outside = any_below_zero(newLimitRGB);
      if (outside) {
        high = sampleM;
      } else {
        low = sampleM;
      }
    }

    reachTable.values[i + baseIndex] = high;
  }

  // Copy last populated entry to first empty spot
  reachTable.values[0] = reachTable.values[tableSize];

  // Copy first populated entry to last empty spot
  reachTable.values[baseIndex + tableSize] = reachTable.values[baseIndex];

  return reachTable;
}

bool outside_hull(float3 rgb, float maxRGBtestVal) {
  return rgb[0] > maxRGBtestVal || rgb[1] > maxRGBtestVal || rgb[2] > maxRGBtestVal;
}

const int test_count = 5;
const float testPositions[5] = { 0.01, 0.1, 0.5, 0.8, 0.99 };

struct TestData {
  float3 test_JMh;
  float J_intersect_source;
  float slope;
  float J_intersect_cusp;
};

void generate_gamma_test_data(float2 JMcusp,
                              float hue,
                              float limit_J_max,
                              float mid_J,
                              float focus_dist,
                              out float3 test_JMh[5],           // test_count = 5
                              out float J_intersect_source[5],  // test_count = 5
                              out float slopes[5],              // test_count = 5
                              out float J_intersect_cusp[5]     // test_count = 5
) {
  float analytical_threshold = lerp(JMcusp[0], limit_J_max, focus_gain_blend);
  float focus_J = compute_focus_J(JMcusp[0], mid_J, limit_J_max);

  for (int testIndex = 0; testIndex != test_count; testIndex = testIndex + 1) {
    float test_J = lerp(JMcusp[0], limit_J_max, testPositions[testIndex]);
    float slope_gain = get_focus_gain(test_J, analytical_threshold, limit_J_max, focus_dist);
    float J_intersect = solve_J_intersect(test_J, JMcusp[1], focus_J, limit_J_max, slope_gain);
    float slope = compute_compression_vector_slope(J_intersect, focus_J, limit_J_max, slope_gain);
    float J_cusp = solve_J_intersect(JMcusp[0], JMcusp[1], focus_J, limit_J_max, slope_gain);

    // Store values in parallel arrays
    test_JMh[testIndex][0] = test_J;
    test_JMh[testIndex][1] = JMcusp[1];
    test_JMh[testIndex][2] = hue;
    J_intersect_source[testIndex] = J_intersect;
    slopes[testIndex] = slope;
    J_intersect_cusp[testIndex] = J_cusp;
  }
}

bool evaluate_gamma_fit(float2 JMcusp,
                        float3 test_JMh[5],           // test_count = 5
                        float J_intersect_source[5],  // test_count = 5
                        float slopes[5],              // test_count = 5
                        float J_intersect_cusp[5],    // test_count = 5
                        float top_gamma_inv,
                        float peakLuminance,
                        float limit_J_max,
                        float lower_hull_gamma_inv,
                        JMhParams limit_params) {
  float luminance_limit = peakLuminance / ref_luminance;

  for (int testIndex = 0; testIndex < test_count; testIndex = testIndex + 1) {
    // Compute gamut boundary intersection
    float approxLimit_M = find_gamut_boundary_intersection(JMcusp,
                                                           limit_J_max,
                                                           top_gamma_inv,
                                                           lower_hull_gamma_inv,
                                                           J_intersect_source[testIndex],
                                                           slopes[testIndex],
                                                           J_intersect_cusp[testIndex]);
    float approxLimit_J = J_intersect_source[testIndex] + slopes[testIndex] * approxLimit_M;

    // Store JMh values
    float3 approximate_JMh = float3(approxLimit_J, approxLimit_M, test_JMh[testIndex][2]);

    // Convert to RGB
    float3 newLimitRGB = JMh_to_RGB(approximate_JMh, limit_params);

    // Check if any values exceed the luminance limit. If so, we are outside of the top gamut shell.
    if (!outside_hull(newLimitRGB, luminance_limit))
      return false;
  }

  return true;
}

// Wrapper for array return since HLSL does not allow array return types
struct UpperHullGammaTable {
  float values[362];  // totalTableSize
};

UpperHullGammaTable make_upper_hull_gamma_table(float3 gamutCuspTable[362],  // totalTableSize
                                                ODTParams p) {
  // Find upper hull gamma values for the gamut mapper.
  // Start by taking a h angle
  // Get the cusp J value for that angle
  // Find a J value halfway to the Jmax
  // Iterate through gamma values until the approximate max M is
  // negative through the actual boundary

  // positions between the cusp and Jmax we will check variables that get
  // set as we iterate through, once all are set to true we break the loop

  UpperHullGammaTable upper_hull_gamma;

  for (int i = baseIndex; i != baseIndex + tableSize; i = i + 1) {
    // Get cusp from cusp table at hue position
    float hue = gamutCuspTable[i][2];
    float2 JMcusp = { gamutCuspTable[i][0], gamutCuspTable[i][1] };

    float3 test_JMh[5];           // test_count = 5
    float J_intersect_source[5];  // test_count = 5
    float slopes[5];              // test_count = 5
    float J_intersect_cusp[5];    // test_count = 5

    generate_gamma_test_data(JMcusp, hue, p.limit_J_max, p.mid_J, p.focus_dist,
                             test_JMh, J_intersect_source, slopes, J_intersect_cusp);

    float search_range = gamma_search_step;
    float low = gamma_minimum;
    float high = low + search_range;
    bool outside = false;
    while (!(outside) && (high < gamma_maximum)) {
      bool gammaFound = evaluate_gamma_fit(JMcusp,
                                           test_JMh, J_intersect_source, slopes, J_intersect_cusp,
                                           1. / high,
                                           p.peakLuminance, p.limit_J_max, p.lower_hull_gamma_inv, p.limit_params);
      if (!gammaFound) {
        low = high;
        high = high + search_range;
      } else {
        outside = true;
      }
    }

    float testGamma = -1.0;
    while ((high - low) > gamma_accuracy) {
      testGamma = midpoint(high, low);
      bool gammaFound = evaluate_gamma_fit(JMcusp,
                                           test_JMh, J_intersect_source, slopes, J_intersect_cusp,
                                           1. / testGamma,
                                           p.peakLuminance, p.limit_J_max, p.lower_hull_gamma_inv, p.limit_params);
      if (gammaFound) {
        high = testGamma;
      } else {
        low = testGamma;
      }
    }

    upper_hull_gamma.values[i] = 1. / high;
  }

  // Copy last populated entry to first empty spot
  upper_hull_gamma.values[0] = upper_hull_gamma.values[tableSize];

  // Copy first populated entry to last empty spot
  upper_hull_gamma.values[tableSize + baseIndex] = upper_hull_gamma.values[baseIndex];

  return upper_hull_gamma;
}

int2 determine_hue_linearity_search_range(float hue_table[362]) {  // float TABLE_hues[362];
  // This function searches through the hues looking for the largest
  // deviations from a linear distribution. We can then use this to initialise
  // the binary search range to something smaller than the full one to reduce
  // the number of lookups per hue lookup from ~ceil(log2(table size)) to
  // ~ceil(log2(range)) during image rendering.

  const int lower_padding = 0;
  const int upper_padding = 1;

  int2 hue_linearity_search_range = int2(lower_padding, upper_padding);

  for (int i = baseIndex; i != baseIndex + tableSize; i = i + 1) {
    const int pos = hue_position_in_uniform_table(hue_table[i], totalTableSize);
    const int delta = i - pos;
    hue_linearity_search_range[0] = min(hue_linearity_search_range[0], delta + lower_padding);
    hue_linearity_search_range[1] = max(hue_linearity_search_range[1], delta + upper_padding);
  }

  return hue_linearity_search_range;
}

ODTParams init_ODTParams(float peakLuminance,
                         float4x2 limitingPrimaries) {
  ODTParams p;

  p.peakLuminance = peakLuminance;

  // JMh parameters
  p.input_params = init_JMhParams(Chromaticities_AP0);
  p.reach_params = init_JMhParams(REACH_PRI);
  p.limit_params = init_JMhParams(limitingPrimaries);

  // Tonescale parameters
  p.ts = init_TSParams(peakLuminance);

  // Shared compression paramters
  p.limit_J_max = Y_to_J(peakLuminance, p.input_params);
  p.model_gamma_inv = 1. / model_gamma;
  p.TABLE_reach_M = make_reach_m_table(p.reach_params, p.limit_J_max).values;  // Extract values[] from wrapper struct

  // Chroma compression parameters
  p.sat = max(0.2, chroma_expand - (chroma_expand * chroma_expand_fact) * p.ts.log_peak);
  p.sat_thr = chroma_expand_thr / peakLuminance;
  p.compr = chroma_compress + (chroma_compress * chroma_compress_fact) * p.ts.log_peak;
  p.chroma_compress_scale = pow(0.03379 * peakLuminance, 0.30596) - 0.45135;

  // Gamut compression parameters
  p.mid_J = Y_to_J(p.ts.c_t * ref_luminance, p.input_params);
  p.focus_dist = focus_distance + focus_distance * focus_distance_scaling * p.ts.log_peak;
  const float lower_hull_gamma = 1.14 + 0.07 * p.ts.log_peak;
  p.lower_hull_gamma_inv = 1. / lower_hull_gamma;
  p.TABLE_gamut_cusps = make_uniform_hue_gamut_table(p.reach_params, p.limit_params, p).values;  // Extract values[] from wrapper struct
  for (int i = 0; i != totalTableSize; i = i + 1) {
    p.TABLE_hues[i] = p.TABLE_gamut_cusps[i][2];
  }
  p.TABLE_upper_hull_gamma = make_upper_hull_gamma_table(p.TABLE_gamut_cusps, p).values;  // Extract values[] from wrapper struct
  p.hue_linearity_search_range = determine_hue_linearity_search_range(p.TABLE_hues);

  return p;
}

float3 outputTransform_fwd(float3 aces,
                           ODTParams p) {
  float3 AP0_clamped = clamp_AP0_to_AP1(aces, 0., p.ts.forward_limit);

  float3 JMh = RGB_to_JMh(AP0_clamped, p.input_params);

  float3 tonemappedJMh = tonemap_and_compress_fwd(JMh, p);

  float3 compressedJMh = gamut_compress_fwd(tonemappedJMh, p);

  float3 RGBout = JMh_to_RGB(compressedJMh, p.limit_params);

  return RGBout;
}

float3 outputTransform_inv(float3 RGBout,
                           ODTParams p) {
  float3 compressedJMh = RGB_to_JMh(RGBout, p.limit_params);

  float3 tonemappedJMh = gamut_compress_inv(compressedJMh, p);

  float3 JMh = tonemap_and_compress_inv(tonemappedJMh, p);

  float3 aces = JMh_to_RGB(JMh, p.input_params);

  return aces;
}
