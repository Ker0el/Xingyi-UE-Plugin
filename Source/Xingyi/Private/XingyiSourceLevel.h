// 星移 Xingyi —— 「素材板关卡」本身
//
// 面板里的预览世界是一个 FPreviewScene。素材板关卡不是被"打开"，而是以流式关卡的形式
// 载入到那个预览世界里 —— 这样它不会动到编辑器正在编辑的那个关卡。
//
// 这个类只管「把关卡搬进预览世界 / 搬走，以及搬进来之后能问它什么」，
// 完全不碰 Slate、不碰视口状态。好处是它脱离了面板也能单独构造和测试，
// 光照、相机、空关卡判定这些都能在这一层验证。

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class AActor;
class FEditorViewportClient;
class UDirectionalLightComponent;
class ULevelStreamingDynamic;
class USkyLightComponent;
class UWorld;

/** 一次载入尝试的结果 */
enum class EXingyiLevelLoadResult : uint8
{
	/** 载进去了 */
	Loaded,

	/** 给的不是一个能用的关卡资产 */
	NotALevelAsset,

	/** 关卡流式载入失败（资产损坏、已被删除之类） */
	LoadFailed,
};

/**
 * 预览世界里的那个「素材板关卡」。
 *
 * 生命周期：构造时只记住预览世界，真正载入要调 Load()；换关卡时旧关卡会先自动卸掉。
 * 析构时不主动动世界 —— 预览世界总是先于它销毁，那时流式关卡已经跟着世界一起没了。
 */
class FXingyiSourceLevel
{
public:
	/** @param InPreviewWorld 关卡要载入到哪个世界（面板的那个预览世界） */
	explicit FXingyiSourceLevel(UWorld* InPreviewWorld);

	/** 载入一个关卡资产；会先把上一个卸掉 */
	EXingyiLevelLoadResult Load(const FAssetData& LevelAsset);

	/** 卸掉当前关卡 */
	void Unload();

	/** 现在载的是哪个关卡；没载就是无效的 FAssetData */
	const FAssetData& GetAsset() const { return LevelAsset; }

	/** 把预览相机摆到关卡自己保存的编辑器视角上 */
	void ApplyStoredCameraView(FEditorViewportClient& ViewportClient) const;

	/** 预览世界里「看得见东西」的 Actor 数量 */
	int32 CountVisibleActors() const;

	/**
	 * 数一数关卡自带的光源。
	 * 面板自带的那两盏（兜底平行光、预览天光）要传进来排除掉。
	 */
	int32 CountLevelLights(const UDirectionalLightComponent* FallbackSun,
		const USkyLightComponent* PreviewSkyLight) const;

	/** 载入的是 World Partition 关卡 */
	bool IsPartitioned() const { return bPartitioned; }

	/** 载入完成时可显示的 Actor 数为 0（典型原因：World Partition 关卡） */
	bool IsEmpty() const { return bEmpty; }

	/**
	 * 把所有可视 Actor 的包围盒并起来，用来「打开关卡后自动框住内容」。
	 * 天空大气 / 体积云 / 高度雾这类不成线索的东西会排除掉。
	 *
	 * @return 是否算出了一个可用的包围盒
	 */
	bool ComputeContentBounds(FBox& OutBounds, int32& OutActorCount) const;

private:
	/** 这个 Actor 有没有「看得见」的东西（网格、光照之类） */
	static bool IsVisibleActor(const AActor* Actor);

	/** 刷新 bPartitioned / bEmpty */
	void RefreshState();

	UWorld* PreviewWorld = nullptr;

	/** 当前这个流式关卡；卸载之后置空 */
	ULevelStreamingDynamic* StreamingLevel = nullptr;

	FAssetData LevelAsset;

	/** 载入的是 World Partition 关卡 */
	bool bPartitioned = false;

	/** 载入完成时可显示的 Actor 数为 0 */
	bool bEmpty = false;
};
