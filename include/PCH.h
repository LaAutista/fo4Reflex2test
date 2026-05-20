#pragma once

#undef DEBUG

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#pragma warning(push)
#include "F4SE/F4SE.h"
#include "RE/Fallout.h"
#include "REX/REX.h"
#pragma warning(pop)

#include "Windows.h"

#include <string>
using namespace std::literals;

#include "detours/detours.h"

#include "SimpleMath.h"

using float2 = DirectX::SimpleMath::Vector2;
using float3 = DirectX::SimpleMath::Vector3;
using float4 = DirectX::SimpleMath::Vector4;
using float4x4 = DirectX::SimpleMath::Matrix;
using uint = uint32_t;

#include <directx/d3dx12.h>

#include <magic_enum/magic_enum.hpp>

#ifdef NDEBUG
#	include <spdlog/sinks/basic_file_sink.h>
#else
#	include <spdlog/sinks/msvc_sink.h>
#endif
#include <spdlog/spdlog.h>



#define DLLEXPORT __declspec(dllexport)

namespace logger
{
	template <class... Args>
	void trace(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Trace, a_fmt, std::forward<Args>(a_args)...);
	}

	inline void trace(std::string_view a_msg)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Trace, a_msg);
	}

	template <class... Args>
	void debug(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Debug, a_fmt, std::forward<Args>(a_args)...);
	}

	inline void debug(std::string_view a_msg)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Debug, a_msg);
	}

	template <class... Args>
	void info(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Info, a_fmt, std::forward<Args>(a_args)...);
	}

	inline void info(std::string_view a_msg)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Info, a_msg);
	}

	template <class... Args>
	void warn(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Warning, a_fmt, std::forward<Args>(a_args)...);
	}

	inline void warn(std::string_view a_msg)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Warning, a_msg);
	}

	template <class... Args>
	void error(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Error, a_fmt, std::forward<Args>(a_args)...);
	}

	inline void error(std::string_view a_msg)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Error, a_msg);
	}

	template <class... Args>
	void critical(std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Critical, a_fmt, std::forward<Args>(a_args)...);
	}

	inline void critical(std::string_view a_msg)
	{
		REX::Impl::Log(std::source_location::current(), REX::ELogLevel::Critical, a_msg);
	}
}

namespace stl
{
	template <class T>
	void write_thunk_call(std::uintptr_t a_src)
	{
		auto& trampoline = REL::GetTrampoline();
		T::func = trampoline.write_call<5>(a_src, T::thunk);
	}

	template <class F, size_t index, class T>
	void write_vfunc()
	{
		REL::Relocation<std::uintptr_t> vtbl{ F::VTABLE[index] };
		T::func = vtbl.write_vfunc(T::size, T::thunk);
	}

	template <class F, class T>
	void write_vfunc()
	{
		write_vfunc<F, 0, T>();
	}

	template <std::size_t idx, class T>
	void write_vfunc(REL::ID id)
	{
		REL::Relocation<std::uintptr_t> vtbl{ id };
		T::func = vtbl.write_vfunc(idx, T::thunk);
	}

	template <class T>
	void detour_thunk(REL::ID a_relId)
	{
		*(uintptr_t*)&T::func = Detours::X64::DetourFunction(a_relId.address(), (uintptr_t)&T::thunk);
	}

	template <class T>
	void detour_thunk_ignore_func(REL::ID a_relId)
	{
		std::ignore = Detours::X64::DetourFunction(a_relId.address(), (uintptr_t)&T::thunk);
	}

	template <std::size_t idx, class T>
	void detour_vfunc(void* target)
	{
		*(uintptr_t*)&T::func = Detours::X64::DetourClassVTable(*(uintptr_t*)target, &T::thunk, idx);
	}
}


namespace DX
{
	// Helper class for COM exceptions
	class com_exception : public std::exception
	{
	public:
		explicit com_exception(HRESULT hr) noexcept :
			result(hr) {}

		const char* what() const override
		{
			static char s_str[64] = {};
			sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
			return s_str;
		}

	private:
		HRESULT result;
	};

	// Helper utility converts D3D API failures into exceptions.
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr)) {
			throw com_exception(hr);
		}
	}
}

#include "Plugin.h"
