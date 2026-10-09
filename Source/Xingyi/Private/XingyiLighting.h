// 星移 Xingyi —— 预览光照策略
//
// 这个文件是星移自己的实现。它解决的问题是：构造面板用的 FPreviewScene 会自带一盏平行光，
// 而用户打开的关卡通常也有一盏太阳，两盏优先级相同的话渲染器就会报
//   「多个定向光源正在竞争成为唯一一个用于前向着色、半透明、水或体积雾的光源」
// 并且这条提示会一直挂在视口上。
//
// 星移的处理分两层：
//   1) 自带那盏灯被钉死在最低优先级（ForwardShadingPriority = -1），永远不可能和关卡的太阳打平；
//   2) 再由这里的策略决定它到底亮不亮。
// 第 1 层是硬性的，第 2 层是用户可以选的。

#pragma once

#include "CoreMinimal.h"
#include "XingyiLighting.generated.h"

/** 面板自带的那盏「兜底平行光」什么时候点亮 */
UENUM()
enum class EXingyiFallbackLightMode : uint8
{
	/** 关卡自己带光源就用关卡的光照，只有关卡完全没光时才点亮兜底光 */
	Auto UMETA(DisplayName = "自动（关卡没光时才点亮）"),

	/** 始终点亮兜底光，预览亮度和官方版一致（默认） */
	AlwaysOn UMETA(DisplayName = "始终点亮"),

	/** 始终关闭兜底光，完全使用关卡自己的光照 */
	AlwaysOff UMETA(DisplayName = "始终关闭")
};

namespace XingyiLighting
{
	/**
	 * 是否点亮面板自带的兜底光源。
	 *
	 * 单独抽成纯函数是为了能不建视口就跑自动化测试 —— 它不依赖任何引擎状态。
	 *
	 * @param Mode            用户选的模式
	 * @param LevelLightCount 预览世界里关卡自带的光源数量（面板自带的那两盏不算）
	 */
	bool ShouldUseFallbackLight(EXingyiFallbackLightMode Mode, int32 LevelLightCount);

	/** 把模式翻译成界面上给用户看的一句话（工具条菜单和状态条都用它） */
	FText DescribeMode(EXingyiFallbackLightMode Mode);
}
