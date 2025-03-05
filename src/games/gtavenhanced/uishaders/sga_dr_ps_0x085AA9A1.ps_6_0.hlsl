#include "../common.hlsl"

float4 main(
    noperspective float4 SV_Position: SV_Position,
    linear float4 COLOR: COLOR)
    : SV_Target {
  float4 SV_Target = COLOR;

  SV_Target.rgb = UIScale(SV_Target.rgb);

  return SV_Target;
}
