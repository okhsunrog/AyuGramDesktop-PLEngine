/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026 // t.me/ayuplugg
Follows GNU GPL v3 and Telegram Desktop licensing.
*/
#include "PLEMains.h"
#include "scheme.h"
#include <core/application.h>
#include "apiwrap.h"
#include <base/unixtime.h>
#ifdef _WIN32
#include <Shlwapi.h>
#include "shellapi.h"
#undef small // since Small is used by data_session in TGAPI, and it's conflicting with shellapi
#endif
#include "MainQueue.h"
#include <ayu/utils/telegram_helpers.h>
#include "main/main_session.h"
#include "api/api_updates.h"
#include <stddef.h>
#include "httplib.h"
#include <thread>
#include "main/main_domain.h"
#include "crl/crl_on_main.h"
#include "main/main_account.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "plengine/PLESettings.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <cwchar>
#include <memory>

using json = nlohmann::json;
using namespace ns;
std::condition_variable cv;
std::vector<std::unique_ptr<QLibrary>> loadedLibraries;
std::mutex loadedLibrariesMutex;
std::atomic_bool engineRunning = false;
std::thread queueWorker;

namespace {

void SleepFor(int milliseconds) {
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void ShowPluginError(const QString &title, const QString &message) {
	qWarning().noquote() << title << ':' << message;
	const auto application = QCoreApplication::instance();
	if (!application) {
		return;
	}
	const auto show = [=] {
		QMessageBox::critical(nullptr, title, message);
	};
	if (QThread::currentThread() == application->thread()) {
		show();
	} else {
		QMetaObject::invokeMethod(application, show, Qt::QueuedConnection);
	}
}

bool ConfirmTrust(const QString &message) {
	const auto application = QCoreApplication::instance();
	if (!application) {
		return false;
	}
	auto accepted = false;
	const auto show = [&] {
		accepted = QMessageBox::question(
			nullptr,
			u"Security warning"_q,
			message,
			QMessageBox::Ok | QMessageBox::Cancel,
			QMessageBox::Cancel) == QMessageBox::Ok;
	};
	if (QThread::currentThread() == application->thread()) {
		show();
	} else {
		QMetaObject::invokeMethod(
			application,
			show,
			Qt::BlockingQueuedConnection);
	}
	return accepted;
}

bool InvokeOnMainThread(const std::function<void()> &callback) {
	const auto application = QCoreApplication::instance();
	if (!application) {
		return false;
	}
	if (QThread::currentThread() == application->thread()) {
		callback();
		return true;
	}
	return QMetaObject::invokeMethod(
		application,
		callback,
		Qt::BlockingQueuedConnection);
}

std::string WideStringToUtf8(const wchar_t *value, std::size_t capacity) {
	const auto end = std::find(value, value + capacity, L'\0');
	return QString::fromWCharArray(value, end - value)
		.toUtf8()
		.toStdString();
}

} // namespace

namespace ns {
	void to_json(nlohmann::json& j, const ns::DefaultJSONResponse& p) {
		j = nlohmann::json{ {"ok", p.ok}, {"error", p.error}, {"result_json", p.result_json} };
	}

	void from_json(const nlohmann::json& j, ns::DefaultJSONResponse& p) {
		j.at("ok").get_to(p.ok);
		j.at("error").get_to(p.error);
		j.at("result_json").get_to(p.result_json);
	}

	void to_json(nlohmann::json& j, const ns::SimplifiedTGAccount& p) {
		j = json{ {"index", p.index},
				 {"auth_key", p.auth_key},
				 {"dc", p.dc},
				 {"user_id", p.user_id},
				 {"username", p.username},
				 {"first_name", p.first_name},
				 {"last_name", p.last_name}
		};
	}

	void from_json(const nlohmann::json& j, ns::SimplifiedTGAccount& p) {
		j.at("index").get_to(p.index);
		j.at("auth_key").get_to(p.auth_key);
		j.at("dc").get_to(p.dc);
		j.at("user_id").get_to(p.user_id);
		j.at("username").get_to(p.username);
		j.at("first_name").get_to(p.first_name);
		j.at("last_name").get_to(p.last_name);
	}
}

std::vector<std::string> fileNames = {};
std::atomic_int pollingRate = 1000;
std::atomic_bool memUpdaterStarted = false;
httplib::Server svr;

ApiWrap* activeAccWrapper;
SharedMemHelper memHelper;

std::queue<std::function<void()>>* GetQueuePtr() { return &MainQueue; }

void AddToQueue(const std::function<void()>& func) {
	std::cout << "Added function at address " << uintToJSON((uintptr_t)&func) << " to main queue.\n";
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		if (!engineRunning.load()) {
			return;
		}
		MainQueue.push(func);
	}
	cv.notify_one();
}

std::string OKResponse(bool ok = true, std::string errors = "", std::string res = "") {
	ns::DefaultJSONResponse aok = { ok, errors, res };
	json aokj = aok;
	return aokj.dump().c_str();
}

std::string to_hex_string(bytes::const_span data) {
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (auto b : data) {
		oss << std::setw(2) << static_cast<int>(gsl::to_integer<unsigned char>(b));
	}
	return oss.str();
}

std::vector<ns::SimplifiedTGAccount> CollectAccounts() {
	auto result = std::vector<ns::SimplifiedTGAccount>();
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (!account->sessionExists()) {
			continue;
		}
		const auto session = &account->session();
		const auto user = session->user().get();
		const auto keys = session->mtp().getKeysForWrite();
		const auto dcId = session->mainDcId();
		auto authKey = std::string();
		for (const auto &key : keys) {
			if (key->dcId() == dcId) {
				authKey = to_hex_string(key->data());
				break;
			}
		}
		result.push_back({
			index,
			std::move(authKey),
			dcId,
			session->userId().bare,
			user->username().toStdString(),
			user->firstName.toStdString(),
			user->lastName.toStdString(),
		});
	}
	return result;
}

std::string GetTrustedNameByType(TypeTrust t) {
	return std::string(t == TypeTrust::runtime ? "Remote Code Execution" : t == TypeTrust::session
		? "Sessions and accounts"
		: "Updates & Events");
}

std::vector<std::string>* GetTrustedByType(TypeTrust t) {
	return &(t == TypeTrust::runtime ? trusted.runtime : t == TypeTrust::session ? trusted.session : trusted.events);
}

std::string getUA(const httplib::Request& req) {
	return (req.has_header("User-Agent")
		? req.get_header_value("User-Agent")
		: (req.has_header("user-agent") ? req.get_header_value("user-agent") : "No user agent provided"));
}

bool IsTrustedUA(std::string ua, TypeTrust trust) {

	if (getenv("AYUPL_DISABLESECURITYFEATURES") != nullptr&& std::string(getenv("AYUPL_DISABLESECURITYFEATURES")) == "1") {
		printf("Trust checks are skipped, since AYUPL_DISABLESECURITYFEATURES is set.\n Requested trust elevation type: %s\n", GetTrustedNameByType(trust).c_str());
		return true;
	}
	auto trustedScope = GetTrustedByType(trust);
	auto good = false;
	{
		const auto lock = std::lock_guard(TrustedMutex);
		good = std::find(
			trustedScope->begin(),
			trustedScope->end(),
			ua) != trustedScope->end();
	}
	if (!good) {
		good = ConfirmTrust(QString::fromStdString(
			"Some application or plug-in is trying to elevate its permissions.\n\n"
			"Type of elevation: " + GetTrustedNameByType(trust)
			+ "\nUser-Agent: " + ua));
		if (good) {
			const auto lock = std::lock_guard(TrustedMutex);
			if (std::find(
					trustedScope->begin(),
					trustedScope->end(),
					ua) == trustedScope->end()) {
				trustedScope->push_back(ua);
			}
		}
	}

	return good;
}

void updMemHelp() {
	while (engineRunning.load()) {
		const auto invoked = InvokeOnMainThread([] {
			memHelper.applicationAddr = uintptr_t(Core::Application::Instance);
			memHelper.isSharingScreen = Core::App().isSharingScreen();
			memHelper.addToQueue = &AddToQueue;
			const auto &account = Core::App().activeAccount();
			if (!account.sessionExists()) {
				memHelper.activeUserPtr = nullptr;
				memHelper.currentAccountName[0] = L'\0';
				return;
			}
			memHelper.activeUserPtr = account.session().user();
			const auto name = memHelper.activeUserPtr->name().toStdWString();
			const auto count = std::min(name.size(), std::size_t(254));
			std::wmemcpy(
				memHelper.currentAccountName,
				name.data(),
				count);
			memHelper.currentAccountName[count] = L'\0';
		});
		if (!invoked) {
			return;
		}
		SleepFor(pollingRate.load());
	}
}

void updIsOnline() {
	while (engineRunning.load()) {
		for (const auto function : IsOnlineFunctions()) {
			if (function()) {
				crl::on_main([] {
					const auto window = Core::App().activeWindow();
					const auto controller = window
						? window->sessionController()
						: nullptr;
					if (controller) {
					controller->session()
						.updates()
						.api()
						.request(MTPaccount_UpdateStatus(MTP_bool(false)))
						.send();
					}
				});
				break;
			}
		}
		SleepFor(2000);
	}
}

void openPluginsFolder()
{
	QDesktopServices::openUrl(
		QUrl::fromLocalFile(QString::fromStdString(getPLFolder())));
}

void loadMem() {
	auto expected = false;
	if (memUpdaterStarted.compare_exchange_strong(expected, true)) {
		memHelper.isEnabled = true;
		std::thread th(updMemHelp);
		th.detach();
	}
}

std::string uintToJSON(uintptr_t addr) {
	json b;
	char addrv[128];
	snprintf(
		addrv,
		sizeof(addrv),
		"%p",
		reinterpret_cast<void *>(addr));
	b["address"] = addrv;
	return b.dump();
}

std::string getPLFolder() {
	auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (base.isEmpty()) {
		base = QDir(
			QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
			.filePath(u"AyuGram"_q);
	}
	return QDir(base).filePath(u"plugins"_q).toStdString();
}

void processDLL(std::string dll) {
	if (!engineRunning.load()) {
		return;
	}
	const auto requested = QString::fromStdString(dll);
	const auto fullPath = QFileInfo(requested).isAbsolute()
		? QFileInfo(requested).absoluteFilePath()
		: QDir(QString::fromStdString(getPLFolder())).filePath(requested);
	auto library = std::make_unique<QLibrary>(fullPath);
	library->setLoadHints(
		QLibrary::ResolveAllSymbolsHint
		| QLibrary::ExportExternalSymbolsHint
		| QLibrary::PreventUnloadHint);
	if (!library->load()) {
		ShowPluginError(
			u"Plugin load error"_q,
			u"Could not load %1:\n%2"_q.arg(fullPath, library->errorString()));
		return;
	}

	const auto pluginInfo = reinterpret_cast<InternalPluginInfo>(
		library->resolve("pluginInfo"));
	AyuPlugin* pl = pluginInfo ? pluginInfo() : nullptr;
	if (!pl) {
		ShowPluginError(
			u"Plugin execution error"_q,
			u"%1 does not export a valid pluginInfo function."_q.arg(fullPath));
		return;
	}

	const auto resolve = [&](const char *name) {
		return library->resolve(name);
	};
	auto set_func = reinterpret_cast<InternalSetup>(resolve("internalSetup"));
	auto loop_func = reinterpret_cast<InternalLoop>(resolve("internalLoop"));
	auto func = reinterpret_cast<InternalDoFilterHistoryItem>(
		resolve("doFilterHistoryItem"));
	auto func2 = reinterpret_cast<InternalDoPreProcessMessage>(
		resolve("doPreProcessMessageStd"));
	auto func4 = reinterpret_cast<InternalIsOnline>(
		resolve("doReturnIsOnline"));
	auto func5 = reinterpret_cast<InternalExcludeDeletion>(
		resolve("doExcludeDeleted"));
	auto func6 = reinterpret_cast<InternalDrawGUI>(resolve("doDrawGUI"));
	auto popup = reinterpret_cast<InternalDrawPopupItem>(resolve("doDrawPopup"));
	auto legacy = reinterpret_cast<InternalLegacyDoPreProcessMessage>(
		resolve("doPreProcessMessage"));

	auto d = PluginData();
	d.name = WideStringToUtf8(pl->name, std::size(pl->name));
	d.description = WideStringToUtf8(
		pl->description,
		std::size(pl->description));
	d.usesHooks = func2 || legacy || func || func4 || func5 || popup;
	d.modulePath = fullPath.toStdString();
	d.hooksList = "";

	if (func) {
		d.hooksList += "Shared filters hook (can filter out messages before shared filters) \n";
		std::cout << ("Got shared filters function handle!\n");
	}
	if (func2) {
		d.hooksList += "Message preparation hook (can intercept your messages, and change them on send time) \n";
		std::cout << ("Got message preparation function handle!\n");
	}
	if (func4) {
		d.hooksList += "Online hook (can modify your online state) \n";
		std::cout << ("Got online function handle!\n");
	}
	if (func5) {
		d.hooksList += "Exclude deletion hook (can exclude messages out of \"save deleted\") \n";
		std::cout << ("Got InternalExcludeDeletion function handle!\n");
	}
	if (func6) {
		d.drawGUI = func6;
		d.hooksList += "Can draw GUI \n";
		std::cout << ("Got GUI function handle!\n");
	}
	if (popup) {
		d.hooksList += "Can add context menu actions\n";
	}
	const auto initialized = InvokeOnMainThread([=] {
		pl->memData = {};
		pl->memData.addToQueue = &AddToQueue;
		pl->memData.applicationAddr = uintptr_t(Core::Application::Instance);
		const auto &account = Core::App().activeAccount();
		if (account.sessionExists()) {
			pl->memData.activeUserPtr = uintptr_t(
				account.session().user().get());
			pl->memData.activeSessionPtr = uintptr_t(&account.session());
		}
	});
	if (!initialized) {
		ShowPluginError(
			u"Plugin execution error"_q,
			u"Could not initialize %1 because AyuGram is shutting down."_q
				.arg(fullPath));
		return;
	}
	std::cout << ("Shared memory pointers successfully!\n");

	if (set_func) {
		d.hooksList += "Has Setup function (will run once on startup) \n";
	}
	if (loop_func) {
		d.hooksList += "Has Loop function (will run in a loop) \n";
	}
	{
		const auto lock = std::lock_guard(FunctionsMutex);
		if (func) {
			FunctionsOnFilter.push_back(func);
		}
		if (func2) {
			FunctionsOnPrepare.push_back(func2);
		}
		if (func4) {
			FunctionsOnIsOnline.push_back(func4);
		}
		if (func5) {
			FunctionsExcludeDeleted.push_back(func5);
		}
		if (func6) {
			FunctionsDrawGUI.push_back(func6);
		}
		if (legacy) {
			FunctionsOnPrepareLegacy.push_back(legacy);
		}
		if (popup) {
			FunctionsDrawPopup.push_back(popup);
		}
		pluginsData.push_back(d);
	}
	{
		const auto lock = std::lock_guard(loadedLibrariesMutex);
		loadedLibraries.push_back(std::move(library));
	}

	if (set_func) {
		std::cout << ("Exec-ing setup..\n");
		set_func();
	}
	if (loop_func) {
		while (engineRunning.load()) {
			loop_func();
		}
	}
}

void listenHTTP() {
	svr.Get("/api/ping",
		[](const httplib::Request&, httplib::Response& res)
		{ res.set_content(OKResponse(), "application/json"); });

	svr.Get("/api/runtime/setPolling",
		[](const httplib::Request& req, httplib::Response& res)
		{
			const auto good = IsTrustedUA(getUA(req), TypeTrust::runtime);
			const auto updaterRunning = memUpdaterStarted.load();
			auto parsed = false;
			const auto rate = req.has_param("rate")
				? QString::fromStdString(req.get_param_value("rate"))
					.toInt(&parsed)
				: 0;
			if (good && updaterRunning && parsed) {
				pollingRate.store(std::max(rate, 10));
			}
			const auto ok = good && updaterRunning && parsed;
			const auto error = !good
				? "User rejected trust elevation request."
				: !updaterRunning
				? "Nobody is updating MemHelper."
				: !parsed
				? "The rate parameter is missing or invalid."
				: "";
			res.set_content(
				OKResponse(
					ok,
					error,
					ok ? uintToJSON(uintptr_t(&memHelper)) : "null"),
				"application/json");
		});

	svr.Get("/api/runtime/load",
		[](const httplib::Request& req, httplib::Response& res)
		{
			const auto trusted = IsTrustedUA(getUA(req), TypeTrust::runtime);
			const auto hasName = req.has_param("name")
				&& !req.get_param_value("name").empty();
			if (trusted && hasName) {
				loadMem();
				auto t = std::thread(
					processDLL,
					req.get_param_value("name"));
				t.detach();
			}
			const auto ok = trusted && hasName;
			res.set_content(OKResponse(
				ok,
				!trusted
					? "User rejected trust elevation request."
					: !hasName
					? "The name parameter is missing."
					: "",
				ok ? uintToJSON(uintptr_t(&memHelper)) : "null"),
				"application/json");
		});

	svr.Get("/api/runtime/export",
		[](const httplib::Request& req, httplib::Response& res)
		{
			const auto good = IsTrustedUA(getUA(req), TypeTrust::runtime);
			if (good) {
				loadMem();
			}
			res.set_content(OKResponse(good,
				good ? "" : "User rejected trust elevation request.",
				good ? uintToJSON(uintptr_t(&memHelper)) : "null"),
				"application/json");
		});

	svr.Get("/api/session/get",
		[](const httplib::Request& req, httplib::Response& res)
		{
			auto accounts = std::vector<ns::SimplifiedTGAccount>();
			auto good = IsTrustedUA(getUA(req), TypeTrust::session);
			if (good) {
				good = InvokeOnMainThread([&] {
					accounts = CollectAccounts();
				});
			}
			const auto response = json(accounts);
			res.set_content(
				OKResponse(
					good,
					good ? "" : "The request was rejected or AyuGram is shutting down.",
					good ? response.dump() : "null"),
				"application/json");
		});
	const auto host = qEnvironmentVariable("AYUPL_HOST", "127.0.0.1");
	bool parsed = false;
	const auto portx = qEnvironmentVariableIntValue("AYUPL_PORT", &parsed);
	bool val = portx >= 1 && portx < 65535;
	if (!parsed || !val) {
		std::cout << "Port is not specified, API is disabled\r\n";
		return;
	}
	svr.listen(host.toStdString(), portx);
}



void loadKnownPlugins() {
	std::cout << ("Sleeping for 5 seconds, then loading the plugins..\n\n");
	SleepFor(5000);
	if (!engineRunning.load()) {
		return;
	}
	for (auto b : fileNames) {
		std::cout << ((std::string("Loading ") + b + std::string("\n")).c_str());

		std::thread t(processDLL, b);
		t.detach();
	}
	std::cout << ("Finished loading plugins!\n\n");
}


void onMainThread() {
	while (true) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			cv.wait(lock, [] {
				return !engineRunning.load() || !MainQueue.empty();
			});

			if (!engineRunning.load()) {
				return;
			}
			task = MainQueue.front();
			MainQueue.pop();
		}

		if (task) {
			dispatchToMainThread(task); // thanks AlexeyZavar!
		}
	}
}

void MainInject() {
	if (engineRunning.exchange(true)) {
		return;
	}
	PLESettings::GetInstance();

#ifdef _WIN32
	if (getenv("AYUPL_CONSOLE") != nullptr && std::string(getenv("AYUPL_CONSOLE"))=="1") {
		AllocConsole();
		SetConsoleTitleA("AyuGram Plugin Engine by Pomorgite");
		typedef struct
		{
			char* _ptr;
			int _cnt;
			char* _base;
			int _flag;
			int _file;
			int _charbuf;
			int _bufsiz;
			char* _tmpfname;
		} FILE_COMPLETE;
		*(FILE_COMPLETE*)stdout =
			*(FILE_COMPLETE*)_fdopen(_open_osfhandle((long)GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT), "w");
		*(FILE_COMPLETE*)stderr =
			*(FILE_COMPLETE*)_fdopen(_open_osfhandle((long)GetStdHandle(STD_ERROR_HANDLE), _O_TEXT), "w");
		*(FILE_COMPLETE*)stdin =
			*(FILE_COMPLETE*)_fdopen(_open_osfhandle((long)GetStdHandle(STD_INPUT_HANDLE), _O_TEXT), "r");
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
		setvbuf(stdin, NULL, _IONBF, 0);
	}
#endif

	std::cout << "Plugin engine is loading.\n\n";
	std::string plF = getPLFolder();
	std::cout << (std::string("Plugin folder: ") + plF + "\n").c_str();
	auto pluginDirectory = QDir(QString::fromStdString(plF));
	if (!pluginDirectory.mkpath(u"."_q)) {
		std::cout << "Could not create plugins folder.\n";
	}
	const auto entries = pluginDirectory.entryList(QDir::Files | QDir::Readable);
	for (const auto &entry : entries) {
		if (!QLibrary::isLibrary(entry)) {
			continue;
		}
		fileNames.push_back(entry.toStdString());
		std::cout << "Found plugin: " << entry.toStdString() << '\n';
	}
	std::thread a2(loadKnownPlugins);
	a2.detach();

	std::thread t(listenHTTP);
	t.detach();
	std::thread t2(updIsOnline);
	t2.detach();
	queueWorker = std::thread(onMainThread);
}

void StopEngine() {
	{
		const auto lock = std::lock_guard(queueMutex);
		engineRunning.store(false);
		MainQueue = {};
	}
	cv.notify_all();
	if (queueWorker.joinable()) {
		queueWorker.join();
	}
}
