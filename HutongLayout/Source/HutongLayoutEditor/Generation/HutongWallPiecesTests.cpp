#include "Generation/HutongWallPieces.h"
#include "Generation/HutongBuildingComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMeshEditor.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Generation/HutongActorSpawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongRoofTile.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using namespace HutongGen::WallPieces;
	using UE::Geometry::FDynamicMesh3;

	TArray<FPiece> PlanProbeWall(const FHutongWallParams& P, double L, const FHutongFootprintSkew& Skew = FHutongFootprintSkew())
	{
		return Plan(P, L, P.GetThickness(), 0.0, 0.0, Skew);
	}

	void AssemblePieces(const FHutongWallParams& P, const TArray<FPiece>& Pieces, FDynamicMesh3& Out)
	{
		Out.EnableAttributes();
		Out.Attributes()->EnableMaterialID();
		for (const FPiece& Piece : Pieces)
		{
			FDynamicMesh3 Mesh;
			BuildPiece(P, Piece, P.GetThickness(), Mesh, EHutongDetail::Near);
			for (const int32 V : Mesh.VertexIndicesItr())
			{
				const FVector3d At = Mesh.GetVertex(V);
				Mesh.SetVertex(V, FVector3d(At.X * Piece.ScaleX + OriginX(Piece), At.Y, At.Z));
			}
			UE::Geometry::FDynamicMeshEditor Editor(&Out);
			UE::Geometry::FMeshIndexMappings Mappings;
			Editor.AppendMesh(&Mesh, Mappings);
		}
	}

	double ShareAgreeingWithWhole(const FHutongWallParams& P, double L, const FDynamicMesh3& Assembled)
	{
		FDynamicMesh3 Whole;
		UHutongWallBuildingComponent::BuildWallMesh(P, L, P.GetThickness(), false, Whole, EHutongDetail::Near);
		UE::Geometry::FDynamicMeshAABBTree3 WholeTree(&Whole), PiecesTree(&Assembled);
		int32 Agree = 0, Total = 0;
		for (double X = 3.0; X < L - 3.0; X += 7.0)
		{
			for (double Z = 2.0; Z < P.GetHeight() + 25.0; Z += 9.0)
			{
				const FRay3d Ray(FVector3d(X, -200.0, Z), FVector3d(0.0, 1.0, 0.0));
				double A = -1.0, B = -1.0;
				int32 Tri = -1;
				const bool bHitA = WholeTree.FindNearestHitTriangle(Ray, A, Tri);
				const bool bHitB = PiecesTree.FindNearestHitTriangle(Ray, B, Tri);
				++Total;
				Agree += (bHitA == bHitB) && (!bHitA || FMath::Abs(A - B) < 1.0);
			}
		}
		return double(Agree) / FMath::Max(Total, 1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallPiecesTest,
	"HutongLayout.Walls.Pieces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallPiecesTest::RunTest(const FString& Parameters)
{
	const auto Covers = [this](const TArray<FPiece>& Pieces, double L, const TCHAR* What)
	{
		double Cursor = 0.0;
		for (const FPiece& Piece : Pieces)
		{
			TestTrue(FString::Printf(TEXT("%s: pieces meet at %.2f"), What, Piece.X0), FMath::Abs(Piece.X0 - Cursor) < 0.001);
			Cursor = Piece.X0 + Piece.Length * Piece.ScaleX;
		}
		TestTrue(FString::Printf(TEXT("%s: reaches the end"), What), FMath::Abs(Cursor - L) < 0.001);
	};

	FHutongWallParams Lane;
	const TArray<FPiece> Plain = PlanProbeWall(Lane, 1000.0);
	Covers(Plain, 1000.0, TEXT("plain"));
	TestEqual(TEXT("1000 cm is a 40 cm filler and four long modules"), Plain.Num(), 5);
	TestEqual(TEXT("filler first"), (int32)Plain[0].Kind, (int32)EKind::Filler);
	FDynamicMesh3 Assembled;
	AssemblePieces(Lane, Plain, Assembled);
	const double Agreement = ShareAgreeingWithWhole(Lane, 1000.0, Assembled);
	AddInfo(FString::Printf(TEXT("plain wall surface agrees at %.3f"), Agreement));
	TestTrue(TEXT("plain wall's surface is the whole wall's"), Agreement > 0.95);

	for (const FPiece& Piece : Plain)
	{
		FDynamicMesh3 Mesh;
		BuildPiece(Lane, Piece, Lane.GetThickness(), Mesh, EHutongDetail::Near);
		const auto Bounds = Mesh.GetBounds();
		const double From = Piece.Kind == EKind::Filler ? -Piece.Length : 0.0;
		TestTrue(TEXT("piece stays within its length"), Bounds.Min.X > From - 0.01 && Bounds.Max.X < From + Piece.Length + 0.01);
	}

	TestEqual(TEXT("a step is whole brick faces"), FMath::Fmod(StepCm, 24.0), 0.0);
	TestEqual(TEXT("a step is whole tile rows"), FMath::Fmod(StepCm, HutongGen::RoofTile::DefaultRowSpacing), 0.0);

	{
		FHutongWallParams Rows;
		const double L = 1013.0;
		FDynamicMesh3 Mesh;
		AssemblePieces(Rows, PlanProbeWall(Rows, L), Mesh);
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);
		const double Y = 0.5 * Rows.GetThickness() - 8.0;
		TArray<double> Heights;
		for (double X = 0.0; X < L; X += 0.5)
		{
			double Hit = -1.0;
			int32 Tri = -1;
			Heights.Add(Tree.FindNearestHitTriangle(FRay3d(FVector3d(X, Y, 2000.0), FVector3d(0.0, 0.0, -1.0)), Hit, Tri) ? 2000.0 - Hit : -1.0);
		}
		TArray<double> Crests;
		for (int32 i = 1; i < Heights.Num() - 1; ++i)
		{
			const bool bRising = Heights[i] > Heights[i - 1] + 0.001;
			if (bRising)
			{
				int32 j = i;
				while (j + 1 < Heights.Num() && FMath::IsNearlyEqual(Heights[j + 1], Heights[i], 0.001)) ++j;
				if (j + 1 < Heights.Num() && Heights[j + 1] < Heights[i] - 0.001) Crests.Add(0.25 * (i + j));
			}
		}
		AddInfo(FString::Printf(TEXT("%d crests along the cap"), Crests.Num()));
		TestTrue(TEXT("the cap has rows to compare"), Crests.Num() >= 10);
		for (int32 i = 2; i < Crests.Num() - 1; ++i)
		{
			const double Gap = Crests[i] - Crests[i - 1];
			TestTrue(FString::Printf(TEXT("row spacing %.1f at %.1f"), Gap, Crests[i]), FMath::Abs(Gap - HutongGen::RoofTile::DefaultRowSpacing) <= 1.0);
		}
	}

	FHutongWallParams Gated;
	Gated.bHasGate = true;
	Gated.GatePosition = 0.37;
	const TArray<FPiece> WithGate = PlanProbeWall(Gated, 1213.0);
	Covers(WithGate, 1213.0, TEXT("gated"));
	const FPiece* Gate = WithGate.FindByPredicate([](const FPiece& Piece) { return Piece.Kind == EKind::Gate; });
	TestNotNull(TEXT("a gate piece"), Gate);
	if (Gate) TestTrue(TEXT("gate centre kept"), FMath::Abs(Gate->X0 + 0.5 * Gate->Length - Gated.GetGateSpans(1213.0)[0].Centre) < 0.01);
	FDynamicMesh3 GatedAssembled;
	AssemblePieces(Gated, WithGate, GatedAssembled);
	const double GateAgreement = ShareAgreeingWithWhole(Gated, 1213.0, GatedAssembled);
	AddInfo(FString::Printf(TEXT("gated wall surface agrees at %.3f"), GateAgreement));
	TestTrue(TEXT("gated wall's surface is the whole wall's"), GateAgreement > 0.95);

	FHutongWallParams Court;
	Court.Role = EHutongWallRole::Courtyard;
	const TArray<FPiece> Windows = PlanProbeWall(Court, 1400.0);
	Covers(Windows, 1400.0, TEXT("windows"));
	const int32 WindowPieces = Windows.FilterByPredicate([](const FPiece& Piece) { return Piece.Kind == EKind::Window; }).Num();
	TestEqual(TEXT("as many window modules as fit the run"), WindowPieces, FMath::FloorToInt32(1400.0 / (3.0 * StepCm)));
	for (const FPiece& Piece : Windows)
	{
		if (Piece.Kind == EKind::Window)
		{
			TestEqual(TEXT("each module carries one window"), PieceParams(Court, Piece).GetWindowCount(Piece.Length), 1);
			TestEqual(TEXT("whole steps"), FMath::Fmod(Piece.Length, StepCm), 0.0);
		}
	}

	TestFalse(TEXT("the run's start closes"), Plain[0].bRunsOnStart);
	TestFalse(TEXT("the run's end closes"), Plain.Last().bRunsOnEnd);
	TestTrue(TEXT("a joint runs on"), Plain[0].bRunsOnEnd && Plain[1].bRunsOnStart);
	{
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Assembled);
		const double RidgeZ = Lane.GetHeight() + 0.5 * HutongCanon::Wall::CapRidgeCourseHeightCm + Lane.CapRidgeHeight;
		int32 Missed = 0;
		for (double X = 1.0; X < 999.0; X += 0.25)
		{
			double Hit = -1.0;
			int32 Tri = -1;
			Missed += !Tree.FindNearestHitTriangle(FRay3d(FVector3d(X, -200.0, RidgeZ), FVector3d(0.0, 1.0, 0.0)), Hit, Tri);
		}
		TestEqual(TEXT("no slit in the ridge"), Missed, 0);
	}

	FHutongFootprintSkew Mitre;
	Mitre.Corner00 = FVector2D(18.3, 0.0);
	Mitre.Corner11 = FVector2D(-18.7, 0.0);
	const TArray<FPiece> Ends = PlanProbeWall(Lane, 1000.0, Mitre);
	TestEqual(TEXT("first piece takes the start corner, snapped"), Ends[0].Skew.Corner00.X, 18.0);
	TestEqual(TEXT("last piece takes the far corner, snapped"), Ends.Last().Skew.Corner11.X, -18.0);
	TestTrue(TEXT("a piece between stays square"), Ends[1].Skew.IsZero());

	Mitre.Mode = EHutongSkewMode::Whole;
	TestEqual(TEXT("whole-mode wall stays one piece"), (int32)PlanProbeWall(Lane, 1000.0, Mitre)[0].Kind, (int32)EKind::Whole);

	TSet<FString> Different;
	for (double L = 300.0; L < 3000.0; L += 37.0)
	{
		for (const FPiece& Piece : PlanProbeWall(Lane, L))
		{
			Different.Add(FString::Printf(TEXT("%d %.0f"), (int32)Piece.Kind, Piece.Length));
		}
	}
	TestTrue(TEXT("different lengths reuse the same modules"), Different.Num() <= 2 + 120);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallInstancesTest,
	"HutongLayout.Walls.PieceInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallInstancesTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Pieces = IConsoleManager::Get().FindConsoleVariable(TEXT("hutong.WallPieces"));
	const bool bWas = Pieces->GetBool();
	Pieces->Set(true);

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	const auto Place = [World](const FVector2D& Size, bool bAlongY)
	{
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform::Identity, TEXT("WallPieces"));
		UHutongWallBuildingComponent* Wall = NewObject<UHutongWallBuildingComponent>(Actor, NAME_None, RF_Transactional);
		Wall->bLengthAlongY = bAlongY;
		Wall->SetFootprintSize(Size);
		Wall->DetailLevel = EHutongDetail::Far;
		Wall->bBuildLODChain = false;
		Actor->AddInstanceComponent(Wall);
		Wall->RegisterComponent();
		Wall->Rebuild();
		return Wall;
	};
	const auto InstanceCount = [](const UHutongBuildingComponent* Wall)
	{
		TArray<UInstancedStaticMeshComponent*> Instances;
		Wall->GetOwner()->GetComponents(Instances);
		int32 Count = 0;
		for (const UInstancedStaticMeshComponent* Comp : Instances) Count += Comp->GetInstanceCount();
		return Count;
	};

	UHutongWallBuildingComponent* Wall = Place(FVector2D(1000.0, 37.0), false);
	TestEqual(TEXT("a filler and four modules"), InstanceCount(Wall), 5);
	TestTrue(TEXT("no one built mesh"), HutongGen::GetBuiltMeshComponent(Wall->GetOwner())->GetStaticMesh() == nullptr);
	TestTrue(TEXT("triangles counted off the instances"), HutongGen::CountBuiltTriangles(Wall->GetOwner(), /*bWaitForBuild*/ true) > 0);
	int32 PiecesPlaced = 0, PieceMeshes = 0;
	HutongGen::CountMeshInstances(Wall->GetOwner(), PiecesPlaced, PieceMeshes);
	TestTrue(TEXT("the readout counts pieces and fewer meshes"), PieceMeshes > 0 && PiecesPlaced >= PieceMeshes);

	Wall->Rebuild();
	TestEqual(TEXT("rebuilt, the same instances"), InstanceCount(Wall), 5);

	UHutongWallBuildingComponent* AlongY = Place(FVector2D(37.0, 1000.0), true);
	TArray<UInstancedStaticMeshComponent*> Turned;
	AlongY->GetOwner()->GetComponents(Turned);
	FBox Ground(ForceInit);
	for (const UInstancedStaticMeshComponent* Comp : Turned)
	{
		for (int32 i = 0; i < Comp->GetInstanceCount(); ++i)
		{
			FTransform Placed;
			Comp->GetInstanceTransform(i, Placed, /*bWorldSpace*/ false);
			TestTrue(TEXT("a turn, not a mirror"), Placed.GetDeterminant() > 0.0);
			Ground += Comp->GetStaticMesh()->GetBoundingBox().TransformBy(Placed);
		}
	}
	AddInfo(FString::Printf(TEXT("along Y stands %s"), *Ground.ToString()));
	TestTrue(TEXT("along Y runs the length of Y"), FMath::Abs(Ground.Min.Y) < 1.0 && FMath::Abs(Ground.Max.Y - 1000.0) < 1.0);
	TestTrue(TEXT("along Y keeps the masonry on the footprint"), Ground.Min.X > -30.0 && Ground.Max.X < 37.0 + 30.0);

	Wall->bPlanOnly = true;
	Wall->Rebuild();
	TestEqual(TEXT("a plan wears no pieces"), InstanceCount(Wall), 0);

	Pieces->Set(false);
	AlongY->Rebuild();
	TestEqual(TEXT("off, no pieces"), InstanceCount(AlongY), 0);
	TestTrue(TEXT("off, one mesh"), HutongGen::GetBuiltMeshComponent(AlongY->GetOwner())->GetStaticMesh() != nullptr);

	Pieces->Set(true);
	AlongY->ApplyPlacementAttachments();
	TestTrue(TEXT("placed built, pieces"), InstanceCount(AlongY) > 0);
	TestTrue(TEXT("placed built, no whole mesh left"), HutongGen::GetBuiltMeshComponent(AlongY->GetOwner())->GetStaticMesh() == nullptr);

	Pieces->Set(bWas);
	World->DestroyWorld(false);
	return true;
}

// Assembled walls in the gallery dump's format, for Scripts/render_sheet.py (HUTONG_GALLERY_DUMP names the file).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallPiecesObjTest,
	"HutongDev.WallPiecesObj",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallPiecesObjTest::RunTest(const FString& Parameters)
{
	const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("HUTONG_GALLERY_DUMP"));
	if (Path.IsEmpty()) return true;
	FHutongWallParams Lane;
	FHutongWallParams Gated;
	Gated.bHasGate = true;
	Gated.GatePosition = 0.37;
	FHutongWallParams Court;
	Court.Role = EHutongWallRole::Courtyard;
	const struct { const TCHAR* Label; const FHutongWallParams& Params; double Length; } Walls[] = {
		{ TEXT("Pieces_Lane"), Lane, 1013.0 },
		{ TEXT("Pieces_Gate"), Gated, 1213.0 },
		{ TEXT("Pieces_Court"), Court, 1213.0 },
	};
	FString Out;
	int32 Base = 1;
	for (const auto& W : Walls)
	{
		FDynamicMesh3 Mesh;
		AssemblePieces(W.Params, PlanProbeWall(W.Params, W.Length), Mesh);
		Out += FString::Printf(TEXT("# item %s 0\n"), W.Label);
		TMap<int32, int32> Index;
		for (const int32 V : Mesh.VertexIndicesItr())
		{
			const FVector3d P = Mesh.GetVertex(V);
			Index.Add(V, Base + Index.Num());
			Out += FString::Printf(TEXT("v %.3f %.3f %.3f\n"), P.X, P.Y, P.Z);
		}
		Base += Index.Num();
		const UE::Geometry::FDynamicMeshMaterialAttribute* Slots = Mesh.Attributes()->GetMaterialID();
		int32 Last = -1;
		for (const int32 T : Mesh.TriangleIndicesItr())
		{
			const int32 Slot = Slots->GetValue(T);
			if (Slot != Last) { Out += FString::Printf(TEXT("usemtl m%d\n"), Slot); Last = Slot; }
			const UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(T);
			Out += FString::Printf(TEXT("f %d %d %d\n"), Index[Tri.A], Index[Tri.B], Index[Tri.C]);
		}
	}
	FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUnicode);
	return true;
}

#endif
