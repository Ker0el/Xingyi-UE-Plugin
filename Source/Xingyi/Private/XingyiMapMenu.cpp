// Copyright 2026 StarGame. All Rights Reserved.
// 星移 Xingyi —— 「选关卡」菜单（实现）

#include "XingyiMapMenu.h"

#include "XingyiCommon.h"
#include "XingyiSettings.h"

#include "Editor.h"
#include "Engine/World.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "XingyiText.h"

#define LOCTEXT_NAMESPACE "Xingyi"

namespace
{
	/** 内容浏览器里选中项最多列几条，免得菜单被撑爆 */
	constexpr int32 MaxContentBrowserEntries = 4;

	/** 资产选择器弹出面板的边长 */
	constexpr float AssetPickerSize = 320.0f;

	/** 用一组关卡造一个子菜单；列表为空时放一条灰掉的说明 */
	void AddLevelListSubMenu(FMenuBuilder& MenuBuilder,
		const FText& Label,
		const FText& Tooltip,
		const FText& EmptyHint,
		const TArray<FAssetData>& Levels,
		const FXingyiOnLevelChosen& OnLevelChosen)
	{
		MenuBuilder.AddSubMenu(Label, Tooltip,
			FNewMenuDelegate::CreateLambda(
				[Levels, EmptyHint, OnLevelChosen](FMenuBuilder& SubMenuBuilder)
				{
					if (Levels.Num() == 0)
					{
						SubMenuBuilder.AddMenuEntry(EmptyHint, EmptyHint, FSlateIcon(), FUIAction());
						return;
					}

					for (const FAssetData& Level : Levels)
					{
						SubMenuBuilder.AddMenuEntry(
							XingyiCommon::GetLevelDisplayName(Level),
							FText::Format(XY_TEXT("Use {0} as an asset board", "把 {0} 当作素材板"),
								XingyiCommon::GetLevelPackageName(Level)),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([Level, OnLevelChosen]()
							{
								OnLevelChosen.ExecuteIfBound(Level);
							})));
					}
				}));
	}
}

FText XingyiMapMenu::DescribePinAction(const FAssetData& CurrentLevel)
{
	if (!CurrentLevel.IsValid())
	{
		return XY_TEXT("Favorite / Unfavorite", "收藏 / 取消收藏");
	}

	const bool bAlreadyPinned = GetDefault<UXingyiSettings>()->IsLevelPinned(CurrentLevel);
	return bAlreadyPinned
		? FText::Format(XY_TEXT("Unfavorite {0}", "取消收藏 {0}"), XingyiCommon::GetLevelDisplayName(CurrentLevel))
		: FText::Format(XY_TEXT("Favorite {0}", "收藏 {0}"), XingyiCommon::GetLevelDisplayName(CurrentLevel));
}

TSharedRef<SWidget> XingyiMapMenu::Build(const FAssetData& CurrentLevel, FXingyiOnLevelChosen OnLevelChosen)
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	const UXingyiSettings* Settings = GetDefault<UXingyiSettings>();

	// ---- 浏览任意关卡 ----
	MenuBuilder.AddSubMenu(
		XY_TEXT("Browse Levels...", "浏览关卡…"),
		XY_TEXT("Pick a level from the Content Browser to use as an asset board", "从内容浏览器里挑一个关卡当素材板"),
		FNewMenuDelegate::CreateLambda([CurrentLevel, OnLevelChosen](FMenuBuilder& SubMenuBuilder)
		{
			FContentBrowserModule& ContentBrowserModule =
				FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

			FAssetPickerConfig PickerConfig;
			PickerConfig.Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
			PickerConfig.InitialAssetViewType = EAssetViewType::List;
			PickerConfig.bAllowDragging = false;
			PickerConfig.bAllowNullSelection = true;
			PickerConfig.bFocusSearchBoxWhenOpened = true;
			PickerConfig.InitialAssetSelection = CurrentLevel;
			PickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(
				[OnLevelChosen](const FAssetData& ChosenLevel)
				{
					FSlateApplication::Get().DismissAllMenus();
					OnLevelChosen.ExecuteIfBound(ChosenLevel);
				});

			SubMenuBuilder.BeginSection("XingyiBrowse", XY_TEXT("Browse", "浏览"));
			SubMenuBuilder.AddWidget(
				SNew(SBox)
				.WidthOverride(AssetPickerSize)
				.HeightOverride(AssetPickerSize)
				[
					ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
				],
				FText::GetEmpty());
			SubMenuBuilder.EndSection();
		}));

	// ---- 内容浏览器里正选中的关卡 ----
	{
		TArray<FAssetData> SelectedAssets;
		GEditor->GetContentBrowserSelections(/*out*/ SelectedAssets);

		int32 Remaining = MaxContentBrowserEntries;
		for (const FAssetData& Asset : SelectedAssets)
		{
			if (!XingyiCommon::IsLevelAsset(Asset))
			{
				continue;
			}

			MenuBuilder.AddMenuEntry(
				FText::Format(XY_TEXT("Open {0} (Content Browser selection)", "打开 {0}（内容浏览器选中项）"),
					XingyiCommon::GetLevelDisplayName(Asset)),
				FText::Format(XY_TEXT("Use {0} as an asset board", "把 {0} 当作素材板"),
					XingyiCommon::GetLevelPackageName(Asset)),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Asset, OnLevelChosen]()
				{
					OnLevelChosen.ExecuteIfBound(Asset);
				})));

			if (--Remaining == 0)
			{
				break;
			}
		}
	}

	// ---- 最近打开 / 收藏 ----
	// 记录里存的是路径，这里才去资产注册表把关卡查出来；查不到的（关卡被删了）就跳过，
	// 免得菜单里挂着一条点不动的死条目。
	auto ResolveRecords = [](const TArray<FXingyiLevelRecord>& Records, TArray<FAssetData>& OutLevels)
	{
		OutLevels.Reset();
		for (const FXingyiLevelRecord& Record : Records)
		{
			const FAssetData Asset = Record.ResolveAsset();
			if (Asset.IsValid())
			{
				OutLevels.Add(Asset);
			}
		}
	};

	TArray<FXingyiLevelRecord> RecentRecords;
	Settings->CollectRecentRecords(RecentRecords, Settings->RecentHistoryLimit);

	TArray<FAssetData> RecentLevels;
	ResolveRecords(RecentRecords, RecentLevels);
	AddLevelListSubMenu(MenuBuilder,
		XY_TEXT("Recent", "最近打开"),
		XY_TEXT("Levels opened in a Xingyi panel recently", "最近在星移面板里打开过的关卡"),
		XY_TEXT("No levels opened yet", "还没有打开过任何关卡"),
		RecentLevels, OnLevelChosen);

	TArray<FXingyiLevelRecord> PinnedRecords;
	Settings->CollectPinnedRecords(PinnedRecords);

	TArray<FAssetData> PinnedLevels;
	ResolveRecords(PinnedRecords, PinnedLevels);
	AddLevelListSubMenu(MenuBuilder,
		XY_TEXT("Favorites", "收藏"),
		XY_TEXT("Levels marked as favorites", "标记为收藏的关卡"),
		XY_TEXT("No favorite levels yet", "还没有收藏任何关卡"),
		PinnedLevels, OnLevelChosen);

	// ---- 收藏 / 取消收藏当前这个 ----
	if (CurrentLevel.IsValid())
	{
		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry(
			DescribePinAction(CurrentLevel),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([CurrentLevel]()
			{
				GetMutableDefault<UXingyiSettings>()->ToggleLevelPinned(CurrentLevel);
			})));
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
