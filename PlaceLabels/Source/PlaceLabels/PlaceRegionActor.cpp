#include "PlaceRegionActor.h"

#include "PlaceRegionComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#endif

APlaceRegionActor::APlaceRegionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Never streamed.
	bIsSpatiallyLoaded = false;

	Region = CreateDefaultSubobject<UPlaceRegionComponent>(TEXT("Region"));
	RootComponent = Region;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteTexture(
			TEXT("/Engine/EditorResources/S_Note"));

		SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
		if (SpriteComponent)
		{
			SpriteComponent->Sprite = SpriteTexture.Object;
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->SetupAttachment(Region);
		}
	}
#endif
}

#if WITH_EDITOR
void APlaceRegionActor::RecomputeParent()
{
	if (!Region)
	{
		return;
	}

	// No transaction: FScopedTransaction lives in UnrealEd and this is a runtime module.
	Modify();
	Region->RecomputeDerivedParent();
}

void APlaceRegionActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (Region)
	{
		Region->RebuildCache();
	}

	// Only on the commit: the gizmo fires this continuously while dragging, and re-resolving a parent sweeps every region in the level.
	if (bFinished && Region)
	{
		Region->RecomputeDerivedParent();
	}
}

void APlaceRegionActor::ClearExplicitParent()
{
	if (!Region || !Region->ExplicitParent)
	{
		return;
	}

	Modify();
	Region->Modify();
	Region->ExplicitParent = nullptr;
}

FString APlaceRegionActor::GetDefaultActorLabel() const
{
	if (!Region)
	{
		return Super::GetDefaultActorLabel();
	}

	// Prefer pinyin over the Chinese name.
	FString Stem;
	if (!Region->Name.Pinyin.IsEmpty())
	{
		Stem = Region->Name.Pinyin.ToString();
	}
	else if (!Region->Name.English.IsEmpty())
	{
		Stem = Region->Name.English.ToString();
	}
	else if (!Region->Name.Chinese.IsEmpty())
	{
		Stem = Region->Name.Chinese.ToString();
	}
	else if (Region->GetTypeId() != NAME_None)
	{
		Stem = Region->GetTypeId().ToString();
	}

	if (Stem.IsEmpty())
	{
		return Super::GetDefaultActorLabel();
	}

	Stem = Stem.Replace(TEXT(" "), TEXT("_"));
	return FString::Printf(TEXT("Region_%s"), *Stem);
}
#endif
