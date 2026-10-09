// 星移 Xingyi —— 预览光照策略（实现）

#include "XingyiLighting.h"

#define LOCTEXT_NAMESPACE "Xingyi"

namespace XingyiLighting
{

bool ShouldUseFallbackLight(EXingyiFallbackLightMode Mode, int32 LevelLightCount)
{
	switch (Mode)
	{
	case EXingyiFallbackLightMode::AlwaysOn:
		return true;

	case EXingyiFallbackLightMode::AlwaysOff:
		return false;

	case EXingyiFallbackLightMode::Auto:
	default:
		// 关卡自带光源就完全用关卡的光照（观感和关卡一致）；
		// 一盏灯都没有时才点亮兜底光，保证不会黑屏。
		// 负数按「没有光源」处理，脏数据不至于让预览全黑。
		return LevelLightCount <= 0;
	}
}

FText DescribeMode(EXingyiFallbackLightMode Mode)
{
	switch (Mode)
	{
	case EXingyiFallbackLightMode::AlwaysOn:
		return LOCTEXT("LightModeAlwaysOn", "始终点亮");

	case EXingyiFallbackLightMode::AlwaysOff:
		return LOCTEXT("LightModeAlwaysOff", "始终关闭");

	case EXingyiFallbackLightMode::Auto:
	default:
		return LOCTEXT("LightModeAuto", "自动");
	}
}

} // namespace XingyiLighting

#undef LOCTEXT_NAMESPACE
