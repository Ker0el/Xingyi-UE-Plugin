// 星移 Xingyi —— 成组拖拽的收尾
//
// 【为什么需要这个】
// 面板里多选之后拖出去，走的是 UE 标准的资产拖放：FAssetDragDropOp 里塞一串资产，
// 由关卡视口的 FLevelEditorViewportClient::DropObjectsAtCoordinates 挨个放置。
// 问题在于它是**循环调用、每次都用同一个光标落点**（见 DropObjectsOnBackground），
// 所以 N 个对象会全部叠在鼠标那一点上；而且 FAssetDragDropOp 只能传"一串资产"，
// 没有任何通道能带上每个对象自己的偏移。
//
// 想让一组对象保持原来的相对位置，就得在 UE 放完之后再摆一次。这个文件干的就是这件事：
//   1) 拖拽开始时记下每个源 Actor 相对「起点 Actor」的偏移 / 朝向 / 缩放；
//   2) 挂 FEditorDelegates::OnNewActorsPlaced，UE 每放完一个资产就收下它建出来的 Actor；
//   3) 全收齐之后，以起点 Actor 落地的位置为基准，把其余的摆回相对位置。
//
// ⚠️ 落点是"事后纠正"，不是拦下落点自己算。所以它依赖两件事：
//   - UE 放置时广播 OnNewActorsPlaced（DropObjectsOnBackground / DropObjectsOnActor 都会）
//   - 放置顺序和拖拽里资产的顺序一致（DropObjectsOnBackground 是按下标顺序循环的）
//   拖拽过程中的"预览"也会广播，但那些 Actor 带 RF_Transient，会被过滤掉。
//   万一某个资产压根没放下（收不齐），视口客户端 Tick 里会在拖拽结束时放弃这次数据。

#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

namespace XingyiGroupDrop
{
	/** 一个源 Actor 相对「起点 Actor」的摆放 */
	struct FPlacement
	{
		/** 位置：相对起点的世界坐标差 */
		FVector LocationOffset = FVector::ZeroVector;

		/** 朝向：源 Actor 的世界朝向。起点自己这个值不参与计算 */
		FQuat Rotation = FQuat::Identity;

		/** 缩放：照搬源 Actor 的（资产里存的是默认缩放，实例上的缩放只有这里能保住） */
		FVector Scale = FVector::OneVector;

		/** 是不是「拖拽起点」那个 Actor */
		bool bAnchor = false;
	};

	/**
	 * 一次成组拖拽的数据：资产 -> 该资产对应的若干个摆放。
	 * 用 TMap 是因为同一个资产可能被多个源 Actor 引用（比如五把一模一样的椅子）；
	 * 每个资产下面的数组按源 Actor 出现的顺序排队，和 UE 放置的顺序对得上。
	 */
	using FPayload = TMap<UObject*, TArray<FPlacement>>;

	/**
	 * 开始一次成组拖拽。
	 *
	 * @param InPayload         每个资产要用的摆放数据
	 * @param AnchorRotation    起点 Actor 在源关卡里的朝向，用来把"落点给起点转了多少"算出来
	 */
	void Begin(FPayload&& InPayload, const FQuat& AnchorRotation);

	/** 放弃这次数据（拖拽被取消）。已经摆过的不会还原 */
	void Cancel();

	/**
	 * 拖拽结束时收尾：把已经收到的 Actor 摆回相对位置，然后清掉数据。
	 * 拖拽过程中如果全收齐了会当场摆好并自动清掉，这时 Flush 什么都不做。
	 * 存在的意义是兜底 —— 万一有资产没放下（收不齐），至少把放下的那些摆对。
	 */
	void Flush();

	/** 还有没有没结掉的数据 */
	bool IsPending();

	/** 模块启动 / 关闭时挂上 / 摘下 FEditorDelegates::OnNewActorsPlaced */
	void Register();
	void Unregister();
}
