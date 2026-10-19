#pragma once

#include <algorithm>
#include <array>
#include <string>

namespace PLEngine {

using MessageHook = void (*)(std::string *, std::string *);
using LegacyMessageHook = void (*)(char *, char *);

inline std::string PrepareMessage(std::string text, MessageHook function) {
	auto output = text;
	function(&text, &output);
	return output;
}

inline std::string PrepareLegacyMessage(
		std::string text,
		LegacyMessageHook function) {
	constexpr auto Capacity = std::size_t(32768);
	if (text.size() >= Capacity) {
		return text;
	}
	auto input = std::array<char, Capacity>();
	auto output = std::array<char, Capacity>();
	std::copy(text.begin(), text.end(), input.begin());
	function(input.data(), output.data());
	const auto end = std::find(output.begin(), output.end(), '\0');
	return end == output.end() ? text : std::string(output.begin(), end);
}

} // namespace PLEngine
