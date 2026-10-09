// 星移 Xingyi —— 成组拖拽的收尾（实现）
//
// 见头文件里那段说明。这里补一点实现细节：
//
// 【为什么要处理"预览"】
// 拖拽过程中，关卡视口每帧都会重新建一批**临时** Actor 来显示落点预览
// （DropObjectsAtCoordinates 用 bCreateDropPreview=true 调进来，那些 Actor 带 RF_Transient）。
// UE 把它们也全摆在光标那一点上，所以预览看起来是**一堆东西叠在一起**，
// 容易让人以为"只拖过来了一个"。
// 这些临时 Actor 同样会走 OnNewActorsPlaced 广播，所以这里对它们也摆一次 —— 预览就能
// 直接看出这组东西的摆法。预览每一帧都会重来，所以每个资产的读指针用完就归零。

#include "XingyiGroupDrop.h"

#include "XingyiCommon.h"

#include "Editor.h"
#include "GameFramework/Actor.h"

namespace XingyiGroupDrop
{
namespace
{
	using FCollected = TArray<TPair<AActor*, FPlacement>>;

	/** 这次拖拽的摆放数据；空表示没有在途的成组拖拽 */
	FPayload GPayload;

	/** 起点 Actor 在源关卡里的朝向 */
	FQuat GAnchorRotation = FQuat::Identity;

	/** 已经收到、等着摆的（正式放置） */
	FCollected GCollected;

	/** 还差几个资产没收到（正式放置） */
	int32 GRemaining = 0;

	bool GPending = false;

	FDelegateHandle GHandle;

	// ---- 预览专用 ----

	/**
	 * 每个资产的读指针。预览一帧一轮，读完归零等下一帧。
	 * 正式放置不吃这个指针，所以两边不会互相干扰。
	 */
	TMap<UObject*, int32> GPreviewCursor;

	/** 预览里当前这个"基准"临时 Actor；被销毁后自动失效，等下一轮重建 */
	TWeakObjectPtr<AActor> GPreviewAnchor;

	/**
	 * 把一个刚放下的 Actor 摆到"相对基准"的位置上。
	 *
	 * @param AnchorSourceRotation 基准 Actor 在源关卡里的朝向 ——
	 *        落点可能给它转了角度（斜面），其余的要跟着一起转，整组才不会散架
	 */
	void PlaceRelativeTo(AActor* Placed, const FPlacement& Placement,
		AActor* AnchorPlaced, const FQuat& AnchorSourceRotation)
	{
		if (!IsValid(Placed) || !IsValid(AnchorPlaced))
		{
			return;
		}

		// 缩放照搬源 Actor：资产里存的是默认缩放，实例上改过的只有这里保得住
		Placed->SetActorScale3D(Placement.Scale);

		// 基准自己保留 UE 放在光标处的那个位置和朝向，只补缩放
		if (Placed == AnchorPlaced)
		{
			return;
		}

		const FQuat DeltaRotation = AnchorPlaced->GetActorQuat() * AnchorSourceRotation.Inverse();
		Placed->SetActorLocationAndRotation(
			AnchorPlaced->GetActorLocation() + DeltaRotation.RotateVector(Placement.LocationOffset),
			DeltaRotation * Placement.Rotation);
	}

	/** 把正式放置收到的这批摆好 */
	void ApplyCollected()
	{
		if (GCollected.Num() == 0)
		{
			return;
		}

		// 起点落到哪儿，其余的就绕着它摆
		AActor* AnchorPlaced = nullptr;
		for (const TPair<AActor*, FPlacement>& Pair : GCollected)
		{
			if (Pair.Value.bAnchor)
			{
				AnchorPlaced = Pair.Key;
				break;
			}
		}

		// 起点没放下（或者压根没标）就拿第一个收到的当基准，总比不摆强
		if (!AnchorPlaced)
		{
			AnchorPlaced = GCollected[0].Key;
		}

		for (const TPair<AActor*, FPlacement>& Pair : GCollected)
		{
			if (IsValid(Pair.Key))
			{
				Pair.Key->Modify();
			}
			PlaceRelativeTo(Pair.Key, Pair.Value, AnchorPlaced, GAnchorRotation);
		}

		UE_LOG(LogXingyi, Display, TEXT("星移：成组拖拽收尾，摆了 %d 个（基准 %s）。"),
			GCollected.Num(), IsValid(AnchorPlaced) ? *AnchorPlaced->GetName() : TEXT("<无>"));

		GCollected.Reset();
	}

	/** 处理一次"预览"广播：摆好临时 Actor，让拖动时就能看出这组的摆法 */
	void HandlePreview(UObject* Asset, const TArray<FPlacement>& Queue, const TArray<AActor*>& PlacedActors)
	{
		int32& Cursor = GPreviewCursor.FindOrAdd(Asset);

		// 上一轮已经读完 -> 这一帧是新一轮，从头开始
		if (Cursor >= Queue.Num())
		{
			Cursor = 0;
			if (Cursor >= Queue.Num())
			{
				return;
			}
		}

		const int32 Take = FMath::Min(Queue.Num() - Cursor, PlacedActors.Num());
		for (int32 Index = 0; Index < Take; ++Index)
		{
			AActor* PreviewActor = PlacedActors[Index];
			const FPlacement& Placement = Queue[Cursor + Index];

			if (Placement.bAnchor)
			{
				// 新一轮的基准。起点永远排在资产列表最前面，所以它一定先到。
				GPreviewAnchor = PreviewActor;
			}
			else if (AActor* Anchor = GPreviewAnchor.Get())
			{
				PlaceRelativeTo(PreviewActor, Placement, Anchor, GAnchorRotation);
			}
		}

		Cursor += Take;
		if (Cursor >= Queue.Num())
		{
			Cursor = 0;
		}
	}

	/** FEditorDelegates::OnNewActorsPlaced 的处理函数 */
	void HandleNewActorsPlaced(UObject* Asset, const TArray<AActor*>& PlacedActors)
	{
		if (!GPending || !Asset || PlacedActors.Num() == 0)
		{
			return;
		}

		TArray<FPlacement>* Queue = GPayload.Find(Asset);
		if (!Queue || Queue->Num() == 0)
		{
			return;
		}

		// 拖拽过程中的落点预览：那些是 RF_Transient 的临时 Actor，只摆不走账
		if (PlacedActors[0]->HasAnyFlags(RF_Transient))
		{
			HandlePreview(Asset, *Queue, PlacedActors);
			return;
		}

		// 一个资产偶尔会一次建出好几个 Actor，按顺序一对一配上
		const int32 Take = FMath::Min(Queue->Num(), PlacedActors.Num());
		for (int32 Index = 0; Index < Take; ++Index)
		{
			GCollected.Emplace(PlacedActors[Index], (*Queue)[Index]);
		}
		Queue->RemoveAt(0, Take, EAllowShrinking::No);
		GRemaining -= Take;

		UE_LOG(LogXingyi, Display, TEXT("星移：收到放置 %s —— 放下 %d 个，收下 %d 个，还差 %d 个。"),
			*Asset->GetName(), PlacedActors.Num(), Take, GRemaining);

		// 全收齐了就当场摆 —— 这样不会出现"先叠在光标上、下一帧才散开"的一帧跳变
		if (GRemaining <= 0)
		{
			ApplyCollected();
			Cancel();
		}
	}
} // namespace

void Begin(FPayload&& InPayload, const FQuat& AnchorRotation)
{
	// 上一次没结掉的直接丢掉
	Cancel();

	GRemaining = 0;
	for (const TPair<UObject*, TArray<FPlacement>>& Pair : InPayload)
	{
		GRemaining += Pair.Value.Num();
	}

	if (GRemaining <= 0)
	{
		return;
	}

	GPayload = MoveTemp(InPayload);
	GAnchorRotation = AnchorRotation;
	GPending = true;
}

void Cancel()
{
	GPayload.Empty();
	GCollected.Reset();
	GPreviewCursor.Reset();
	GPreviewAnchor.Reset();
	GRemaining = 0;
	GPending = false;
	GAnchorRotation = FQuat::Identity;
}

void Flush()
{
	if (!GPending)
	{
		return;
	}

	// 可能有资产没放下（收不齐），这时候把已经收到的先摆好，剩下的不管了
	if (GRemaining > 0 && GCollected.Num() > 0)
	{
		UE_LOG(LogXingyi, Display, TEXT("星移：成组拖拽收尾（拖拽已结束，还差 %d 个没收到）。"), GRemaining);
	}

	ApplyCollected();
	Cancel();
}

bool IsPending()
{
	return GPending;
}

void Register()
{
	Unregister();
	GHandle = FEditorDelegates::OnNewActorsPlaced.AddStatic(&HandleNewActorsPlaced);
}

void Unregister()
{
	if (GHandle.IsValid())
	{
		FEditorDelegates::OnNewActorsPlaced.Remove(GHandle);
		GHandle.Reset();
	}

	Cancel();
}

} // namespace XingyiGroupDrop
