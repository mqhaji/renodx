#ifndef MGSV_FSR2_NATIVE_RESOLUTION_HLSLI
#define MGSV_FSR2_NATIVE_RESOLUTION_HLSLI

// Include after ffx_fsr2_callbacks_hlsl.h. Object-like aliases redirect later
// AMD algorithm calls to constant getters while retaining the cbuffer ABI.
float MgsvFsr2Exposure() { return 1.f; }
float MgsvFsr2PreExposure() { return 1.f; }
float MgsvFsr2PreviousFramePreExposure() { return 1.f; }
float2 MgsvFsr2DownscaleFactor() { return float2(1.f, 1.f); }
int2 MgsvFsr2RenderSizeAlias() { return RenderSize(); }
int MgsvFsr2LumaMipLevel() { return 4; }
float MgsvFsr2JitterSequenceLength() { return 8.f; }
float MgsvFsr2ViewSpaceToMetersFactor() { return 1.f; }

#define Exposure MgsvFsr2Exposure
#define PreExposure MgsvFsr2PreExposure
#define PreviousFramePreExposure MgsvFsr2PreviousFramePreExposure
#define DownscaleFactor MgsvFsr2DownscaleFactor
#define MaxRenderSize MgsvFsr2RenderSizeAlias
#define DisplaySize MgsvFsr2RenderSizeAlias
#define InputColorResourceDimensions MgsvFsr2RenderSizeAlias
#define LumaMipLevelToUse MgsvFsr2LumaMipLevel
#define JitterSequenceLength MgsvFsr2JitterSequenceLength
#define ViewSpaceToMetersFactor MgsvFsr2ViewSpaceToMetersFactor

#endif  // MGSV_FSR2_NATIVE_RESOLUTION_HLSLI