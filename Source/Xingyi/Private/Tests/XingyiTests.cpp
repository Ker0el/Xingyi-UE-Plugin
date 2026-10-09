// 星移 Xingyi —— 自动化测试
//
// 跑法（命令行，无界面）：
//   UnrealEditor-Cmd.exe <工程> -unattended -nop4 -nosplash -stdout -NullRHI ^
//       "-ExecCmds=Automation RunTests Xingyi;Quit" -TestExit="Automation Test Queue Empty"
// 或者编辑器里：工具 → 会话前端 → 自动化 → 搜 Xingyi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "XingyiCommon.h"
#include "XingyiLighting.h"
#include "XingyiSettings.h"
#include "XingyiViewportClient.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/LightComponentBase.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/** 造一个不落盘、也不进资产注册表的假关卡，专门用来跑列表逻辑 */
	FAssetData MakeFakeLevelAsset(const TCHAR* AssetName)
	{
		return FAssetData(
			FName(*FString::Printf(TEXT("/Game/%s"), AssetName)),
			FName(TEXT("/Game")),
			FName(AssetName),
			UWorld::StaticClass()->GetClassPathName());
	}

	/** 从一批记录里取出路径，方便断言 */
	TArray<FString> PathsOf(const TArray<FXingyiLevelRecord>& Records)
	{
		TArray<FString> Paths;
		Paths.Reserve(Records.Num());
		for (const FXingyiLevelRecord& Record : Records)
		{
			Paths.Add(Record.LevelPath);
		}
		return Paths;
	}
}

//////////////////////////////////////////////////////////////////////////
// 预览光照判定
//
// Auto 模式必须做到「关卡有光就用关卡的、没光才补光」—— 关卡有光还点亮兜底灯的话，
// 两盏平行光就会打架，那正是官方版被吐槽的那条提示。
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXingyiFallbackLightDecisionTest,
	"Xingyi.Lighting.FallbackDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXingyiFallbackLightDecisionTest::RunTest(const FString& Parameters)
{
	using XingyiLighting::ShouldUseFallbackLight;

	TestTrue(TEXT("Auto + 关卡没有光源 -> 点亮兜底光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::Auto, 0));
	TestFalse(TEXT("Auto + 关卡有 1 盏灯 -> 用关卡的光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::Auto, 1));
	TestFalse(TEXT("Auto + 关卡有 8 盏灯 -> 用关卡的光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::Auto, 8));

	TestTrue(TEXT("始终点亮 + 没光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::AlwaysOn, 0));
	TestTrue(TEXT("始终点亮 + 有光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::AlwaysOn, 3));
	TestFalse(TEXT("始终关闭 + 没光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::AlwaysOff, 0));
	TestFalse(TEXT("始终关闭 + 有光"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::AlwaysOff, 3));

	// 脏数据不能崩，也不能把预览搞成全黑
	TestTrue(TEXT("Auto + 负数按没有光源处理"),
		ShouldUseFallbackLight(EXingyiFallbackLightMode::Auto, -1));

	return true;
}

//////////////////////////////////////////////////////////////////////////
// 最近打开 / 收藏 / 每个面板上次看的关卡
//
// 这三种状态共用 KnownLevels 一张表，所以重点验它们之间不会互相打架：
// 重复打开不重复建条目、超额裁剪不动收藏、面板号越界不写坏数据。
//
// 这里用的假关卡不在资产注册表里，所以只能查记录层面的接口
// （CollectRecentRecords / GetPaneLastLevelPath）—— 这正是把它们和资产查询
// 分开的原因：排序逻辑能单独测，不受资产注册表加载到哪一步的影响。
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXingyiSettingsLevelListTest,
	"Xingyi.Settings.RecentAndPinned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXingyiSettingsLevelListTest::RunTest(const FString& Parameters)
{
	UXingyiSettings* Settings = NewObject<UXingyiSettings>();

	// 测试实例是从 CDO 拷来的，可能带着用户真实的记录，先清干净；
	// 再把 bSuppressSave 打开，免得测试把用户的配置文件覆盖掉。
	Settings->KnownLevels.Reset();
	Settings->PaneLastLevel.Reset();
	Settings->OpenCounter = 0;
	Settings->RecentHistoryLimit = 3;
	Settings->bSuppressSave = true;

	const FAssetData LevelA = MakeFakeLevelAsset(TEXT("LvlA"));
	const FAssetData LevelB = MakeFakeLevelAsset(TEXT("LvlB"));
	const FAssetData LevelC = MakeFakeLevelAsset(TEXT("LvlC"));
	const FString PathA = LevelA.GetObjectPathString();
	const FString PathB = LevelB.GetObjectPathString();
	const FString PathC = LevelC.GetObjectPathString();

	TestTrue(TEXT("假资产本身有效"), LevelA.IsValid() && LevelB.IsValid() && LevelC.IsValid());

	// ---- 打开一个关卡 ----
	Settings->NoteLevelOpened(LevelA, /*PaneNumber=*/1);
	{
		TArray<FXingyiLevelRecord> Recent;
		Settings->CollectRecentRecords(Recent, 10);
		TestEqual(TEXT("最近打开 1 条"), Recent.Num(), 1);
		TestTrue(TEXT("最近第一条是 A"), PathsOf(Recent) == TArray<FString>{ PathA });
	}
	TestEqual(TEXT("只建了一条记录"), Settings->KnownLevels.Num(), 1);
	TestEqual(TEXT("1 号面板记住了 A"), Settings->GetPaneLastLevelPath(1), PathA);

	// ---- 再打开 B：B 排到最前，A 退一位 ----
	Settings->NoteLevelOpened(LevelB, 1);
	{
		TArray<FXingyiLevelRecord> Recent;
		Settings->CollectRecentRecords(Recent, 10);
		TestTrue(TEXT("B 冒到最前、A 退到第二"), PathsOf(Recent) == TArray<FString>{ PathB, PathA });
	}

	// ---- 重复打开 A：应该冒泡，而不是多出一条 ----
	Settings->NoteLevelOpened(LevelA, 1);
	{
		TArray<FXingyiLevelRecord> Recent;
		Settings->CollectRecentRecords(Recent, 10);
		TestEqual(TEXT("重复打开不增加条目"), Recent.Num(), 2);
		TestTrue(TEXT("A 回到最前"), PathsOf(Recent) == TArray<FString>{ PathA, PathB });
	}

	// ---- 不同面板各记各的 ----
	Settings->NoteLevelOpened(LevelC, /*PaneNumber=*/3);
	TestEqual(TEXT("3 号面板记的是 C"), Settings->GetPaneLastLevelPath(3), PathC);
	TestEqual(TEXT("1 号面板还是 A"), Settings->GetPaneLastLevelPath(1), PathA);
	TestTrue(TEXT("没打开过的 2 号面板返回空"), Settings->GetPaneLastLevelPath(2).IsEmpty());
	TestTrue(TEXT("越界的面板号也返回空"), Settings->GetPaneLastLevelPath(99).IsEmpty());

	// ---- 上限裁剪 ----
	Settings->RecentHistoryLimit = 2;
	Settings->ForgetStaleLevels();
	{
		TArray<FXingyiLevelRecord> Recent;
		Settings->CollectRecentRecords(Recent, 10);
		TestEqual(TEXT("最近打开裁到 2 条"), Recent.Num(), 2);
		TestTrue(TEXT("留下的是最近打开过的 C 和 A（B 最旧，被裁掉）"),
			PathsOf(Recent) == TArray<FString>{ PathC, PathA });
	}
	TestEqual(TEXT("表里也只剩 2 条"), Settings->KnownLevels.Num(), 2);

	// ---- 收藏 ----
	TestFalse(TEXT("A 一开始没收藏"), Settings->IsLevelPinned(LevelA));
	TestTrue(TEXT("收藏操作返回新状态"), Settings->ToggleLevelPinned(LevelA));
	TestTrue(TEXT("收藏之后查得到"), Settings->IsLevelPinned(LevelA));

	// 收藏的关卡被挤出「最近打开」之后也不该被清掉
	Settings->RecentHistoryLimit = 0;
	Settings->ForgetStaleLevels();
	{
		TArray<FXingyiLevelRecord> Pinned;
		Settings->CollectPinnedRecords(Pinned);
		TestEqual(TEXT("收藏列表只有一条"), Pinned.Num(), 1);
		TestTrue(TEXT("被挤出最近列表的收藏还在"), PathsOf(Pinned) == TArray<FString>{ PathA });
	}
	{
		TArray<FXingyiLevelRecord> Recent;
		Settings->CollectRecentRecords(Recent, 10);
		TestEqual(TEXT("最近打开被裁空"), Recent.Num(), 0);
	}

	// ---- 取消收藏：它既不是收藏也不再算最近，但面板还指着它，所以记录要留着 ----
	TestFalse(TEXT("再点一次取消收藏"), Settings->ToggleLevelPinned(LevelA));
	Settings->ForgetStaleLevels();
	{
		TArray<FXingyiLevelRecord> Pinned;
		Settings->CollectPinnedRecords(Pinned);
		TestEqual(TEXT("收藏列表空了"), Pinned.Num(), 0);
	}
	{
		TArray<FXingyiLevelRecord> Recent;
		Settings->CollectRecentRecords(Recent, 10);
		TestEqual(TEXT("最近打开也空了"), Recent.Num(), 0);
	}
	TestEqual(TEXT("A 和 C 还是 1 号 / 3 号面板上次看的关卡，记录不能删"),
		Settings->KnownLevels.Num(), 2);

	// ---- 脏数据 ----
	const int32 RecordsBefore = Settings->KnownLevels.Num();
	Settings->NoteLevelOpened(FAssetData(), 1);
	TestEqual(TEXT("空资产不会新增记录"), Settings->KnownLevels.Num(), RecordsBefore);

	const int32 PaneSlotsBefore = Settings->PaneLastLevel.Num();
	Settings->NoteLevelOpened(LevelB, /*PaneNumber=*/0);
	TestEqual(TEXT("面板号 0 非法，不新增面板记录"), Settings->PaneLastLevel.Num(), PaneSlotsBefore);
	TestEqual(TEXT("面板号 0 也不会留下记录"), Settings->KnownLevels.Num(), RecordsBefore);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// 冒烟测试：视口客户端能不能被构造出来
//
// 「面板一打开就崩」这类问题的第一现场就在构造函数里（比如 FEngineShowFlags 的无参构造、
// 预览场景还没建好就去访问它）。这个测试把构造过程跑一遍，崩了测试就红。
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXingyiViewportClientSmokeTest,
	"Xingyi.Smoke.ViewportClientConstruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXingyiViewportClientSmokeTest::RunTest(const FString& Parameters)
{
	FXingyiViewportClient* Client = new FXingyiViewportClient(/*InPaneNumber=*/1);
	TestNotNull(TEXT("视口客户端能构造出来"), Client);

	if (Client)
	{
		TestEqual(TEXT("还没载入关卡时，关卡光源数应为 0"), Client->GetLevelLightCount(), 0);
		TestFalse(TEXT("构造后就有光照状态文案（底部状态条要显示）"), Client->GetLightingStatusText().IsEmpty());
		TestFalse(TEXT("构造后没有打开任何关卡"), Client->GetSourceLevelAsset().IsValid());
		TestNotNull(TEXT("构造后预览世界是存在的"), Client->GetPreviewWorld());
		TestEqual(TEXT("面板编号原样带过来"), Client->GetPaneNumber(), 1);

		// 改一遍设置再刷一次，确认「重新应用设置」这条路也不会崩
		UXingyiSettings* Settings = GetMutableDefault<UXingyiSettings>();
		const bool bSavedPerformanceMode = Settings->bPerformanceMode;
		const bool bSavedGrid = Settings->bShowGrid;

		Settings->bPerformanceMode = true;
		Settings->bShowGrid = false;
		Client->ApplyDisplaySettings();

		Settings->bPerformanceMode = false;
		Settings->bShowGrid = true;
		Client->ApplyDisplaySettings();

		Settings->bPerformanceMode = bSavedPerformanceMode;
		Settings->bShowGrid = bSavedGrid;

		Client->RefreshPreviewLighting();
		TestEqual(TEXT("刷新光照后关卡光源数依然为 0"), Client->GetLevelLightCount(), 0);

		// 没打开关卡就调这些，也不该崩
		Client->DropPaletteSelection();
		Client->RestoreStoredCamera();
	}

	delete Client;
	return true;
}

//////////////////////////////////////////////////////////////////////////
// 诊断：World Partition 关卡到底加载出了什么
//
// 不做断言，只把预览世界里的 Actor 逐个打进日志（类名 / 名字 / 位置 / 有没有网格或灯）
// 外加包围盒和相机位置。用来区分「内容压根没加载」和「加载了但相机没对准」。
// 工程里没有那个关卡就自动跳过。
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXingyiWorldPartitionDiagnosticTest,
	"Xingyi.Diagnostic.WorldPartitionLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FXingyiWorldPartitionDiagnosticTest::RunTest(const FString& Parameters)
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	const FAssetData LevelAsset =
		AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(TEXT("/Game/111.111")));

	if (!LevelAsset.IsValid())
	{
		AddWarning(TEXT("工程里没有 /Game/111.111（World Partition 测试关卡），跳过诊断。"));
		return true;
	}

	FXingyiViewportClient* Client = new FXingyiViewportClient(/*InPaneNumber=*/1);
	Client->OpenSourceLevel(LevelAsset);

	UWorld* PreviewWorld = Client->GetPreviewWorld();
	TestNotNull(TEXT("预览世界存在"), PreviewWorld);

	if (PreviewWorld)
	{
		FBox Bounds(ForceInit);
		int32 Total = 0;
		int32 Renderable = 0;

		for (TActorIterator<AActor> It(PreviewWorld); It; ++It)
		{
			AActor* Actor = *It;
			++Total;

			const bool bHasPrimitive = Actor->GetComponentByClass<UPrimitiveComponent>() != nullptr;
			const bool bHasLight = Actor->GetComponentByClass<ULightComponentBase>() != nullptr;
			if (bHasPrimitive || bHasLight)
			{
				++Renderable;
				Bounds += Actor->GetComponentsBoundingBox(/*bNonColliding=*/true);
			}

			UE_LOG(LogXingyi, Display, TEXT("[诊断] %s | %s | 位置 %s | 网格=%s 灯=%s"),
				*Actor->GetClass()->GetName(),
				*Actor->GetName(),
				*Actor->GetActorLocation().ToCompactString(),
				bHasPrimitive ? TEXT("有") : TEXT("无"),
				bHasLight ? TEXT("有") : TEXT("无"));
		}

		UE_LOG(LogXingyi, Display, TEXT("[诊断] 预览世界里 Actor 共 %d 个，其中可显示 %d 个"), Total, Renderable);
		UE_LOG(LogXingyi, Display, TEXT("[诊断] 内容包围盒 %s"),
			Bounds.IsValid ? *Bounds.ToString() : TEXT("<无效>"));
		UE_LOG(LogXingyi, Display, TEXT("[诊断] 相机位置 %s / 旋转 %s"),
			*Client->GetViewLocation().ToCompactString(),
			*Client->GetViewRotation().ToCompactString());
	}

	delete Client;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
