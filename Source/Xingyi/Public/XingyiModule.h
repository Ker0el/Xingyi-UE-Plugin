// 星移 Xingyi —— 模块入口

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SXingyiPalette;
class FWorkspaceItem;
class SDockTab;
class FSpawnTabArgs;

/**
 * 星移的模块入口。
 *
 * 它只管一件事：注册「星移面板1 ~ 星移面板4」这四个可停靠标签页，并把它们挂到
 * 编辑器的「工具」菜单下。面板本身的逻辑全在 SXingyiPalette 里。
 */
class FXingyiModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** 可以同时打开的面板数量。面板编号从 1 开始，到 MaxPanes 为止 */
	static constexpr int32 MaxPanes = 4;

private:
	/** 面板编号 -> 标签页 ID */
	static FName MakePaneTabId(int32 PaneNumber);

	/** 注册 / 注销一个面板的标签页；菜单组由调用方建好传进来 */
	void RegisterPane(int32 PaneNumber, const TSharedRef<FWorkspaceItem>& MenuGroup);
	void UnregisterPane(int32 PaneNumber);

	/** 标签页被拉开时真的造一个面板出来 */
	TSharedRef<SDockTab> SpawnPane(const FSpawnTabArgs& SpawnTabArgs, int32 PaneNumber);

	/** 标签页标题。面板里打开了关卡就把关卡名一起挂上，一眼看出谁在看谁 */
	FText MakePaneTitle(int32 PaneNumber) const;

	/**
	 * 标签页 ID -> 当前活着的面板控件。
	 * 只有 MaxPanes 个键，用 WeakPtr 是为了不拖住面板的生命周期（面板关了它自己就失效）。
	 */
	TMap<FName, TWeakPtr<SXingyiPalette>> LivePanes;
};
