/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026 // t.me/ayuplugg
Follows GNU GPL v3 and Telegram Desktop licensing.
*/

#pragma once

#include <QByteArray>
#include <QSettings>
#include <QtGlobal>

#include <string>

class PLESettings {
public:
	PLESettings(const PLESettings &obj) = delete;

	static PLESettings *GetInstance() {
		if (Instance == nullptr) {
			Instance = new PLESettings();
		}
		return Instance;
	}

	bool consoleEnabled() {
		return _consoleEnabled;
	}

	void setConsoleEnabled(bool value) {
		_consoleEnabled = value;
		value
			? WriteEnvironment("AYUPL_CONSOLE", "1")
			: WriteEnvironment("AYUPL_CONSOLE", "");
	}

	bool apiEnabled() {
		return _apiEnabled;
	}

	void setApiEnabled(bool value) {
		_apiEnabled = value;
		if (value) {
			WriteEnvironment("AYUPL_PORT", "8081");
			WriteEnvironment("AYUPL_HOST", "127.0.0.1");
		} else {
			WriteEnvironment("AYUPL_PORT", "");
			WriteEnvironment("AYUPL_HOST", "");
		}
	}

	bool apiSecurityDisabled() {
		return _apiSecurityDisabled;
	}

	void setApiSecurityDisabled(bool value) {
		_apiSecurityDisabled = value;
		value
			? WriteEnvironment("AYUPL_DISABLESECURITYFEATURES", "1")
			: WriteEnvironment("AYUPL_DISABLESECURITYFEATURES", "");
	}

private:
	PLESettings() {
		RestoreEnvironment("AYUPL_CONSOLE");
		RestoreEnvironment("AYUPL_HOST");
		RestoreEnvironment("AYUPL_PORT");
		RestoreEnvironment("AYUPL_DISABLESECURITYFEATURES");
		_consoleEnabled = IsEnvSet("AYUPL_CONSOLE");
		_apiEnabled = IsEnvSet("AYUPL_PORT");
		_apiSecurityDisabled = IsEnvSet("AYUPL_DISABLESECURITYFEATURES");
	}

	static bool IsEnvSet(const std::string &name) {
		const auto value = qgetenv(name.c_str());
		return !value.isEmpty() && value != "0";
	}

	static void WriteEnvironment(
			const std::string &name,
			const std::string &value) {
		auto settings = QSettings("AyuGram", "PLEngine");
		if (value.empty()) {
			qunsetenv(name.c_str());
			settings.remove(QString::fromStdString(name));
		} else {
			qputenv(name.c_str(), QByteArray::fromStdString(value));
			settings.setValue(
				QString::fromStdString(name),
				QString::fromStdString(value));
		}
	}

	static void RestoreEnvironment(const char *name) {
		if (qEnvironmentVariableIsSet(name)) {
			return;
		}
		const auto settings = QSettings("AyuGram", "PLEngine");
		const auto value = settings.value(QString::fromLatin1(name)).toString();
		if (!value.isEmpty()) {
			qputenv(name, value.toUtf8());
		}
	}

	bool _consoleEnabled = false;
	bool _apiEnabled = false;
	bool _apiSecurityDisabled = false;
	inline static PLESettings *Instance = nullptr;

};
