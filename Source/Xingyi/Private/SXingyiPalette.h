// 星移 Xingyi —— 面板本体
//
// 一个面板 = 一个预览视口 + 两层空状态引导（盖在视口上）+ 一条底部状态条。

#pragma once

#include "Widgets/SCompoundWidget.h"

class FDragDropEvent;
class FXingyiViewportClient;
class SXingyiViewport;

/** 星移面板本体：预览视口 + 空状态引导 + 底部状态条 */
class SXingyiPalette : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SXingyiPalette) {}
	SLATE_END_ARGS()

	/** @param InPaneNumber 面板编号，从 1 开始 */
	void Construct(const FArguments& InArgs, int32 InPaneNumber);
	virtual ~SXingyiPalette();

	// 把关卡资产从内容浏览器直接拖到面板上也能打开
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	/** 当前面板打开的关卡名；没打开时为空。标签页标题用它 */
	FText GetSourceLevelName() const;

	/** 面板编号，从 1 开始 */
	int32 GetPaneNumber() const { return PaneNumber; }

private:
	/** 底部状态条左边：「当前关卡：xxx」 */
	FText GetLevelLabel() const;

	/** 底部状态条右边：「预览光照：…」 */
	FText GetLightingLabel() const;

	/** 底部状态条右边那句操作提示；多选时改成显示选了几个 */
	FText GetStatusHint() const;

	/** 还没打开任何关卡时，显示居中的引导 */
	EVisibility GetEmptyHintVisibility() const;

	/** 关卡打开了、但预览里一个 Actor 都没有（World Partition 关卡的典型症状） */
	EVisibility GetPreviewEmptyVisibility() const;

	/** 空状态里那个「打开关卡…」按钮的菜单 */
	TSharedRef<SWidget> BuildLevelMenu();

	/** 从一次拖放事件里挑出可以当素材板的关卡；挑不到返回无效的 FAssetData */
	static FAssetData PickLevelFromDragDrop(const FDragDropEvent& DragDropEvent);

	TSharedPtr<SXingyiViewport> Viewport;
	TSharedPtr<FXingyiViewportClient> ViewportClient;

	int32 PaneNumber = 1;
};
