// 星移 Xingyi —— 插件设置（实现）

#include "XingyiSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(XingyiSettings)

//////////////////////////////////////////////////////////////////////////
// FXingyiLevelRecord
//////////////////////////////////////////////////////////////////////////

FAssetData FXingyiLevelRecord::ResolveAsset() const
{
	if (LevelPath.IsEmpty())
	{
		return FAssetData();
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	return AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(LevelPath));
}

//////////////////////////////////////////////////////////////////////////
// UXingyiSettings
//////////////////////////////////////////////////////////////////////////

UXingyiSettings::UXingyiSettings()
{
	CategoryName = TEXT("Plugins");
}

#if WITH_EDITOR
void UXingyiSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ForgetStaleLevels();
}
#endif

int32 UXingyiSettings::IndexOfLevel(const FString& LevelPath) const
{
	if (LevelPath.IsEmpty())
	{
		return INDEX_NONE;
	}

	return KnownLevels.IndexOfByPredicate(
		[&LevelPath](const FXingyiLevelRecord& Record) { return Record.LevelPath == LevelPath; });
}

int32 UXingyiSettings::EnsureLevelRecord(const FString& LevelPath)
{
	const int32 Existing = IndexOfLevel(LevelPath);
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}

	const int32 Created = KnownLevels.AddDefaulted();
	KnownLevels[Created].LevelPath = LevelPath;
	return Created;
}

void UXingyiSettings::NoteLevelOpened(const FAssetData& LevelAsset, int32 PaneNumber)
{
	if (!LevelAsset.IsValid())
	{
		return;
	}

	const FString LevelPath = LevelAsset.GetObjectPathString();
	if (LevelPath.IsEmpty())
	{
		return;
	}

	// 自增序号就是「谁更新」的唯一依据
	++OpenCounter;

	const int32 RecordIndex = EnsureLevelRecord(LevelPath);
	KnownLevels[RecordIndex].LastOpenOrder = OpenCounter;

	// 面板号非法时只更新最近次序，不动「哪个面板在看哪个关卡」
	if (PaneNumber >= 1)
	{
		if (PaneLastLevel.Num() < PaneNumber)
		{
			PaneLastLevel.AddDefaulted(PaneNumber - PaneLastLevel.Num());
		}
		PaneLastLevel[PaneNumber - 1] = LevelPath;
	}

	ForgetStaleLevels();
	PersistIfAllowed();
}

bool UXingyiSettings::ToggleLevelPinned(const FAssetData& LevelAsset)
{
	if (!LevelAsset.IsValid())
	{
		return false;
	}

	const FString LevelPath = LevelAsset.GetObjectPathString();
	if (LevelPath.IsEmpty())
	{
		return false;
	}

	const int32 RecordIndex = EnsureLevelRecord(LevelPath);
	KnownLevels[RecordIndex].bPinned = !KnownLevels[RecordIndex].bPinned;

	ForgetStaleLevels();
	PersistIfAllowed();

	// ForgetStaleLevels 会把「刚取消收藏、又没有别的用途」的记录整条丢掉，
	// 所以状态以整理之后的表为准，别直接返回刚才那个布尔值。
	const int32 FinalIndex = IndexOfLevel(LevelPath);
	return FinalIndex != INDEX_NONE && KnownLevels[FinalIndex].bPinned;
}

bool UXingyiSettings::IsLevelPinned(const FAssetData& LevelAsset) const
{
	if (!LevelAsset.IsValid())
	{
		return false;
	}

	const int32 RecordIndex = IndexOfLevel(LevelAsset.GetObjectPathString());
	return RecordIndex != INDEX_NONE && KnownLevels[RecordIndex].bPinned;
}

void UXingyiSettings::CollectRecentRecords(TArray<FXingyiLevelRecord>& OutRecords, int32 Limit) const
{
	OutRecords.Reset();

	if (Limit <= 0)
	{
		return;
	}

	for (const FXingyiLevelRecord& Record : KnownLevels)
	{
		if (Record.LastOpenOrder > 0)
		{
			OutRecords.Add(Record);
		}
	}

	// 序号越大越新
	OutRecords.Sort([](const FXingyiLevelRecord& A, const FXingyiLevelRecord& B)
	{
		return A.LastOpenOrder > B.LastOpenOrder;
	});

	if (OutRecords.Num() > Limit)
	{
		OutRecords.SetNum(Limit, EAllowShrinking::No);
	}
}

void UXingyiSettings::CollectPinnedRecords(TArray<FXingyiLevelRecord>& OutRecords) const
{
	OutRecords.Reset();

	for (const FXingyiLevelRecord& Record : KnownLevels)
	{
		if (Record.bPinned)
		{
			OutRecords.Add(Record);
		}
	}
}

FString UXingyiSettings::GetPaneLastLevelPath(int32 PaneNumber) const
{
	const int32 SlotIndex = PaneNumber - 1;
	return PaneLastLevel.IsValidIndex(SlotIndex) ? PaneLastLevel[SlotIndex] : FString();
}

FAssetData UXingyiSettings::GetPaneLastLevel(int32 PaneNumber) const
{
	const FString LevelPath = GetPaneLastLevelPath(PaneNumber);
	if (LevelPath.IsEmpty())
	{
		return FAssetData();
	}

	const int32 RecordIndex = IndexOfLevel(LevelPath);
	return RecordIndex != INDEX_NONE ? KnownLevels[RecordIndex].ResolveAsset() : FAssetData();
}

void UXingyiSettings::ForgetStaleLevels()
{
	const int32 KeepCount = FMath::Max(RecentHistoryLimit, 0);

	// 序号是单调自增的，所以「留最新的 KeepCount 条」等价于「只留序号大于第 KeepCount+1 大的那些」。
	// 这样不用排序记录本身，也不用指针数组，逻辑短且不会踩到排序谓词的签名问题。
	TArray<int64> Orders;
	Orders.Reserve(KnownLevels.Num());
	for (const FXingyiLevelRecord& Record : KnownLevels)
	{
		if (Record.LastOpenOrder > 0)
		{
			Orders.Add(Record.LastOpenOrder);
		}
	}
	Orders.Sort([](int64 A, int64 B) { return A > B; });

	// CutOff 为 0 表示没有超编的记录需要处理
	int64 CutOff = 0;
	if (Orders.Num() > KeepCount)
	{
		CutOff = Orders[KeepCount];
	}

	// 超出上限的记录不再算「最近打开」。注意这里只是把序号清掉、不删记录 ——
	// 收藏和「面板上次看的是哪个关卡」都还可能要它。
	for (FXingyiLevelRecord& Record : KnownLevels)
	{
		if (Record.LastOpenOrder > 0 && Record.LastOpenOrder <= CutOff)
		{
			Record.LastOpenOrder = 0;
		}
	}

	// 三种用途都不沾的记录才有资格被丢掉：
	//   收藏了 / 还在最近上限里 / 是某个面板上次看的那一个
	KnownLevels.RemoveAll([this](const FXingyiLevelRecord& Record)
	{
		if (Record.bPinned || Record.LastOpenOrder > 0)
		{
			return false;
		}

		return !PaneLastLevel.Contains(Record.LevelPath);
	});

	// 面板记录指向的关卡如果连记录都没了，那条面板记录也清掉
	for (FString& PaneLevel : PaneLastLevel)
	{
		if (!PaneLevel.IsEmpty() && IndexOfLevel(PaneLevel) == INDEX_NONE)
		{
			PaneLevel.Reset();
		}
	}
}

void UXingyiSettings::PersistIfAllowed()
{
	if (bSuppressSave)
	{
		return;
	}

#if WITH_EDITOR
	PostEditChange();
	SaveConfig();
#endif
}
