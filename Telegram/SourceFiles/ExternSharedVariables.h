#pragma once

/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026 // t.me/ayuplugg
Follows GNU GPL v3 and Telegram Desktop licensing.
*/

#include "AyuPlugin.h"

#include <mutex>
#include <vector>

struct TrustedUAs {
	std::vector<std::string> runtime;
	std::vector<std::string> session;
	std::vector<std::string> events;
};

struct PluginData {
	std::string name;
	std::string description;
	std::string modulePath;
	bool usesHooks = false;
	std::string hooksList;
	InternalDrawGUI drawGUI = nullptr;
};

inline std::vector<InternalDoFilterHistoryItem> FunctionsOnFilter;
inline std::vector<InternalDoPreProcessMessage> FunctionsOnPrepare;
inline std::vector<InternalLegacyDoPreProcessMessage> FunctionsOnPrepareLegacy;
inline std::vector<InternalIsOnline> FunctionsOnIsOnline;
inline std::vector<InternalExcludeDeletion> FunctionsExcludeDeleted;
inline std::vector<InternalDrawGUI> FunctionsDrawGUI;
inline std::vector<InternalDrawPopupItem> FunctionsDrawPopup;
inline std::vector<PluginData> pluginsData;
inline TrustedUAs trusted;
inline std::mutex FunctionsMutex;
inline std::mutex TrustedMutex;

inline std::vector<InternalDoFilterHistoryItem> FilterFunctions() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return FunctionsOnFilter;
}

inline std::vector<InternalDoPreProcessMessage> PrepareFunctions() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return FunctionsOnPrepare;
}

inline std::vector<InternalIsOnline> IsOnlineFunctions() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return FunctionsOnIsOnline;
}

inline std::vector<InternalExcludeDeletion> ExcludeDeletionFunctions() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return FunctionsExcludeDeleted;
}

inline std::vector<PluginData> PluginsData() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return pluginsData;
}

inline TrustedUAs TrustedUserAgents() {
	const auto lock = std::lock_guard(TrustedMutex);
	return trusted;
}

inline std::vector<InternalDrawPopupItem> PopupFunctions() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return FunctionsDrawPopup;
}

inline std::vector<InternalLegacyDoPreProcessMessage> LegacyPrepareFunctions() {
	const auto lock = std::lock_guard(FunctionsMutex);
	return FunctionsOnPrepareLegacy;
}
