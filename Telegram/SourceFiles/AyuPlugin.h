#include "plengine/plugin_api.h"
#pragma once
#include <history/history.h>
#include <ayu/ui/settings/ayu_builder.h>
#include <settings/settings_builder.h>
#include <plengine/settings/settings_plugins.h>

/*
Emil Kh, AKA Pomorgite - t.me/Pomorgite // pmrgt.com
AyuGram Plugin engine, 2026 // t.me/ayuplugg
Follows GNU GPL v3 and Telegram Desktop licensing.
*/

#include "plengine/message_hooks.h"

#include <cstdint>
#include <string>

namespace Ui { class PseudoPopup; }
enum FilteredState
{
	NoMatch,
	Filtered,
	RejectFurtherFiltering
};

typedef void (*InternalSetup)(void);
typedef void (*InternalLoop)(void);
typedef FilteredState (*InternalDoFilterHistoryItem)(HistoryItem *);
typedef bool (*InternalExcludeDeletion)(HistoryItem *);
using InternalDoPreProcessMessage = PLEngine::MessageHook;
using InternalLegacyDoPreProcessMessage = PLEngine::LegacyMessageHook;
typedef void (*InternalDrawGUI)(Settings::Builder::SectionBuilder& builder, Settings::AyuBuilder::AyuSectionBuilder& ayu, Settings::PLEPlugins* ple);
typedef bool (*InternalIsOnline)(void);
typedef AyuPlugin *(*InternalPluginInfo)();

typedef void (*InternalDrawPopupItem)(Ui::PseudoPopup *, HistoryItem *);
