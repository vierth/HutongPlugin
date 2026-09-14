#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Generation/HutongFootprint.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongMeshInspect.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Tools/HutongSnap.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongExchange.h"
#include "HutongLayoutEdMode.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

namespace
{
	// A trapezoid: the +X end wall angled, the rest of the rectangle where it was. Along the run,
	// so both modes read it the same.
	FHutongFootprintSkew EndWallSkew(EHutongSkewMode Mode = EHutongSkewMode::Ends)
	{
		FHutongFootprintSkew S;
		S.Mode = Mode;
		S.Corner10 = FVector2D(60.0, 0.0);
		S.Corner11 = FVector2D(-40.0, 0.0);
		return S;
	}

	UHutongBuildingComponent* PlaceSkewed(UWorld* World, UClass* Class, const FVector2D& Footprint,
		const FHutongFootprintSkew& Skew, const FTransform& Xform, const TCHAR* NameBase, bool bPlanOnly)
	{
		UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(GetTransientPackage(), Class);
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;
		Template->SetFootprintSize(Footprint);
		Template->FootprintSkew = Skew;

		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);
		AStaticMeshActor* Actor = bPlanOnly
			? HutongGen::SpawnEmptyActor(World, Xform, NameBase)
			: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, NameBase, FHutongPalette());
		if (!Actor) return nullptr;

		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(Actor, Class, NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		B->bPlanOnly = bPlanOnly;
		B->SetFootprintSize(Footprint);
		B->FootprintSkew = Skew;
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		if (bPlanOnly) B->ApplyPlanOutline();
		return B;
	}

	// Distance of P from the line through A and B, in plan.
	double DistanceToLine(const FVector2D& A, const FVector2D& B, const FVector2D& P)
	{
		const FVector2D D = (B - A).GetSafeNormal();
		return FMath::Abs(HutongFootprint::Cross(D, P - A));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintWarpBoxTest,
	"HutongLayout.Footprint.WarpBox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintWarpBoxTest::RunTest(const FString& Parameters)
{
	const double W = 400.0, D = 300.0, H = 250.0;
	const FHutongFootprintSkew Whole = EndWallSkew(EHutongSkewMode::Whole);
	FVector2D Corners[4];
	HutongFootprint::Corners(FVector2D(W, D), Whole, Corners);

	// The face that was at x == W now lies on the line between the two moved corners; z is untouched.
	FDynamicMesh3 Mesh;
	HutongMeshUtils::AppendBox(Mesh, FVector3d(0, 0, 0), FVector3d(W, D, H));
	TArray<int32> EndFace;
	TArray<double> Heights;
	for (int32 vid : Mesh.VertexIndicesItr())
	{
		if (FMath::IsNearlyEqual(Mesh.GetVertex(vid).X, W, 1.0e-6))
		{
			EndFace.Add(vid);
			Heights.Add(Mesh.GetVertex(vid).Z);
		}
	}
	TestTrue(TEXT("the box has an end face"), EndFace.Num() == 4);
	TestTrue(TEXT("the warp accepts a trapezoid"), HutongMeshUtils::WarpFootprint(Mesh, W, D, Whole));
	for (int32 i = 0; i < EndFace.Num(); ++i)
	{
		const FVector3d P = Mesh.GetVertex(EndFace[i]);
		TestTrue(TEXT("an end vertex lies on the skewed end line"),
			DistanceToLine(Corners[1], Corners[2], FVector2D(P.X, P.Y)) < 1.0e-6);
		TestTrue(TEXT("z is untouched"), FMath::IsNearlyEqual(P.Z, Heights[i], 1.0e-9));
	}
	const HutongMeshInspect::FShellReport Report = HutongMeshInspect::InspectShell(Mesh);
	TestTrue(TEXT("the warped box is still a solid"), Report.IsSolid());
	// Quad area times height: (W*D + half the end wall's shear, +60 and -40 make +10 over the depth).
	const double ExpectedVolume = HutongFootprint::QuadArea(Corners) * H;
	TestTrue(TEXT("the volume is the quadrilateral's"), FMath::IsNearlyEqual(Report.Volume, ExpectedVolume, 1.0e-3 * ExpectedVolume));

	// Zero offsets are the identity.
	{
		FDynamicMesh3 Plain, Same;
		HutongMeshUtils::AppendBox(Plain, FVector3d(0, 0, 0), FVector3d(W, D, H));
		Same.Copy(Plain);
		TestTrue(TEXT("the rectangle is accepted"), HutongMeshUtils::WarpFootprint(Same, W, D, FHutongFootprintSkew()));
		bool bIdentical = true;
		for (int32 vid : Plain.VertexIndicesItr())
		{
			bIdentical = bIdentical && Plain.GetVertex(vid).Equals(Same.GetVertex(vid), 1.0e-9);
		}
		TestTrue(TEXT("zero offsets move nothing"), bIdentical);
	}

	// A corner dragged past the opposite edge folds the quadrilateral and is refused whole.
	{
		FDynamicMesh3 Before, After;
		HutongMeshUtils::AppendBox(Before, FVector3d(0, 0, 0), FVector3d(W, D, H));
		After.Copy(Before);
		FHutongFootprintSkew Fold;
		Fold.Mode = EHutongSkewMode::Whole;
		Fold.Corner11 = FVector2D(-W - 50.0, 0.0);
		TestFalse(TEXT("a folded quad is not valid"), HutongFootprint::IsSkewValid(FVector2D(W, D), Fold));
		TestFalse(TEXT("the warp refuses a fold"), HutongMeshUtils::WarpFootprint(After, W, D, Fold));
		Fold.Mode = EHutongSkewMode::Ends;
		TestFalse(TEXT("under Ends a corner pulled past its zone is refused too"), HutongFootprint::IsSkewValid(FVector2D(W, D), Fold));
		bool bUntouched = true;
		for (int32 vid : Before.VertexIndicesItr())
		{
			bUntouched = bUntouched && Before.GetVertex(vid).Equals(After.GetVertex(vid), 1.0e-9);
		}
		TestTrue(TEXT("a refused warp leaves the mesh as built"), bUntouched);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintHouseGableTest,
	"HutongLayout.Footprint.HouseGableOnSkewLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintHouseGableTest::RunTest(const FString& Parameters)
{
	// A whole house through the component's own build: the +X gable wall, taken from the
	// unwarped mesh, ends up on the skewed end line once the skew is set.
	const FVector2D Footprint(1040.0, 600.0);
	UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	B->DetailLevel = EHutongDetail::Near;
	B->bBuildLODChain = false;
	B->SetFootprintSize(Footprint);

	TArray<FDynamicMesh3> Plain;
	B->BuildLODs(Plain);
	if (!TestTrue(TEXT("the plain house builds"), Plain.Num() == 1 && Plain[0].TriangleCount() > 0)) return false;

	// The gable plane: of the x planes near the +X end, in the wall band below the eave, the one
	// with the most vertices on it. The largest x alone is a 墀頭 or 散水 edge with a handful.
	const double Eave = B->Params.GetEaveHeight();
	TMap<int64, int32> Planes;
	for (int32 vid : Plain[0].VertexIndicesItr())
	{
		const FVector3d P = Plain[0].GetVertex(vid);
		if (P.Z > 10.0 && P.Z < Eave - 10.0 && P.X > 0.8 * Footprint.X)
		{
			Planes.FindOrAdd(FMath::RoundToInt64(P.X * 100.0))++;
		}
	}
	double GableX = -1.0;
	int32 Most = 0;
	for (const auto& It : Planes)
	{
		if (It.Value > Most) { Most = It.Value; GableX = It.Key / 100.0; }
	}
	TArray<int32> Wall;
	for (int32 vid : Plain[0].VertexIndicesItr())
	{
		const FVector3d P = Plain[0].GetVertex(vid);
		if (FMath::IsNearlyEqual(P.X, GableX, 1.0e-3) && P.Z > 10.0 && P.Z < Eave - 10.0) Wall.Add(vid);
	}
	TestTrue(TEXT("the gable plane carries vertices"), Wall.Num() >= 4);

	B->FootprintSkew = EndWallSkew();
	TArray<FDynamicMesh3> Skewed;
	B->BuildLODs(Skewed);
	// The split at the end zone adds vertices; the built ones keep their ids and are what is probed.
	if (!TestTrue(TEXT("the skewed house builds"), Skewed.Num() == 1 && Skewed[0].VertexCount() >= Plain[0].VertexCount())) return false;

	// The plane x == GableX maps to the line between its two mapped ends.
	const FVector2D L0 = HutongFootprint::Map(Footprint, B->FootprintSkew, GableX, 0.0);
	const FVector2D L1 = HutongFootprint::Map(Footprint, B->FootprintSkew, GableX, Footprint.Y);
	double Worst = 0.0;
	for (int32 vid : Wall)
	{
		const FVector3d P = Skewed[0].GetVertex(vid);
		Worst = FMath::Max(Worst, DistanceToLine(L0, L1, FVector2D(P.X, P.Y)));
	}
	TestTrue(FString::Printf(TEXT("the gable wall lies on the skewed line (worst %.4f cm)"), Worst), Worst < 1.0e-3);
	TestTrue(TEXT("the skewed end is off the rectangle"), FMath::Abs(L0.X - L1.X) > 50.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintEndsWallTest,
	"HutongLayout.Footprint.EndsCutKeepsBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintEndsWallTest::RunTest(const FString& Parameters)
{
	// A wall's end cut on the bias: the body up to the end zone is exactly as built and the long
	// faces never leave their lines.
	const double L = 1200.0, T = 37.0, H = 300.0;
	const FVector2D Size(L, T);
	FHutongFootprintSkew Skew;
	Skew.Corner10 = FVector2D(-60.0, 0.0);
	Skew.Corner11 = FVector2D(30.0, 0.0);

	FDynamicMesh3 Mesh;
	HutongMeshUtils::AppendBox(Mesh, FVector3d(0, 0, 0), FVector3d(L, T, H));
	// A second box overlapping the run, the way a 下鹼 or cap does.
	HutongMeshUtils::AppendBox(Mesh, FVector3d(-5.0, -5.0, 0), FVector3d(L + 5.0, T + 5.0, 60.0));
	const int32 BuiltVertices = Mesh.VertexCount();
	TestTrue(TEXT("the ends-cut skew is valid"), HutongFootprint::IsSkewValid(Size, Skew));
	TestTrue(TEXT("the warp accepts it"), HutongMeshUtils::WarpFootprint(Mesh, L, T, Skew));
	TestTrue(TEXT("the split gave the run a seam"), Mesh.VertexCount() > BuiltVertices);

	const double Zone = HutongFootprint::EndZone(Size, Skew, false);
	TestTrue(TEXT("the zone is a little past the thickness and the pull"), Zone > T && Zone < 0.5 * L);
	TestTrue(TEXT("the start end is untouched"), HutongFootprint::EndZone(Size, Skew, true) == 0.0);

	FVector2D C[4];
	HutongFootprint::Corners(Size, Skew, C);
	TestTrue(TEXT("the corner is where it was pulled"), FMath::IsNearlyEqual(C[1].Y, 0.0) && FMath::IsNearlyEqual(C[1].X, L - 60.0));
	const HutongMeshInspect::FShellReport Report = HutongMeshInspect::InspectShell(Mesh);
	TestTrue(TEXT("still solid after the split and the shear"), Report.IsSolid());

	int32 OnEnd = 0;
	for (int32 vid : Mesh.VertexIndicesItr())
	{
		const FVector3d P = Mesh.GetVertex(vid);
		// y is never moved under Ends: the split puts new vertices on the faces' diagonals, at any y
		// between the long faces, but nothing lands outside the two boxes' own across extent.
		TestTrue(TEXT("nothing moves across the run"), P.Y >= -5.0 - 1e-6 && P.Y <= T + 5.0 + 1e-6);
		// Anything short of the zone is where it was built: still on x == 0, x == -5 or x == the seam.
		if (P.X < L - Zone - 1.0)
		{
			TestTrue(TEXT("the body keeps its x"),
				FMath::IsNearlyEqual(P.X, 0.0, 1e-6) || FMath::IsNearlyEqual(P.X, -5.0, 1e-6));
		}
		if (FMath::IsNearlyEqual(P.Y, 0.0, 1e-6) && P.X > L - 61.0 && P.X < L - 59.0) ++OnEnd;
		if (FMath::IsNearlyEqual(P.Y, T, 1e-6) && P.X > L + 29.0 && P.X < L + 31.0) ++OnEnd;
	}
	TestTrue(TEXT("the end corners landed on the cut"), OnEnd >= 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintEndsAcrossTest,
	"HutongLayout.Footprint.EndsPushesAcross",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintEndsAcrossTest::RunTest(const FString& Parameters)
{
	// A corner pulled across the run under Ends: the end face turns, the long face runs straight
	// from the zone seam to the moved corner, and nothing short of the seam moves at all.
	const double L = 1200.0, T = 37.0, H = 300.0;
	const FVector2D Size(L, T);
	FHutongFootprintSkew Skew;
	Skew.Corner11 = FVector2D(0.0, 30.0);

	TestTrue(TEXT("an across pull is valid"), HutongFootprint::IsSkewValid(Size, Skew));
	const double Zone = HutongFootprint::EndZone(Size, Skew, false);
	TestTrue(TEXT("an across pull opens a zone"), Zone > T && Zone < 0.5 * L);
	TestTrue(TEXT("the start end is untouched"), HutongFootprint::EndZone(Size, Skew, true) == 0.0);

	// The map: the seam stays, the corner lands, the +Y face between them is the straight line.
	const FVector2D Seam = HutongFootprint::Map(Size, Skew, L - Zone, T);
	const FVector2D End = HutongFootprint::Map(Size, Skew, L, T);
	const FVector2D Mid = HutongFootprint::Map(Size, Skew, L - 0.5 * Zone, T);
	TestTrue(TEXT("the seam is where it was built"), Seam.Equals(FVector2D(L - Zone, T), 1.0e-9));
	TestTrue(TEXT("the corner landed across"), End.Equals(FVector2D(L, T + 30.0), 1.0e-9));
	TestTrue(TEXT("the long face is straight between them"), DistanceToLine(Seam, End, Mid) < 1.0e-9);
	TestTrue(TEXT("the other corner at that end stays"), HutongFootprint::Map(Size, Skew, L, 0.0).Equals(FVector2D(L, 0.0), 1.0e-9));
	TestTrue(TEXT("the body stays"), HutongFootprint::Map(Size, Skew, 0.5 * L, T).Equals(FVector2D(0.5 * L, T), 1.0e-9));

	FDynamicMesh3 Mesh;
	HutongMeshUtils::AppendBox(Mesh, FVector3d(0, 0, 0), FVector3d(L, T, H));
	TestTrue(TEXT("the warp accepts it"), HutongMeshUtils::WarpFootprint(Mesh, L, T, Skew));
	TestTrue(TEXT("still solid"), HutongMeshInspect::InspectShell(Mesh).IsSolid());
	int32 OnCorner = 0;
	for (int32 vid : Mesh.VertexIndicesItr())
	{
		const FVector3d P = Mesh.GetVertex(vid);
		TestTrue(TEXT("nothing moves along the run"), P.X >= -1e-6 && P.X <= L + 1e-6);
		if (P.X < L - Zone - 1.0)
		{
			TestTrue(TEXT("the body keeps its y"), FMath::IsNearlyEqual(P.Y, 0.0, 1e-6) || FMath::IsNearlyEqual(P.Y, T, 1e-6));
		}
		if (FMath::IsNearlyEqual(P.X, L, 1e-6) && FMath::IsNearlyEqual(P.Y, T + 30.0, 1e-6)) ++OnCorner;
	}
	TestTrue(TEXT("the corner's vertices landed"), OnCorner >= 2);

	// Pushed across past the other corner's line, the end edge reverses and the skew is refused.
	FHutongFootprintSkew Fold;
	Fold.Corner11 = FVector2D(0.0, -T - 10.0);
	TestFalse(TEXT("a corner pushed past its neighbour is refused"), HutongFootprint::IsSkewValid(Size, Fold));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintEndsOverhangTest,
	"HutongLayout.Footprint.EndsOverhangsStopAtTheCorner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintEndsOverhangTest::RunTest(const FString& Parameters)
{
	// A wall cut on a shallow bias: one corner pulled a long way along the run, the other left.
	// The cap and the 下鹼 stand proud of the long faces, and on the cut plane's own continuation
	// a course that stands proud by c extends c / tan(angle) past the corner — a blade some metres
	// long at a few degrees. Nothing may reach past the corner by more than it stands proud.
	const double L = 1200.0, T = 37.0;
	FHutongWallParams P;
	FDynamicMesh3 Plain;
	UHutongWallBuildingComponent::BuildWallMesh(P, L, T, /*bAlongY*/ false, Plain, EHutongDetail::Near);
	if (!TestTrue(TEXT("the wall builds"), Plain.TriangleCount() > 0)) return false;
	double Proud = 0.0, PlainMaxX = -1e9;
	for (int32 vid : Plain.VertexIndicesItr())
	{
		const FVector3d V = Plain.GetVertex(vid);
		Proud = FMath::Max(Proud, FMath::Max(-V.Y, V.Y - T));
		PlainMaxX = FMath::Max(PlainMaxX, V.X);
	}
	TestTrue(TEXT("something stands proud of the faces"), Proud > 1.0);
	const bool bPlainSolid = HutongMeshInspect::InspectShell(Plain).IsSolid();

	FHutongFootprintSkew Skew;
	Skew.Corner11 = FVector2D(600.0, 0.0);
	FDynamicMesh3 Mesh;
	Mesh.Copy(Plain);
	TestTrue(TEXT("the warp accepts the shallow cut"), HutongMeshUtils::WarpFootprint(Mesh, L, T, Skew));
	double MaxX = -1e9;
	for (int32 vid : Mesh.VertexIndicesItr()) MaxX = FMath::Max(MaxX, Mesh.GetVertex(vid).X);
	const double Overshoot = MaxX - (L + 600.0);
	TestTrue(FString::Printf(TEXT("nothing reaches past the pulled corner by more than it stands proud (%.1f cm past, %.1f proud, plain run ended %.1f past)"), Overshoot, Proud, PlainMaxX - L),
		Overshoot <= Proud + (PlainMaxX - L) + 1.0e-6);
	TestEqual(FString::Printf(TEXT("as solid as it was built (%.1f cm past the corner)"), Overshoot),
		HutongMeshInspect::InspectShell(Mesh).IsSolid(), bPlainSolid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintCornerUndoTest,
	"HutongLayout.Footprint.CornerUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintCornerUndoTest::RunTest(const FString& Parameters)
{
	// A corner moved inside a transaction comes back with one undo, on a component made the way
	// the placement tools make one: a bare NewObject, no flags asked for.
	if (!GEditor || !GEditor->Trans) { AddError(TEXT("no transaction buffer to undo through")); return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform::Identity, TEXT("CornerUndo"));
	if (!TestNotNull(TEXT("the actor spawns"), Actor)) { World->DestroyWorld(false); return false; }
	UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(Actor, TEXT("House"));
	B->DetailLevel = EHutongDetail::Massing;
	B->bBuildLODChain = false;
	B->bPlanOnly = true;
	B->SetFootprintSize(FVector2D(1040.0, 600.0));
	Actor->AddInstanceComponent(B);
	B->RegisterComponent();
	B->ApplyPlanOutline();
	TestTrue(TEXT("a building component is transactional"), B->HasAnyFlags(RF_Transactional));

	const FHutongFootprintSkew Moved = EndWallSkew();
	GEditor->BeginTransaction(FText::FromString(TEXT("Move Corner")));
	Actor->Modify();
	B->Modify();
	if (UActorComponent* Outline = Actor->FindComponentByClass<UHutongPlanOutlineComponent>()) Outline->Modify();
	B->FootprintSkew = Moved;
	B->Rebuild();
	GEditor->EndTransaction();
	TestTrue(TEXT("the corner moved"), B->FootprintSkew == Moved);

	GEditor->UndoTransaction();
	TestTrue(TEXT("one undo puts the corner back"), B->FootprintSkew.IsZero());
	if (const UHutongPlanOutlineComponent* Outline = Actor->FindComponentByClass<UHutongPlanOutlineComponent>())
	{
		TestTrue(TEXT("the outline is the rectangle again"), Outline->Skew.IsZero());
	}
	GEditor->RedoTransaction();
	TestTrue(TEXT("redo moves it again"), B->FootprintSkew == Moved);
	GEditor->UndoTransaction();

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintQuadHitTest,
	"HutongLayout.Footprint.QuadHitTest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintQuadHitTest::RunTest(const FString& Parameters)
{
	const FVector2D Size(400.0, 300.0);
	FVector2D Q[4];
	HutongFootprint::Corners(Size, EndWallSkew(), Q);

	TestTrue(TEXT("the middle is inside"), HutongFootprint::PointInQuad(Q, FVector2D(200.0, 150.0)));
	// Past the rectangle's +X edge but inside the pushed-out front corner.
	TestTrue(TEXT("the pushed-out corner's ground is inside"), HutongFootprint::PointInQuad(Q, FVector2D(430.0, 20.0)));
	// Inside the rectangle but outside the pulled-in rear corner.
	TestFalse(TEXT("the pulled-in corner's ground is outside"), HutongFootprint::PointInQuad(Q, FVector2D(390.0, 280.0)));
	TestFalse(TEXT("far away is outside"), HutongFootprint::PointInQuad(Q, FVector2D(-10.0, 150.0)));

	TestTrue(TEXT("edge distance is to the nearest side"),
		FMath::IsNearlyEqual(HutongFootprint::DistanceToQuadEdge(Q, FVector2D(200.0, 30.0)), 30.0, 1.0e-6));
	TestTrue(TEXT("the rectangle's area is the rectangle's"),
		FMath::IsNearlyEqual(HutongFootprint::QuadArea(Q), 400.0 * 300.0 + 0.5 * (60.0 - 40.0) * 300.0, 1.0e-6));
	TestTrue(TEXT("a wall is line-like"), HutongFootprint::IsLineLike(FVector2D(1200.0, 37.0)));
	TestFalse(TEXT("a room is not"), HutongFootprint::IsLineLike(Size));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintSnapCacheTest,
	"HutongLayout.Footprint.SnapCacheCorners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintSnapCacheTest::RunTest(const FString& Parameters)
{
	// A skewed building offers its own corners and bearings to the next placement.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FVector2D Footprint(1040.0, 600.0);
	const FTransform Xform(FRotator(0.0, 30.0, 0.0), FVector(500.0, 700.0, 0.0));
	UHutongBuildingComponent* B = PlaceSkewed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		Footprint, EndWallSkew(), Xform, TEXT("SnapSkew"), /*bPlanOnly*/ false);
	if (!TestNotNull(TEXT("the house places"), B)) { World->DestroyWorld(false); return false; }

	HutongSnap::FFootprintCache Cache;
	Cache.Refresh(World);
	if (!TestEqual(TEXT("one footprint is cached"), Cache.Items.Num(), 1)) { World->DestroyWorld(false); return false; }

	FVector2D Local[4];
	B->GetFootprintCorners(Local);
	const FVector Expected = Xform.TransformPosition(FVector(Local[2].X, Local[2].Y, 0.0));
	TestTrue(TEXT("the cached corner is the skewed one"), Cache.Items[0].Corners[2].Equals(Expected, 1.0e-3));
	const FVector RectCorner = Xform.TransformPosition(FVector(Footprint.X, Footprint.Y, 0.0));
	TestFalse(TEXT("and not the rectangle's"), Cache.Items[0].Corners[2].Equals(RectCorner, 1.0));

	// Three bearings on a trapezoid: the two long sides are still parallel, the ends are not.
	const TArray<double> Yaws = HutongSnap::GatherEdgeYaws(Cache.Items, Expected, 200.0, nullptr);
	TestEqual(TEXT("a trapezoid offers three bearings"), Yaws.Num(), 3);

	// Probed from outside the corner, along the diagonal, so the corner is nearer than either edge.
	const FVector Centre = Xform.TransformPosition(FVector(0.5 * Footprint.X, 0.5 * Footprint.Y, 0.0));
	const FVector Outward = (Expected - Centre).GetSafeNormal2D();
	const HutongSnap::FResult R = HutongSnap::FindSnap(Cache.Items, Expected + Outward * 30.0, 80.0, nullptr);
	TestTrue(TEXT("the skewed corner snaps"), R.bSnapped && R.Point.Equals(Expected, 1.0e-3));

	// The plan outline of a laid-out one carries the same corners.
	UHutongBuildingComponent* Plan = PlaceSkewed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		Footprint, EndWallSkew(), FTransform(FVector(9000.0, 0.0, 0.0)), TEXT("PlanSkew"), /*bPlanOnly*/ true);
	if (TestNotNull(TEXT("the plan places"), Plan))
	{
		const UHutongPlanOutlineComponent* Outline =
			Plan->GetOwner()->FindComponentByClass<UHutongPlanOutlineComponent>();
		if (TestNotNull(TEXT("the plan has an outline"), Outline))
		{
			TestTrue(TEXT("the outline carries the skew"), Outline->Skew == Plan->FootprintSkew);
			const FBox Bounds = Outline->CalcBounds(Plan->GetOwner()->GetActorTransform()).GetBox();
			TestTrue(TEXT("the bounds reach the pushed-out corner"), Bounds.Max.X > 9000.0 + Footprint.X + 50.0);
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintSnapAlongLineTest,
	"HutongLayout.Footprint.SnapAlongLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintSnapAlongLineTest::RunTest(const FString& Parameters)
{
	// A corner held to a line snaps to where the line crosses a neighbour's face, from however
	// far off the face the cursor is, so long as the corner itself is within reach of the crossing.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FTransform Xform(FRotator(0.0, 30.0, 0.0), FVector(500.0, 700.0, 0.0));
	UHutongBuildingComponent* B = PlaceSkewed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		FVector2D(1040.0, 600.0), FHutongFootprintSkew(), Xform, TEXT("Face"), /*bPlanOnly*/ true);
	if (!TestNotNull(TEXT("the neighbour places"), B)) { World->DestroyWorld(false); return false; }
	HutongSnap::FFootprintCache Cache;
	Cache.Refresh(World);
	if (!TestEqual(TEXT("one footprint is cached"), Cache.Items.Num(), 1)) { World->DestroyWorld(false); return false; }

	const FVector A = Cache.Items[0].Corners[0], Bc = Cache.Items[0].Corners[1];
	const FVector M = A + (Bc - A) * 0.4;
	const FVector2D Edge = FVector2D(Bc.X - A.X, Bc.Y - A.Y).GetSafeNormal();
	// A line 50° off the face, through M.
	const double Rad = FMath::DegreesToRadians(50.0);
	const FVector2D Dir(Edge.X * FMath::Cos(Rad) - Edge.Y * FMath::Sin(Rad), Edge.X * FMath::Sin(Rad) + Edge.Y * FMath::Cos(Rad));
	const FVector Origin = M - FVector(Dir.X, Dir.Y, 0.0) * 300.0;

	// The corner sits 40 cm short of the face: it lands on it.
	{
		const FVector Query = M - FVector(Dir.X, Dir.Y, 0.0) * 40.0;
		const HutongSnap::FResult R = HutongSnap::FindSnapAlongLine(Cache.Items, Origin, Dir, Query, 100.0);
		TestTrue(TEXT("the crossing snaps"), R.bSnapped);
		TestTrue(TEXT("and lands on the face"), R.bSnapped && FVector::Dist2D(R.Point, M) < 1.0e-3);
		TestTrue(TEXT("the face's bearing comes back"), R.bSnapped && FMath::Abs(FMath::FindDeltaAngleDegrees(R.EdgeYawDeg,
			FMath::RadiansToDegrees(FMath::Atan2(Edge.Y, Edge.X)))) < 1.0e-3);
	}
	// Too far short, no snap; and a line parallel to the face crosses nothing.
	{
		const FVector Far = M - FVector(Dir.X, Dir.Y, 0.0) * 250.0;
		TestFalse(TEXT("out of reach does not snap"), HutongSnap::FindSnapAlongLine(Cache.Items, Origin, Dir, Far, 100.0).bSnapped);
		const FVector Beside = M + FVector(-Edge.Y, Edge.X, 0.0) * 30.0;
		TestFalse(TEXT("a parallel line does not snap"), HutongSnap::FindSnapAlongLine(Cache.Items, Beside, Edge, Beside, 100.0).bSnapped);
	}
	// A line that crosses the face's line well past the end of the segment finds nothing there.
	{
		const FVector Past = Bc + FVector(Edge.X, Edge.Y, 0.0) * 400.0;
		const FVector O2 = Past - FVector(Dir.X, Dir.Y, 0.0) * 300.0;
		const FVector Q2 = Past - FVector(Dir.X, Dir.Y, 0.0) * 40.0;
		TestFalse(TEXT("past the segment does not snap"), HutongSnap::FindSnapAlongLine(Cache.Items, O2, Dir, Q2, 100.0).bSnapped);
	}
	// The corner's own start is never a target.
	{
		const HutongSnap::FResult R = HutongSnap::FindSnapAlongLine(Cache.Items, M, Dir, M, 100.0);
		TestFalse(TEXT("the origin itself is refused"), R.bSnapped && FVector::Dist2D(R.Point, M) < 2.0);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintToolForBuildingTest,
	"HutongLayout.Footprint.ToolForBuilding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintToolForBuildingTest::RunTest(const FString& Parameters)
{
	// Every kind a tool places names that tool, so a click on it brings the tool up; a wall names
	// the tool for its role. The compound's own passage is placed by no tool and names none.
	TArray<UClass*> Classes;
	GetDerivedClasses(UHutongBuildingComponent::StaticClass(), Classes, /*bRecursive*/ true);
	for (UClass* Class : Classes)
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
		UHutongBuildingComponent* Probe = NewObject<UHutongBuildingComponent>(GetTransientPackage(), Class);
		const FString Tool = UHutongLayoutEdMode::ToolIdentifierFor(Probe);
		const bool bPlacedByNoTool = Class == UHutongPassageBuildingComponent::StaticClass();
		TestEqual(FString::Printf(TEXT("%s names a tool (%s)"), *Class->GetName(), *Tool), Tool.IsEmpty(), bPlacedByNoTool);
	}
	UHutongWallBuildingComponent* Wall = NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	Wall->Params.Role = EHutongWallRole::Courtyard;
	TestEqual(TEXT("a courtyard wall names the court wall tool"), UHutongLayoutEdMode::ToolIdentifierFor(Wall), FString(TEXT("HutongCourtWallTool")));
	Wall->Params.Role = EHutongWallRole::Perimeter;
	TestEqual(TEXT("a lane wall names the wall tool"), UHutongLayoutEdMode::ToolIdentifierFor(Wall), FString(TEXT("HutongWallTool")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintExchangeTest,
	"HutongLayout.Footprint.ExchangeCarriesSkew",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintExchangeTest::RunTest(const FString& Parameters)
{
	// The corners travel in the blob, in the layout-only fields, and on the record for the ghost.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FVector2D Footprint(1040.0, 600.0);
	UHutongBuildingComponent* Skewed = PlaceSkewed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		Footprint, EndWallSkew(), FTransform(FRotator(0.0, 40.0, 0.0), FVector(300.0, 200.0, 0.0)), TEXT("Skewed"), false);
	UHutongBuildingComponent* Plain = PlaceSkewed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		Footprint, FHutongFootprintSkew(), FTransform(FVector(5000.0, 0.0, 0.0)), TEXT("Plain"), false);
	if (!Skewed || !Plain) { World->DestroyWorld(false); return false; }

	// The component blob: present when set, pruned when it is the rectangle.
	{
		TSharedPtr<FJsonObject> Blob = HutongExchange::WriteComponent(Skewed);
		TestTrue(TEXT("a skewed blob carries the offsets"), Blob.IsValid() && Blob->HasField(TEXT("footprintSkew")));
		TSharedPtr<FJsonObject> PlainBlob = HutongExchange::WriteComponent(Plain);
		TestTrue(TEXT("a rectangle's blob does not"), PlainBlob.IsValid() && !PlainBlob->HasField(TEXT("footprintSkew")));

		UHutongSiheyuanBuildingComponent* Target = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		FString Problem;
		TestTrue(TEXT("the blob reads back"), Blob.IsValid() && HutongExchange::ReadComponent(Blob.ToSharedRef(), Target, Problem));
		TestTrue(TEXT("the offsets are restored"), Target->FootprintSkew == EndWallSkew());
	}

	// The layout-only record: the Footprint category on the base class travels, and the ghost's corners follow.
	{
		HutongExchange::FSceneFile File;
		HutongExchange::FResult Result;
		TArray<UHutongBuildingComponent*> Both = { Skewed, Plain };
		TestTrue(TEXT("the set gathers"), HutongExchange::Gather(Both, World, File, Result, /*bLayoutOnly*/ true));
		const HutongExchange::FRecord* Rec = File.Records.FindByPredicate(
			[&](const HutongExchange::FRecord& R) { return R.Id == Skewed->BuildingId; });
		if (TestNotNull(TEXT("the skewed record is in the set"), Rec))
		{
			TestTrue(TEXT("the record carries the skew"), Rec->Skew == EndWallSkew());
			TestTrue(TEXT("the layout fields carry it"), Rec->FootprintFields.IsValid() && Rec->FootprintFields->HasField(TEXT("footprintSkew")));
			FVector2D Corners[4];
			HutongExchange::FootprintCornersInSetFrame(*Rec, Corners);
			FVector2D Local[4];
			Skewed->GetFootprintCorners(Local);
			const double RectEnd = FVector2D::Distance(Corners[1], Corners[2]);
			const double SkewEnd = FVector2D::Distance(Local[1], Local[2]);
			TestTrue(TEXT("the ghost's end wall has the skewed length"), FMath::IsNearlyEqual(RectEnd, SkewEnd, 1.0e-3));
		}

		// Through a file and back into a fresh world.
		const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("HutongSkew.hutong.json"));
		TestTrue(TEXT("the file writes"), HutongExchange::Write(File, Path, Result));
		UWorld* Other = UWorld::CreateWorld(EWorldType::Editor, false);
		HutongExchange::FResult ImportResult;
		HutongExchange::ImportAtRecordedTransforms(Other, Path, HutongExchange::EMode::Additive, NAME_None, ImportResult);
		// An additive import mints fresh ids, so the match is on the corners themselves.
		int32 Restored = 0, Total = 0;
		for (UHutongBuildingComponent* C : HutongDetailOps::CollectLoaded(Other))
		{
			++Total;
			if (C->FootprintSkew == EndWallSkew()) ++Restored;
		}
		TestEqual(TEXT("both buildings import"), Total, 2);
		TestEqual(TEXT("the layout import restores the skewed building"), Restored, 1);
		Other->DestroyWorld(false);
		IFileManager::Get().Delete(*Path);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFootprintConvertTest,
	"HutongLayout.Footprint.ConvertCarriesSkew",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFootprintConvertTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const HutongDetailOps::FConvertTarget House = HutongDetailOps::FindConvertTarget(TEXT("house (房)"));
	const HutongDetailOps::FConvertTarget LaneWall = HutongDetailOps::FindConvertTarget(TEXT("lane wall (院牆)"));
	if (!TestTrue(TEXT("the targets exist"), House.IsValid() && LaneWall.IsValid())) { World->DestroyWorld(false); return false; }

	// 鋪面房 → 房 keeps the corners, and so does 房 → 院牆: a wall's end can be cut on the bias.
	UHutongBuildingComponent* Shop = PlaceSkewed(World, UHutongShopfrontBuildingComponent::StaticClass(),
		FVector2D(1120.0, 640.0), EndWallSkew(), FTransform(FVector(0.0, 0.0, 0.0)), TEXT("Shop"), false);
	if (!TestNotNull(TEXT("the shop places"), Shop)) { World->DestroyWorld(false); return false; }
	UHutongBuildingComponent* AsHouse = HutongDetailOps::ConvertBuilding(Shop, House, FString());
	if (TestNotNull(TEXT("the shop converts"), AsHouse))
	{
		TestTrue(TEXT("a house keeps the shop's corners"), AsHouse->FootprintSkew == EndWallSkew());
		UHutongBuildingComponent* AsWall = HutongDetailOps::ConvertBuilding(AsHouse, LaneWall, FString());
		if (TestNotNull(TEXT("the house converts to a wall"), AsWall))
		{
			TestTrue(TEXT("a wall keeps the corner offsets"), AsWall->FootprintSkew == EndWallSkew());
		}
	}

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
