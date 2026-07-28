/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026 // t.me/ayuplugg
Follows GNU GPL v3 and Telegram Desktop licensing.

This part of source code is based on Ayugram's sources by Radolyn.
*/

#include "plengine/settings/settings_trustedUA.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "settings/sections/settings_main.h"
#include "lang_auto.h"
#include "plengine/settings/settings_main.h"
#include "ayu/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "ExternSharedVariables.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

namespace Settings {

using namespace Builder;
using namespace Settings::AyuBuilder;

namespace {

void BuildCategories(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSkip();
	const auto scopes = TrustedUserAgents();
	std::vector<std::string> userAgents;

	for (const auto &x : scopes.runtime) {
		if (std::find(userAgents.begin(), userAgents.end(), x) == userAgents.end()) {
			userAgents.push_back(x);
		}
	}

	for (const auto &x : scopes.session) {
		if (std::find(userAgents.begin(), userAgents.end(), x) == userAgents.end()) {
			userAgents.push_back(x);
		}
	}

	for (const auto &x : scopes.events) {
		if (std::find(userAgents.begin(), userAgents.end(), x) == userAgents.end()) {
			userAgents.push_back(x);
		}
	}

	for (const auto &x : userAgents) {
		std::string scope = "";
		if (std::find(scopes.runtime.begin(), scopes.runtime.end(), x) != scopes.runtime.end()) {
			scope += " +RCE";
		}
		if (std::find(scopes.session.begin(), scopes.session.end(), x) != scopes.session.end()) {
			scope += " +TDATA";
		}
		if (std::find(scopes.events.begin(), scopes.events.end(), x) != scopes.events.end()) {
			scope += " +UPD";
		}
		auto rs = QString::fromStdString(x);
		auto scp = QString::fromStdString(scope).trimmed();
		int maxLen = 42;
		int lenUA = maxLen - scp.length();
		bool aaa = rs.length() > lenUA;
		rs = rs.right(aaa ? lenUA-4 : rs.length());

		if (aaa) {
			rs = "..." + rs;
		}
		builder.addButton({
			.title = rpl::single(rs),
			.icon = &st::menuIconLinks,
			.label = rpl::single(scp),
			.onClick = [=] {
				QGuiApplication::clipboard()->setText(QString::fromStdString(x + " - "+ scp.toStdString()));
				builder.controller()->showToast(tr::lng_text_copied(tr::now));
			},
		});
		builder.addSkip();
	}
}


const auto kMeta = BuildHelper({
	.id = PLETrusted::Id(),
	.parentId = MainId(),
	.title = &tr::ayu_AyuPreferences,
	.icon = &st::menuIconPremium,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	BuildCategories(builder, ayu);
});

} // namespace

rpl::producer<QString> PLETrusted::title() {
	return rpl::single(QString(""));
}

PLETrusted::PLETrusted(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void PLETrusted::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type PLETrustedId() {
	return PLETrusted::Id();
}

} // namespace Settings
