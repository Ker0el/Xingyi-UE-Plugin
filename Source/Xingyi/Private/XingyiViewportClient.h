// 星移 Xingyi —— 面板的视口客户端

#pragma once

#include "PreviewScene.h"
#include "EditorViewportClient.h"

#include "XingyiSourceLevel.h"

class AActor;
class SEditorViewport;

/**
 * 星移面板的视口客户端。
 *
 * 它只做三件事：
 *   1) 提供一个 FPreviewScene 当预览世界（素材板关卡就载进这里面，它自带一盏平行光）；
 *   2) 按设置摆弄显示标志（网格、公告板精灵、性能模式）和预览光照；
 *   3) 处理「在面板里点一下 Actor，直接把它拖进当前关卡」这条交互。
 *
 * 关卡的搬运本身不在这个类里 —— 全交给 FXingyiSourceLevel。
 *
 * 【预览光照】
 * FPreviewScene 自带的平行光如果和关卡里的太阳优先级相同，渲染器会报
 * 「多个定向光源正在竞争成为唯一一个用于前向着色、半透明、水或体积雾的光源」。
 * 星移把那盏灯钉死在最低优先级（-1），从根上不可能和关卡的太阳打平；
 * 亮不亮则由 EXingyiFallbackLightMode 决定。
 */
class FXingyiViewportClient : public FEditorViewportClient
{
public:
	/** @param InPaneNumber 面板编号，从 1 开始 */
	explicit FXingyiViewportClient(int32 InPaneNumber);

	//~ FViewportClient
	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override;
	//~ End of FViewportClient

	//~ FEditorViewportClient
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End of FEditorViewportClient

	/** 换一个素材板关卡 */
	EXingyiLevelLoadResult OpenSourceLevel(const FAssetData& LevelAsset);

	/** 当前素材板关卡；没打开时是无效的 FAssetData */
	const FAssetData& GetSourceLevelAsset() const { return SourceLevel.GetAsset(); }

	/** 当前素材板关卡的名字；没打开时为空 */
	FText GetSourceLevelName() const;

	/** 这个视口属于几号面板（从 1 开始） */
	int32 GetPaneNumber() const { return PaneNumber; }

	/** 面板控件建好之后回填进来，假拖拽要靠它找到宿主窗口 */
	void AttachViewportWidget(const TWeakPtr<SEditorViewport>& InViewportWidget);

	/** 把相机摆回关卡自己保存的那个编辑器视角 */
	void RestoreStoredCamera();

	/** 重新算一遍预览光照。换关卡、改设置之后都要调 */
	void RefreshPreviewLighting();

	/** 把显示相关的设置重新套一遍（从默认标志出发再叠加，这样「性能模式」关得掉） */
	void ApplyDisplaySettings();

	/** 把面板里选中的那个 Actor 从编辑器选中集里摘掉 */
	void DropPaletteSelection();

	// ---- 面板底部状态条要用的信息 ----

	int32 GetLevelLightCount() const { return LevelLightCount; }
	FText GetLightingStatusText() const { return LightingStatusText; }

	/** 关卡载进来了，但预览里一个 Actor 都没有（World Partition 关卡的典型症状） */
	bool IsSourceLevelEmpty() const { return SourceLevel.IsEmpty(); }

	/** 载入的是 World Partition 关卡 */
	bool IsSourceLevelPartitioned() const { return SourceLevel.IsPartitioned(); }

	/** 预览世界（诊断和测试用） */
	UWorld* GetPreviewWorld() const { return OwnedPreviewScene.GetWorld(); }

private:
	/** 给用户一条失败提示：写日志 + 弹一条编辑器通知 */
	void ReportFailure(const FText& Message) const;

	/** 「打开关卡后自动框住内容」那一项 */
	void FrameContentIfRequested();

	/**
	 * 处理一次落在面板里的左键。
	 * @return true 表示这次输入已经被吃掉，不该再交给基类
	 */
	bool HandlePaletteClick(const FInputKeyEventArgs& InEventArgs);

	/** 从命中信息里找出一个能拖拽的 Actor；找不到返回 nullptr */
	static AActor* ResolveActorUnderCursor(const FInputKeyEventArgs& InEventArgs);

	/** 替这个 Actor 发起一次「把它的资产拖出去」的假拖拽 */
	void BeginDragOutOfPalette(AActor* PaletteActor);

	/** 把面板里那个 Actor 记进编辑器选中集；拖拽期间它需要处于选中状态 */
	void MarkPaletteActorSelected(AActor* PaletteActor, bool bNotifySelectionChange);

	/** 面板编号，从 1 开始 */
	int32 PaneNumber;

	/** 预览世界。必须先于 SourceLevel 声明 —— SourceLevel 构造时要拿它的世界 */
	FPreviewScene OwnedPreviewScene;

	/** 素材板关卡（载入 / 卸载 / 数 Actor 都在它里面） */
	FXingyiSourceLevel SourceLevel;

	/** 面板控件，用来找宿主窗口 */
	TWeakPtr<SEditorViewport> ViewportWidget;

	/** 面板里点选的那个 Actor。它属于预览世界，绝不能留在编辑器选中集里 */
	TWeakObjectPtr<AActor> PaletteSelectedActor;

	/** 视口背景色 */
	FLinearColor BackgroundColor;

	/**
	 * 构造时的显示标志快照。重新应用设置时先回到它再叠加，
	 * 否则「性能模式」关掉之后那些高级特性回不来。
	 *
	 * ⚠️ 必须带初始化参数：FEngineShowFlags() 的无参构造在引擎里会直接 FatalError
	 * （"for internal usage only for hot-reload purposes"）。
	 */
	FEngineShowFlags DefaultEngineShowFlags{ ESFIM_Editor };

	/** 面板底部显示的光照状态文案 */
	FText LightingStatusText;

	/** 预览世界里关卡自带的光源数量（面板自带的那两盏不算） */
	int32 LevelLightCount = 0;

	/** 正在用假拖拽把资产拖出面板 */
	bool bDragOutActive = false;
};
