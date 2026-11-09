#include "Generation/HutongMeshTurn.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongMeshUtils.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/GalleryTool.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMeshTurnTest,
	"HutongLayout.Detail.FacingShares",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMeshTurnTest::RunTest(const FString& Parameters)
{
	using HutongGen::FMeshTurn;

	const FVector3d Probe(37.5, 12.25, 9.0);
	for (const EHutongBaySide Side : { EHutongBaySide::MinusY, EHutongBaySide::PlusX, EHutongBaySide::PlusY, EHutongBaySide::MinusX })
	{
		const FVector3d Old = HutongGen::BaySide::RotateVertex(Side, Probe, 600.0, 450.0);
		const FMeshTurn Turn = FMeshTurn::OntoSide(Side, 600.0, 450.0);
		TestTrue(TEXT("a side's turn is RotateVertex's"), Turn.Apply(Probe).Equals(Old, 1.0e-9));
		TestTrue(TEXT("and undoes"), Turn.Unapply(Turn.Apply(Probe)).Equals(Probe, 1.0e-9));
		TestTrue(TEXT("and is the transform the mesh component carries"),
			Turn.ToTransform().TransformPosition(FVector(Probe)).Equals(FVector(Old), 1.0e-6));
	}
	{
		FDynamicMesh3 M;
		const int32 V = M.AppendVertex(Probe);
		HutongMeshUtils::YawVerticesFrom(M, 0, FVector2d(80.0, 80.0), 90.0);
		TestTrue(TEXT("a quarter about a pivot is the yaw's"),
			FMeshTurn::About(FVector2d(80.0, 80.0), 1).Apply(Probe).Equals(M.GetVertex(V), 1.0e-9));
		const FMeshTurn Both = FMeshTurn::About(FVector2d(80.0, 30.0), 2).Then(FMeshTurn::About(FVector2d(30.0, 30.0), 1));
		TestTrue(TEXT("turns compose in order"), Both.Apply(Probe).Equals(
			FMeshTurn::About(FVector2d(30.0, 30.0), 1).Apply(FMeshTurn::About(FVector2d(80.0, 30.0), 2).Apply(Probe)), 1.0e-9));
	}

	const TArray<FString>& Names = HutongPresets::BuiltInSiheyuanNames();
	if (!TestTrue(TEXT("there are house presets"), Names.Num() > 0)) return false;
	const double Frontage = 1040.0, Depth = 600.0;

	auto MakeHouse = [&](UObject* Outer, EHutongBaySide Side)
	{
		UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(Outer, NAME_None, RF_Transactional);
		B->ApplyPresetParams(Names[0]);
		B->DetailLevel = EHutongDetail::Far;
		B->bBuildLODChain = false;
		B->BaySide = Side;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
		B->FootprintX = bAlongX ? Frontage : Depth;
		B->FootprintY = bAlongX ? Depth : Frontage;
		return B;
	};

	TArray<FDynamicMesh3> Canon;
	for (const EHutongBaySide Side : { EHutongBaySide::MinusY, EHutongBaySide::PlusX, EHutongBaySide::PlusY, EHutongBaySide::MinusX })
	{
		const UHutongSiheyuanBuildingComponent* B = MakeHouse(GetTransientPackage(), Side);
		TArray<FDynamicMesh3> OnFootprint, InFrame;
		FMeshTurn Turn;
		B->BuildLODs(OnFootprint);
		B->BuildLODs(InFrame, &Turn);
		if (!TestTrue(TEXT("the house builds"), OnFootprint.Num() > 0 && InFrame.Num() == OnFootprint.Num())) return false;
		TestEqual(TEXT("the turn is the facing's"), Turn.Quarters, (int32)Side);

		double Worst = 0.0;
		for (const int32 Vid : OnFootprint[0].VertexIndicesItr())
		{
			Worst = FMath::Max(Worst, FVector3d::Dist(Turn.Apply(InFrame[0].GetVertex(Vid)), OnFootprint[0].GetVertex(Vid)));
		}
		TestTrue(FString::Printf(TEXT("turned back it lands on the footprint build (worst %.4f cm)"), Worst), Worst < 0.01);
		Canon.Add(MoveTemp(InFrame[0]));
	}
	for (int32 i = 1; i < Canon.Num(); ++i)
	{
		bool bSame = Canon[i].VertexCount() == Canon[0].VertexCount();
		for (const int32 Vid : Canon[0].VertexIndicesItr())
		{
			if (!bSame) break;
			bSame = Canon[i].IsVertex(Vid) && Canon[i].GetVertex(Vid) == Canon[0].GetVertex(Vid);
		}
		TestTrue(FString::Printf(TEXT("facing %d is the same mesh as facing 0 in its own frame"), i), bSame);
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;
	auto Place = [&](EHutongBaySide Side, const FVector& At)
	{
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform(At), TEXT("FacingShares"));
		UHutongSiheyuanBuildingComponent* B = MakeHouse(Actor, Side);
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		B->Rebuild();
		return Actor;
	};
	const AStaticMeshActor* East = Place(EHutongBaySide::PlusX, FVector::ZeroVector);
	const AStaticMeshActor* West = Place(EHutongBaySide::MinusX, FVector(5000.0, 0.0, 0.0));
	const UStaticMeshComponent* EastMesh = HutongGen::GetBuiltMeshComponent(East);
	const UStaticMeshComponent* WestMesh = HutongGen::GetBuiltMeshComponent(West);
	if (TestTrue(TEXT("both are built"), EastMesh && WestMesh && EastMesh->GetStaticMesh() && WestMesh->GetStaticMesh()))
	{
		TestTrue(TEXT("on one shared mesh"), EastMesh->GetStaticMesh() == WestMesh->GetStaticMesh());
		TestTrue(TEXT("worn by a child of the root"), EastMesh != East->GetStaticMeshComponent() && !East->GetStaticMeshComponent()->GetStaticMesh());
		TestFalse(TEXT("turned two ways"), EastMesh->GetRelativeTransform().Equals(WestMesh->GetRelativeTransform(), 1.0e-3));
		const FBox Bounds = EastMesh->Bounds.GetBox();
		TestTrue(FString::Printf(TEXT("the east house covers its footprint (%s)"), *Bounds.ToString()),
			Bounds.Min.X < 1.0 && Bounds.Max.X > Depth - 1.0 && Bounds.Min.Y < 1.0 && Bounds.Max.Y > Frontage - 1.0);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryFacingsTest,
	"HutongLayout.Detail.GalleryFacingsShare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryFacingsTest::RunTest(const FString& Parameters)
{
	// Excluded: walls (along Y a mirror) and the city wall (ramp side fixed on the ground, CityWallBuiltParams).
	UHutongGalleryToolProperties* Settings = NewObject<UHutongGalleryToolProperties>();
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world"), World)) return false;

	auto InFrame = [](const UHutongBuildingComponent* B, FDynamicMesh3& Out)
	{
		TArray<FDynamicMesh3> LODs;
		HutongGen::FMeshTurn Turn;
		B->BuildLODs(LODs, &Turn);
		if (LODs.Num() == 0) return false;
		Out = MoveTemp(LODs[0]);
		return true;
	};
	auto Same = [](const FDynamicMesh3& A, const FDynamicMesh3& B)
	{
		if (A.VertexCount() != B.VertexCount() || A.TriangleCount() != B.TriangleCount()) return false;
		for (const int32 Vid : A.VertexIndicesItr())
		{
			if (!B.IsVertex(Vid) || A.GetVertex(Vid) != B.GetVertex(Vid)) return false;
		}
		return true;
	};

	int32 Checked = 0;
	TArray<FString> Differ;
	HutongGallery::ForEachAttached(Settings, World, [&](const FString& Label, AStaticMeshActor* Actor)
	{
		UHutongBuildingComponent* B = Actor->FindComponentByClass<UHutongBuildingComponent>();
		if (!B || !B->HasGeometry()) return;
		B->DetailLevel = EHutongDetail::Far;
		B->bBuildLODChain = false;
		B->bPlanOnly = false;
		FDynamicMesh3 Laid;
		if (!InFrame(B, Laid)) return;
		for (int32 Quarter = 1; Quarter < 4; ++Quarter)
		{
			UHutongBuildingComponent* T = DuplicateObject<UHutongBuildingComponent>(B, Actor);
			if (!HutongGallery::TurnPiece(T, Quarter)) continue;
			FDynamicMesh3 Turned;
			++Checked;
			if (!InFrame(T, Turned) || !Same(Laid, Turned))
			{
				if (!B->IsA<UHutongWallBuildingComponent>() && !B->IsA<UHutongCityWallBuildingComponent>()) Differ.Add(FString::Printf(TEXT("%s turned %d°"), *Label, 90 * Quarter));
			}
		}
	});
	TestTrue(FString::Printf(TEXT("pieces were turned (%d)"), Checked), Checked > 20);
	TestEqual(FString::Printf(TEXT("every turned piece but the walls shares its mesh; these do not: %s"),
		*FString::Join(Differ, TEXT("; "))), Differ.Num(), 0);

	World->DestroyWorld(false);
	return true;
}

#endif
