// 星移 Xingyi —— 模块依赖
//
// 这个文件是星移自己的，没有沿用任何 Unreal Engine 示例插件的代码。

using UnrealBuildTool;

public class Xingyi : ModuleRules
{
	public Xingyi(ReadOnlyTargetRules Target) : base(Target)
	{
		// 关掉「未定义标识符按错误处理」即可，对正常代码没有影响。

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"ContentBrowser",
				"WorkspaceMenuStructure",
				"DeveloperSettings"
			}
			);
	}
}
