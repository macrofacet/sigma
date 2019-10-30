struct VS_Out
{
	float4 pos : SV_Position;
	float2 texCoord : TEXCOORD;
};

VS_Out main(in float4 pos : POSITION, in float2 texCoord : TEXCOORD )
{
	VS_Out o;
	o.pos = pos;
	o.texCoord = texCoord;
	return o;
}