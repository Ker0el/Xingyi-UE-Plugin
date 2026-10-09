// 星移 Xingyi —— 面板的视口客户端（实现）

#include "XingyiViewportClient.h"

#include "XingyiCommon.h"
#include "XingyiGroupDrop.h"
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

	// World Partition 关卡的内容不在 .umap 里，而是靠流送系统按区域加载，而预览世界不会自己跑
	// 那套流送 —— 所以这类关卡只能看到「始终加载」的那一部分（天空、大气、雾、光照、地形壳）。
	// 引擎没有公开接口能绕开这一点（唯一有效的那个是全局编辑器开关，翻转会连主编辑器世界一起
	// 全量加载，不能碰），所以这里只记一行日志，界面上的说明交给状态条和空状态卡片。
	if (SourceLevel.IsPartitioned())
	{
		UE_LOG(LogXingyi, Display, TEXT("星移：%s 是 World Partition 关卡，只能显示「始终加载」的部分。"),
			*SourceLevel.GetAsset().AssetName.ToString());
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
	USelection* Selection = GEditor ? GEditor->GetSelectedActors() : nullptr;
	bool bDeselectedAny = false;

	for (const TWeakObjectPtr<AActor>& WeakActor : PaletteSelection)
	{
		AActor* Actor = WeakActor.Get();
		if (Actor && Selection)
		{
			// 只摘掉我们自己那几个，不动用户在关卡里原本就选中的东西
			Selection->Deselect(Actor);
			bDeselectedAny = true;
		}
	}

	PaletteSelection.Reset();

	if (bDeselectedAny && GEditor)
	{
		GEditor->NoteSelectionChange();
	}

	// ⚠️ 必须自己重画面板。
	// NoteSelectionChange() 只会让**关卡视口**重画；面板里的预览世界是另一个世界，
	// 面板又是个独立的 FEditorViewportClient、默认还不实时（SetRealtime(false)），
	// 所以不主动 Invalidate 的话，选中状态是清了，但描边会一直挂在画面上，
	// 看着就像"还在选中"。
	Invalidate();
}

int32 FXingyiViewportClient::GetPaletteSelectionCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& WeakActor : PaletteSelection)
	{
		Count += WeakActor.IsValid() ? 1 : 0;
	}
	return Count;
}

TArray<AActor*> FXingyiViewportClient::GetSelectedPaletteActors() const
{
	TArray<AActor*> Actors;
	Actors.Reserve(PaletteSelection.Num());

	for (const TWeakObjectPtr<AActor>& WeakActor : PaletteSelection)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			Actors.Add(Actor);
		}
	}

	return Actors;
}

bool FXingyiViewportClient::IsPaletteActorSelected(const AActor* PaletteActor) const
{
	for (const TWeakObjectPtr<AActor>& WeakActor : PaletteSelection)
	{
		if (WeakActor.Get() == PaletteActor)
		{
			return true;
		}
	}

	return false;
}

bool FXingyiViewportClient::HasDraggableAsset(AActor* PaletteActor)
{
	if (!IsValid(PaletteActor))
	{
		return false;
	}

	// 面板里的 Actor 靠"自己引用的那份资产"才能拖出去。
	// 灯光、天空球、体积框之类没有引用资产，拖不了 —— 没必要让它们进选中集。
	TArray<UObject*> ReferencedAssets;
	PaletteActor->GetReferencedContentObjects(ReferencedAssets);
	return ReferencedAssets.Num() > 0 && ReferencedAssets[0] != nullptr;
}

void FXingyiViewportClient::SetMultiSelectMode(bool bEnabled)
{
	if (bMultiSelectMode == bEnabled)
	{
		return;
	}

	bMultiSelectMode = bEnabled;

	// 关掉多选就把选中清干净 —— 否则关掉之后点已选中的还会拖整组，
	// 那就不是"和以前一模一样"了。
	if (!bMultiSelectMode)
	{
		DropPaletteSelection();
	}

	Invalidate();
}

void FXingyiViewportClient::SetPaletteActorSelected(AActor* PaletteActor, bool bSelected, bool bNotifySelectionChange)
{
	if (!IsValid(PaletteActor) || !GEditor)
	{
		return;
	}

	if (bSelected == IsPaletteActorSelected(PaletteActor))
	{
		return; // 状态没变，别白折腾编辑器的选中集
	}

	USelection* Selection = GEditor->GetSelectedActors();

	if (bSelected)
	{
		PaletteSelection.Add(PaletteActor);

		// 也塞进编辑器选中集：这样预览里会给它描边，用户看得见自己选了哪些
		if (Selection)
		{
			Selection->Select(PaletteActor, /*bNotify=*/true);
		}
	}
	else
	{
		PaletteSelection.RemoveAllSwap([PaletteActor](const TWeakObjectPtr<AActor>& WeakActor)
		{
			return WeakActor.Get() == PaletteActor;
		});

		if (Selection)
		{
			Selection->Deselect(PaletteActor);
		}
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

void FXingyiViewportClient::BeginDragOutOfPalette(const TArray<AActor*>& PaletteActors, AActor* AnchorActor)
{
	if (!IsValid(AnchorActor) || PaletteActors.Num() == 0)
	{
		return;
	}

	// 起点 Actor 在源关卡里的变换。其余的都相对它记 ——
	// 拖拽落点会把起点放在鼠标那儿，其余的就绕着它摆回去。
	const FTransform AnchorTransform = AnchorActor->GetActorTransform();

	TArray<FAssetData> Assets;
	XingyiGroupDrop::FPayload Payload;
	int32 ActorsWithAsset = 0;

	for (AActor* Actor : PaletteActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		// 面板里的 Actor 属于预览世界，它「代表」的是自己引用的那份资产 ——
		// 把资产拖出去，落到关卡里就是新建一份。灯光、空 Actor 之类没有引用资产，拖不了。
		TArray<UObject*> ReferencedAssets;
		Actor->GetReferencedContentObjects(ReferencedAssets);
		if (ReferencedAssets.Num() == 0 || !ReferencedAssets[0])
		{
			UE_LOG(LogXingyi, Display, TEXT("星移：%s 没有可拖拽的资产，跳过。"), *Actor->GetName());
			continue;
		}

		++ActorsWithAsset;

		UObject* Asset = ReferencedAssets[0];
		const FTransform ActorTransform = Actor->GetActorTransform();

		XingyiGroupDrop::FPlacement Placement;
		Placement.LocationOffset = ActorTransform.GetLocation() - AnchorTransform.GetLocation();
		Placement.Rotation = ActorTransform.GetRotation();
		Placement.Scale = ActorTransform.GetScale3D();
		Placement.bAnchor = (Actor == AnchorActor);

		Payload.FindOrAdd(Asset).Add(Placement);

		// 起点排在资产列表最前面：UE 的放置顺序跟着这个列表走，
		// 收尾那边按资产 + 顺序对号入座，起点一定配得上。
		// ⚠️ 第二个参数是 bAllowBlueprintClass（老版本叫 bInUseLoadingClass），不是"用加载类"。
		const FAssetData AssetData(Asset, /*bAllowBlueprintClass=*/true);
		if (Placement.bAnchor)
		{
			Assets.Insert(AssetData, 0);
		}
		else
		{
			Assets.Add(AssetData);
		}
	}

	UE_LOG(LogXingyi, Display, TEXT("星移：成组拖拽 —— 选中 %d 个，有资产的 %d 个，送进拖拽 %d 个资产，起点=%s。"),
		PaletteActors.Num(), ActorsWithAsset, Assets.Num(), *AnchorActor->GetName());

	if (Assets.Num() == 0)
	{
		return;
	}

	TSharedPtr<FAssetDragDropOp> DragOperation = FAssetDragDropOp::New(Assets);
	if (!DragOperation.IsValid())
	{
		return;
	}

	// 拖着鼠标时那个缩略图，FAssetDragDropOp 内部**只画第一个资产**
	// （InitThumbnail 里用的是 AssetData[0]），而且 UE 的落点预览会把所有对象
	// 全叠在光标那一点上、位置由引擎写死（见 FActorPositioning::GetSurfaceAlignedTransform，
	// SurfaceLocation 就是光标落点，StartTransform 只参与旋转和缩放）。
	// 所以拖多个时，光看光标那儿根本看不出拖了几个 —— 这里补一句实情。
	if (Assets.Num() > 1)
	{
		// 图标沿用拖拽操作自己的（一般是空的，那也没关系，提示文字本来就不需要图标）
		DragOperation->SetToolTip(
			FText::Format(
				LOCTEXT("DragGroupHint", "{0} 个对象 · 放下后按原布局摆放"),
				FText::AsNumber(Assets.Num())),
			DragOperation->GetIcon());
	}

	// 把摆放数据交出去。UE 会把这组资产全放在光标那一点上，
	// 放置完它广播 OnNewActorsPlaced，收尾模块收到之后把它们摆回相对位置。
	XingyiGroupDrop::Begin(MoveTemp(Payload), AnchorTransform.GetRotation());

	// ⚠️ 必须走 SetPaletteActorSelected —— 它会同时记进 PaletteSelection。
	// 光去动编辑器选中集的话，拖拽结束时的清理会漏掉这几个 Actor，
	// 它们就会永远留在编辑器里被描边（按 Delete 会误删关卡里的东西）。
	// 这正是官方 Actor Palette 上那个「跨世界选中残留」的老毛病。
	for (AActor* Actor : PaletteActors)
	{
		SetPaletteActorSelected(Actor, /*bSelected=*/true, /*bNotifySelectionChange=*/false);

		if (IsValid(Actor))
		{
			Actor->MarkComponentsRenderStateDirty();
		}
	}

	if (GEditor)
	{
		GEditor->NoteSelectionChange();
	}

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

	// —— 多选模式 ——
	// 只有工具栏上那个「多选」按钮开着时才走这条路。默认关着，行为完全等于以前。
	if (bMultiSelectMode)
	{
		// Ctrl + 左键 = 移出选中。多选模式下"点已选中的"已经被"拖出去"占用了，
		// 这里是唯一的减选入口。
		if (FSlateApplication::Get().GetModifierKeys().IsControlDown())
		{
			SetPaletteActorSelected(HitActor, /*bSelected=*/false);
			Invalidate();
			return true;
		}

		// 点还没选上的 -> 加进选中
		if (!IsPaletteActorSelected(HitActor))
		{
			// 没有可拖拽资产的不收进来。
			// 关卡里满天都是天空球、大气、体积框，想点"空处"时射线十有八九打在它们身上；
			// 收进来只会让选中计数虚高，拖的时候又被跳过，反而看不懂。
			if (!HasDraggableAsset(HitActor))
			{
				UE_LOG(LogXingyi, Display, TEXT("星移：%s 没有可拖拽的资产，不进选中集。"), *HitActor->GetName());
				return true;
			}

			SetPaletteActorSelected(HitActor, /*bSelected=*/true);
			Invalidate();
			return true;
		}

		// 点已经选上的 -> 把这一组一起拖出去，它当起点
		BeginDragOutOfPalette(GetSelectedPaletteActors(), HitActor);
		return true;
	}

	// —— 普通模式：完全等于原来的行为 —— 点一下就拖这一个 ——

	// 顺手把多选模式下可能残留的选中清掉
	DropPaletteSelection();

	if (!HasDraggableAsset(HitActor))
	{
		// 没有可拖拽的资产（灯光、空 Actor 之类），退化成「点一下选中它」
		SetPaletteActorSelected(HitActor, /*bSelected=*/true);
		Invalidate();
		return true;
	}

	BeginDragOutOfPalette({ HitActor }, HitActor);
	return true;
}

bool FXingyiViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	// ⚠️ InputKey 在「按下」和「松开」时各来一次。面板的点选 / 拖拽只认**按下**：
	// 不加这个判断的话，按住 Shift 点一下会在按下时加选、松手时又减掉，等于白点。
	// （修饰键读的是"这一下点击发生时"的状态，不是切换模式 —— 松开 Shift 立刻恢复正常点击。）
	if (InEventArgs.Key == EKeys::LeftMouseButton
		&& InEventArgs.Event == IE_Pressed
		&& HandlePaletteClick(InEventArgs))
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

	// 假拖拽放下或者被取消之后，做两件收尾的事：
	//   1) 成组拖拽还没摆完的（被取消、或者有资产没放下），在这里把已经收到的摆好；
	//   2) 把面板里的 Actor 从编辑器选中集里摘干净 —— 否则它们会一直挂在细节面板上，
	//      被「删除选中项」之类的操作误伤。
	if (bDragOutActive && !FSlateApplication::Get().IsDragDropping())
	{
		bDragOutActive = false;
		XingyiGroupDrop::Flush();
		DropPaletteSelection();
	}
}

#undef LOCTEXT_NAMESPACE
