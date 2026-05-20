#include "Util.h"

#include <algorithm>
#include <cmath>
#include <d3dcompiler.h>
#include <winrt/base.h>

#include "RE/P/PlayerCamera.h"

namespace Util
{
	DirectX::XMMATRIX ToXMMatrix(const __m128* a_matrix)
	{
		return DirectX::XMMATRIX(a_matrix[0], a_matrix[1], a_matrix[2], a_matrix[3]);
	}

	float VerticalFOVFromProjection(const DirectX::XMMATRIX& a_projection)
	{
		DirectX::XMFLOAT4X4 projection{};
		DirectX::XMStoreFloat4x4(&projection, a_projection);

		const auto yScale = std::abs(projection._22);
		return yScale > 0.0f ? 2.0f * std::atan(1.0f / yScale) : 0.0f;
	}

	float VerticalFOVFromProjection(const __m128* a_projection)
	{
		return VerticalFOVFromProjection(ToXMMatrix(a_projection));
	}

	bool IsPlausibleVerticalFOV(float a_fov)
	{
		return a_fov > 0.2f && a_fov < 2.8f;
	}

	float VerticalFOVFromHorizontalDegrees(float a_horizontalDegrees, float a_aspectRatio)
	{
		constexpr auto pi = 3.14159265358979323846f;
		const auto horizontalDegrees = std::clamp(a_horizontalDegrees, 1.0f, 170.0f);
		const auto horizontalRadians = horizontalDegrees * pi / 180.0f;
		const auto aspectRatio = std::max(a_aspectRatio, 0.001f);
		return 2.0f * std::atan(std::tan(horizontalRadians * 0.5f) / aspectRatio);
	}

	float PlayerCameraVerticalFOV(float a_aspectRatio)
	{
		const auto playerCamera = RE::PlayerCamera::GetSingleton();
		if (!playerCamera) {
			return 0.0f;
		}

		const auto horizontalFOV = playerCamera->worldFOV + playerCamera->fovAdjustCurrent + playerCamera->fovAnimatorAdjust;
		return VerticalFOVFromHorizontalDegrees(horizontalFOV, a_aspectRatio);
	}

	CameraProjection GetCameraProjection(float a_aspectRatio)
	{
		auto* gameViewport = State_GetSingleton();
		auto& viewData = gameViewport->cameraState.camViewData;

		const auto viewMatrix = ToXMMatrix(viewData.viewMat);
		const auto viewProjUnjittered = ToXMMatrix(viewData.viewProjUnjittered);
		const auto invViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		const auto cameraViewToClipA = DirectX::XMMatrixMultiply(invViewMatrix, viewProjUnjittered);
		const auto cameraViewToClipB = DirectX::XMMatrixMultiply(viewProjUnjittered, invViewMatrix);

		CameraProjection result{};
		result.fovA = VerticalFOVFromProjection(cameraViewToClipA);
		result.fovB = VerticalFOVFromProjection(cameraViewToClipB);
		result.playerCameraFOV = PlayerCameraVerticalFOV(a_aspectRatio);
		result.usedAlternateProjectionOrder = !IsPlausibleVerticalFOV(result.fovA) && IsPlausibleVerticalFOV(result.fovB);
		result.cameraViewToClip = result.usedAlternateProjectionOrder ? cameraViewToClipB : cameraViewToClipA;

		const auto matrixFOV = result.usedAlternateProjectionOrder ? result.fovB : result.fovA;
		result.usedMatrixFOV = IsPlausibleVerticalFOV(matrixFOV);
		result.cameraFOV = result.usedMatrixFOV ? matrixFOV : result.playerCameraFOV;
		return result;
	}

	ID3D11DeviceChild* CompileShader(const wchar_t* FilePath, const std::vector<std::pair<const char*, const char*>>& Defines, const char* ProgramType, const char* Program)
	{
		static auto rendererData = RE::BSGraphics::GetRendererData();
		static auto device = reinterpret_cast<ID3D11Device*>(rendererData->device);

		// Build defines (aka convert vector->D3DCONSTANT array)
		std::vector<D3D_SHADER_MACRO> macros;

		for (auto& i : Defines)
			macros.push_back({ i.first, i.second });
		
		// Add null terminating entry
		macros.push_back({ nullptr, nullptr });

		// Compiler setup
		uint32_t flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

		winrt::com_ptr<ID3DBlob> shaderBlob;
		winrt::com_ptr<ID3DBlob> shaderErrors;

		std::string str;
		std::wstring path{ FilePath };
		std::transform(path.begin(), path.end(), std::back_inserter(str), [](wchar_t c) {
			return (char)c;
		});
		if (!std::filesystem::exists(FilePath)) {
			logger::error("Failed to compile shader; {} does not exist", str);
			return nullptr;
		}
		if (FAILED(D3DCompileFromFile(FilePath, macros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, Program, ProgramType, flags, 0, shaderBlob.put(), shaderErrors.put()))) {
			logger::warn("Shader compilation failed:\n\n{}", shaderErrors ? static_cast<char*>(shaderErrors->GetBufferPointer()) : "Unknown error");
			return nullptr;
		}
		if (shaderErrors)
			logger::debug("Shader logs:\n{}", static_cast<char*>(shaderErrors->GetBufferPointer()));

		if (!_stricmp(ProgramType, "ps_5_0")) {
			ID3D11PixelShader* regShader;
			device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		}
		else if (!_stricmp(ProgramType, "vs_5_0")) {
			ID3D11VertexShader* regShader;
			device->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		}
		else if (!_stricmp(ProgramType, "hs_5_0")) {
			ID3D11HullShader* regShader;
			device->CreateHullShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		}
		else if (!_stricmp(ProgramType, "ds_5_0")) {
			ID3D11DomainShader* regShader;
			device->CreateDomainShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader);
			return regShader;
		}
		else if (!_stricmp(ProgramType, "cs_5_0")) {
			ID3D11ComputeShader* regShader;
			DX::ThrowIfFailed(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader));
			return regShader;
		}
		else if (!_stricmp(ProgramType, "cs_4_0")) {
			ID3D11ComputeShader* regShader;
			DX::ThrowIfFailed(device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &regShader));
			return regShader;
		}

		return nullptr;
	}
}
