/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026 // t.me/ayuplugg
Follows GNU GPL v3 and Telegram Desktop licensing.
*/

#pragma once

#include "data/data_user.h"
#include "AyuPlugin.h"
#include "ExternSharedVariables.h"

#include <json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace ns {

struct SimplifiedTGAccount {
	int index = 0;
	std::string auth_key;
	int dc = 0;
	uint64_t user_id = 0;
	std::string username;
	std::string first_name;
	std::string last_name;
};

struct DefaultJSONResponse {
	bool ok = false;
	std::string error;
	std::string result_json;
};

void to_json(nlohmann::json &j, const ns::DefaultJSONResponse &p);
void from_json(const nlohmann::json &j, ns::DefaultJSONResponse &p);
void to_json(nlohmann::json &j, const ns::SimplifiedTGAccount &p);
void from_json(const nlohmann::json &j, ns::SimplifiedTGAccount &p);

} // namespace ns

enum TypeTrust {
	runtime,
	session,
	events
};

struct SharedMemHelper {
	uintptr_t applicationAddr = 0;
	UserData *activeUserPtr = nullptr;
	ExAddToQueue addToQueue = nullptr;
	wchar_t currentAccountName[255] = {};
	bool isSharingScreen = false;
	bool isEnabled = false;
};

void MainInject();
void onMainThread();
void loadKnownPlugins();
void listenHTTP();
void processDLL(std::string dll);
std::string getPLFolder();
std::string uintToJSON(uintptr_t addr);
void loadMem();
void updIsOnline();
void openPluginsFolder();
