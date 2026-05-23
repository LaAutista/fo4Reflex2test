#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace Reflex2
{
	enum class NgxResult : int
	{
		kSuccess = 1,
		kFail = -1160773632,
		kFeatureNotSupported = -1160773631,
		kInvalidParameter = -1160773627,
		kNotInitialized = -1160773625,
	};

	enum class NgxFeature : int
	{
		kLatewarp = 15,  // NVSDK_NGX_Feature_Reserved15 in Reflex2Demo2's PDB.
	};

	struct NgxHandle;

	struct NgxCoordinates
	{
		uint32_t X = 0;
		uint32_t Y = 0;
	};

	struct NgxDimensions
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	struct NgxParameter
	{
		virtual void Set(const char* name, void* value) = 0;
		virtual void Set(const char* name, ID3D12Resource* value) = 0;
		virtual void Set(const char* name, ID3D11Resource* value) = 0;
		virtual void Set(const char* name, int value) = 0;
		virtual void Set(const char* name, unsigned value) = 0;
		virtual void Set(const char* name, double value) = 0;
		virtual void Set(const char* name, float value) = 0;
		virtual void Set(const char* name, unsigned long long value) = 0;

		virtual NgxResult Get(const char* name, void** value) const = 0;
		virtual NgxResult Get(const char* name, ID3D12Resource** value) const = 0;
		virtual NgxResult Get(const char* name, ID3D11Resource** value) const = 0;
		virtual NgxResult Get(const char* name, int* value) const = 0;
		virtual NgxResult Get(const char* name, unsigned* value) const = 0;
		virtual NgxResult Get(const char* name, double* value) const = 0;
		virtual NgxResult Get(const char* name, float* value) const = 0;
		virtual NgxResult Get(const char* name, unsigned long long* value) const = 0;

		virtual void Reset() = 0;
	};

	static_assert(sizeof(NgxCoordinates) == 8);
	static_assert(sizeof(NgxDimensions) == 8);
	static_assert(sizeof(NgxParameter*) == 8);

	struct LatewarpEvalParams
	{
		ID3D12Resource* InBackbuffer = nullptr;
		ID3D12Resource* InHudlessColor = nullptr;
		ID3D12Resource* InUIColorAlpha = nullptr;
		ID3D12Resource* InDepth = nullptr;
		ID3D12Resource* InMotionVectors = nullptr;
		ID3D12Resource* OutColor = nullptr;
		ID3D12Resource* InNoWarpMask = nullptr;
		NgxCoordinates InBackbufferSubrectBase{};
		NgxCoordinates InHudlessColorSubrectBase{};
		NgxCoordinates InUIColorAlphaSubrectBase{};
		NgxCoordinates InDepthSubrectBase{};
		NgxCoordinates InMVSubrectBase{};
		NgxCoordinates InNoWarpMaskSubrectBase{};
		NgxCoordinates InOutputSubrectBase{};
		NgxDimensions InBackbufferSubrectDimensions{};
		NgxDimensions InHudlessColorSubrectDimensions{};
		NgxDimensions InUIColorAlphaSubrectDimensions{};
		NgxDimensions InDepthSubrectDimensions{};
		NgxDimensions InMVSubrectDimensions{};
		NgxDimensions InNoWarpMaskSubrectDimensions{};
		NgxDimensions InOutputSubrectDimensions{};
		uint32_t FrameID = 0;
		bool DepthInverted = false;
		bool UsePremultiplyUIAlpha = false;
		float InJitterOffsetX = 0.0f;
		float InJitterOffsetY = 0.0f;
		float* WorldToViewMatrix = nullptr;
		float* ViewToClipMatrix = nullptr;
		float* PrevRenderedWorldToViewMatrix = nullptr;
		float* PrevRenderedViewToClipMatrix = nullptr;
	};

	static_assert(offsetof(LatewarpEvalParams, FrameID) == 168);
	static_assert(offsetof(LatewarpEvalParams, WorldToViewMatrix) == 184);
	static_assert(sizeof(LatewarpEvalParams) == 216);

	struct LatewarpCreateParams
	{
		uint32_t InOutputWidth = 0;
		uint32_t InOutputHeight = 0;
	};

	static_assert(sizeof(LatewarpCreateParams) == 8);

	struct LatewarpInputs
	{
		ID3D12Resource* backbuffer = nullptr;
		ID3D12Resource* hudlessColor = nullptr;
		ID3D12Resource* uiColorAlpha = nullptr;
		ID3D12Resource* depth = nullptr;
		ID3D12Resource* motionVectors = nullptr;
		ID3D12Resource* outputColor = nullptr;
		ID3D12Resource* noWarpMask = nullptr;

		NgxDimensions backbufferSize{};
		NgxDimensions hudlessColorSize{};
		NgxDimensions uiColorAlphaSize{};
		NgxDimensions depthSize{};
		NgxDimensions motionVectorSize{};
		NgxDimensions outputSize{};
		NgxDimensions noWarpMaskSize{};

		uint32_t frameID = 0;
		bool isRenderedFrame = true;
		bool depthInverted = false;
		bool usePremultiplyUIAlpha = false;
		float jitterOffsetX = 0.0f;
		float jitterOffsetY = 0.0f;

		float* worldToViewMatrix = nullptr;
		float* viewToClipMatrix = nullptr;
		float* previousRenderedWorldToViewMatrix = nullptr;
		float* previousRenderedViewToClipMatrix = nullptr;
	};

	class Latewarp
	{
	public:
		static Latewarp* GetSingleton()
		{
			static Latewarp singleton;
			return &singleton;
		}

		bool Load(const std::filesystem::path& a_runtimeDirectory);
		void Shutdown();
		bool IsLoaded() const { return loaded; }
		bool Evaluate(ID3D12GraphicsCommandList* a_commandList, const LatewarpInputs& a_inputs);

	private:
		bool CreateFeature(ID3D12GraphicsCommandList* a_commandList, uint32_t a_width, uint32_t a_height);
		void DestroyFeature();
		NgxParameter* AllocateParameters() const;
		void DestroyParameters(NgxParameter* a_parameters) const;

		HMODULE ngx = nullptr;
		HMODULE latewarp = nullptr;
		NgxHandle* latewarpHandle = nullptr;
		uint32_t outputWidth = 0;
		uint32_t outputHeight = 0;
		bool loaded = false;

		using AllocateParametersFn = NgxResult(__cdecl*)(NgxParameter**);
		using DestroyParametersFn = NgxResult(__cdecl*)(NgxParameter*);
		using CreateFeatureFn = NgxResult(__cdecl*)(ID3D12GraphicsCommandList*, NgxFeature, NgxParameter*, NgxHandle**);
		using EvaluateFeatureFn = NgxResult(__cdecl*)(ID3D12GraphicsCommandList*, const NgxHandle*, const NgxParameter*, void(__cdecl*)(float, bool&));
		using ReleaseFeatureFn = NgxResult(__cdecl*)(NgxHandle*);

		AllocateParametersFn allocateParameters = nullptr;
		DestroyParametersFn destroyParameters = nullptr;
		CreateFeatureFn createFeature = nullptr;
		EvaluateFeatureFn evaluateFeature = nullptr;
		ReleaseFeatureFn releaseFeature = nullptr;
	};
}
