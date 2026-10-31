// Copyright 2026 StarGame. All Rights Reserved.
// Xingyi - editor text helper (implementation)

#include "XingyiText.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace XingyiText
{
	bool IsChineseUI()
	{
		const FInternationalization& Internationalization = FInternationalization::Get();
		const TSharedPtr<const FCulture> CurrentLanguage = Internationalization.GetCurrentLanguage();
		if (!CurrentLanguage.IsValid())
		{
			return false;
		}

		return CurrentLanguage->GetName().StartsWith(TEXT("zh"), ESearchCase::IgnoreCase);
	}

	FText Pick(const TCHAR* InEnglish, const TCHAR* InChinese)
	{
		return FText::FromString(IsChineseUI() ? InChinese : InEnglish);
	}
}
