#include "Tools/GalleryTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryCoverageTest,
	"HutongLayout.Gallery.EveryType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryCoverageTest::RunTest(const FString& Parameters)
{
	// The gallery is where every type is seen side by side: a building type it does not place is one
	// nobody checks by eye.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to attach in"), World)) return false;
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	const TArray<UClass*> Placed = HutongGallery::AttachedClasses(S, World);
	int32 Types = 0;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* C = *It;
		if (!C->IsChildOf(UHutongBuildingComponent::StaticClass()) || C == UHutongBuildingComponent::StaticClass()) continue;
		if (C->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
		if (C->GetName().StartsWith(TEXT("SKEL_")) || C->GetName().StartsWith(TEXT("REINST_"))) continue;
		++Types;
		TestTrue(FString::Printf(TEXT("the gallery places a %s"), *C->GetName()), Placed.Contains(C));
	}
	TestTrue(FString::Printf(TEXT("building types found (%d)"), Types), Types >= 17);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryClusterTest,
	"HutongLayout.Gallery.Clusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryClusterTest::RunTest(const FString& Parameters)
{
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	const TArray<HutongGallery::FPlacedPiece> All = HutongGallery::Plan(S);
	TestTrue(TEXT("the whole gallery places something"), All.Num() > 0);

	// The category tools split the whole gallery: every piece in exactly one of them.
	TArray<FString> FromCategories;
	for (const EHutongGalleryCategory C : { EHutongGalleryCategory::Walls, EHutongGalleryCategory::Houses,
		EHutongGalleryCategory::Gates, EHutongGalleryCategory::Courtyard, EHutongGalleryCategory::Street,
		EHutongGalleryCategory::Temples })
	{
		S->GalleryCategory = C;
		const TArray<HutongGallery::FPlacedPiece> Part = HutongGallery::Plan(S);
		TestTrue(FString::Printf(TEXT("the %s gallery places something"), *HutongGallery::CategoryName(C)), Part.Num() > 0);
		for (const HutongGallery::FPlacedPiece& P : Part)
		{
			TestEqual(FString::Printf(TEXT("%s is in its own category"), *P.Label), (int32)P.Category, (int32)C);
			FromCategories.Add(P.Label);
		}
	}
	TestEqual(TEXT("the categories add up to the whole gallery"), FromCategories.Num(), All.Num());
	for (const HutongGallery::FPlacedPiece& P : All)
	{
		TestTrue(FString::Printf(TEXT("%s is in a category gallery"), *P.Label), FromCategories.Contains(P.Label));
	}

	// Laid out: no two footprints touch, a cluster's pieces are one run, categories are bands.
	for (int32 i = 0; i < All.Num(); ++i)
	{
		for (int32 j = i + 1; j < All.Num(); ++j)
		{
			TestFalse(FString::Printf(TEXT("%s clear of %s"), *All[i].Label, *All[j].Label),
				All[i].Footprint.Intersect(All[j].Footprint));
		}
	}
	TSet<FString> Closed;
	for (int32 i = 1; i < All.Num(); ++i)
	{
		const FString Prev = HutongGallery::CategoryName(All[i - 1].Category) + All[i - 1].Cluster;
		const FString Here = HutongGallery::CategoryName(All[i].Category) + All[i].Cluster;
		if (Prev != Here)
		{
			Closed.Add(Prev);
			TestFalse(FString::Printf(TEXT("cluster %s is together"), *All[i].Cluster), Closed.Contains(Here));
		}
		if (All[i].Category != All[i - 1].Category)
		{
			double PrevBack = -BIG_NUMBER;
			for (int32 k = 0; k < i; ++k) PrevBack = FMath::Max(PrevBack, All[k].Footprint.Max.Y);
			TestTrue(FString::Printf(TEXT("%s stands behind the band before it"), *HutongGallery::CategoryName(All[i].Category)),
				All[i].Footprint.Min.Y >= PrevBack + 2.0 * S->Spacing);
		}
	}
	return true;
}

#endif
