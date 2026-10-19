#include "plengine/message_hooks.h"

#include <cassert>
#include <cstring>
#include <iostream>

int main() {
	const auto unicode = std::string("Привет 🌍");
	const auto longText = std::string(65536, 'x') + unicode;
	assert(PLEngine::PrepareMessage(longText, [](auto in, auto out) {
		*out = *in + "!";
	}) == longText + "!");
	assert(PLEngine::PrepareMessage(unicode, [](auto, auto) {}) == unicode);
	assert(PLEngine::PrepareMessage(unicode, [](auto, auto out) {
		out->clear();
	}).empty());
	assert(PLEngine::PrepareLegacyMessage(unicode, [](auto in, auto out) {
		std::strcpy(out, in);
	}) == unicode);
	assert(PLEngine::PrepareLegacyMessage(unicode, [](auto, auto out) {
		std::memset(out, 'x', 32768);
	}) == unicode);
	assert(PLEngine::PrepareLegacyMessage(longText, [](auto, auto) {
		std::abort();
	}) == longText);
	std::cout << "Message hooks: long UTF-8, empty output and legacy bounds passed\n";
}
