// Copyright 2026 StarGame. All Rights Reserved.
// Xingyi - editor text helper
//
// The plugin ships an English UI by default and keeps the Chinese UI for
// editors whose display language is Chinese. Every user-facing string goes
// through XY_TEXT(English, Chinese); the English string is what an English
// editor (and the Fab review build) shows.

#pragma once

#include "CoreMinimal.h"

namespace XingyiText
{
	/** True when the editor's current language is Chinese (zh, zh-Hans, zh-Hant, ...) */
	bool IsChineseUI();

	/** Pick the Chinese string on Chinese editors, the English one everywhere else */
	FText Pick(const TCHAR* InEnglish, const TCHAR* InChinese);
}

/** User-facing text: XY_TEXT("Create Level", "新建关卡") */
#define XY_TEXT(English, Chinese) (XingyiText::Pick(TEXT(English), TEXT(Chinese)))
