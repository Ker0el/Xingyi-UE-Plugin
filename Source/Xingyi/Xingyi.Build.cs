// Copyright 2026 StarGame. All Rights Reserved.
// 星移 Xingyi —— 模块依赖（UnrealBuildTool）
//
// 星移是一个纯编辑器模块：只依赖编辑器 UI、关卡编辑和内容浏览器相关的模块。
// 所有类型都只在编辑器内使用，打包后的游戏不会加载它。

using UnrealBuildTool;

public class Xingyi : ModuleRules
{
	public Xingyi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		// 编辑器 UI：Slate 控件、菜单、工作区菜单分组
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"UnrealEd",
		});

		// 关卡与世界：资产加载、视口客户端、内容浏览器拖放
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"InputCore",
			"ContentBrowser",
			"Projects",
			"DeveloperSettings",
		});
	}
}
