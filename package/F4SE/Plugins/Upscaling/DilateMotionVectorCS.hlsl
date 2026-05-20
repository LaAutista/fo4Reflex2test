Texture2D<float2> MotionVectorInput : register(t0);
Texture2D<float> DepthInput : register(t1);

RWTexture2D<float2> MotionVectorOutput : register(u0);

cbuffer Upscaling : register(b0)
{
	uint2 ScreenSize;
	uint2 RenderSize;
	float4 CameraData;
};

float GetScreenDepth(float depth)
{
	return (CameraData.w / (-depth * CameraData.z + CameraData.x));
}

bool IsFiniteFloat(float value)
{
	return value == value && abs(value) < 3.402823466e38;
}

bool IsFiniteFloat2(float2 value)
{
	return all(value == value) && all(abs(value) < 3.402823466e38);
}

bool IsValidRawDepth(float depth)
{
	return IsFiniteFloat(depth) && depth >= 0.0 && depth <= 1.0;
}

bool IsValidLinearDepth(float depth)
{
	return IsFiniteFloat(depth) && depth > 0.0;
}

bool IsDepthCandidate(float currentLinearDepth, float neighborLinearDepth)
{
	if (neighborLinearDepth >= currentLinearDepth)
		return false;

	// Very near first-person geometry can have unreliable motion vectors. Do not
	// let it donate motion to far background pixels such as sight apertures.
	const float nearForegroundDepth = 256.0;
	const float extremeDepthGap = 4096.0 * 2.5;
	if (neighborLinearDepth < nearForegroundDepth && (currentLinearDepth - neighborLinearDepth) > extremeDepthGap)
		return false;

	return true;
}

[numthreads(8, 8, 1)] void main(uint3 dispatchID : SV_DispatchThreadID) {
	// Early exit if dispatch thread is outside texture dimensions
	if (any(dispatchID.xy >= RenderSize))
		return;

	float depth = DepthInput[dispatchID.xy];
	float currentLinearDepth = GetScreenDepth(depth);

	// Find longest motion vector in 5x5 neighborhood
	float2 motionVector = MotionVectorInput[dispatchID.xy];
	if (!IsFiniteFloat2(motionVector))
		motionVector = float2(0.0, 0.0);

	float2 longestMotionVector = motionVector;
	float maxMotionLengthSq = dot(motionVector, motionVector);

	if (IsValidRawDepth(depth) && IsValidLinearDepth(currentLinearDepth) && currentLinearDepth > (4096.0 * 2.5)){
		[unroll]
		for (int y = -2; y <= 2; y++) {
			[unroll]
			for (int x = -2; x <= 2; x++) {
				int2 samplePos = int2(dispatchID.xy) + int2(x, y);

				// Skip samples outside texture dimensions
				if (any(samplePos < 0) || any(samplePos >= int2(RenderSize)))
					continue;

				float neighborDepth = DepthInput[samplePos];
				if (!IsValidRawDepth(neighborDepth))
					continue;

				float neighborLinearDepth = GetScreenDepth(neighborDepth);
				if (!IsValidLinearDepth(neighborLinearDepth))
					continue;

				// Take neighbor if it is a compatible foreground candidate and has longer motion.
				if (IsDepthCandidate(currentLinearDepth, neighborLinearDepth)){
					float2 neighborMotionVector = MotionVectorInput[samplePos];
					if (!IsFiniteFloat2(neighborMotionVector))
						continue;

					// Square motion vector for length
					float motionLengthSq = dot(neighborMotionVector, neighborMotionVector);

					if (motionLengthSq > maxMotionLengthSq){
						maxMotionLengthSq = motionLengthSq;
						longestMotionVector = neighborMotionVector;
					}
				}
			}
		}
	}

	MotionVectorOutput[dispatchID.xy] = longestMotionVector;
}
