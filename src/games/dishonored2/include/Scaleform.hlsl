#include "./Common.hlsl"

// 0) Raw linear blend instead of sRGB gamma blend (looks very different from Vanilla).
// 1) Apply pow on blend alpha (luminance based). Looks better than linear blends, though results vary on the background brightness.
// 2) DEPRECATED: Apply pow on blend alpha (percentage based).
// 3) Pre-blend against mid gray (alpha based): attempts to pre-blend with a mid gray background to better predict the color shift from sRGB gamma blends.
//    This generally looks best and closest to sRGB gamma blends, though results vary on the background brightness.
// 4) DEPRECATED: Pre-blend against mid gray (rgb based).
#define UI_EMULATED_COMPOSITION_TYPE 4
//TODOFT1: clean up...

cbuffer PER_BATCH : register(b0)
{
  row_major float2x4 cBitmapColorTransform : packoffset(c0);
  float cPremultiplyAlpha : packoffset(c2);
}

float4 AdjustForMultiply( float4 col )
{
	return lerp( float4( 1, 1, 1, 1 ), col, col.a );
}

//TODOFT: make sure this isn't ever called on a render target that isn't the swapchain. Seems to be so... And also that there's no other UI shaders? And also that all set the blend state to pre-multiplied alpha
float4 PremultiplyAlpha( float4 col )
{
#if !ENABLE_UI //TODOFT: some of these are still missing
	return 0;
#endif

#if 1 // LUMA FT: custom implementation to linearize the UI and scale its brightness

#if POST_PROCESS_SPACE_TYPE == 1

#if UI_EMULATED_COMPOSITION_TYPE > 0

	float4 UIColorGammaSpace = col;
	float4 UIColorLinearSpace = float4(game_gamma_to_linear_mirrored(UIColorGammaSpace.rgb), UIColorGammaSpace.a);

	float HDRUIBlendPow = 1.f / DefaultGamma;
#if UI_EMULATED_COMPOSITION_TYPE == 2
	HDRUIBlendPow = lerp(1.f, HDRUIBlendPow, 0.5);
	UIColorLinearSpace.a = pow(UIColorLinearSpace.a, HDRUIBlendPow);
	col.a = UIColorLinearSpace.a;
#elif UI_EMULATED_COMPOSITION_TYPE == 1
	// Base the alpha pow application percentage on the color luminance.
	// Generally speaking dark colors (with a low alpha) are meant as darkneing (transparent) backgrounds,
	// while brighter/whiter colors are meant to replace the background color directly (opaque),
	// so we could take that into account to avoid cases where the alpha pow would make stuff look worse.
	HDRUIBlendPow = lerp(HDRUIBlendPow, 1.f, saturate(linear_to_game_gamma_mirrored(GetLuminance(UIColorLinearSpace.xyz))));
	UIColorLinearSpace.a = pow(UIColorLinearSpace.a, HDRUIBlendPow);
	col.a = UIColorLinearSpace.a;
#endif // UI_EMULATED_COMPOSITION_TYPE

	// Blend in in gamma space with the average color of the background (guessed to be 0.5, even if it's likely a lot lower than that, and finding the actual average pixel brightness of the game could yield better results),
	// then, with the pre-multiplied alpha inverse formula, find the new UI color with the average gamma space "offset" baked into it.
	static const float MidGrayBackgroundColorGammaSpace = 0.5f / 3.f; //TODOFT1: best value and make it mathematically nice. 0.5/3 seems good
	static const float MidGrayBackgroundColorLinearSpace = gamma_to_linear1(MidGrayBackgroundColorGammaSpace); // No need to use "game_gamma_to_linear_mirrored()" here
	const float3 linearBlendColorLinear = UIColorLinearSpace.rgb + (MidGrayBackgroundColorLinearSpace * (1.f - UIColorLinearSpace.a));
	const float3 gammaBlendColorLinear = game_gamma_to_linear_mirrored(UIColorGammaSpace.rgb + (MidGrayBackgroundColorGammaSpace * (1.f - UIColorLinearSpace.a)));
// Find the matching alpha (theoretically less correct but it looks better)
#if UI_EMULATED_COMPOSITION_TYPE == 3
	// Note: for now we let it possibly go < 0, as theoretically it's more accurate (I don't know if it can even happen, but probably not as it ends up adding alpha, not removing it)
	UIColorLinearSpace.a = -((average(gammaBlendColorLinear - UIColorLinearSpace.rgb) / MidGrayBackgroundColorLinearSpace) - 1.f);
	col.a = UIColorLinearSpace.a;
// Find the matching rgb color (theoretically more correct but it seems to cause issues with alpha blending, probably because rgb values go below 0)
#elif UI_EMULATED_COMPOSITION_TYPE == 4

#if 0
	UIColorLinearSpace.rgb = gammaBlendColorLinear - (MidGrayBackgroundColorLinearSpace * (1.f - UIColorLinearSpace.a));
	col.rgb = linear_to_game_gamma_mirrored(UIColorLinearSpace.rgb);
#elif 1 // New version
	// Pre-blend against mid gray in gamma space
    float3 GammaSpaceBlendedColorGammaSpace = (UIColorGammaSpace.rgb * UIColorGammaSpace.a) + (MidGrayBackgroundColorGammaSpace * (1.f - UIColorGammaSpace.a));
	float3 GammaSpaceBlendedColorLinearSpace = game_gamma_to_linear_mirrored(GammaSpaceBlendedColorGammaSpace);
    float3 LinearSpaceBlendedColorLinearSpace = (UIColorLinearSpace.rgb * UIColorLinearSpace.a) + (MidGrayBackgroundColorLinearSpace * (1.f - UIColorLinearSpace.a));
	float3 LinearSpaceBlendedColorGammaSpace = linear_to_game_gamma_mirrored(LinearSpaceBlendedColorLinearSpace);
	// Pre-bake in the rgb difference between blending against mid gray in gamma space and linear space, so that when we blend in against a color (that is likely close to mid gray),
	// it applies the inverse offset, and the result looks similar to gamma space blends (it might look further away from gamma space blends on pure black and pure white backgrounds, but it doesn't matter).
	if (UIColorGammaSpace.a != 0.0)
	{
		float3 UIColorEmulatedGammaSpaceBlendsLinearSpace = (GammaSpaceBlendedColorLinearSpace - (MidGrayBackgroundColorLinearSpace * (1.f - UIColorLinearSpace.a))) / UIColorLinearSpace.a;
#if 1
		// Clip out any color beyond sRGB. This theoretically produces less HDR colors, but emulates SDR gamma space blends more accurately, if we don't do this, some pixels will become overly dark.
		
		col.rgb = linear_to_game_gamma_mirrored(UIColorEmulatedGammaSpaceBlendsLinearSpace);
#if 1 //TODOFT2: pick best branch
		float luminance = GetLuminance(col.xyz);
		if (luminance < -FLT_MIN)
		{
			// Make the color more "SDR" (less saturated, and thus less beyond Rec.709) until the luminance is not negative anymore (negative luminance means the color was beyond Rec.709 to begin with, unless all components were negative).
			// This is preferrable to simply clipping all negative colors or snapping to black, because it keeps some HDR colors, even if overall it's still "black", luminance wise.
			// This should work even in case "positiveLuminance" was <= 0, as it will simply make the color black.
			float3 positiveColor = max(col.xyz, 0.0);
			float3 negativeColor = min(col.xyz, 0.0);
			float positiveLuminance = GetLuminance(positiveColor);
			float negativeLuminance = GetLuminance(negativeColor);
			float negativePositiveLuminanceRatio = positiveLuminance / -negativeLuminance;
			negativeColor.xyz *= negativePositiveLuminanceRatio;
			col.xyz = positiveColor + negativeColor;
		}
#elif 1
		col.rgb = max(col.rgb, 0.0);
#else // This removes the chroma background behind text
		// Increase brightness after clipping negative colors
		col.rgb = max(col.rgb, 0.0) * max((GetLuminance(col.rgb)) / GetLuminance(max(col.rgb, 0.0)), 0);
#endif
#else //TODOFT2: try with the alpha changing
		float AverageUIColorEmulatedGammaSpaceBlendsLinearSpace = UIColorEmulatedGammaSpaceBlendsLinearSpace;
		float EmulatedGammaSpaceBlendedAverageColorLinearSpace = (AverageUIColorEmulatedGammaSpaceBlendsLinearSpace * UIColorLinearSpace.a) + (MidGrayBackgroundColorLinearSpace * (1.f - UIColorLinearSpace.a));
		LinearSpaceBlendedColorLinearSpace = EmulatedGammaSpaceBlendedAverageColorLinearSpace;
		col.a = 1;
#endif
	}
#elif 1 // Easier to read version
    col.rgb += MidGrayBackgroundColorGammaSpace * (1.f - UIColorGammaSpace.a);
    col.rgb = game_gamma_to_linear_mirrored(col.rgb);
    col.rgb -= MidGrayBackgroundColorLinearSpace * (1.f - UIColorLinearSpace.a);
    col.rgb = linear_to_game_gamma_mirrored(col.rgb);
#endif

#endif // UI_EMULATED_COMPOSITION_TYPE

#endif // UI_EMULATED_COMPOSITION_TYPE

  //TODOFT: keep linear space instead of converting back and forth!?

#endif // POST_PROCESS_SPACE_TYPE == 1

	bool gammaSpace = true;
#if 0 // LUMA FT: try to linearize by luminance to keep the SDR color more??? Probably won't work (this loses a lot of color/saturation...)
    float lumOld = GetLuminance(col.rgb);
    float lumNew = game_gamma_to_linear_mirrored(lumOld.xxx).x;
    col.rgb *= safeDivision(lumNew, lumOld, 1);
	gammaSpace = false;
#endif
	// LUMA FT: to further keep the Vanilla look of the UI, we should theoretically pre-multiply it in gamma space after "cPremultiplyAlpha" is applied, but that's not in use by the game
	col = SDRToHDR(col, gammaSpace, false, true);

	// LUMA FT: this doesn't actually seem to be used by Prey, it simply sets the blend state to pre-multiplied alpha. The alpha of the background (render target) is ignored and never affects UI.
	if (cPremultiplyAlpha)
	{
		col = col * col.a;
#if TEST_UI
		col.rgba = float4(2, 0, 0, 1);
#endif
	}

#if 1 //TODOFT
	// avoid fade outs on hud tooltips causing a black after image to appear (???)
	col.a = saturate(col.a);
#endif

	return col;
#else

	return cPremultiplyAlpha ? col * col.a : col;
	
#endif
}