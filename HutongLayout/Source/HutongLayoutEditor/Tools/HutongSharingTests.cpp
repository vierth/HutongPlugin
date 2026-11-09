#include "Tools/HutongSharing.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongExchange.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	template <typename T>
	T* MakeSharingProbe(const FVector2D& Size)
	{
		T* Building = NewObject<T>(GetTransientPackage());
		Building->SetFootprintSize(Size);
		return Building;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSharingRulesTest,
	"HutongLayout.Sharing.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSharingRulesTest::RunTest(const FString& Parameters)
{
	const TArray<HutongSharing::FRule> Rules = HutongSharing::Rules();
	const HutongSharing::FRule& AsLaid = Rules[0];
	const HutongSharing::FRule& Toward = Rules[2];
	const EHutongTracedOutline Outline = EHutongTracedOutline::Walls;

	const UHutongBuildingComponent* A = MakeSharingProbe<UHutongSiheyuanBuildingComponent>(FVector2D(1020.0, 520.0));
	const UHutongBuildingComponent* B = MakeSharingProbe<UHutongSiheyuanBuildingComponent>(FVector2D(1012.0, 516.0));
	TestNotEqual(TEXT("different as drawn"), HutongSharing::Measure(A, AsLaid, Outline).Mesh, HutongSharing::Measure(B, AsLaid, Outline).Mesh);
	const HutongSharing::FOutcome Moved = HutongSharing::Measure(A, Toward, Outline);
	TestEqual(TEXT("one mesh on the step"), Moved.Mesh, HutongSharing::Measure(B, Toward, Outline).Mesh);
	TestTrue(TEXT("whole steps"), FMath::IsNearlyZero(FMath::Fmod(Moved.Size.X, 32.0)) && FMath::IsNearlyZero(FMath::Fmod(Moved.Size.Y, 32.0)));
	TestTrue(TEXT("no side past 16 cm"), Moved.EdgeMove <= 16.0);
	TestEqual(TEXT("the building itself keeps its size"), A->GetFootprintSize(), FVector2D(1020.0, 520.0));
	TestFalse(TEXT("a plan is measured as what it would build"), Moved.Mesh.IsEmpty());

	auto TracedDepth = [&](double Depth)
	{
		UHutongUnknownBuildingComponent* Traced = NewObject<UHutongUnknownBuildingComponent>(GetTransientPackage());
		Traced->CourtRole = EHutongCourtRole::FrontRow;
		Traced->SetFootprintSize(FVector2D(960.0, Depth));
		return HutongSharing::Measure(Traced, Toward, Outline).Size.Y;
	};
	TestEqual(TEXT("grows rather than leave the band"), TracedDepth(360.0), 384.0);
	TestEqual(TEXT("shrinks rather than leave the band"), TracedDepth(425.0), 416.0);

	for (double Width = 500.0; Width < 2000.0; Width += 13.0)
	{
		const UHutongBuildingComponent* House = MakeSharingProbe<UHutongSiheyuanBuildingComponent>(FVector2D(Width, 600.0));
		TestEqual(TEXT("bay count kept"), HutongSharing::Measure(House, Toward, Outline).Bays, House->GetDrawnBayCount());
		TestEqual(TEXT("bay count kept, one side"), HutongSharing::Measure(House, Rules.Last(), Outline).Bays, House->GetDrawnBayCount());
	}

	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
		TArray<UHutongBuildingComponent*> Row;
		for (int32 i = 0; i < 3; ++i)
		{
			AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform(FVector(1010.0 * i, 0.0, 0.0)), TEXT("SharingRow"));
			UHutongUnknownBuildingComponent* House = NewObject<UHutongUnknownBuildingComponent>(Actor, NAME_None, RF_Transactional);
			House->SetFootprintSize(FVector2D(1010.0, 640.0));
			Actor->AddInstanceComponent(House);
			House->RegisterComponent();
			Row.Add(House);
		}
		const auto Results = [&](const HutongSharing::FRule& Rule, double& OutWorst, double& OutMean, double& OutLength)
		{
			const TMap<const UHutongBuildingComponent*, HutongSharing::FChosen> Chosen = HutongSharing::ChooseWithNeighbours(Row, Rule, Outline);
			OutWorst = OutMean = OutLength = 0.0;
			for (const UHutongBuildingComponent* House : Row)
			{
				const HutongSharing::FChosen& C = Chosen.FindChecked(House);
				OutWorst = FMath::Max(OutWorst, C.Shift.Size());
				OutMean += C.Shift.X / 3.0;
				OutLength += C.Size.X;
			}
		};
		double Alone, AloneMean, AloneLength, Cancel, CancelMean, CancelLength;
		Results(Rules[2], Alone, AloneMean, AloneLength);
		Results(Rules[3], Cancel, CancelMean, CancelLength);
		TestEqual(TEXT("alone they all grow"), AloneLength, 3072.0);
		TestEqual(TEXT("alone the ends go about 14 cm"), Alone, 14.0, 0.2);
		TestEqual(TEXT("cancelling, up and down alternate"), CancelLength, 3008.0);
		TestTrue(TEXT("cancelling, barely anything moves"), Cancel < 5.0);
		TestTrue(TEXT("the row's centre stays"), FMath::Abs(AloneMean) < 0.1 && FMath::Abs(CancelMean) < 0.1);
		World->DestroyWorld(false);
	}

	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
		TArray<UHutongBuildingComponent*> Placed;
		const auto PlaceAt = [&]<typename T>(T* Each, AStaticMeshActor* Actor, const FVector2D& Size)
		{
			Each->SetFootprintSize(Size);
			Actor->AddInstanceComponent(Each);
			Each->RegisterComponent();
			Placed.Add(Each);
			return Each;
		};
		const auto PlaceHouse = [&](const FVector2D& At, const FVector2D& Size)
		{
			AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform(FVector(At, 0.0)), TEXT("SharingRow"));
			return PlaceAt(NewObject<UHutongUnknownBuildingComponent>(Actor, NAME_None, RF_Transactional), Actor, Size);
		};
		const UHutongBuildingComponent* First = nullptr;
		for (int32 i = 0; i < 9; ++i)
		{
			const UHutongBuildingComponent* Each = PlaceHouse(FVector2D(1010.0 * i, 0.0), FVector2D(1010.0, 640.0));
			if (i == 0) First = Each;
		}
		const UHutongBuildingComponent* Beyond = PlaceHouse(FVector2D(1010.0 * 9 + 36.0, 0.0), FVector2D(992.0, 640.0));
		AStaticMeshActor* WallActor = HutongGen::SpawnEmptyActor(World, FTransform(FVector(100.0, 640.0, 0.0)), TEXT("SharingRow"));
		const UHutongBuildingComponent* Alongside = PlaceAt(NewObject<UHutongWallBuildingComponent>(WallActor, NAME_None, RF_Transactional), WallActor, FVector2D(800.0, 37.0));
		const TMap<const UHutongBuildingComponent*, HutongSharing::FChosen> Chosen = HutongSharing::ChooseWithNeighbours(Placed, Rules[2], Outline);
		const HutongSharing::FChosen& Held = Chosen.FindChecked(First);
		TestTrue(TEXT("pushed less"), Held.bHeldBack);
		TestTrue(TEXT("less than the 56 cm free"), Held.Shift.Size() < 50.0);
		TestFalse(TEXT("what it does not join is not in its group"), Chosen.FindChecked(Beyond).bHeldBack);
		const HutongSharing::FChosen& WallMove = Chosen.FindChecked(Alongside);
		TestEqual(TEXT("the wall keeps its length"), WallMove.Size.X, 800.0, 0.5);
		World->DestroyWorld(false);
	}

	UHutongWallBuildingComponent* Wall = MakeSharingProbe<UHutongWallBuildingComponent>(FVector2D(1000.0, 37.0));
	const double Thickness = Wall->GetFootprintSize().Y;
	Wall->FootprintSkew.Corner00 = FVector2D(5.0, 0.0);
	const HutongSharing::FOutcome Small = HutongSharing::Measure(Wall, Toward, Outline);
	TestEqual(TEXT("wall length on the step"), Small.Size.X, 992.0);
	TestEqual(TEXT("wall thickness untouched"), Small.Size.Y, Thickness);
	TestTrue(TEXT("small corner squared"), Small.bSquared);
	Wall->FootprintSkew.Corner00 = FVector2D(40.0, 0.0);
	TestFalse(TEXT("large corner stays"), HutongSharing::Measure(Wall, Toward, Outline).bSquared);

	UHutongBuildingComponent* Locked = MakeSharingProbe<UHutongSiheyuanBuildingComponent>(FVector2D(1010.0, 530.0));
	Locked->bLocked = true;
	TestEqual(TEXT("locked keeps its size"), HutongSharing::Measure(Locked, Toward, Outline).Size, FVector2D(1010.0, 530.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSharingApplyTest,
	"HutongLayout.Sharing.Apply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSharingApplyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	TArray<UHutongBuildingComponent*> Placed;
	const auto Place = [&]<typename T>(T* Each, AActor* Actor, const FVector2D& Size)
	{
		Each->bPlanOnly = true;
		Each->SetFootprintSize(Size);
		Actor->AddInstanceComponent(Each);
		Each->RegisterComponent();
		Placed.Add(Each);
		return Each;
	};
	const auto PlaceHouse = [&](double X, const FVector2D& Size)
	{
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform(FVector(X, 0.0, 0.0)), TEXT("SharingApply"));
		return Place(NewObject<UHutongSiheyuanBuildingComponent>(Actor, NAME_None, RF_Transactional), Actor, Size);
	};
	const UHutongBuildingComponent* Left = PlaceHouse(0.0, FVector2D(1010.0, 640.0));
	const UHutongBuildingComponent* Right = PlaceHouse(1010.0, FVector2D(1003.0, 640.0));
	AStaticMeshActor* WallActor = HutongGen::SpawnEmptyActor(World, FTransform(FVector(2013.0, 0.0, 0.0)), TEXT("SharingApply"));
	const UHutongBuildingComponent* Wall = Place(NewObject<UHutongWallBuildingComponent>(WallActor, NAME_None, RF_Transactional), WallActor, FVector2D(700.0, 37.0));
	UHutongBuildingComponent* Locked = PlaceHouse(9000.0, FVector2D(1010.0, 640.0));
	Locked->bLocked = true;

	const auto Start = [](const UHutongBuildingComponent* B) { return B->GetOwner()->GetActorLocation().X; };
	const auto End = [&](const UHutongBuildingComponent* B) { return Start(B) + B->GetFootprintSize().X; };
	const double Centre = 0.5 * ((Start(Left) + End(Left)) / 2.0 + (Start(Right) + End(Right)) / 2.0);
	const double WallFarEnd = End(Wall);

	const HutongSharing::FApplied First = HutongSharing::Apply(Placed, EHutongTracedOutline::Walls, /*bShowDialog*/ false);
	TestTrue(TEXT("something changed"), First.Resized > 0);
	for (const UHutongBuildingComponent* House : { Left, Right })
	{
		TestTrue(TEXT("whole steps"), FMath::IsNearlyZero(FMath::Fmod(House->GetFootprintSize().X, 32.0)));
		TestTrue(TEXT("a plan stays a plan"), House->bPlanOnly);
	}
	TestEqual(TEXT("the join stays closed"), End(Left), Start(Right), 0.5);
	TestEqual(TEXT("the wall meets the house"), End(Right), Start(Wall), 0.5);
	TestEqual(TEXT("the wall's far end stays"), End(Wall), WallFarEnd, 0.5);
	TestEqual(TEXT("the group's centre stays"), 0.5 * ((Start(Left) + End(Left)) / 2.0 + (Start(Right) + End(Right)) / 2.0), Centre, 0.5);
	TestEqual(TEXT("locked keeps its size"), Locked->GetFootprintSize(), FVector2D(1010.0, 640.0));
	TestEqual(TEXT("locked stays"), Start(Locked), 9000.0);

	const HutongSharing::FApplied Again = HutongSharing::Apply(Placed, EHutongTracedOutline::Walls, /*bShowDialog*/ false);
	TestEqual(TEXT("applied twice, nothing resizes"), Again.Resized, 0);
	TestEqual(TEXT("applied twice, nothing moves"), Again.Moved, 0);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSharingApplyDumpTest,
	"HutongDev.SharingApply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSharingApplyDumpTest::RunTest(const FString& Parameters)
{
	const FString Input = FPaths::ProjectSavedDir() / TEXT("HutongSharingInput.hutong.json");
	if (!IFileManager::Get().FileExists(*Input))
	{
		AddInfo(TEXT("No input file."));
		return true;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	HutongExchange::FResult Result;
	HutongExchange::ImportAtRecordedTransforms(World, Input, HutongExchange::EMode::Additive, NAME_None, Result);
	const double Start = FPlatformTime::Seconds();
	const HutongSharing::FApplied Applied = HutongSharing::Apply(HutongDetailOps::CollectLoaded(World), EHutongTracedOutline::Walls, /*bShowDialog*/ false);
	UE_LOG(LogTemp, Display, TEXT("SHARING applied in %.0f s: %d resized, %d moved, %d held back, %d collide"),
		FPlatformTime::Seconds() - Start, Applied.Resized, Applied.Moved, Applied.HeldBack, Applied.Collides);
	const FString Output = FPaths::ProjectSavedDir() / TEXT("HutongSharingApplied.hutong.json");
	HutongExchange::FResult Exported;
	HutongExchange::ExportLoaded(World, Output, Exported);
	const HutongSharing::FRule AsLaid = HutongSharing::Rules()[0];
	const HutongSharing::FWallPieceCount Walls = HutongSharing::CountWallPieces(HutongDetailOps::CollectLoaded(World));
	UE_LOG(LogTemp, Display, TEXT("SHARING after applying:%s"), *HutongSharing::DescribeWallPieces(Walls));
	TSet<FString> Meshes;
	for (const UHutongBuildingComponent* Building : HutongDetailOps::CollectLoaded(World))
	{
		const FString Mesh = HutongSharing::Measure(Building, AsLaid, EHutongTracedOutline::Walls).Mesh;
		if (!Mesh.IsEmpty()) Meshes.Add(Mesh);
	}
	UE_LOG(LogTemp, Display, TEXT("SHARING after applying: %d meshes besides walls"), Meshes.Num());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSharingDumpTest,
	"HutongDev.SharingReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSharingDumpTest::RunTest(const FString& Parameters)
{
	const FString Input = FPaths::ProjectSavedDir() / TEXT("HutongSharingInput.hutong.json");
	if (!IFileManager::Get().FileExists(*Input))
	{
		AddInfo(TEXT("No input file."));
		return true;
	}
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	HutongExchange::FResult Result;
	HutongExchange::ImportAtRecordedTransforms(World, Input, HutongExchange::EMode::Additive, NAME_None, Result);
	const double Start = FPlatformTime::Seconds();
	TArray<HutongSharing::FRow> Rows;
	HutongSharing::MeasureAll(HutongDetailOps::CollectLoaded(World), EHutongTracedOutline::Walls, /*bShowDialog*/ false, Rows);
	UE_LOG(LogTemp, Display, TEXT("SHARING measured in %.0f s\n%s"), FPlatformTime::Seconds() - Start, *HutongSharing::SummaryTable(Rows));
	const double PiecesStart = FPlatformTime::Seconds();
	const FString Pieces = HutongSharing::DescribeWallPieces(HutongSharing::CountWallPieces(HutongDetailOps::CollectLoaded(World), &Rows));
	UE_LOG(LogTemp, Display, TEXT("SHARING pieces in %.0f s%s"), FPlatformTime::Seconds() - PiecesStart, *Pieces);
	HutongSharing::WriteCsv(Rows, FPaths::ProjectSavedDir() / TEXT("HutongSharing.csv"));
	HutongSharing::WriteChecksCsv(Rows, FPaths::ProjectSavedDir() / TEXT("HutongSharingChecks.csv"));
	World->DestroyWorld(false);
	return true;
}

#endif
