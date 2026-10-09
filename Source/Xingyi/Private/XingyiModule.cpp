// 星移 Xingyi —— 模块入口（实现）

#include "XingyiModule.h"

#include "SXingyiPalette.h"
#include "XingyiCommon.h"
#include "XingyiGroupDrop.h"

#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "Xingyi"

DEFINE_LOG_CATEGORY(LogXingyi);

namespace
{
	/** 标签页 ID 的前缀，后面直接拼面板编号 */
	const TCHAR* const PaneTabIdPrefix = TEXT("XingyiPaletteTab");

	/** 工具菜单里那个「星移」分组用的图标：引擎自带的星形，和插件图标（星群）对得上 */
	FSlateIcon GetPaneIcon()
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Star");
	}
}

FName FXingyiModule::MakePaneTabId(int32 PaneNumber)
{
	return FName(*FString::Printf(TEXT("%s%d"), PaneTabIdPrefix, PaneNumber));
}

void FXingyiModule::StartupModule()
{
	// 成组拖拽的收尾要盯着编辑器"放置了新 Actor"的广播，模块起来就得挂上
	XingyiGroupDrop::Register();

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	const TSharedRef<FWorkspaceItem> MenuGroup = MenuStructure.GetToolsCategory()->AddGroup(
		LOCTEXT("XingyiMenuGroup", "星移"),
		LOCTEXT("XingyiMenuTooltip", "打开一个星移面板，把关卡当作素材板浏览。"),
		GetPaneIcon(),
		/*bSortSubGroups=*/true);

	for (int32 PaneNumber = 1; PaneNumber <= MaxPanes; ++PaneNumber)
	{
		RegisterPane(PaneNumber, MenuGroup);
	}
}

void FXingyiModule::ShutdownModule()
{
	XingyiGroupDrop::Unregister();

	for (int32 PaneNumber = 1; PaneNumber <= MaxPanes; ++PaneNumber)
	{
		UnregisterPane(PaneNumber);
	}

	LivePanes.Reset();
}

void FXingyiModule::RegisterPane(int32 PaneNumber, const TSharedRef<FWorkspaceItem>& MenuGroup)
{
	const FName TabId = MakePaneTabId(PaneNumber);

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			TabId,
			FOnSpawnTab::CreateRaw(this, &FXingyiModule::SpawnPane, PaneNumber))
		.SetDisplayName(MakePaneTitle(PaneNumber))
		.SetTooltipText(LOCTEXT("XingyiMenuTooltip", "打开一个星移面板，把关卡当作素材板浏览。"))
		.SetGroup(MenuGroup)
		.SetIcon(GetPaneIcon());
}

void FXingyiModule::UnregisterPane(int32 PaneNumber)
{
	const FName TabId = MakePaneTabId(PaneNumber);

	if (FGlobalTabmanager::Get()->HasTabSpawner(TabId))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
	}
}

TSharedRef<SDockTab> FXingyiModule::SpawnPane(const FSpawnTabArgs& SpawnTabArgs, int32 PaneNumber)
{
	// 官方版在这个位置是 check()：同一个面板 ID 被二次 spawn（典型场景是刚关掉、
	// 控件还没销毁完就又拉开）会直接断言崩溃。这里降级成一条开发期提示，照样把新面板建出来。
	const TWeakPtr<SXingyiPalette>* Existing = LivePanes.Find(MakePaneTabId(PaneNumber));
	ensureMsgf(!(Existing && Existing->IsValid()),
		TEXT("星移：%d 号面板的上一个实例还没销毁，这次会再建一个。"), PaneNumber);

	const TSharedRef<SXingyiPalette> NewPalette = SNew(SXingyiPalette, PaneNumber);
	LivePanes.Add(MakePaneTabId(PaneNumber), NewPalette);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(TAttribute<FText>::Create(
			TAttribute<FText>::FGetter::CreateRaw(this, &FXingyiModule::MakePaneTitle, PaneNumber)))
		[
			NewPalette
		];
}

FText FXingyiModule::MakePaneTitle(int32 PaneNumber) const
{
	const FText PaneLabel = FText::Format(
		LOCTEXT("XingyiPaneLabel", "星移面板{0}"), FText::AsNumber(PaneNumber));

	const TWeakPtr<SXingyiPalette>* LivePane = LivePanes.Find(MakePaneTabId(PaneNumber));
	if (LivePane && LivePane->IsValid())
	{
		// 面板里打开了关卡就把关卡名一起挂上，多开几个面板时一眼看出谁在看哪个关卡
		const FText LevelName = LivePane->Pin()->GetSourceLevelName();
		if (!LevelName.IsEmpty())
		{
			return FText::Format(LOCTEXT("XingyiPaneLabelWithLevel", "{0}：{1}"), PaneLabel, LevelName);
		}
	}

	return PaneLabel;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXingyiModule, Xingyi)
