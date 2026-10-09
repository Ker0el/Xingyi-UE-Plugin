// 星移 Xingyi —— 面板本体（实现）

#include "SXingyiPalette.h"

#include "SXingyiViewport.h"
#include "XingyiCommon.h"
#include "XingyiMapMenu.h"
#include "XingyiSettings.h"
#include "XingyiViewportClient.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "DragAndDrop/AssetDragDropOp.h"

#define LOCTEXT_NAMESPACE "Xingyi"

namespace
{
	/** 空状态那块浮层的配色 */
	const FLinearColor HintTextColor(0.65f, 0.65f, 0.65f);
	const FLinearColor HintWarnColor(0.85f, 0.78f, 0.45f);
	const FLinearColor HintFaintColor(0.55f, 0.55f, 0.55f);
	const FLinearColor StatusFaintColor(0.6f, 0.6f, 0.6f);

	/** 浮层顶部要留出的高度，免得盖住工具条那排按钮 */
	constexpr float HintTopPadding = 40.0f;
}

void SXingyiPalette::Construct(const FArguments& InArgs, int32 InPaneNumber)
{
	PaneNumber = InPaneNumber;

	// 视口客户端要先建出来，视口控件构造时要把它递进去
	ViewportClient = MakeShareable(new FXingyiViewportClient(PaneNumber));

	const TSharedRef<SXingyiViewport> ViewportWidget = SNew(SXingyiViewport, ViewportClient.ToSharedRef());
	Viewport = ViewportWidget;
	ViewportClient->AttachViewportWidget(ViewportWidget);

	ChildSlot
	[
		SNew(SVerticalBox)

		// 预览视口，上面盖两层：没打开关卡的引导 / 打开了但里面没东西的提示
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)

			// 第 0 层：视口本体
			+ SOverlay::Slot()
			[
				ViewportWidget
			]

			// 第 1 层：还没打开关卡时的居中引导
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, HintTopPadding, 0.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(24.0f, 20.0f))
				.Visibility(this, &SXingyiPalette::GetEmptyHintVisibility)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("EmptyHintTitle", "这个星移面板还没有打开关卡"))
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					.Padding(0.0f, 6.0f, 0.0f, 12.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("EmptyHintBody", "挑一个关卡当作素材板，里面的 Actor 点一下就能拖进当前关卡"))
						.ColorAndOpacity(HintTextColor)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SComboButton)
						.ContentPadding(FMargin(16.0f, 6.0f))
						.OnGetMenuContent(this, &SXingyiPalette::BuildLevelMenu)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OpenLevelButton", "打开关卡…"))
						]
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("EmptyHintDrag", "也可以直接把内容浏览器里的关卡资产拖到本面板上"))
						.ColorAndOpacity(HintFaintColor)
					]
				]
			]

			// 第 2 层：关卡打开了，但预览世界里一个 Actor 都没载进来
			// （World Partition / 开放世界关卡的典型症状，说清楚免得用户以为坏了）
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, HintTopPadding, 0.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(24.0f, 20.0f))
				.Visibility(this, &SXingyiPalette::GetPreviewEmptyVisibility)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PreviewEmptyTitle", "这个关卡在面板里没有可预览的 Actor"))
						.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PreviewEmptyLine1", "World Partition（开放世界）关卡的内容存放在外部 Actor 文件里，"))
						.ColorAndOpacity(HintTextColor)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PreviewEmptyLine2", "由流送系统按区域加载。面板已经试着把它全部加载出来，但没拿到东西。"))
						.ColorAndOpacity(HintTextColor)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PreviewEmptyLine3", "请换一个普通关卡当素材板；"))
						.ColorAndOpacity(HintWarnColor)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PreviewEmptyLine4", "或者把要用的 Actor 先复制到一个普通关卡里，再用面板打开那个关卡。"))
						.ColorAndOpacity(HintWarnColor)
					]
				]
			]
		]

		// 底部状态条：现在看的是哪个关卡、预览用的是谁的光照
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(FMargin(6.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SXingyiPalette::GetLevelLabel)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(16.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SXingyiPalette::GetLightingLabel)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SXingyiPalette::GetStatusHint)
					.ColorAndOpacity(StatusFaintColor)
				]
			]
		]
	];
}

SXingyiPalette::~SXingyiPalette()
{
	// 面板关掉时，把预览世界里被选中的那个 Actor 从编辑器选中集里摘干净。
	// 不摘的话它会一直挂在细节面板上，被「删除选中项」之类的操作误伤。
	if (ViewportClient.IsValid())
	{
		ViewportClient->DropPaletteSelection();
	}

	Viewport.Reset();
	ViewportClient.Reset();
}

FText SXingyiPalette::GetSourceLevelName() const
{
	return ViewportClient.IsValid() ? ViewportClient->GetSourceLevelName() : FText::GetEmpty();
}

FText SXingyiPalette::GetLevelLabel() const
{
	const FText LevelName = GetSourceLevelName();

	return LevelName.IsEmpty()
		? LOCTEXT("LevelLabelNone", "当前关卡：未选择")
		: FText::Format(LOCTEXT("LevelLabel", "当前关卡：{0}"), LevelName);
}

FText SXingyiPalette::GetLightingLabel() const
{
	if (!ViewportClient.IsValid())
	{
		return FText::GetEmpty();
	}

	FText Status = FText::Format(
		LOCTEXT("LightingLabel", "预览光照：{0}"), ViewportClient->GetLightingStatusText());

	if (ViewportClient->IsSourceLevelPartitioned())
	{
		// 说清楚为什么这个关卡里东西这么少，别让用户以为坏了
		Status = FText::Format(
			LOCTEXT("LightingLabelPartitioned", "{0}    World Partition 关卡：只显示始终加载的部分"),
			Status);
	}

	return Status;
}

FText SXingyiPalette::GetStatusHint() const
{
	if (!ViewportClient.IsValid())
	{
		return FText::GetEmpty();
	}

	// 普通模式：和以前一样，一句话说明白就够了
	if (!ViewportClient->IsMultiSelectMode())
	{
		return LOCTEXT("StatusHintNormal", "左键点 Actor 直接拖进关卡");
	}

	// 多选模式：把当前选了几个、下一步该干什么写清楚
	const int32 SelectedCount = ViewportClient->GetPaletteSelectionCount();
	if (SelectedCount > 0)
	{
		return FText::Format(
			LOCTEXT("StatusHintMultiSelected", "已选 {0} 个 · 拖动其中一个放入 · Ctrl+点移出"),
			FText::AsNumber(SelectedCount));
	}

	return LOCTEXT("StatusHintMultiEmpty", "多选：点 Actor 加进选中");
}

EVisibility SXingyiPalette::GetEmptyHintVisibility() const
{
	const bool bHasLevel = ViewportClient.IsValid() && ViewportClient->GetSourceLevelAsset().IsValid();
	return bHasLevel ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SXingyiPalette::GetPreviewEmptyVisibility() const
{
	if (!ViewportClient.IsValid() || !ViewportClient->GetSourceLevelAsset().IsValid())
	{
		return EVisibility::Collapsed;
	}

	return ViewportClient->IsSourceLevelEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SXingyiPalette::BuildLevelMenu()
{
	if (!ViewportClient.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("PaneNotReady", "面板还没准备好"));
	}

	const TSharedPtr<SXingyiViewport> LiveViewport = Viewport;
	if (!LiveViewport.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("PaneNotReady", "面板还没准备好"));
	}

	return XingyiMapMenu::Build(
		ViewportClient->GetSourceLevelAsset(),
		FXingyiOnLevelChosen::CreateSP(LiveViewport.ToSharedRef(), &SXingyiViewport::OpenLevel));
}

FAssetData SXingyiPalette::PickLevelFromDragDrop(const FDragDropEvent& DragDropEvent)
{
	const TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (!AssetOp.IsValid())
	{
		return FAssetData();
	}

	for (const FAssetData& Asset : AssetOp->GetAssets())
	{
		if (XingyiCommon::IsLevelAsset(Asset))
		{
			return Asset;
		}
	}

	return FAssetData();
}

FReply SXingyiPalette::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	return PickLevelFromDragDrop(DragDropEvent).IsValid() ? FReply::Handled() : FReply::Unhandled();
}

FReply SXingyiPalette::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const FAssetData DroppedLevel = PickLevelFromDragDrop(DragDropEvent);
	if (!DroppedLevel.IsValid())
	{
		return FReply::Unhandled();
	}

	if (Viewport.IsValid())
	{
		UE_LOG(LogXingyi, Display, TEXT("星移：拖放载入关卡 %s（面板层收到）"), *DroppedLevel.GetObjectPathString());
		Viewport->OpenLevel(DroppedLevel);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
