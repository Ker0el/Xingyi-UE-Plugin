// Copyright 2026 StarGame. All Rights Reserved.
// 星移 Xingyi —— 预览视口（实现）

#include "SXingyiViewport.h"

#include "XingyiCommon.h"
#include "XingyiMapMenu.h"
#include "XingyiSettings.h"
#include "XingyiViewportClient.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "XingyiText.h"

#define LOCTEXT_NAMESPACE "Xingyi"

void SXingyiViewport::Construct(const FArguments& InArgs, const TSharedRef<FXingyiViewportClient>& InViewportClient)
{
	TypedViewportClient = InViewportClient;
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

TSharedRef<FEditorViewportClient> SXingyiViewport::MakeEditorViewportClient()
{
	return TypedViewportClient.ToSharedRef();
}

//////////////////////////////////////////////////////////////////////////
// 工具条
//////////////////////////////////////////////////////////////////////////

#if UE_VERSION_OLDER_THAN(5, 5, 0)
TSharedPtr<SWidget> SXingyiViewport::MakeViewportToolbar()
#else
TSharedPtr<SWidget> SXingyiViewport::BuildViewportToolbar()
#endif
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			// 选择关卡
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SXingyiViewport::BuildLevelMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(XY_TEXT("Select Level", "选择关卡"))
					.ToolTipText(XY_TEXT("Pick a level to use as an asset board, or drag a level in from the Content Browser", "挑一个关卡当作素材板，也可以直接从内容浏览器拖关卡进来"))
				]
			]

			// 重载
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SXingyiViewport::OnReloadClicked)
				.Text(XY_TEXT("Reload", "重载"))
				.ToolTipText(XY_TEXT("Reload the level this panel opened last time", "重新载入这个面板上次打开过的关卡"))
			]

			// 收藏 / 取消收藏
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SXingyiViewport::OnTogglePinClicked)
				.ToolTipText(XY_TEXT("Add or remove the current level from favorites", "把当前关卡加入收藏 / 从收藏里移除"))
				[
					SNew(STextBlock)
					.Text(this, &SXingyiViewport::GetPinButtonLabel)
				]
			]

			// 光照
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SXingyiViewport::BuildLightingMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(XY_TEXT("Lighting", "光照"))
					.ToolTipText(XY_TEXT("Preview lighting strategy: use the level's own lights when it has them, light the fallback sun only when it has none", "预览场景的光照策略：关卡自带光源时用关卡的光，关卡没光源时才点亮兜底光"))
				]
			]

			// 实时
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.0f, 0.0f, 2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SXingyiViewport::GetRealtimeState)
				.OnCheckStateChanged(this, &SXingyiViewport::OnRealtimeChanged)
				.ToolTipText(XY_TEXT("Turn on to actually refresh the preview in real time (particles, animation, texture streaming)", "勾上才会真的实时刷新预览（粒子、动画、贴图流送）"))
				[
					SNew(STextBlock)
					.Text(XY_TEXT("Realtime", "实时"))
				]
			]

			// 游戏视图
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SXingyiViewport::OnToggleGameViewClicked)
				.ToolTipText(XY_TEXT("Show in game view (hides editor helpers)", "按游戏视图显示（隐藏编辑器辅助显示）"))
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return IsGameViewOn()
							? XY_TEXT("Game View ✓", "游戏视图 ✓")
							: XY_TEXT("Game View", "游戏视图");
					})
				]
			]

			// 重置相机
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SXingyiViewport::OnResetCameraClicked)
				.Text(XY_TEXT("Reset Camera", "重置相机"))
				.ToolTipText(XY_TEXT("Return to the camera position stored in this level", "回到这个关卡保存时的相机位置"))
			]

			// 多选
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.0f, 0.0f, 2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SXingyiViewport::GetMultiSelectState)
				.OnCheckStateChanged(this, &SXingyiViewport::OnMultiSelectChanged)
				.ToolTipText(XY_TEXT("Multi-select: click an unselected actor to add it; click a selected one to drag the group into the level. Ctrl + click removes it from the selection", "多选：点没选中的 Actor 把它加进选中；点已选中的就把这一组一起拖进关卡。Ctrl + 点可以移出选中"))
				[
					SNew(STextBlock)
					.Text(XY_TEXT("Multi-Select", "多选模式"))
				]
			]

			// 清空选中
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.OnClicked(this, &SXingyiViewport::OnClearSelectionClicked)
				.Text(XY_TEXT("Clear Selection", "清空选中"))
				.ToolTipText(XY_TEXT("Clears the selection in the panel. Levels are full of sky spheres, atmosphere and volumes, so clicking empty space is not a reliable way to clear it - hence a dedicated button", "清空面板里的选中。关卡里满天都是天空球、大气和体积框，想靠点空处来清空基本点不到，所以单给一个按钮"))
			]
		];
}

TSharedRef<SWidget> SXingyiViewport::BuildLevelMenu()
{
	const FAssetData CurrentLevel = TypedViewportClient.IsValid()
		? TypedViewportClient->GetSourceLevelAsset()
		: FAssetData();

	return XingyiMapMenu::Build(CurrentLevel, FXingyiOnLevelChosen::CreateSP(this, &SXingyiViewport::OpenLevel));
}

TSharedRef<SWidget> SXingyiViewport::BuildLightingMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	const UXingyiSettings* Settings = GetDefault<UXingyiSettings>();

	MenuBuilder.BeginSection("XingyiLightingMode", XY_TEXT("Preview Lighting", "预览光照"));
	{
		auto AddLightModeEntry = [this, &MenuBuilder, Settings](
			EXingyiFallbackLightMode Mode, const FText& Tooltip)
		{
			MenuBuilder.AddMenuEntry(
				XingyiLighting::DescribeMode(Mode),
				Tooltip,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SXingyiViewport::SetFallbackLightMode, Mode),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([Settings, Mode]()
					{
						return Settings->FallbackLightMode == Mode;
					})),
				NAME_None,
				EUserInterfaceActionType::RadioButton);
		};

		AddLightModeEntry(EXingyiFallbackLightMode::Auto,
			XY_TEXT("When the level has its own lights, use them exclusively - the preview matches the level", "关卡自带光源时完全使用关卡的光照，观感和关卡一致"));
		AddLightModeEntry(EXingyiFallbackLightMode::AlwaysOn,
			XY_TEXT("The fallback light stays on. It always sits at the lowest priority, so it never competes with the level's sun", "兜底灯一直亮着。它永远排最低优先级，不会和关卡的太阳打架"));
		AddLightModeEntry(EXingyiFallbackLightMode::AlwaysOff,
			XY_TEXT("Use only the level's own lighting, with no fill light at all", "完全使用关卡自己的光照，一点补光都不加"));
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("XingyiDisplay", XY_TEXT("Display", "显示"));
	{
		MenuBuilder.AddMenuEntry(XY_TEXT("Show Ground Grid", "显示地面网格"), FText::GetEmpty(), FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					FlipSetting([](UXingyiSettings& S) { S.bShowGrid = !S.bShowGrid; });
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Settings]() { return Settings->bShowGrid; })),
			NAME_None, EUserInterfaceActionType::ToggleButton);

		MenuBuilder.AddMenuEntry(XY_TEXT("Show Billboard Sprites", "显示公告板精灵"),
			XY_TEXT("Sprite icons for particles and material spheres; they become invisible when turned off", "粒子、材质球这类精灵图标；关掉它们会看不见"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					FlipSetting([](UXingyiSettings& S) { S.bShowBillboardSprites = !S.bShowBillboardSprites; });
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Settings]() { return Settings->bShowBillboardSprites; })),
			NAME_None, EUserInterfaceActionType::ToggleButton);

		MenuBuilder.AddMenuEntry(XY_TEXT("Performance Mode", "性能模式"),
			XY_TEXT("Turns off advanced features such as Lumen for a higher frame rate; changes how the preview looks", "关闭 Lumen 等高级特性换取帧率，会改变预览观感"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					FlipSetting([](UXingyiSettings& S) { S.bPerformanceMode = !S.bPerformanceMode; });
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([Settings]() { return Settings->bPerformanceMode; })),
			NAME_None, EUserInterfaceActionType::ToggleButton);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("XingyiHint", XY_TEXT("Current State", "当前状态"));
	{
		// 一条不可点的说明，告诉用户强度、角度这些去哪儿调
		MenuBuilder.AddMenuEntry(
			XY_TEXT("Sun intensity and angle: Project Settings - Plugins - Xingyi", "阳光强度、角度等项目设置 → 插件 → 星移"),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

//////////////////////////////////////////////////////////////////////////
// 交互
//////////////////////////////////////////////////////////////////////////

void SXingyiViewport::ApplySettingsAndRefresh()
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->ApplyDisplaySettings();
		TypedViewportClient->RefreshPreviewLighting();
	}
}

void SXingyiViewport::FlipSetting(const TFunctionRef<void(UXingyiSettings&)>& Flip)
{
	UXingyiSettings* Settings = GetMutableDefault<UXingyiSettings>();
	Flip(*Settings);
	Settings->PersistIfAllowed();
	ApplySettingsAndRefresh();
}

void SXingyiViewport::SetFallbackLightMode(EXingyiFallbackLightMode NewMode)
{
	FlipSetting([NewMode](UXingyiSettings& S) { S.FallbackLightMode = NewMode; });
}

FReply SXingyiViewport::OnReloadClicked()
{
	if (!TypedViewportClient.IsValid())
	{
		return FReply::Handled();
	}

	// 优先用这个面板上次打开的关卡；没有就重载当前这个（等价于强制刷新一遍）
	const UXingyiSettings* Settings = GetDefault<UXingyiSettings>();
	const FAssetData LastLevel = Settings->GetPaneLastLevel(TypedViewportClient->GetPaneNumber());

	if (LastLevel.IsValid())
	{
		OpenLevel(LastLevel);
	}
	else if (TypedViewportClient->GetSourceLevelAsset().IsValid())
	{
		OpenLevel(TypedViewportClient->GetSourceLevelAsset());
	}

	return FReply::Handled();
}

FReply SXingyiViewport::OnResetCameraClicked()
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->RestoreStoredCamera();
		TypedViewportClient->Invalidate();
	}
	return FReply::Handled();
}

FReply SXingyiViewport::OnToggleGameViewClicked()
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->SetGameView(!TypedViewportClient->IsInGameView());
		TypedViewportClient->Invalidate();
	}
	return FReply::Handled();
}

FReply SXingyiViewport::OnTogglePinClicked()
{
	if (!TypedViewportClient.IsValid())
	{
		return FReply::Handled();
	}

	const FAssetData CurrentLevel = TypedViewportClient->GetSourceLevelAsset();
	if (CurrentLevel.IsValid())
	{
		GetMutableDefault<UXingyiSettings>()->ToggleLevelPinned(CurrentLevel);
	}

	return FReply::Handled();
}

bool SXingyiViewport::IsGameViewOn() const
{
	return TypedViewportClient.IsValid() && TypedViewportClient->IsInGameView();
}

FText SXingyiViewport::GetPinButtonLabel() const
{
	const FAssetData CurrentLevel = TypedViewportClient.IsValid()
		? TypedViewportClient->GetSourceLevelAsset()
		: FAssetData();

	if (!CurrentLevel.IsValid())
	{
		return XY_TEXT("Favorite", "收藏");
	}

	return GetDefault<UXingyiSettings>()->IsLevelPinned(CurrentLevel)
		? XY_TEXT("Favorited", "已收藏")
		: XY_TEXT("Favorite", "收藏");
}

ECheckBoxState SXingyiViewport::GetRealtimeState() const
{
	return (TypedViewportClient.IsValid() && TypedViewportClient->IsRealtime())
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SXingyiViewport::OnRealtimeChanged(ECheckBoxState NewState)
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->SetRealtime(NewState == ECheckBoxState::Checked);
		TypedViewportClient->Invalidate();
	}
}

ECheckBoxState SXingyiViewport::GetMultiSelectState() const
{
	return (TypedViewportClient.IsValid() && TypedViewportClient->IsMultiSelectMode())
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SXingyiViewport::OnMultiSelectChanged(ECheckBoxState NewState)
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->SetMultiSelectMode(NewState == ECheckBoxState::Checked);
	}
}

FReply SXingyiViewport::OnClearSelectionClicked()
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->DropPaletteSelection();
	}

	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////
// 关卡
//////////////////////////////////////////////////////////////////////////

void SXingyiViewport::OpenLevel(const FAssetData& LevelAsset)
{
	if (TypedViewportClient.IsValid())
	{
		TypedViewportClient->OpenSourceLevel(LevelAsset);
	}
}

FReply SXingyiViewport::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (!AssetOp.IsValid())
	{
		return FReply::Unhandled();
	}

	for (const FAssetData& Asset : AssetOp->GetAssets())
	{
		if (XingyiCommon::IsLevelAsset(Asset))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SXingyiViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (!AssetOp.IsValid())
	{
		return FReply::Unhandled();
	}

	for (const FAssetData& Asset : AssetOp->GetAssets())
	{
		if (XingyiCommon::IsLevelAsset(Asset))
		{
			UE_LOG(LogXingyi, Display, TEXT("Xingyi: level %s loaded by drag and drop (received by the viewport)"), *Asset.GetObjectPathString());
			OpenLevel(Asset);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
