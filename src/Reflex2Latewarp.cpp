#include "Reflex2Latewarp.h"

namespace
{
	constexpr bool Succeeded(Reflex2::NgxResult a_result)
	{
		return a_result == Reflex2::NgxResult::kSuccess;
	}

	template <class T>
	T LoadProc(HMODULE a_module, const char* a_name)
	{
		return reinterpret_cast<T>(GetProcAddress(a_module, a_name));
	}

	void SetSubrect(
		Reflex2::NgxParameter* a_parameters,
		const char* a_baseX,
		const char* a_baseY,
		const char* a_width,
		const char* a_height,
		const Reflex2::NgxCoordinates& a_base,
		const Reflex2::NgxDimensions& a_dimensions)
	{
		a_parameters->Set(a_baseX, a_base.X);
		a_parameters->Set(a_baseY, a_base.Y);
		a_parameters->Set(a_width, a_dimensions.Width);
		a_parameters->Set(a_height, a_dimensions.Height);
	}
}

namespace Reflex2
{
	bool Latewarp::Load(const std::filesystem::path& a_runtimeDirectory)
	{
		if (loaded) {
			return true;
		}

		const auto ngxPath = a_runtimeDirectory / L"nvngx.dll";
		const auto latewarpPath = a_runtimeDirectory / L"nvngx_latewarp.dll";

		ngx = LoadLibraryW(ngxPath.c_str());
		latewarp = LoadLibraryW(latewarpPath.c_str());
		if (!ngx || !latewarp) {
			logger::warn(
				"[Reflex2] Could not load Latewarp runtime: nvngx={} latewarp={} lastError={}",
				static_cast<void*>(ngx),
				static_cast<void*>(latewarp),
				GetLastError());
			Shutdown();
			return false;
		}

		allocateParameters = LoadProc<AllocateParametersFn>(ngx, "NVSDK_NGX_D3D12_AllocateParameters");
		destroyParameters = LoadProc<DestroyParametersFn>(ngx, "NVSDK_NGX_D3D12_DestroyParameters");
		createFeature = LoadProc<CreateFeatureFn>(latewarp, "NVSDK_NGX_D3D12_CreateFeature");
		evaluateFeature = LoadProc<EvaluateFeatureFn>(latewarp, "NVSDK_NGX_D3D12_EvaluateFeature");
		releaseFeature = LoadProc<ReleaseFeatureFn>(latewarp, "NVSDK_NGX_D3D12_ReleaseFeature");

		loaded =
			allocateParameters &&
			destroyParameters &&
			createFeature &&
			evaluateFeature &&
			releaseFeature;

		if (!loaded) {
			logger::warn("[Reflex2] Latewarp runtime loaded, but required NGX exports are missing");
			Shutdown();
			return false;
		}

		logger::info("[Reflex2] Latewarp runtime loaded from {}", a_runtimeDirectory.string());
		return true;
	}

	void Latewarp::Shutdown()
	{
		DestroyFeature();

		allocateParameters = nullptr;
		destroyParameters = nullptr;
		createFeature = nullptr;
		evaluateFeature = nullptr;
		releaseFeature = nullptr;
		loaded = false;

		if (latewarp) {
			FreeLibrary(latewarp);
			latewarp = nullptr;
		}

		if (ngx) {
			FreeLibrary(ngx);
			ngx = nullptr;
		}
	}

	bool Latewarp::Evaluate(ID3D12GraphicsCommandList* a_commandList, const LatewarpInputs& a_inputs)
	{
		++evaluateAttempts;
		lastEvaluateSuccessful = false;

		if (!loaded || !a_commandList || !a_inputs.backbuffer || !a_inputs.hudlessColor || !a_inputs.depth || !a_inputs.motionVectors || !a_inputs.outputColor) {
			lastResult = loaded ? NgxResult::kInvalidParameter : NgxResult::kNotInitialized;
			return false;
		}

		if (!CreateFeature(a_commandList, a_inputs.outputSize.Width, a_inputs.outputSize.Height)) {
			return false;
		}

		auto* params = AllocateParameters();
		if (!params) {
			logger::warn("[Reflex2] Could not allocate Latewarp evaluate parameters");
			lastResult = NgxResult::kFail;
			return false;
		}

		params->Set("Latewarp.Backbuffer", a_inputs.backbuffer);
		params->Set("Latewarp.HudlessColor", a_inputs.hudlessColor);
		params->Set("Depth", a_inputs.depth);
		params->Set("MotionVectors", a_inputs.motionVectors);
		params->Set("Output", a_inputs.outputColor);
		params->Set("Backbuffer", a_inputs.backbuffer);
		params->Set("HudlessColor", a_inputs.hudlessColor);
		if (a_inputs.uiColorAlpha) {
			params->Set("Latewarp.UIColorAlpha", a_inputs.uiColorAlpha);
			params->Set("UIColorAlpha", a_inputs.uiColorAlpha);
		}
		if (a_inputs.noWarpMask) {
			params->Set("Latewarp.NoWarpMask", a_inputs.noWarpMask);
			params->Set("NoWarpMask", a_inputs.noWarpMask);
		}

		SetSubrect(params, "Latewarp.Backbuffer.Subrect.Base.X", "Latewarp.Backbuffer.Subrect.Base.Y", "Latewarp.Backbuffer.Subrect.Width", "Latewarp.Backbuffer.Subrect.Height", {}, a_inputs.backbufferSize);
		SetSubrect(params, "Latewarp.HudlessColor.Subrect.Base.X", "Latewarp.HudlessColor.Subrect.Base.Y", "Latewarp.HudlessColor.Subrect.Width", "Latewarp.HudlessColor.Subrect.Height", {}, a_inputs.hudlessColorSize);
		SetSubrect(params, "Latewarp.Depth.Subrect.Base.X", "Latewarp.Depth.Subrect.Base.Y", "Latewarp.Depth.Subrect.Width", "Latewarp.Depth.Subrect.Height", {}, a_inputs.depthSize);
		SetSubrect(params, "Latewarp.MV.Subrect.Base.X", "Latewarp.MV.Subrect.Base.Y", "Latewarp.MV.Subrect.Width", "Latewarp.MV.Subrect.Height", {}, a_inputs.motionVectorSize);
		if (a_inputs.uiColorAlpha) {
			SetSubrect(params, "Latewarp.UIColorAlpha.Subrect.Base.X", "Latewarp.UIColorAlpha.Subrect.Base.Y", "Latewarp.UIColorAlpha.Subrect.Width", "Latewarp.UIColorAlpha.Subrect.Height", {}, a_inputs.uiColorAlphaSize);
		}
		if (a_inputs.noWarpMask) {
			SetSubrect(params, "Latewarp.NoWarpMask.Subrect.Base.X", "Latewarp.NoWarpMask.Subrect.Base.Y", "Latewarp.NoWarpMask.Subrect.Width", "Latewarp.NoWarpMask.Subrect.Height", {}, a_inputs.noWarpMaskSize);
		}
		SetSubrect(params, "Latewarp.Output.Subrect.Base.X", "Latewarp.Output.Subrect.Base.Y", "Latewarp.Output.Subrect.Width", "Latewarp.Output.Subrect.Height", {}, a_inputs.outputSize);

		params->Set("Latewarp.FrameID", a_inputs.frameID);
		params->Set("Latewarp.IsRenderedFrame", a_inputs.isRenderedFrame ? 1 : 0);
		params->Set("Latewarp.DepthInverted", a_inputs.depthInverted ? 1 : 0);
		params->Set("Latewarp.UsePremultiplyUIAlpha", a_inputs.usePremultiplyUIAlpha ? 1 : 0);
		params->Set("Latewarp.EvalFlags", 0u);
		params->Set("Latewarp.Reserved1", 0u);
		params->Set("Jitter.Offset.X", a_inputs.jitterOffsetX);
		params->Set("Jitter.Offset.Y", a_inputs.jitterOffsetY);
		params->Set("Latewarp.WorldToViewMatrix", a_inputs.worldToViewMatrix);
		params->Set("Latewarp.ViewToClipMatrix", a_inputs.viewToClipMatrix);
		params->Set("Latewarp.PrevRenderedWorldToViewMatrix", a_inputs.previousRenderedWorldToViewMatrix);
		params->Set("Latewarp.PrevRenderedViewToClipMatrix", a_inputs.previousRenderedViewToClipMatrix);

		const auto result = evaluateFeature(a_commandList, latewarpHandle, params, nullptr);
		DestroyParameters(params);
		lastResult = result;

		if (!Succeeded(result)) {
			logger::warn("[Reflex2] Latewarp evaluate failed: 0x{:08X}", static_cast<uint32_t>(result));
			return false;
		}

		lastEvaluateSuccessful = true;
		++evaluateSuccesses;
		return true;
	}

	bool Latewarp::CreateFeature(ID3D12GraphicsCommandList* a_commandList, uint32_t a_width, uint32_t a_height)
	{
		if (!a_width || !a_height) {
			lastResult = NgxResult::kInvalidParameter;
			return false;
		}

		if (latewarpHandle && outputWidth == a_width && outputHeight == a_height) {
			return true;
		}

		DestroyFeature();

		auto* params = AllocateParameters();
		if (!params) {
			logger::warn("[Reflex2] Could not allocate Latewarp create parameters");
			lastResult = NgxResult::kFail;
			return false;
		}

		params->Set("Latewarp.Output.Width", a_width);
		params->Set("Latewarp.Output.Height", a_height);

		const auto result = createFeature(a_commandList, NgxFeature::kLatewarp, params, &latewarpHandle);
		DestroyParameters(params);

		if (!Succeeded(result) || !latewarpHandle) {
			logger::warn("[Reflex2] Could not create Latewarp feature: 0x{:08X}", static_cast<uint32_t>(result));
			latewarpHandle = nullptr;
			outputWidth = 0;
			outputHeight = 0;
			lastResult = result;
			return false;
		}

		lastResult = result;
		outputWidth = a_width;
		outputHeight = a_height;
		logger::info("[Reflex2] Latewarp feature created at {}x{}", outputWidth, outputHeight);
		return true;
	}

	void Latewarp::DestroyFeature()
	{
		if (latewarpHandle && releaseFeature) {
			const auto result = releaseFeature(latewarpHandle);
			if (!Succeeded(result)) {
				logger::warn("[Reflex2] Latewarp release failed: 0x{:08X}", static_cast<uint32_t>(result));
			}
		}

		latewarpHandle = nullptr;
		outputWidth = 0;
		outputHeight = 0;
	}

	NgxParameter* Latewarp::AllocateParameters() const
	{
		if (!allocateParameters) {
			return nullptr;
		}

		NgxParameter* params = nullptr;
		const auto result = allocateParameters(&params);
		if (!Succeeded(result)) {
			logger::warn("[Reflex2] NGX parameter allocation failed: 0x{:08X}", static_cast<uint32_t>(result));
			return nullptr;
		}

		return params;
	}

	void Latewarp::DestroyParameters(NgxParameter* a_parameters) const
	{
		if (destroyParameters && a_parameters) {
			const auto result = destroyParameters(a_parameters);
			if (!Succeeded(result)) {
				logger::warn("[Reflex2] NGX parameter destroy failed: 0x{:08X}", static_cast<uint32_t>(result));
			}
		}
	}
}
