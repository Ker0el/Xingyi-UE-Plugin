// 星移 Xingyi —— 「素材板关卡」本身的实现

#include "XingyiSourceLevel.h"

#include "XingyiCommon.h"
#include "XingyiSettings.h"

#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineUtils.h"
#include "Components/LightComponentBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "Xingyi"

namespace
{
	/**
	 * 天空大气 / 体积云 / 高度雾。
	 * 它们的包围盒要么是「整个世界」那么大，要么埋在离地形很深的地方，
	 * 都不是「内容在哪儿」的线索，框相机时一律排除。
	 */
	bool IsAtmosphereActor(const AActor* Actor)
	{
		const FString ClassName = Actor->GetClass()->GetName();
		return ClassName.Contains(TEXT("SkyAtmosphere"))
			|| ClassName.Contains(TEXT("VolumetricCloud"))
			|| ClassName.Contains(TEXT("HeightFog"));
	}

	/** 包围盒大到「整个世界」的（物理体积、天空盒）拿它框相机等于没框，直接跳过 */
	constexpr double MaxMeaningfulBoundsExtent = 500000.0;
}

FXingyiSourceLevel::FXingyiSourceLevel(UWorld* InPreviewWorld)
	: PreviewWorld(InPreviewWorld)
{
}

bool FXingyiSourceLevel::IsVisibleActor(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	// 用 ULightComponentBase 才能把天光也算进来 —— 它和 ULightComponent 是兄弟类，不是子类
	TInlineComponentArray<UActorComponent*> Components(const_cast<AActor*>(Actor));
	for (UActorComponent* Component : Components)
	{
		if (Cast<UPrimitiveComponent>(Component) || Cast<ULightComponentBase>(Component))
		{
			return true;
		}
	}

	return false;
}

void FXingyiSourceLevel::RefreshState()
{
	bPartitioned = PreviewWorld ? PreviewWorld->IsPartitionedWorld() : false;
	bEmpty = CountVisibleActors() == 0;
}

EXingyiLevelLoadResult FXingyiSourceLevel::Load(const FAssetData& InLevelAsset)
{
	// 换关卡之前先把上一个搬走，别留下「名牌换了、场景没换」的不一致状态
	Unload();

	LevelAsset = FAssetData();
	bPartitioned = false;
	bEmpty = false;

	if (!PreviewWorld)
	{
		return EXingyiLevelLoadResult::LoadFailed;
	}

	UWorld* SourceWorld = Cast<UWorld>(InLevelAsset.GetAsset());
	if (!SourceWorld)
	{
		return EXingyiLevelLoadResult::NotALevelAsset;
	}

	FScopedSlowTask SlowTask(0.0f, LOCTEXT("LoadingLevelAsPalette", "正在把关卡载入星移面板…"));

	bool bLoadSucceeded = false;
	ULevelStreamingDynamic* NewLevel = ULevelStreamingDynamic::LoadLevelInstance(
		PreviewWorld, SourceWorld->GetPathName(), FVector::ZeroVector, FRotator::ZeroRotator, bLoadSucceeded);

	if (!bLoadSucceeded || !NewLevel)
	{
		// 旧关卡这时已经卸掉了，调用方要记得重算光照和面板状态
		return EXingyiLevelLoadResult::LoadFailed;
	}

	// 关卡流式载入的既知坑：目标世界不是 PIE 世界时，如果同名关卡已经载入过会走错分支。
	// 换个名字绕开它。
	NewLevel->RenameForPIE(1);
	StreamingLevel = NewLevel;

	PreviewWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	// 关卡自己保存的编辑器视角，拿来当面板的初始机位
	PreviewWorld->EditorViews = SourceWorld->EditorViews;

	LevelAsset = InLevelAsset;

	// 关卡进来之后才知道它是不是 WP、里面有没有东西
	bPartitioned = PreviewWorld->IsPartitionedWorld();
	bEmpty = CountVisibleActors() == 0;

	return EXingyiLevelLoadResult::Loaded;
}

void FXingyiSourceLevel::Unload()
{
	if (StreamingLevel && PreviewWorld)
	{
		StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
		PreviewWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	}

	StreamingLevel = nullptr;
}

void FXingyiSourceLevel::ApplyStoredCameraView(FEditorViewportClient& ViewportClient) const
{
	if (!PreviewWorld)
	{
		return;
	}

	const int32 ViewIndex = static_cast<int32>(ViewportClient.GetViewportType());
	if (PreviewWorld->EditorViews.IsValidIndex(ViewIndex))
	{
		const FLevelViewportInfo& ViewInfo = PreviewWorld->EditorViews[ViewIndex];
		ViewportClient.SetInitialViewTransform(
			ViewportClient.GetViewportType(),
			ViewInfo.CamPosition,
			ViewInfo.CamRotation,
			ViewInfo.CamOrthoZoom);
	}
}

int32 FXingyiSourceLevel::CountVisibleActors() const
{
	int32 Count = 0;
	if (!PreviewWorld)
	{
		return Count;
	}

	for (TActorIterator<AActor> It(PreviewWorld); It; ++It)
	{
		if (IsVisibleActor(*It))
		{
			++Count;
		}
	}

	return Count;
}

int32 FXingyiSourceLevel::CountLevelLights(const UDirectionalLightComponent* FallbackSun,
	const USkyLightComponent* PreviewSkyLight) const
{
	int32 Count = 0;
	if (!PreviewWorld)
	{
		return Count;
	}

	for (TActorIterator<AActor> It(PreviewWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		TInlineComponentArray<ULightComponentBase*> Lights(Actor);
		for (ULightComponentBase* Light : Lights)
		{
			// 面板自带的那两盏不算「关卡的光」
			if (!Light || Light == FallbackSun || Light == PreviewSkyLight || !Light->IsVisible())
			{
				continue;
			}
			++Count;
		}
	}

	return Count;
}

bool FXingyiSourceLevel::ComputeContentBounds(FBox& OutBounds, int32& OutActorCount) const
{
	OutBounds = FBox(ForceInit);
	OutActorCount = 0;

	if (!PreviewWorld)
	{
		return false;
	}

	for (TActorIterator<AActor> It(PreviewWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsVisibleActor(Actor) || IsAtmosphereActor(Actor))
		{
			continue;
		}

		const FBox ActorBounds = Actor->GetComponentsBoundingBox(/*bNonColliding=*/true);

		// 体积（DefaultPhysicsVolume 之类）和天空盒的边界常常是「整个世界」那么大
		if (!ActorBounds.IsValid || ActorBounds.GetSize().GetMax() > MaxMeaningfulBoundsExtent)
		{
			continue;
		}

		OutBounds += ActorBounds;
		++OutActorCount;
	}

	return OutBounds.IsValid && OutActorCount > 0;
}

#undef LOCTEXT_NAMESPACE
