#include "Misc/AutomationTest.h"
#include "Tools/HutongWallChain.h"
#include "Tools/HutongWallRun.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	double LineDistance(const FVector2D& P, const FVector2D& A, const FVector2D& Dir)
	{
		const FVector2D D = Dir.GetSafeNormal();
		const FVector2D R = P - A;
		return FMath::Abs(R.X * D.Y - R.Y * D.X);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallChainMiterTest,
	"HutongLayout.Walls.ChainMelds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallChainMiterTest::RunTest(const FString& Parameters)
{
	using namespace HutongWallChain;
	const double T = 37.0;

	// Three legs (left turn, sharper right) on the centre line: at each join, one segment's end corners are the next one's start corners.
	{
		TArray<FVector2D> Points = { FVector2D(0, 0), FVector2D(800, 0), FVector2D(800, 600), FVector2D(300, 900) };
		TArray<FSegment> Segments;
		TestTrue(TEXT("the chain builds"), Build(Points, T, 0.5 * T, FEndFace(), FEndFace(), Segments));
		if (!TestEqual(TEXT("three segments"), Segments.Num(), 3)) return false;
		for (int32 i = 0; i + 1 < Segments.Num(); ++i)
		{
			FVector2D A[4], B[4];
			Corners(Segments[i], T, A);
			Corners(Segments[i + 1], T, B);
			// End of i: corners 1 (y = 0), 2 (y = T); start of i + 1: corners 0 (y = 0), 3 (y = T).
			TestTrue(FString::Printf(TEXT("join %d melds on the outer face"), i), FVector2D::Distance(A[1], B[0]) < 1.0e-6);
			TestTrue(FString::Printf(TEXT("join %d melds on the inner face"), i), FVector2D::Distance(A[2], B[3]) < 1.0e-6);
			TestFalse(FString::Printf(TEXT("join %d is cut"), i), Segments[i].Skew.IsZero());
		}
		// Outer ends rest on nothing, so they are square.
		TestTrue(TEXT("the start is square"), Segments[0].Skew.Corner00.IsNearlyZero() && Segments[0].Skew.Corner01.IsNearlyZero());
		TestTrue(TEXT("the end is square"), Segments.Last().Skew.Corner10.IsNearlyZero() && Segments.Last().Skew.Corner11.IsNearlyZero());
		// Centre line drawn: first segment's faces at +/- half thickness about y = 0.
		FVector2D C[4];
		Corners(Segments[0], T, C);
		TestTrue(TEXT("the drawn line is the centre"), FMath::IsNearlyEqual(C[0].Y, -0.5 * T, 1.0e-6) && FMath::IsNearlyEqual(C[3].Y, 0.5 * T, 1.0e-6));
	}

	// Straight run needs no cut; a repeated click leaves no segment.
	{
		TArray<FVector2D> Points = { FVector2D(0, 0), FVector2D(500, 0), FVector2D(1000, 0), FVector2D(1002, 0) };
		TArray<FSegment> Segments;
		TestTrue(TEXT("the straight chain builds"), Build(Points, T, 0.5 * T, FEndFace(), FEndFace(), Segments));
		TestEqual(TEXT("the repeated point is dropped"), Segments.Num(), 2);
		for (const FSegment& S : Segments) TestTrue(TEXT("a straight join is square"), S.Skew.IsZero());
	}

	// Drawn along one face: body entirely on one side, joins still meld.
	{
		TArray<FVector2D> Points = { FVector2D(0, 0), FVector2D(600, 0), FVector2D(600, 600) };
		TArray<FSegment> Segments;
		TestTrue(TEXT("the face-drawn chain builds"), Build(Points, T, 0.0, FEndFace(), FEndFace(), Segments));
		if (!TestEqual(TEXT("two segments"), Segments.Num(), 2)) return false;
		FVector2D A[4], B[4];
		Corners(Segments[0], T, A);
		Corners(Segments[1], T, B);
		TestTrue(TEXT("the drawn line is the y = 0 face"), FMath::IsNearlyEqual(A[0].Y, 0.0, 1.0e-6) && FMath::IsNearlyEqual(A[3].Y, T, 1.0e-6));
		TestTrue(TEXT("the outer corner of the turn is the drawn corner"), FVector2D::Distance(A[1], FVector2D(600, 0)) < 1.0e-6);
		TestTrue(TEXT("the faces meld"), FVector2D::Distance(A[1], B[0]) < 1.0e-6 && FVector2D::Distance(A[2], B[3]) < 1.0e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallChainFlushTest,
	"HutongLayout.Walls.ChainEndsFlush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallChainFlushTest::RunTest(const FString& Parameters)
{
	using namespace HutongWallChain;
	const double T = 37.0;

	// End resting on a face 60° off: both end corners on that face's line (flush). Start on a square face stays plain.
	FEndFace Start;
	Start.bSet = true; Start.Point = FVector2D(0, 0); Start.Dir = FVector2D(0, 1);
	FEndFace End;
	End.bSet = true; End.Point = FVector2D(1000, 0);
	const double A = FMath::DegreesToRadians(60.0);
	End.Dir = FVector2D(FMath::Cos(A), FMath::Sin(A));

	TArray<FVector2D> Points = { FVector2D(0, 0), FVector2D(1000, 0) };
	TArray<FSegment> Segments;
	TestTrue(TEXT("the run builds"), Build(Points, T, 0.5 * T, Start, End, Segments));
	if (!TestEqual(TEXT("one segment"), Segments.Num(), 1)) return false;
	FVector2D C[4];
	Corners(Segments[0], T, C);
	TestTrue(TEXT("the end's outer corner is on the face"), LineDistance(C[1], End.Point, End.Dir) < 1.0e-6);
	TestTrue(TEXT("the end's inner corner is on the face"), LineDistance(C[2], End.Point, End.Dir) < 1.0e-6);
	TestFalse(TEXT("the end is cut"), Segments[0].Skew.Corner10.IsNearlyZero() && Segments[0].Skew.Corner11.IsNearlyZero());
	TestTrue(TEXT("a square face leaves the start square"), Segments[0].Skew.Corner00.IsNearlyZero() && Segments[0].Skew.Corner01.IsNearlyZero());
	TestTrue(TEXT("the start corners are on the square face"), LineDistance(C[0], Start.Point, Start.Dir) < 1.0e-6 && LineDistance(C[3], Start.Point, Start.Dir) < 1.0e-6);

	// Face nearly along the run is not a flush end: the cut would run metres.
	FEndFace Along;
	Along.bSet = true; Along.Point = FVector2D(1000, 0); Along.Dir = FVector2D(1, 0.05).GetSafeNormal();
	TArray<FSegment> Shallow;
	Build(Points, T, 0.5 * T, FEndFace(), Along, Shallow);
	TestTrue(TEXT("a near-parallel face is left alone"), Shallow.Num() == 1 && Shallow[0].Skew.IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallChainSideTest,
	"HutongLayout.Walls.OuterFaceOnNeighbourFace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallChainSideTest::RunTest(const FString& Parameters)
{
	using namespace HutongWallChain;
	const double T = 37.0;
	// SideAlongFace, as a placed run's vertex drag uses it (drawing takes the corner rule, CornerEnd).
	// House front along +X, inside toward +Y. A run continuing it along +X puts its outer face on the
	// house face: body toward +Y, drawn line at y = 0. Reversed, body still toward +Y, now the y = T face.
	double DrawnY = -1.0;
	TestTrue(TEXT("a face along the run sets a side"), SideAlongFace(FVector2D(1, 0), 0.0, -1000.0, FVector2D(0, 1), T, DrawnY));
	TestTrue(TEXT("the body is on the house's side"), FMath::IsNearlyEqual(DrawnY, 0.0));
	TestTrue(TEXT("run the other way too"), SideAlongFace(FVector2D(-1, 0), 0.0, -1000.0, FVector2D(0, 1), T, DrawnY));
	TestTrue(TEXT("still on the house's side"), FMath::IsNearlyEqual(DrawnY, T));
	// At the house corner both edges are offered, inward diagonal: the edge along the run decides the side.
	TestTrue(TEXT("a corner offers the edge along the run"), SideAlongFace(FVector2D(1, 0), 90.0, 0.0, FVector2D(-1, 1).GetSafeNormal(), T, DrawnY));
	TestTrue(TEXT("and the body goes inside"), FMath::IsNearlyEqual(DrawnY, 0.0));
	// Face across the run is a butt joint, not a side.
	TestFalse(TEXT("a face across the run sets no side"), SideAlongFace(FVector2D(1, 0), 90.0, -1000.0, FVector2D(1, 0), T, DrawnY));
	TestFalse(TEXT("nor one 45° off"), SideAlongFace(FVector2D(1, 0), 45.0, -1000.0, FVector2D(-1, 1).GetSafeNormal(), T, DrawnY));
	// 13° off: not along, unless it was along last frame.
	TestFalse(TEXT("13° off is not along"), SideAlongFace(FVector2D(1, 0), 13.0, -1000.0, FVector2D(0, 1), T, DrawnY));
	TestTrue(TEXT("but stays along once decided"), SideAlongFace(FVector2D(1, 0), 13.0, -1000.0, FVector2D(0, 1), T, DrawnY, AlongFaceDegAfter));
	// Setback: drawn line falls outside the wall away from the house; built face lands Setback in from it.
	const double S = 3.0;
	TestTrue(TEXT("a setback still sets a side"), SideAlongFace(FVector2D(1, 0), 0.0, -1000.0, FVector2D(0, 1), T, DrawnY, AlongFaceDeg, S));
	TestTrue(TEXT("the drawn line is the setback outside the y = 0 face"), FMath::IsNearlyEqual(DrawnY, -S));
	{
		TArray<FSegment> Segments;
		FVector2D C[4];
		TestTrue(TEXT("the run builds off the set-back line"), Build({ FVector2D(0, 0), FVector2D(500, 0) }, T, DrawnY, FEndFace(), FEndFace(), Segments));
		Corners(Segments[0], T, C);
		TestTrue(TEXT("the outer face is the setback inside the house's face"), FMath::IsNearlyEqual(C[0].Y, S, 1.0e-6) && FMath::IsNearlyEqual(C[1].Y, S, 1.0e-6));
		TestTrue(TEXT("the body is on the house's side"), FMath::IsNearlyEqual(C[2].Y, S + T, 1.0e-6));
	}
	TestTrue(TEXT("run the other way with a setback"), SideAlongFace(FVector2D(-1, 0), 0.0, -1000.0, FVector2D(0, 1), T, DrawnY, AlongFaceDeg, S));
	TestTrue(TEXT("the drawn line is the setback outside the y = T face"), FMath::IsNearlyEqual(DrawnY, T + S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallRunVertexTest,
	"HutongLayout.Walls.RunVertexMoves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallRunVertexTest::RunTest(const FString& Parameters)
{
	// Tool-placed run reads back as a run; moving the join rebuilds both legs melded; moving an outer end onto a face cuts it flush.
	using namespace HutongWallChain;
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;
	const double T = 37.0;

	TArray<FVector2D> Points = { FVector2D(0, 0), FVector2D(800, 0), FVector2D(800, 600) };
	TArray<FSegment> Segments;
	Build(Points, T, 0.5 * T, FEndFace(), FEndFace(), Segments);
	TArray<UHutongWallBuildingComponent*> Legs;
	for (const FSegment& S : Segments)
	{
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform(FRotator(0.0, S.YawDeg, 0.0), FVector(S.Origin.X, S.Origin.Y, 0.0)), TEXT("Run"));
		UHutongWallBuildingComponent* Leg = NewObject<UHutongWallBuildingComponent>(Actor);
		Leg->bPlanOnly = true;
		Leg->Length = S.Length;
		Leg->FootprintSkew = S.Skew;
		Actor->AddInstanceComponent(Leg);
		Leg->RegisterComponent();
		Legs.Add(Leg);
	}
	if (!TestEqual(TEXT("two legs placed"), Legs.Num(), 2)) { World->DestroyWorld(false); return false; }

	HutongWallRun::FRun Run;
	TestTrue(TEXT("the run gathers from its second leg"), HutongWallRun::Gather(Legs[1], Legs, Run));
	TestEqual(TEXT("both legs are in it, in order"), Run.NumLegs(), 2);
	TestTrue(TEXT("the first leg leads"), Run.Legs.Num() == 2 && Run.Legs[0].Get() == Legs[0]);
	TestTrue(TEXT("the vertices are the drawn points"), Run.Vertices.Num() == 3
		&& Run.Vertices[0].Equals(Points[0], 0.01) && Run.Vertices[1].Equals(Points[1], 0.01) && Run.Vertices[2].Equals(Points[2], 0.01));
	TestFalse(TEXT("the outer ends are square"), Run.StartFace.bSet || Run.EndFace.bSet);

	// Join moved: both legs turn and still meet on one line.
	auto WorldCorners = [](const UHutongWallBuildingComponent* Leg, FVector2D Out[4])
	{
		FVector2D Local[4];
		Leg->GetFootprintCorners(Local);
		const FTransform Xf = Leg->GetOwner()->GetActorTransform();
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector W = Xf.TransformPosition(FVector(Local[i].X, Local[i].Y, 0.0));
			Out[i] = FVector2D(W.X, W.Y);
		}
	};
	TArray<FVector2D> Moved = Run.Vertices;
	Moved[1] = FVector2D(900, 150);
	TArray<FSegment> Rebuilt;
	TestTrue(TEXT("the run rebuilds for the moved join"), HutongWallRun::Rebuild(Run, Moved, Run.StartFace, Run.EndFace, Rebuilt));
	HutongWallRun::Apply(Run, Rebuilt);
	FVector2D A[4], B[4];
	WorldCorners(Legs[0], A);
	WorldCorners(Legs[1], B);
	TestTrue(TEXT("the legs still meld on the outer face"), FVector2D::Distance(A[1], B[0]) < 1.0e-3);
	TestTrue(TEXT("and on the inner face"), FVector2D::Distance(A[2], B[3]) < 1.0e-3);
	FVector2D S0, E0, S1, E1;
	HutongWallRun::LegEnds(Legs[0], S0, E0);
	HutongWallRun::LegEnds(Legs[1], S1, E1);
	TestTrue(TEXT("the join is where it was put"), E0.Equals(Moved[1], 1.0e-3) && S1.Equals(Moved[1], 1.0e-3));
	TestTrue(TEXT("the far ends stayed"), S0.Equals(Points[0], 1.0e-3) && E1.Equals(Points[2], 1.0e-3));

	// Far end onto a 45° face: cut flush, read back as resting on it.
	FEndFace Face;
	Face.bSet = true; Face.Point = FVector2D(800, 700); Face.Dir = FVector2D(1, 1).GetSafeNormal();
	Moved[2] = Face.Point;
	TestTrue(TEXT("the run rebuilds for the flush end"), HutongWallRun::Rebuild(Run, Moved, Run.StartFace, Face, Rebuilt));
	HutongWallRun::Apply(Run, Rebuilt);
	WorldCorners(Legs[1], B);
	TestTrue(TEXT("both end corners are on the face"), LineDistance(B[1], Face.Point, Face.Dir) < 1.0e-3 && LineDistance(B[2], Face.Point, Face.Dir) < 1.0e-3);
	const FEndFace ReadBack = HutongWallRun::OuterFace(Legs[1], false);
	TestTrue(TEXT("the face is read back off the corners"), ReadBack.bSet && LineDistance(Face.Point, ReadBack.Point, ReadBack.Dir) < 1.0e-3);

	World->DestroyWorld(false);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallChainCornerTest,
	"HutongLayout.Walls.CornerEnds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallChainCornerTest::RunTest(const FString& Parameters)
{
	// Two houses on a street, the run across the gap from a front corner of one to a front corner of
	// the other, a little off the line: at each end the run's outside corner is the house's corner and
	// its short face lies along the house's gable.
	using namespace HutongWallChain;
	const double T = 37.0;
	// House A: x in [-600, 0], y in [0, 400], front on y = 0; the run leaves its corner (0, 0).
	// House B: x in [700, 1300], y in [-100, 300], set 1 m forward; the run arrives at its corner (700, -100).
	const FVector2D A(0.0, 0.0), B(700.0, -100.0);
	const FVector2D InA = (FVector2D(-300.0, 200.0)).GetSafeNormal(), InB = (FVector2D(300.0, 200.0)).GetSafeNormal();

	double DrawnY = 0.0, EndY = 0.0;
	FEndFace Start, End;
	CornerEnd(B - A, true, 180.0, 90.0, InA, A, T, DrawnY, Start);
	CornerEnd(B - A, false, 0.0, 90.0, InB, B, T, EndY, End);
	TestEqual(TEXT("both ends agree on the side"), DrawnY, EndY);
	TestTrue(TEXT("the start rests on house A's gable (x = 0), not its front"), FMath::Abs(Start.Dir.X) < 1.0e-9);
	TestEqual(TEXT("the body stands behind the street face"), DrawnY, 0.0);

	TArray<FSegment> Segments;
	if (!TestTrue(TEXT("the run builds"), Build({ A, B }, T, DrawnY, Start, End, Segments) && Segments.Num() == 1)) return false;
	FVector2D C[4];
	Corners(Segments[0], T, C);
	auto Nearest = [&](const FVector2D& P) { double D = TNumericLimits<double>::Max(); for (const FVector2D& Q : C) D = FMath::Min(D, FVector2D::Distance(P, Q)); return D; };
	TestTrue(FString::Printf(TEXT("the outside corner is house A's corner (%.4f cm off)"), Nearest(A)), Nearest(A) < 1.0e-6);
	TestTrue(FString::Printf(TEXT("the outside corner is house B's corner (%.4f cm off)"), Nearest(B)), Nearest(B) < 1.0e-6);
	// The other corner at each end lies on the house's face, inside its edge.
	int32 OnA = 0, OnB = 0;
	for (const FVector2D& Q : C)
	{
		if (FMath::Abs(Q.X) < 1.0e-6 && Q.Y > 1.0 && Q.Y < 400.0) ++OnA;
		if (FMath::Abs(Q.X - 700.0) < 1.0e-6 && Q.Y > -99.0 && Q.Y < 300.0) ++OnB;
	}
	TestEqual(TEXT("the start's short face lies along house A"), OnA, 1);
	TestEqual(TEXT("the end's short face lies along house B"), OnB, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallChainFaceEndTest,
	"HutongLayout.Walls.FaceEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallChainFaceEndTest::RunTest(const FString& Parameters)
{
	// An end snapped mid-face, at a slant: the short face lies on the face's line, the line centred.
	using namespace HutongWallChain;
	const double T = 37.0;
	const FVector2D A(0.0, 0.0), B(600.0, 300.0);
	FEndFace End;
	End.bSet = true;
	End.Point = B;
	End.Dir = FVector2D(1.0, 0.0);
	TArray<FSegment> Segments;
	if (!TestTrue(TEXT("the run builds"), Build({ A, B }, T, 0.5 * T, FEndFace(), End, Segments) && Segments.Num() == 1)) return false;
	FVector2D C[4];
	Corners(Segments[0], T, C);
	int32 OnFace = 0;
	for (const FVector2D& Q : C) if (FMath::Abs(Q.Y - 300.0) < 1.0e-6) ++OnFace;
	TestEqual(TEXT("both end corners lie on the face"), OnFace, 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
