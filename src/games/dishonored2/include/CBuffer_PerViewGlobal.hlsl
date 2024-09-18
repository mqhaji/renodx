cbuffer CBPerViewGlobal : register(b13)
{
  // View/camera projection matrix but based around the camera zero (???).
  // It's unclear what that means exactly, this is not the same as "CV_ViewMatr" so it does have some additional transformation to it.
  // It also includes jitters.
  row_major float4x4 CV_ViewProjZeroMatr : packoffset(c0);
  float4 CV_AnimGenParams : packoffset(c4);
  // View/camera projection matrix.
  // This includes jitters until the anti aliasing passes start, at that point, jitters are removed from the matrix (not exactly sure why, but it worked for what they were doing with TAA, and it might have been to avoid jittering lens effects and sun shafts).
  // If you have a world space position (relative to the camera), you can reproject it to where it used to be on screen in the last frame, like "float3 screenPosition = mul(CV_ViewProjMatr, float4(worldPosition, 1))".
  row_major float4x4 CV_ViewProjMatr : packoffset(c5);
  // Like "CV_ViewProjZeroMatr", but based on the nearest projection matrix (not sure how that compares with the "normal" (non nearest) projection matrix). Also includes jitters.
  row_major float4x4 CV_ViewProjNearestMatr : packoffset(c9);
  row_major float4x4 CV_InvViewProj : packoffset(c13);
  // LUMA FT: in Prey, this matrix is "wrongly named" and it's set equal to the (current) projection matrix (all the times, except for shadow projection maps draw calls).
  // In vanilla CryEngine, it was set "properly" to the previou frame "view/camera projection matrix" ("CV_ViewProjMatr"), except it used the previous frame "view/camera matrix" combined with the current frame "projection matrix",
  // meaning that it wouldn't acknowledge the jitters difference, nor any change in FOV.
  // Yet somehow, in Prey, motion vectors (which are based on this), manage to be generated correctly, even if it uses a different matrix from vanilla CryEngine (despite the MVs generation code being ~identical).
  // Supposedly they've also replaced and misnamed other variables in shaders so it all works out even if reading the code makes no sense.
  // With Luma hooks, we have kept it at it was in vanilla Prey, but included the previous jitters in it (it's actually based on the previous one).
  // Similar variables are also called "matReprojection", "mReprojection", "mViewProjPrev" and "PrevViewProjMatrix".
  row_major float4x4 CV_PrevViewProjMatr : packoffset(c17);
  // Same logic as in "CV_PrevViewProjMatr" (this was also broken in Prey code without Luma).
  row_major float4x4 CV_PrevViewProjNearestMatr : packoffset(c21);
  // From camera pixel space coordinates (and depth) to world space position.
  // This is jittered too.
  row_major float3x4 CV_ScreenToWorldBasis : packoffset(c25);
  float4 CV_TessInfo : packoffset(c28);
  float4 CV_CameraRightVector : packoffset(c29);
  float4 CV_CameraFrontVector : packoffset(c30);
  float4 CV_CameraUpVector : packoffset(c31);
  // xy is current viewport resolution, so before and during uspcaling/downscaling to the final/output resolution, it's the rendering resolution, and after it's the output resolution.
  // zw is the half of the inverse of the final/output resolution (no rendering resolution scaling acknowledged by it).
  // Note that this is not always the actual "swapchain" resolution, in most passes it's adapted to actually represent the render target texture resolution.
  float4 CV_ScreenSize : packoffset(c32);
  // "CV_ScreenSize.xy" divided by "CV_HPosScale.xy" gives the final output resolution (so this has values < 1 for a rendering resolution lower than the output one). zw are the values from the previous frame.
  float4 CV_HPosScale : packoffset(c33);
  // Max (bottom right) texture UV coordinates of the render resolution area of the target texture. Z and W are the ones from the previous frame.
  float4 CV_HPosClamp : packoffset(c34);
  // bReverseDepth ? near / (near - far) : far / (far - near), bReverseDepth ? near / (far - near) : near / (near - far), 1 / hfov, 1.
  float4 CV_ProjRatio : packoffset(c35);
  float4 CV_NearestScaled : packoffset(c36);
  // near, far, far / maxViewDistance, 1/far.
  float4 CV_NearFarClipDist : packoffset(c37);
  float4 CV_SunLightDir : packoffset(c38);
  float4 CV_SunColor : packoffset(c39);
  float4 CV_SkyColor : packoffset(c40);
  float4 CV_FogColor : packoffset(c41);
  float4 CV_TerrainInfo : packoffset(c42);
  float4 CV_DecalZFightingRemedy : packoffset(c43);
  row_major float4x4 CV_FrustumPlaneEquation : packoffset(c44);
  float4 CV_WindGridOffset : packoffset(c48);
  // View/camera matrix (sometimes called "model view matrix").
  // This is not directly influenced by the jitters.
  // The inverse can be used to retrieve back the (non view) projection matrix, which contains the actual raw jitter values.
  row_major float4x4 CV_ViewMatr : packoffset(c49);
  row_major float4x4 CV_InvViewMatr : packoffset(c53);
  float CV_LookingGlass_SunSelector : packoffset(c57);
  float CV_LookingGlass_DepthScalar : packoffset(c57.y);
  float CV_PADDING0 : packoffset(c57.z);
  float CV_PADDING1 : packoffset(c57.w);
}