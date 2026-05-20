Texture2D<float4> InputOpaqueOnlyColor : register(t0);
Texture2D<float4> InputFinalColor : register(t1);

RWTexture2D<float> OutputTransparencyMask : register(u0);

float TransparencySignal(int2 samplePos)
{
	const float3 opaqueOnly = InputOpaqueOnlyColor[samplePos].rgb;
	const float3 finalColor = InputFinalColor[samplePos].rgb;
	const float3 delta = abs(finalColor - opaqueOnly);
	const float maxDelta = max(delta.r, max(delta.g, delta.b));
	const float lumaDelta = dot(delta, float3(0.2126, 0.7152, 0.0722));

	return saturate(max(maxDelta * 8.0, lumaDelta * 16.0));
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint width;
	uint height;
	OutputTransparencyMask.GetDimensions(width, height);
	if (DTid.x >= width || DTid.y >= height) {
		return;
	}

	OutputTransparencyMask[DTid.xy] = TransparencySignal(int2(DTid.xy));
}
