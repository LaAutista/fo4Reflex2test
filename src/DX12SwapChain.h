#pragma once

#include <atomic>

#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "Buffer.h"

class Streamline;

class D3D11D3D12SharedTexture
{
public:
	D3D11D3D12SharedTexture(const D3D11_TEXTURE2D_DESC& a_desc, ID3D11Device5* a_d3d11Device, ID3D12Device* a_d3d12Device);

	winrt::com_ptr<ID3D11Texture2D> resource11;
	winrt::com_ptr<ID3D12Resource> resource12;
};

struct DXGISwapChainProxy final : IDXGISwapChain4
{
	explicit DXGISwapChainProxy(IDXGISwapChain4* a_swapChain);

	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;

	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData) override;
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) override;
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) override;

	HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override;
	HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) override;
	HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) override;
	HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) override;
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) override;
	HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override;
	HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) override;
	HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput) override;
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) override;
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount) override;

	HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) override;
	HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) override;
	HRESULT STDMETHODCALLTYPE GetHwnd(HWND* pHwnd) override;
	HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void** ppUnk) override;
	HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) override;
	BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
	HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) override;
	HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* pColor) override;
	HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* pColor) override;
	HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
	HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* pRotation) override;

	HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override;
	HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* pWidth, UINT* pHeight) override;
	HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
	HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency) override;
	HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override;
	HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix) override;
	HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix) override;

	UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override;
	HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport) override;
	HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override;
	HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue) override;

	HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData) override;

private:
	std::atomic<ULONG> refCount{ 1 };
	winrt::com_ptr<IDXGISwapChain4> swapChain;
};

class DX12SwapChain
{
public:
	static DX12SwapChain* GetSingleton()
	{
		static DX12SwapChain singleton;
		return &singleton;
	}

	void CreateD3D12Device(IDXGIAdapter* a_adapter, Streamline* a_streamline);
	void CreateSwapChain(IDXGIFactory5* a_dxgiFactory, const DXGI_SWAP_CHAIN_DESC& a_swapChainDesc, Streamline* a_streamline, bool a_useFidelityFXFrameGeneration);
	void CreateInterop();

	void SetD3D11Device(ID3D11Device* a_d3d11Device);
	void SetD3D11DeviceContext(ID3D11DeviceContext* a_d3d11Context);

	DXGISwapChainProxy* GetSwapChainProxy() const { return swapChainProxy; }
	bool IsReady() const { return swapChainProxy && swapChain; }
	UINT GetFrameIndex() const { return frameIndex; }
	ID3D12Device* GetD3D12Device() const { return d3d12Device.get(); }

	HRESULT Present(UINT SyncInterval, UINT Flags);
	bool EvaluateD3D12DLSSForCurrentFrame();
	bool EvaluateD3D12FSRForCurrentFrame();
	bool EvaluateFSRFrameGenerationForCurrentFrame();
	HRESULT GetBuffer(UINT a_buffer, REFIID a_riid, void** a_surface);
	HRESULT GetDevice(REFIID a_riid, void** a_device);
	void InstallWndProcHook(HWND a_hwnd);
	LRESULT CallOriginalWndProc(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam) const;

	winrt::com_ptr<ID3D12Device> d3d12Device;
	winrt::com_ptr<ID3D12CommandQueue> commandQueue;
	winrt::com_ptr<ID3D12CommandAllocator> commandAllocators[2];
	winrt::com_ptr<ID3D12GraphicsCommandList4> commandLists[2];
	winrt::com_ptr<IDXGISwapChain4> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

private:
	void RefreshBackBuffers();

	winrt::com_ptr<ID3D12Device> proxyD3D12Device;
	winrt::com_ptr<ID3D11Device5> d3d11Device;
	winrt::com_ptr<ID3D11DeviceContext4> d3d11Context;
	winrt::com_ptr<ID3D11Fence> d3d11Fence;
	winrt::com_ptr<ID3D12Fence> d3d12Fence;
	winrt::com_ptr<ID3D12Fence> commandFence;
	winrt::com_ptr<ID3D12Resource> swapChainBuffers[2];
	std::unique_ptr<Texture2D> swapChainBufferProxy;
	std::unique_ptr<D3D11D3D12SharedTexture> swapChainBufferProxyENB;
	std::unique_ptr<D3D11D3D12SharedTexture> swapChainBufferWrapped[2];
	DXGISwapChainProxy* swapChainProxy = nullptr;
	UINT frameIndex = 0;
	UINT64 fenceValue = 1;
	UINT64 commandFenceValue = 1;
	UINT64 commandAllocatorFenceValues[2]{};
	bool useFrameLatencyWaitable = false;
	double desktopRefreshHz = 0.0;
	HWND hwnd = nullptr;
	WNDPROC originalWndProc = nullptr;
};
