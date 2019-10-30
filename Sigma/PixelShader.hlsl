Texture2D<float4> Texture;
SamplerState TextureSampler;
float4 main(float4 pos :SV_POSITION, float2 texCoord : TEXCOORD) : SV_TARGET
{
	return Texture.Sample(TextureSampler, texCoord);
}