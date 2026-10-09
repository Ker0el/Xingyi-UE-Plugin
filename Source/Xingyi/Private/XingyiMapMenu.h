// 星移 Xingyi —— 「选关卡」菜单
//
// 工具条上的「选择关卡」按钮，和空面板正中央那个「打开关卡…」按钮，用的是同一份菜单。
// 所以把它单独拎出来，两个地方共用一份实现，改一处两边都变。
//
// 菜单里涉及「最近打开 / 收藏」的部分全部走 UXingyiSettings 提供的那几个查询接口，
// 这里不直接去读它内部的数组。

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class SWidget;

/** 用户从菜单里挑中一个关卡时回调 */
DECLARE_DELEGATE_OneParam(FXingyiOnLevelChosen, const FAssetData&);

namespace XingyiMapMenu
{
	/**
	 * 造一份「选关卡」的菜单。
	 *
	 * @param CurrentLevel  当前面板打开的关卡；用来决定「收藏 / 取消收藏 xxx」那一项怎么写。
	 *                      没有打开任何关卡时传一个无效的 FAssetData。
	 * @param OnLevelChosen 用户挑中关卡时调用
	 */
	TSharedRef<SWidget> Build(const FAssetData& CurrentLevel, FXingyiOnLevelChosen OnLevelChosen);

	/** 「收藏 / 再点取消收藏」那一项的文案 */
	FText DescribePinAction(const FAssetData& CurrentLevel);
}
