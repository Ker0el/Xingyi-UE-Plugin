// 星移 Xingyi —— 公共小工具

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/World.h"

/** 星移自己的日志分类（定义在 XingyiModule.cpp） */
DECLARE_LOG_CATEGORY_EXTERN(LogXingyi, Log, All);

namespace XingyiCommon
{
	/**
	 * 这个资产算不算「关卡」。
	 *
	 * 判断故意放宽：从内容浏览器拖过来的 FAssetData 类路径不一定是 /Script/Engine.World，
	 * 只要类路径里带 World 就认（World / WorldPartition 之类）。
	 */
	inline bool IsLevelAsset(const FAssetData& Asset)
	{
		if (!Asset.IsValid())
		{
			return false;
		}

		return Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName()
			|| Asset.AssetClassPath.ToString().Contains(TEXT("World"));
	}

	/** 关卡资产的显示名；无效资产返回空文本 */
	inline FText GetLevelDisplayName(const FAssetData& Asset)
	{
		return Asset.IsValid() ? FText::FromName(Asset.AssetName) : FText::GetEmpty();
	}

	/** 关卡资产所在的包名，用在菜单项的悬停提示里 */
	inline FText GetLevelPackageName(const FAssetData& Asset)
	{
		return Asset.IsValid() ? FText::FromName(Asset.PackageName) : FText::GetEmpty();
	}
}
