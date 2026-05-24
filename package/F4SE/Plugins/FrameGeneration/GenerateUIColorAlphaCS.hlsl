Texture2D<float4> InputTexturePreAlpha : register(t0);
Texture2D<float4> InputTextureAfterAlpha : register(t1);

RWTexture2D<float4> OutputUIColorAlpha : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint width;
	uint height;
	OutputUIColorAlpha.GetDimensions(width, height);
	if (DTid.x >= width || DTid.y >= height) {
		return;
	}

	const float4 colorPre = InputTexturePreAlpha[DTid.xy];
	const float4 colorPost = InputTextureAfterAlpha[DTid.xy];
	const float4 delta = abs(colorPost - colorPre);
	const float rgbDelta = max(delta.r, max(delta.g, delta.b));
	const float lumaDelta = dot(delta.rgb, float3(0.2126, 0.7152, 0.0722));
	const float alpha = saturate(max(max(rgbDelta * 8.0, lumaDelta * 16.0), delta.a * 1000.0));

	OutputUIColorAlpha[DTid.xy] = float4(colorPost.rgb, alpha);
}
