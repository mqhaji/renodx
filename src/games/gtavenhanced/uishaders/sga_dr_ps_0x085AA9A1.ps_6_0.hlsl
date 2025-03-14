#include "../shared.h"

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR)
    : SV_Target {
  float4 SV_Target = COLOR;

  SV_Target.rgb = renodx::draw::RenderIntermediatePass(renodx::color::srgb::Decode(max(0, SV_Target.rgb)));

  return SV_Target;
}
