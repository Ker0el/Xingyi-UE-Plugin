// 星移 Xingyi —— 预览视口

#pragma once

#include "SEditorViewport.h"
#include "Misc/EngineVersionComparison.h"

#include "XingyiLighting.h"

class FXingyiViewportClient;
class UXingyiSettings;

/** 星移面板里的预览视口。它自带一条工具条（选关卡 / 光照 / 实时 / 相机…） */
class SXingyiViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SXingyiViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FXingyiViewportClient>& InViewportClient);

	//~ SEditorViewport
	// UE 5.5 起引擎把工具栏接口从 MakeViewportToolbar() 换成了 BuildViewportToolbar()，
	// 5.4 只有旧的那个（新版本里旧的还被标了 final）。按版本走，两头都能编。
#if UE_VERSION_OLDER_THAN(5, 5, 0)
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
#else
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;
#endif
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	//~ End of SEditorViewport

	// 把关卡资产直接拖到视口上也能打开。
	// Slate 的拖放事件只发给「指针下那一串控件」里第一个返回 Handled 的，
	// 所以在视口和面板两层都接一遍，哪层先拿到都能兜住。
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	/** 把某个关卡资产载入这个面板 */
	void OpenLevel(const FAssetData& LevelAsset);

private:
	/** 工具条上那几个按钮 */
	FReply OnReloadClicked();
	FReply OnResetCameraClicked();
	FReply OnToggleGameViewClicked();
	FReply OnTogglePinClicked();

	ECheckBoxState GetRealtimeState() const;
	void OnRealtimeChanged(ECheckBoxState NewState);
	bool IsGameViewOn() const;
	FText GetPinButtonLabel() const;

	/** 工具条上两个下拉菜单的内容 */
	TSharedRef<SWidget> BuildLevelMenu();
	TSharedRef<SWidget> BuildLightingMenu();

	/** 换兜底光模式：存盘 + 刷新 */
	void SetFallbackLightMode(EXingyiFallbackLightMode NewMode);

	/** 把一个开关型设置取反、存盘、刷新视口 */
	void FlipSetting(const TFunctionRef<void(UXingyiSettings&)>& Flip);

	/** 显示相关的设置和光照都重刷一遍 */
	void ApplySettingsAndRefresh();

	TSharedPtr<FXingyiViewportClient> TypedViewportClient;
};
