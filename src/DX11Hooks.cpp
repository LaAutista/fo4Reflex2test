#include "DX11Hooks.h"

#include <d3d11.h>
#include <magic_enum/magic_enum.hpp>

#include "DX12SwapChain.h"
#include "Streamline.h"
#include "Upscaling.h"

extern bool enbLoaded;

namespace
{
	bool WantsD3D12FrameGeneration()
	{
		const auto mode = GetPrivateProfileIntA("Settings", "iFrameGenerationMode", 0, "Data\\MCM\\Settings\\Upscaling.ini");
		return mode > 0;
	}

	bool WantsD3D12Upscaling()
	{
		const auto method = GetPrivateProfileIntA("Settings", "iUpscaleMethodPreference", 2, "Data\\MCM\\Settings\\Upscaling.ini");
		return method == static_cast<int>(Upscaling::UpscaleMethod::kFSR) ||
			method == static_cast<int>(Upscaling::UpscaleMethod::kDLSS);
	}

	winrt::com_ptr<IDXGIAdapter> ResolveAdapter(IDXGIAdapter* a_adapter, ID3D11Device* a_device)
	{
		winrt::com_ptr<IDXGIAdapter> result;
		if (a_adapter) {
			result.copy_from(a_adapter);
			return result;
		}

		winrt::com_ptr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(a_device->QueryInterface(IID_PPV_ARGS(dxgiDevice.put())))) {
			IDXGIAdapter* adapter = nullptr;
			if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
				result.attach(adapter);
			}
		}

		return result;
	}
}

struct hkIDXGISwapChainPresent
{
	static HRESULT STDMETHODCALLTYPE thunk(IDXGISwapChain* This, UINT SyncInterval, UINT Flags)
	{
		auto streamline = Streamline::GetSingleton();
		streamline->OnPresentStart();
		const auto result = func(This, SyncInterval, Flags);
		streamline->OnPresentEnd(result);
		return result;
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

struct hkD3D11CreateDeviceAndSwapChain
{
	static HRESULT WINAPI thunk(
		IDXGIAdapter* pAdapter,
		D3D_DRIVER_TYPE DriverType,
		HMODULE Software,
		UINT Flags,
		const D3D_FEATURE_LEVEL* pFeatureLevels,
		UINT FeatureLevels,
		UINT SDKVersion,
		const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
		IDXGISwapChain** ppSwapChain,
		ID3D11Device** ppDevice,
		D3D_FEATURE_LEVEL* pFeatureLevel,
		ID3D11DeviceContext** ppImmediateContext)
	{
		const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
		pFeatureLevels = &featureLevel;
		FeatureLevels = 1;

		auto streamline = Streamline::GetSingleton();
		const bool wantsD3D12FrameGeneration = streamline->interposer && WantsD3D12FrameGeneration();
		const bool wantsD3D12Upscaling = WantsD3D12Upscaling();
		const bool useD3D12Proxy = !enbLoaded && pSwapChainDesc && pSwapChainDesc->Windowed;
		if (useD3D12Proxy) {
			try {
				logger::info("[DX12SwapChain] D3D12 proxy swapchain requested at startup frameGeneration={} upscaling={}", wantsD3D12FrameGeneration, wantsD3D12Upscaling);
				Streamline* streamlineForProxy = nullptr;
				if (streamline->interposer) {
					streamline->Initialize(sl::RenderAPI::eD3D12);
					if (streamline->UsesD3D12() && !(Upscaling::kForceFSRFrameGenerationForTesting && wantsD3D12FrameGeneration)) {
						streamlineForProxy = streamline;
					}
				}

				winrt::com_ptr<ID3D11Device> d3d11Device;
				winrt::com_ptr<ID3D11DeviceContext> d3d11Context;
				D3D_FEATURE_LEVEL createdFeatureLevel{};
				DX::ThrowIfFailed(D3D11CreateDevice(
					pAdapter,
					DriverType,
					Software,
					Flags,
					pFeatureLevels,
					FeatureLevels,
					SDKVersion,
					d3d11Device.put(),
					&createdFeatureLevel,
					d3d11Context.put()));

				auto adapter = ResolveAdapter(pAdapter, d3d11Device.get());
				if (!adapter) {
					throw DX::com_exception(E_FAIL);
				}

				winrt::com_ptr<IDXGIFactory5> dxgiFactory;
				DX::ThrowIfFailed(adapter->GetParent(IID_PPV_ARGS(dxgiFactory.put())));

				auto dx12SwapChain = DX12SwapChain::GetSingleton();
				dx12SwapChain->SetD3D11Device(d3d11Device.get());
				dx12SwapChain->SetD3D11DeviceContext(d3d11Context.get());
				dx12SwapChain->CreateD3D12Device(adapter.get(), streamlineForProxy);
				if (!streamlineForProxy && streamline->UsesD3D12() && streamline->slSetD3DDevice) {
					if (SL_FAILED(result, streamline->slSetD3DDevice(dx12SwapChain->GetD3D12Device()))) {
						logger::warn("[DX12SwapChain] slSetD3DDevice(D3D12 native) failed: {}", magic_enum::enum_name(result));
					}
				}

				if (streamline->UsesD3D12()) {
					streamline->CheckFeatures(adapter.get());
					streamline->PostDevice();
				}

				const bool useFidelityFXFrameGeneration = (Upscaling::kForceFSRFrameGenerationForTesting || !streamline->featureDLSSG);
				if (useFidelityFXFrameGeneration) {
					logger::info("[DX12SwapChain] FidelityFX frame generation selected force={} dlssgAvailable={}", Upscaling::kForceFSRFrameGenerationForTesting, streamline->featureDLSSG);
				}
				dx12SwapChain->CreateSwapChain(dxgiFactory.get(), *pSwapChainDesc, streamlineForProxy, useFidelityFXFrameGeneration);
				dx12SwapChain->CreateInterop();

				*ppSwapChain = dx12SwapChain->GetSwapChainProxy();
				*ppDevice = d3d11Device.detach();
				*ppImmediateContext = d3d11Context.detach();
				if (pFeatureLevel) {
					*pFeatureLevel = createdFeatureLevel;
				}

				if (streamlineForProxy) {
					streamline->SetSwapChain(*ppSwapChain);
				}
				return S_OK;
			} catch (const std::exception& e) {
				logger::error("[DX12SwapChain] Failed to create D3D12 proxy swapchain: {}", e.what());
				logger::warn("[DX12SwapChain] Falling back to the native D3D11 swapchain; D3D12-only upscalers will remain unavailable until restart");
				if (streamline->UsesD3D12()) {
					streamline->Shutdown();
				}
			}
		}

		if (streamline->interposer && !streamline->initialized) {
			streamline->Initialize(sl::RenderAPI::eD3D11);
		}

		DX::ThrowIfFailed(func(pAdapter,
			DriverType,
			Software,
			Flags,
			pFeatureLevels,
			FeatureLevels,
			SDKVersion,
			pSwapChainDesc,
			ppSwapChain,
			ppDevice,
			pFeatureLevel,
			ppImmediateContext));
			
		if (streamline->interposer){
			if (!enbLoaded)
				streamline->slUpgradeInterface((void**)&(*ppSwapChain));
			streamline->slSetD3DDevice(*ppDevice);

			streamline->SetSwapChain(*ppSwapChain);

			static bool presentHooked = false;
			if (!presentHooked && *ppSwapChain) {
				stl::detour_vfunc<8, hkIDXGISwapChainPresent>(*ppSwapChain);
				presentHooked = true;
			}

			IDXGIAdapter* featureAdapter = pAdapter;
			winrt::com_ptr<IDXGIAdapter> queriedAdapter;
			if (!featureAdapter && *ppDevice) {
				winrt::com_ptr<IDXGIDevice> dxgiDevice;
				if (SUCCEEDED((*ppDevice)->QueryInterface(IID_PPV_ARGS(dxgiDevice.put())))) {
					IDXGIAdapter* adapter = nullptr;
					if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
						queriedAdapter.attach(adapter);
						featureAdapter = queriedAdapter.get();
					}
				}
			}

			streamline->CheckFeatures(featureAdapter);
			streamline->PostDevice();
		}

		return S_OK;
	}
	static inline REL::Relocation<decltype(thunk)> func;
};

namespace DX11Hooks
{
	void Install()
	{
		auto streamline = Streamline::GetSingleton();
		streamline->LoadInterposer();

		uintptr_t moduleBase = (uintptr_t)GetModuleHandle(nullptr);
		// Hook BSGraphics::CreateD3DAndSwapChain::D3D11CreateDeviceAndSwapChain to use D3D_FEATURE_LEVEL_11_1
		(uintptr_t&)hkD3D11CreateDeviceAndSwapChain::func = Detours::IATHook(moduleBase, "d3d11.dll", "D3D11CreateDeviceAndSwapChain", (uintptr_t)hkD3D11CreateDeviceAndSwapChain::thunk);
	}
}
