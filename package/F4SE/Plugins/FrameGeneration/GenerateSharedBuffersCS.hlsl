Texture2D<float4> InputTexturePreAlpha : register(t0);
Texture2D<float4> InputTextureAfterAlpha : register(t1);
Texture2D<float2> InputMotionVectors : register(t2);
Texture2D<float> InputDepth : register(t3);

RWTexture2D<float2> OutputMotionVectors : register(u0);
RWTexture2D<float> OutputDepth : register(u1);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint width;
	uint height;
	OutputMotionVectors.GetDimensions(width, height);
	if (DTid.x >= width || DTid.y >= height) {
		return;
	}

    float alphaPre = InputTexturePreAlpha[DTid.xy].w;
    float alphaPost = InputTextureAfterAlpha[DTid.xy].w;
    float depth = InputDepth[DTid.xy];

    float mask = abs(alphaPre - alphaPost);
    mask *= 1000.0;
    mask = 1.0 - saturate(mask);

	OutputMotionVectors[DTid.xy] = lerp(0.0, InputMotionVectors[DTid.xy], mask);
	OutputDepth[DTid.xy] = lerp(min(depth, 0.1), depth, mask);
}
