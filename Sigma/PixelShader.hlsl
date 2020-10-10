Texture2D Texture2DTable[] : register(t0, space0);
SamplerState TextureSampler;

struct PerDrawConstants
{
	uint TextureIndex;
};
ConstantBuffer<PerDrawConstants> perDrawConstants : register(b0, space0);

float4 main(float4 pos :SV_POSITION, float2 texCoord : TEXCOORD) : SV_TARGET
{
	return Texture2DTable[perDrawConstants.TextureIndex].Sample(TextureSampler, texCoord);
}