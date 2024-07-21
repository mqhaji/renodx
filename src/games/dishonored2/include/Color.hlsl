// SDR linear mid gray.
// This is based on the commonly used value, though perception space mid gray (0.5) in sRGB or Gamma 2.2 would theoretically be ~0.2155 in linear.
static const float MidGray = 0.18f;
static const float DefaultGamma = 2.2f;
static const float3 Rec709_Luminance = float3( 0.2126f, 0.7152f, 0.0722f );
static const float HDR10_MaxWhiteNits = 10000.0f;
static const float ITU_WhiteLevelNits = 203.0f;
static const float sRGB_WhiteLevelNits = 80.0f;

static const float3x3 BT709_2_XYZ = float3x3
  (0.412390798f,  0.357584327f, 0.180480793f,
   0.212639003f,  0.715168654f, 0.0721923187f, // ~same as "Rec709_Luminance"
   0.0193308182f, 0.119194783f, 0.950532138f);

float GetLuminance( float3 color )
{
	return dot( color, Rec709_Luminance );
}

float3 linear_to_gamma(float3 Color, float Gamma = DefaultGamma)
{
	return pow(Color, 1.f / Gamma);
}

float3 linear_to_gamma_mirrored(float3 Color, float Gamma = DefaultGamma)
{
	return linear_to_gamma(abs(Color), Gamma) * sign(Color);
}

// 1 component
float gamma_to_linear1(float Color, float Gamma = DefaultGamma)
{
	return pow(Color, Gamma);
}

float3 gamma_to_linear(float3 Color, float Gamma = DefaultGamma)
{
	return pow(Color, Gamma);
}

float3 gamma_to_linear_mirrored(float3 Color, float Gamma = DefaultGamma)
{
	return gamma_to_linear(abs(Color), Gamma) * sign(Color);
}

float gamma_sRGB_to_linear(float channel)
{
	if (channel <= 0.04045f)
	{
		channel = channel / 12.92f;
	}
	else
	{
		channel = pow((channel + 0.055f) / 1.055f, 2.4f);
	}
	return channel;
}

float3 gamma_sRGB_to_linear(float3 Color)
{
	return float3(gamma_sRGB_to_linear(Color.r), gamma_sRGB_to_linear(Color.g), gamma_sRGB_to_linear(Color.b));
}

// The sRGB gamma formula already works beyond the 0-1 range but mirroring (and thus running the pow below 0 too makes it look better)
float3 gamma_sRGB_to_linear_mirrored(float3 Color)
{
	return gamma_sRGB_to_linear(abs(Color)) * sign(Color);
}

float linear_to_sRGB_gamma(float channel)
{
	if (channel <= 0.0031308f)
	{
		channel = channel * 12.92f;
	}
	else
	{
		channel = 1.055f * pow(channel, 1.f / 2.4f) - 0.055f;
	}
	return channel;
}

float3 linear_to_sRGB_gamma(float3 Color)
{
	return float3(linear_to_sRGB_gamma(Color.r), linear_to_sRGB_gamma(Color.g), linear_to_sRGB_gamma(Color.b));
}

// The sRGB gamma formula already works beyond the 0-1 range but mirroring (and thus running the pow below 0 too makes it look better)
float3 linear_to_sRGB_gamma_mirrored(float3 Color)
{
	return linear_to_sRGB_gamma(abs(Color)) * sign(Color);
}

// LUMA FT: avoid using this, use "linear_to_sRGB_gamma_mirrored()" instead
float3 LinearToSRGB(float3 col)
{
#if 0 // LUMA FT: disabled saturate(), it's unnecessary
	col = saturate(col);
#endif
#if 1
  return linear_to_sRGB_gamma(col);
#else // CryEngine version (compiles to the same code)
	return (col < 0.0031308) ? 12.92 * col : 1.055 * pow(col, 1.0 / 2.4) - float3(0.055, 0.055, 0.055);
#endif
}

// Optimized gamma<->linear functions (don't use unless really necessary, they are not accurate)
float3 sqr_mirrored(float3 x)
{
	return (x * x) * sign(x); // LUMA FT: added mirroring to support negative colors
}
float3 sqrt_mirrored(float3 x)
{
	return sqrt(abs(x)) * sign(x); // LUMA FT: added mirroring to support negative colors
}

static const float PQ_constant_M1 =  0.1593017578125f;
static const float PQ_constant_M2 = 78.84375f;
static const float PQ_constant_C1 =  0.8359375f;
static const float PQ_constant_C2 = 18.8515625f;
static const float PQ_constant_C3 = 18.6875f;

// PQ (Perceptual Quantizer - ST.2084) encode/decode used for HDR10 BT.2100
float3 Linear_to_PQ(float3 LinearColor, bool clampNegative = false)
{
  if (clampNegative)
  {
	  LinearColor = max(LinearColor, 0.f);
  }
	float3 colorPow = pow(LinearColor, PQ_constant_M1);
	float3 numerator = PQ_constant_C1 + PQ_constant_C2 * colorPow;
	float3 denominator = 1.f + PQ_constant_C3 * colorPow;
	float3 pq = pow(numerator / denominator, PQ_constant_M2);
	return pq;
}

float3 PQ_to_Linear(float3 ST2084Color, bool clampNegative = false)
{
  if (clampNegative)
  {
	  ST2084Color = max(ST2084Color, 0.f);
  }
	float3 colorPow = pow(ST2084Color, 1.f / PQ_constant_M2);
	float3 numerator = max(colorPow - PQ_constant_C1, 0.f);
	float3 denominator = PQ_constant_C2 - (PQ_constant_C3 * colorPow);
	float3 linearColor = pow(numerator / denominator, 1.f / PQ_constant_M1);
	return linearColor;
}