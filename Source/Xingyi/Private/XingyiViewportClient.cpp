// 星移 Xingyi —— 面板的视口客户端（实现）

#include "XingyiViewportClient.h"

#include "XingyiCommon.h"
#include "XingyiLighting.h"
#include "XingyiSettings.h"

#include "Editor.h"
#include "Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Widgets/SWindow.h"
#include "SEditorViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "InputKeyEventArgs.h"
#include "Input/Events.h"

#define LOCTEXT_NAMESPACE "Xingyi"

FXingyiViewportClient::FXingyiViewportClient(int32 InPaneNumber)
	: FEditorViewportClient(nullptr, nullptr, nullptr)
	, PaneNumber(InPaneNumber)
	, SourceLevel(OwnedPreviewScene.GetWorld())
{
	// 先留一份默认显示标志，重新应用设置时从它出发
	DefaultEngineShowFlags = EngineShowFlags;

	SetViewModes(VMI_Lit, VMI_Lit);
	SetRealtime(false);

	BackgroundColor = FLinearColor(0.02f, 0.02f, 0.025f);

	PreviewScene = &OwnedPreviewScene;

	// 预览场景自带的那盏平行光，优先级永远排最后 —— 这样它绝不会和关卡里的太阳「打平」，
	// 「多个定向光源正在竞争…」那条提示从根上不会由它触发。
	// 直接改属性而不是调 SetForwardShadingPriority()，是因为那个会把值夹到 >= 0。
	if (OwnedPreviewScene.DirectionalLight)
	{
		OwnedPreviewScene.DirectionalLight->ForwardShadingPriority = -1;
	}

	ApplyDisplaySettings();
	RefreshPreviewLighting();
}

FLinearColor FXingyiViewportClient::GetBackgroundColor() const
{
	return BackgroundColor;
}

void FXingyiViewportClient::AttachViewportWidget(const TWeakPtr<SEditorViewport>& InViewportWidget)
{
	ViewportWidget = InViewportWidget;
}

FText FXingyiViewportClient::GetSourceLevelName() const
{
	return XingyiCommon::GetLevelDisplayName(SourceLevel.GetAsset());
}

void FXingyiViewportClient::ReportFailure(const FText& Message) const
{
	UE_LOG(LogXingyi, Warning, TEXT("星移：%s"), *Message.ToString());

	FNotificationInfo Info(Message);
	Info.ExpireDuration = 6.0f;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info);
}

//////////////////////////////////////////////////////////////////////////
// 显示与光照
//////////////////////////////////////////////////////////////////////////

void FXingyiViewportClient::ApplyDisplaySettings()
{
	const UXingyiSettings* Settings = GetDefault<UXingyiSettings>();

	// 游戏视图状态先记下来，别被下面的整体重置吃掉
	const bool bWasGameView = IsInGameView();

	// 先回到默认值再往上叠：不这样做的话「性能模式」关掉之后，那些被一次性关掉的高级特性回不来
	EngineShowFlags = DefaultEngineShowFlags;

	EngineShowFlags.Selection = true;
	EngineShowFlags.SelectionOutline = true;
	EngineShowFlags.Grid = Settings->bShowGrid;
	DrawHelper.bDrawGrid = Settings->bShowGrid;

	if (Settings->bPerformanceMode)
	{
		EngineShowFlags.DisableAdvancedFeatures();
	}

	// 必须排在性能模式之后：公告板精灵属于 Advanced 组，会被上面那行一并关掉
	EngineShowFlags.BillboardSprites = Settings->bShowBillboardSprites;

	if (bWasGameView)
	{
		SetGameView(true);
	}

	Invalidate();
}

void FXingyiViewportClient::RefreshPreviewLighting()
{
	const UXingyiSettings* Settings = GetDefault<UXingyiSettings>();

	UDirectionalLightComponent* FallbackSun = OwnedPreviewScene.DirectionalLight;
	USkyLightComponent* PreviewSkyLight = OwnedPreviewScene.SkyLight;

	LevelLightCount = SourceLevel.CountLevelLights(FallbackSun, PreviewSkyLight);

	const bool bLightTheFallback =
		XingyiLighting::ShouldUseFallbackLight(Settings->FallbackLightMode, LevelLightCount);

	if (FallbackSun)
	{
		OwnedPreviewScene.SetLightDirection(Settings->FallbackLightRotation);
		FallbackSun->SetIntensity(Settings->FallbackLightIntensity);
		FallbackSun->SetVisibility(bLightTheFallback, /*bPropagateToChildren=*/true);
	}

	// 面板自带的天光同理：用关卡光照时跟兜底灯一起关掉，免得又叠一层环境光。
	// （它本来就是个没有 cubemap 的 SpecifiedCubemap，关了更干净。）
	if (PreviewSkyLight)
	{
		PreviewSkyLight->SetVisibility(bLightTheFallback, /*bPropagateToChildren=*/true);
	}

	if (bLightTheFallback && LevelLightCount > 0)
	{
		LightingStatusText = FText::Format(
			LOCTEXT("LightingFallbackWithLevel", "兜底光源 + 关卡光照（{0} 盏灯）"),
			FText::AsNumber(LevelLightCount));
	}
	else if (bLightTheFallback)
	{
		LightingStatusText = LOCTEXT("LightingFallbackOnly", "兜底光源已点亮（关卡里没有光源）");
	}
	else
	{
		LightingStatusText = FText::Format(
			LOCTEXT("LightingFromLevelOnly", "使用关卡自身光照（{0} 盏灯）"),
			FText::AsNumber(LevelLightCount));
	}

	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
// 关卡
//////////////////////////////////////////////////////////////////////////

void FXingyiViewportClient::RestoreStoredCamera()
{
	SourceLevel.ApplyStoredCameraView(*this);
}

void FXingyiViewportClient::FrameContentIfRequested()
{
	if (SourceLevel.IsEmpty() || !GetDefault<UXingyiSettings>()->bFrameContentOnLoad)
	{
		return;
	}

	FBox ContentBounds(ForceInit);
	int32 ConsideredActors = 0;
	if (!SourceLevel.ComputeContentBounds(ContentBounds, ConsideredActors))
	{
		UE_LOG(LogXingyi, Display, TEXT("星移：没能算出关卡内容的包围盒，保持关卡自己保存的视角。"));
		return;
	}

	FocusViewportOnBox(ContentBounds, /*bInstant=*/true);

	UE_LOG(LogXingyi, Display, TEXT("星移：相机已框到 %d 个 Actor 上，包围盒 %s ~ %s，框后相机位置 %s。"),
		ConsideredActors,
		*ContentBounds.Min.ToCompactString(),
		*ContentBounds.Max.ToCompactString(),
		*GetViewLocation().ToCompactString());
}

EXingyiLevelLoadResult FXingyiViewportClient::OpenSourceLevel(const FAssetData& LevelAsset)
{
	UE_LOG(LogXingyi, Display, TEXT("星移：请求打开关卡 %s"), *LevelAsset.GetObjectPathString());

	// 换关卡之前，先把上一个关卡留下的选中状态清干净
	DropPaletteSelection();

	const EXingyiLevelLoadResult Result = SourceLevel.Load(LevelAsset);

	if (Result == EXingyiLevelLoadResult::NotALevelAsset)
	{
		ReportFailure(FText::Format(
			LOCTEXT("NotALevelAsset", "「{0}」不是一个可用的关卡资产。"),
			FText::FromString(LevelAsset.GetObjectPathString())));
	}
	else if (Result == EXingyiLevelLoadResult::LoadFailed)
	{
		ReportFailure(FText::Format(
			LOCTEXT("LevelLoadFailed", "关卡「{0}」载入失败，面板保持原样。"),
			FText::FromString(LevelAsset.AssetName.ToString())));
	}

	if (Result != EXingyiLevelLoadResult::Loaded)
	{
		// 上一个关卡这时已经被卸掉了，光照和画面得跟着重算
		RefreshPreviewLighting();
		Invalidate();
		return Result;
	}

	// 相机默认沿用关卡自己保存的编辑器视角（和官方版一致）
	RestoreStoredCamera();

	// 到这里才算真的换成功了，再改面板状态
	UXingyiSettings* Settings = GetMutableDefault<UXingyiSettings>();
	Settings->NoteLevelOpened(SourceLevel.GetAsset(), PaneNumber);

	// World Partition 关卡的内容不在 .umap 里，而是靠流送系统按区域加载，
	// 而预览世界不会自己跑那套流送 —— 所以这里主动把流送关掉，让 WP 把单元全部加载出来。
	if (SourceLevel.IsPartitioned())
	{
		if (Settings->bLoadWorldPartitionCells)
		{
			UE_LOG(LogXingyi, Display, TEXT("星移：%s 是 World Partition 关卡，正在强制加载全部单元…"),
				*SourceLevel.GetAsset().AssetName.ToString());

			const int32 VisibleActors = SourceLevel.ForcePartitionCellsLoaded(/*TimeBudgetSeconds=*/4.0f);

			UE_LOG(LogXingyi, Display, TEXT("星移：World Partition 加载结束，预览世界里可显示的 Actor 数：%d"),
				VisibleActors);
		}
		else
		{
			UE_LOG(LogXingyi, Display, TEXT("星移：%s 是 World Partition 关卡，但设置里关掉了「自动加载全部单元」。"),
				*SourceLevel.GetAsset().AssetName.ToString());
		}
	}

	UE_LOG(LogXingyi, Display, TEXT("星移：关卡 %s 载入完成，可显示 Actor %d 个。"),
		*SourceLevel.GetAsset().AssetName.ToString(), SourceLevel.CountVisibleActors());

	FrameContentIfRequested();

	// 关卡进来之后才知道它有没有光源，光照要在这里重算
	RefreshPreviewLighting();
	Invalidate();

	return EXingyiLevelLoadResult::Loaded;
}

//////////////////////////////////////////////////////////////////////////
// 面板里的点选与拖拽
//////////////////////////////////////////////////////////////////////////

void FXingyiViewportClient::DropPaletteSelection()
{
	AActor* PreviousActor = PaletteSelectedActor.Get();
	if (PreviousActor && GEditor)
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			// 只摘掉我们自己那一个，不动用户在关卡里原本就选中的东西
			Selection->Deselect(PreviousActor);
			GEditor->NoteSelectionChange();
		}
	}

	PaletteSelectedActor = nullptr;
}

void FXingyiViewportClient::MarkPaletteActorSelected(AActor* PaletteActor, bool bNotifySelectionChange)
{
	if (!PaletteActor || !GEditor)
	{
		return;
	}

	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		Selection->Select(PaletteActor, /*bNotify=*/true);
		PaletteSelectedActor = PaletteActor;
	}

	if (bNotifySelectionChange)
	{
		GEditor->NoteSelectionChange();
	}
}

AActor* FXingyiViewportClient::ResolveActorUnderCursor(const FInputKeyEventArgs& InEventArgs)
{
	if (!InEventArgs.Viewport)
	{
		return nullptr;
	}

	HHitProxy* HitProxy = InEventArgs.Viewport->GetHitProxy(
		InEventArgs.Viewport->GetMouseX(), InEventArgs.Viewport->GetMouseY());

	HActor* ActorProxy = HitProxyCast<HActor>(HitProxy);
	if (!ActorProxy || !ActorProxy->Actor)
	{
		return nullptr;
	}

	// 锁了位置的 Actor 拖不动，当作没点中
	return ActorProxy->Actor->IsLockLocation() ? nullptr : ActorProxy->Actor;
}

void FXingyiViewportClient::BeginDragOutOfPalette(AActor* PaletteActor)
{
	if (!PaletteActor)
	{
		return;
	}

	// 面板里的 Actor 属于预览世界，它「代表」的是自己引用的那份资产 ——
	// 把资产拖出去，落到关卡里就是新建一份。灯光、空 Actor 之类没有引用资产，拖不了。
	TArray<UObject*> ReferencedAssets;
	PaletteActor->GetReferencedContentObjects(ReferencedAssets);
	if (ReferencedAssets.Num() == 0 || !ReferencedAssets[0])
	{
		return;
	}

	TSharedPtr<FAssetDragDropOp> DragOperation = FAssetDragDropOp::New(FAssetData(ReferencedAssets[0], true));
	if (!DragOperation.IsValid())
	{
		return;
	}

	MarkPaletteActorSelected(PaletteActor, /*bNotifySelectionChange=*/false);
	PaletteActor->MarkComponentsRenderStateDirty();
	Invalidate();

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.CancelDragDrop();

	TSet<FKey> PressedButtons;
	PressedButtons.Add(EKeys::LeftMouseButton);

	const FPointerEvent PointerEvent(
		SlateApp.GetUserIndexForMouse(),
		FSlateApplicationBase::CursorPointerIndex,
		SlateApp.GetCursorPos(),
		SlateApp.GetLastCursorPos(),
		PressedButtons,
		EKeys::Invalid,
		0,
		FModifierKeysState());

	const FDragDropEvent DragDropEvent(PointerEvent, DragOperation);

	// 官方版这里直接 ToSharedRef()：面板控件还没挂进窗口的时候就是个空指针崩溃。
	// 这里先判空，找不到窗口就干脆不发起这次拖拽。
	TSharedPtr<SWindow> OwnerWindow;
	if (ViewportWidget.IsValid())
	{
		OwnerWindow = SlateApp.FindWidgetWindow(ViewportWidget.Pin().ToSharedRef());
	}

	if (!OwnerWindow.IsValid())
	{
		UE_LOG(LogXingyi, Warning, TEXT("星移：还没找到面板所在的窗口，这次拖拽取消。"));
		return;
	}

	bDragOutActive = true;
	SlateApp.ProcessDragEnterEvent(OwnerWindow.ToSharedRef(), DragDropEvent);
}

bool FXingyiViewportClient::HandlePaletteClick(const FInputKeyEventArgs& InEventArgs)
{
	// 已经有一次拖拽在进行（多半就是我们自己发起的那次假拖拽），不重复处理
	if (FSlateApplication::Get().IsDragDropping())
	{
		return true;
	}

	AActor* HitActor = ResolveActorUnderCursor(InEventArgs);
	if (!HitActor)
	{
		// 点在空处：把面板自己留下的选中状态清掉，剩下的交给基类（框选、相机操作）
		DropPaletteSelection();
		return false;
	}

	TArray<UObject*> ReferencedAssets;
	HitActor->GetReferencedContentObjects(ReferencedAssets);

	DropPaletteSelection();

	if (ReferencedAssets.Num() == 0 || !ReferencedAssets[0])
	{
		// 没有可拖拽的资产（灯光、空 Actor 之类），退化成「点一下选中它」
		MarkPaletteActorSelected(HitActor, /*bNotifySelectionChange=*/true);
		Invalidate();
		return true;
	}

	BeginDragOutOfPalette(HitActor);
	return true;
}

bool FXingyiViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (InEventArgs.Key == EKeys::LeftMouseButton && HandlePaletteClick(InEventArgs))
	{
		return true;
	}

	// 面板里要能像视口一样自由飞行：把右键按左键喂给基类。
	// 左键走到这里说明是点在空处，不需要改写。
	FInputKeyEventArgs ForwardedArgs = InEventArgs;
	if (ForwardedArgs.Key == EKeys::RightMouseButton)
	{
		ForwardedArgs.Key = EKeys::LeftMouseButton;
	}

	return FEditorViewportClient::InputKey(ForwardedArgs);
}

void FXingyiViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// 勾了「实时」才真的实时：让预览世界里的粒子、动画、贴图流送动起来
	if (IsRealtime())
	{
		if (UWorld* World = OwnedPreviewScene.GetWorld())
		{
			World->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}

	// 假拖拽放下或者被取消之后，把面板里的 Actor 从编辑器选中集里摘干净 ——
	// 否则它会一直挂在细节面板上，被「删除选中项」之类的操作误伤。
	if (bDragOutActive && !FSlateApplication::Get().IsDragDropping())
	{
		bDragOutActive = false;
		DropPaletteSelection();
	}
}

#undef LOCTEXT_NAMESPACE
