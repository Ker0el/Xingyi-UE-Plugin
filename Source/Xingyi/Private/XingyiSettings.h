// 星移 Xingyi —— 插件设置
//
// 【数据模型】
// 「最近打开 / 收藏 / 每个面板上次看的关卡」这三种状态全部装在 KnownLevels 这一个数组里，
// 每条记录自己带收藏标记和最近次序。不拆成几张平行的字符串表，是因为那样每加一处状态
// 都要同步维护好几张表，很容易出现「列表里有、明细里没有」的半残数据。
//
// 次序用一个单调递增的 OpenCounter 表示，而不是靠数组下标 —— 这样「冒泡到最前」
// 只是改一个数字，不涉及搬移整个数组。

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AssetRegistry/AssetData.h"

#include "XingyiLighting.h"
#include "XingyiSettings.generated.h"

/**
 * 星移记住的一个关卡。
 *
 * 收藏和最近次序都在这一条记录里，所以任何一次操作都只改数组里的一个元素。
 */
USTRUCT()
struct FXingyiLevelRecord
{
	GENERATED_BODY()

	/** 关卡资产的完整对象路径，同时充当这条记录的唯一键 */
	UPROPERTY(EditAnywhere, Category = "星移")
	FString LevelPath;

	/** 收藏过的关卡不受「记住多少个最近打开的关卡」上限影响 */
	UPROPERTY()
	bool bPinned = false;

	/** 最后一次被打开的序号，越大越新；0 表示不算「最近打开」了（可能只是被收藏着，或面板还指着它） */
	UPROPERTY()
	int64 LastOpenOrder = 0;

	/** 把路径查回资产数据；资产已经被删掉时返回一个无效的 FAssetData */
	FAssetData ResolveAsset() const;
};

/**
 * 星移的设置（项目设置 → 插件 → 星移）。
 *
 * 存在 EditorPerProjectUserSettings 里，所以是一台机器一份、不进版本库。
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "星移"))
class UXingyiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UXingyiSettings();

	// ------------------------------------------------------------------
	// 关卡记录
	// ------------------------------------------------------------------

	/** 打开过的关卡。最近打开和收藏都从这一张表里筛出来 */
	UPROPERTY(config)
	TArray<FXingyiLevelRecord> KnownLevels;

	/** 「最近打开」最多记住多少条（收藏不受这个上限影响） */
	UPROPERTY(config, EditAnywhere, Category = "列表", meta = (ClampMin = 0, ClampMax = 25, DisplayName = "记住多少个最近打开的关卡"))
	int32 RecentHistoryLimit = 10;

	/** 每个面板最后一次打开的关卡路径；下标 0 对应 1 号面板 */
	UPROPERTY(config)
	TArray<FString> PaneLastLevel;

	/** 打开序号的自增计数器 */
	UPROPERTY(config)
	int64 OpenCounter = 0;

	// ------------------------------------------------------------------
	// 预览光照
	// ------------------------------------------------------------------

	/** 兜底光的点亮策略。默认始终点亮，预览亮度和官方版一致 */
	UPROPERTY(config, EditAnywhere, Category = "预览光照", meta = (DisplayName = "兜底光源模式"))
	EXingyiFallbackLightMode FallbackLightMode = EXingyiFallbackLightMode::AlwaysOn;

	/** 兜底光的强度（只有点亮时才有效） */
	UPROPERTY(config, EditAnywhere, Category = "预览光照", meta = (ClampMin = 0.0, ClampMax = 20.0, DisplayName = "兜底光强度"))
	float FallbackLightIntensity = 3.1415926f;

	/** 兜底光的角度 */
	UPROPERTY(config, EditAnywhere, Category = "预览光照", meta = (DisplayName = "兜底光角度"))
	FRotator FallbackLightRotation = FRotator(-40.0f, -67.5f, 0.0f);

	// ------------------------------------------------------------------
	// 显示
	// ------------------------------------------------------------------

	/** 预览里的地面网格 */
	UPROPERTY(config, EditAnywhere, Category = "显示", meta = (DisplayName = "显示地面网格"))
	bool bShowGrid = true;

	/**
	 * 打开关卡后是否自动把相机框到关卡内容上。
	 * 默认关：用关卡自己存的编辑器视角（和官方版一样）。因为「内容」里常有雾、体积云这类
	 * 位置很奇怪的东西，一框就容易把相机拽到地形底下去。
	 */
	UPROPERTY(config, EditAnywhere, Category = "显示", meta = (DisplayName = "打开关卡后自动框住内容"))
	bool bFrameContentOnLoad = false;

	/** 公告板精灵（粒子、材质球没了它就是空的） */
	UPROPERTY(config, EditAnywhere, Category = "显示", meta = (DisplayName = "显示公告板精灵"))
	bool bShowBillboardSprites = true;

	/** 关掉 Lumen 等高级特性换取帧率，会改变预览观感 */
	UPROPERTY(config, EditAnywhere, Category = "显示", meta = (DisplayName = "性能模式（关闭 Lumen 等高级特性）"))
	bool bPerformanceMode = false;

	/**
	 * 打开 World Partition（开放世界）关卡时，是否强制把它的单元全部加载出来。
	 * 关掉流送后 WP 会把所有单元当成已加载，面板里才看得到东西；
	 * 超大开放世界可能会卡几秒，卡就关掉这项（面板会改为提示）。
	 */
	UPROPERTY(config, EditAnywhere, Category = "显示", meta = (DisplayName = "World Partition 关卡自动加载全部单元"))
	bool bLoadWorldPartitionCells = true;

	// ------------------------------------------------------------------
	// 记录维护
	// ------------------------------------------------------------------

	/** 记下「某个关卡刚在第 PaneNumber 个面板里打开过」 */
	void NoteLevelOpened(const FAssetData& LevelAsset, int32 PaneNumber);

	/** 收藏 / 取消收藏；返回操作之后是不是处于收藏状态 */
	bool ToggleLevelPinned(const FAssetData& LevelAsset);

	/** 这个关卡收藏了没有 */
	bool IsLevelPinned(const FAssetData& LevelAsset) const;

	/**
	 * 按「最近打开」从新到旧取出前 Limit 条记录。
	 *
	 * 这里只做排序，不去资产注册表查关卡存不存在 —— 那样会把「排序」和「资产查询」
	 * 搅在一起，既没法单独测，也会让排序结果依赖于资产注册表加载到哪一步了。
	 * 调用方拿到记录之后再自己 ResolveAsset()。
	 */
	void CollectRecentRecords(TArray<FXingyiLevelRecord>& OutRecords, int32 Limit) const;

	/** 取出所有收藏记录（同样不做资产查询） */
	void CollectPinnedRecords(TArray<FXingyiLevelRecord>& OutRecords) const;

	/** 第 PaneNumber 个面板上次打开的关卡路径；没有记录时返回空串 */
	FString GetPaneLastLevelPath(int32 PaneNumber) const;

	/** 第 PaneNumber 个面板上次打开的关卡；没有记录或资产已不存在时返回无效的 FAssetData */
	FAssetData GetPaneLastLevel(int32 PaneNumber) const;

	/** 丢掉不值得再留的记录（超出上限的最近项，以及既没收藏也没用的老记录） */
	void ForgetStaleLevels();

	/** 写配置。bSuppressSave 打开时什么都不做 */
	void PersistIfAllowed();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * 自动化测试用：置 true 之后所有写操作都不落盘，免得测试把用户的真实配置覆盖掉。
	 * 不是 UPROPERTY，不进配置文件。
	 */
	bool bSuppressSave = false;

private:
	/** 找路径对应的记录下标；没有返回 INDEX_NONE */
	int32 IndexOfLevel(const FString& LevelPath) const;

	/** 保证这个路径有对应的记录，返回它的下标（没有就新建一条） */
	int32 EnsureLevelRecord(const FString& LevelPath);
};
