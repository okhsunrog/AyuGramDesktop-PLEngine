// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026
Follows GNU GPL v3 and Telegram Desktop licensing.
*/

#pragma once

#include "plengine/platform_compat.h"

#include "styles/style_widgets.h"
#include "ui/widgets/menu/menu.h"
#include "ui/effects/animations.h"
#include "ui/effects/panel_animation.h"
#include "ui/widgets/shadow.h"
#include "ui/round_rect.h"
#include "ui/rp_widget.h"
#include "base/object_ptr.h"
#include "ui/widgets/popup_menu.h"
#include "base/unique_qptr.h"

namespace Ui {
	__declspec(dllexport) class PseudoPopup {
	public:
		Ui::PopupMenu* _menu;
		PseudoPopup(Ui::PopupMenu* menu);

		__declspec(dllexport) not_null<QAction*> addAction(base::unique_qptr<Menu::ItemBase> widget);
		__declspec(dllexport) not_null<QAction*> addAction(
			const std::string text,
			Fn<void()> callback,
			const style::icon* icon = nullptr,
			const style::icon* iconOver = nullptr);
		__declspec(dllexport) not_null<QAction*> addAction(
			const QString& text,
			std::unique_ptr<PopupMenu> submenu,
			const style::icon* icon = nullptr,
			const style::icon* iconOver = nullptr);
		__declspec(dllexport) not_null<QAction*> addSeparator(
			const style::MenuSeparator* st = nullptr);
		__declspec(dllexport) not_null<QAction*> insertAction(
			int position,
			base::unique_qptr<Menu::ItemBase> widget);
		__declspec(dllexport) void removeAction(int position);
		__declspec(dllexport) void clearActions();

		__declspec(dllexport)  [[nodiscard]] const std::vector<not_null<QAction*>>& actions() const;
		__declspec(dllexport) void checkSubmenuShow();
		__declspec(dllexport) bool empty() const;

		__declspec(dllexport) void deleteOnHide(bool del);
		__declspec(dllexport) void popup(const QPoint& p);
		__declspec(dllexport)  [[nodiscard]] static QPoint ConstrainToParentScreen(
			not_null<PseudoPopup*> menu,
			QPoint globalPos);
		__declspec(dllexport) bool prepareGeometryFor(const QPoint& p);
		__declspec(dllexport) void popupPrepared();
		__declspec(dllexport) void hideMenu(bool fast = false);
		__declspec(dllexport) void setTopShift(int topShift);
		__declspec(dllexport) void setForceWidth(int forceWidth);
		__declspec(dllexport) void setForcedOrigin(PanelAnimation::Origin origin);
		__declspec(dllexport) void setForcedVerticalOrigin(Ui::PopupMenu::VerticalOrigin origin);
		__declspec(dllexport) void setAdditionalMenuPadding(QMargins padding, QMargins margins);
		__declspec(dllexport) [[nodiscard]] QMargins additionalMenuPadding() const;
		__declspec(dllexport) [[nodiscard]] QMargins additionalMenuMargins() const;

		__declspec(dllexport) [[nodiscard]] PanelAnimation::Origin preparedOrigin() const;
		__declspec(dllexport) [[nodiscard]] QMargins preparedPadding() const;
		__declspec(dllexport) [[nodiscard]] QMargins preparedMargins() const;
		__declspec(dllexport) [[nodiscard]] bool useTransparency() const;

		__declspec(dllexport) [[nodiscard]] int scrollTop() const;
		__declspec(dllexport) [[nodiscard]] rpl::producer<int> scrollTopValue() const;
	};

} // namespace Ui
