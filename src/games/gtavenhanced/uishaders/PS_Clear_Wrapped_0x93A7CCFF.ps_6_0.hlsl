float4 main(
  noperspective float4 SV_Position : SV_Position,
  linear float4 TEXCOORD : TEXCOORD
) : SV_Target {
  float4 SV_Target;
  SV_Target.x = (TEXCOORD.x);
  SV_Target.y = (TEXCOORD.y);
  SV_Target.z = (TEXCOORD.z);
  SV_Target.w = (TEXCOORD.w);
  return SV_Target;
}
