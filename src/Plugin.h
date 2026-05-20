#pragma once

#include <string_view>

namespace Plugin
{
	using namespace std::literals;

	inline constexpr REL::Version VERSION{ 1u, 1u, 1u, 0u };
	inline constexpr auto         NAME = "Upscaling"sv;
}
