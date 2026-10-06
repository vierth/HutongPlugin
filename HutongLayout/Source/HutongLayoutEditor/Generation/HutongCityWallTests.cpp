#include "Generation/CityWallGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/HutongSnap.h"
#include "UObject/UnrealType.h"
#include "Generation/HutongMeshInspect.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAABBTree3;

	FHutongCityWallParams CityWallTestParams()
	{
		FHutongCityWallParams P;
		P.Length = 12000.0;
		return P;
	}

	// First hit along the ray, or a large number.
	double CityWallHit(const FDynamicMeshAABBTree3& Tree, const FVector3d& From, const FVector3d& Dir)
	{
		double T = 0.0;
		int32 Tid = -1;
		return Tree.FindNearestHitTriangle(FRay3d(From, Dir), T, Tid) ? T : 1.0e9;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallSolidTest,
	"HutongLayout.CityWall.Solid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallSolidTest::RunTest(const FString& Parameters)
{
	// Watertight, whatever is turned on, both cities, every level: no open edge. Touching pieces (merlon and
	// crenel, incline and landing) repeat each other's edges, so duplicates are expected.
	for (const EHutongCityWallRank Rank : { EHutongCityWallRank::Inner, EHutongCityWallRank::Outer })
	{
		for (int32 Case = 0; Case < 4; ++Case)
		{
			for (const bool bBattlements : { false, true })
			{
				FHutongCityWallParams P = CityWallTestParams();
				P.Rank = Rank;
				P.bBastions = Case >= 1;
				P.bRamp = Case >= 2;
				P.bRampRisesTowardStart = Case == 3;
				FDynamicMesh3 Mesh;
				HutongGen::BuildCityWall(Mesh, P, bBattlements, bBattlements);
				const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);
				TestTrue(FString::Printf(TEXT("rank %d, case %d, battlements %d: closed (%d unmatched, volume %.0f)"),
					(int32)Rank, Case, bBattlements ? 1 : 0, R.Unmatched, R.Volume), R.Unmatched == 0 && R.Volume > 0.0);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallRampTest,
	"HutongLayout.CityWall.Ramp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallRampTest::RunTest(const FString& Parameters)
{
	// In through the gateway, up the ramp without a step, off the landing through the 宇牆 onto the walk.
	for (const bool bTowardStart : { false, true })
	{
		FHutongCityWallParams P = CityWallTestParams();
		P.bRamp = true;
		P.bRampRisesTowardStart = bTowardStart;
		P.RampPosition = bTowardStart ? 0.2 : 0.8;
		FDynamicMesh3 Mesh;
		HutongGen::BuildCityWall(Mesh, P, true, true);
		FDynamicMeshAABBTree3 Tree(&Mesh);

		const double H = P.GetHeight(), B = P.GetBaseWidth(), Bat = P.GetBatter();
		const double Yc = B + 0.5 * P.RampWidth;
		const double D = bTowardStart ? -1.0 : 1.0;
		// The landing's far end, from the clamp in the generator.
		const double Land = FMath::Max(P.RampWidth, 300.0);
		const double Run = P.GetRampRun();
		const double Total = HutongCanon::CityWall::RampGateThicknessCm + Run + Land;
		const double Margin = HutongCanon::CityWall::RampParapetThicknessCm + 20.0;
		const double XT = FMath::Clamp(P.RampPosition * P.Length, D > 0.0 ? Total + Margin : Margin, D > 0.0 ? P.Length - Margin : P.Length - Total - Margin);
		const double XFoot = XT - D * (Land + Run);

		// Up the middle of the ramp: the surface climbs steadily, never steeper than asked, to the walk.
		const double MaxRise = FMath::Tan(FMath::DegreesToRadians(P.RampSlopeDeg + 1.0)) * 20.0 + 0.5;
		double Prev = -1.0;
		bool bSteady = true;
		// Short of the parapet across the landing's end.
		for (double S = 10.0; S < Run + Land - HutongCanon::CityWall::RampParapetThicknessCm - 10.0; S += 20.0)
		{
			const double Z = H + 500.0 - CityWallHit(Tree, FVector3d(XFoot + D * S, Yc, H + 500.0), FVector3d(0, 0, -1));
			if (Prev >= 0.0 && (Z < Prev - 0.5 || Z - Prev > MaxRise)) bSteady = false;
			Prev = Z;
		}
		TestTrue(TEXT("the ramp climbs steadily"), bSteady);
		TestTrue(FString::Printf(TEXT("the landing is at the walk (%.1f of %.1f)"), Prev, H), FMath::Abs(Prev - H) <= 1.5);

		// Through the 馬道門 at walking height.
		const double GateOut = XFoot - D * (HutongCanon::CityWall::RampGateThicknessCm + 100.0);
		TestTrue(TEXT("the gateway lets a walker onto the ramp"),
			CityWallHit(Tree, FVector3d(GateOut, Yc, 100.0), FVector3d(D, 0, 0)) > 200.0);

		// Off the landing toward the wall, chest high: clear past the 宇牆 line.
		const double XLandMid = XT - D * 0.5 * Land;
		const double ToWall = Yc - (B - Bat - P.GetInnerParapetThickness() - 50.0);
		TestTrue(TEXT("the 宇牆 is open at the landing"),
			CityWallHit(Tree, FVector3d(XLandMid, Yc, H + 120.0), FVector3d(0, -1, 0)) > ToWall);
		// And shut elsewhere along the walk.
		TestTrue(TEXT("the 宇牆 stands away from the ramp"),
			CityWallHit(Tree, FVector3d(XT - D * (Land + Run + 1000.0), B + 50.0, H + 50.0), FVector3d(0, -1, 0)) < 50.0 + Bat + 10.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallBastionTest,
	"HutongLayout.CityWall.Bastions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallBastionTest::RunTest(const FString& Parameters)
{
	FHutongCityWallParams P = CityWallTestParams();
	P.bBastions = true;
	FDynamicMesh3 Mesh;
	HutongGen::BuildCityWall(Mesh, P, true, true);
	FDynamicMeshAABBTree3 Tree(&Mesh);
	const double H = P.GetHeight(), Bat = P.GetBatter();

	// One bastion at the middle of a 120 m leg at 90 m spacing: from outside, its face stands proud by the
	// projection, and its top is walk under the parapet carried round it.
	const double Mid = 0.5 * P.Length;
	const double Z = 0.5 * H;
	const double Face = -2000.0 + CityWallHit(Tree, FVector3d(Mid, -2000.0, Z), FVector3d(0, 1, 0));
	TestTrue(FString::Printf(TEXT("bastion face at %.0f"), Face), FMath::IsNearlyEqual(Face, -P.GetBastionProjection() + Bat * Z / H, 2.0));
	const double Plain = -2000.0 + CityWallHit(Tree, FVector3d(P.Length - 300.0, -2000.0, Z), FVector3d(0, 1, 0));
	TestTrue(FString::Printf(TEXT("no bastion near the end (%.0f)"), Plain), FMath::IsNearlyEqual(Plain, Bat * Z / H, 2.0));
	const double TopZ = H + 1000.0 - CityWallHit(Tree, FVector3d(Mid, -0.5 * P.GetBastionProjection(), H + 1000.0), FVector3d(0, 0, -1));
	TestTrue(FString::Printf(TEXT("bastion top is the walk (%.1f)"), TopZ), FMath::IsNearlyEqual(TopZ, H, 1.0));
	// The main parapet does not cross the bastion's mouth.
	TestTrue(TEXT("the walk runs out onto the bastion"),
		CityWallHit(Tree, FVector3d(Mid, Bat + 300.0, H + 60.0), FVector3d(0, -1, 0)) > 300.0 + P.GetBastionProjection() - 2.0 * Bat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallRankTest,
	"HutongLayout.CityWall.Rank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallRankTest::RunTest(const FString& Parameters)
{
	FHutongCityWallParams P;
	P.Rank = EHutongCityWallRank::Outer;
	TestEqual(TEXT("outer city height"), P.GetHeight(), HutongCanon::CityWall::OuterHeightCm);
	TestEqual(TEXT("outer city base"), P.GetBaseWidth(), HutongCanon::CityWall::OuterBaseWidthCm);
	TestTrue(TEXT("outer city lower than inner"), P.GetHeight() < FHutongCityWallParams().GetHeight());
	// Pinned, a hand edit holds and the rest keep the rank's figures.
	P.PinFromRank();
	P.Height = 700.0;
	TestEqual(TEXT("pinned height"), P.GetHeight(), 700.0);
	TestEqual(TEXT("pinned base kept"), P.GetBaseWidth(), HutongCanon::CityWall::OuterBaseWidthCm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallRampSideTest,
	"HutongLayout.CityWall.RampDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallRampSideTest::RunTest(const FString& Parameters)
{
	// The ramp rises the way its leg says whichever side the battlements are on (the mesh's half turn once
	// reversed it), and stands on the side away from them.
	for (const bool bTowardStart : { false, true })
	{
		FHutongCityWallParams P;
		P.bRamp = true;
		P.bRampRisesTowardStart = bTowardStart;
		P.RampPosition = bTowardStart ? 0.15 : 0.85;
		const double L = 12000.0, B = P.GetBaseWidth();
		for (const EHutongBaySide Outer : { EHutongBaySide::MinusY, EHutongBaySide::PlusY })
		{
			FVector2D C[4], From, To, Gap[2];
			if (!TestTrue(TEXT("a ramp"), UHutongCityWallBuildingComponent::GetRampOutline(P, L, Outer, C, From, To, Gap))) continue;
			const bool bUpTowardEnd = To.X > From.X;
			TestEqual(FString::Printf(TEXT("outer %d, toward start %d: rises the leg's way"), (int32)Outer, bTowardStart ? 1 : 0),
				bUpTowardEnd, !bTowardStart);
			const bool bInnerOnPlusY = From.Y > 0.5 * B;
			TestEqual(FString::Printf(TEXT("outer %d: ramp on the inner side"), (int32)Outer), bInnerOnPlusY, Outer == EHutongBaySide::MinusY);
			// The top (the landing, a ramp's width short of it) lands near where it was asked along the leg.
			const double TopX = To.X;
			TestTrue(FString::Printf(TEXT("outer %d: top near %.0f%% (%.0f)"), (int32)Outer, 100.0 * P.RampPosition, TopX),
				FMath::Abs(TopX - P.RampPosition * L) < 1000.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallRampMarkTest,
	"HutongLayout.CityWall.RampMark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallRampMarkTest::RunTest(const FString& Parameters)
{
	// The laid-out plan marks a ramp: one square inside the footprint on the inner side, its arrow the way
	// the ramp rises; none without a ramp.
	UHutongCityWallBuildingComponent* Wall = NewObject<UHutongCityWallBuildingComponent>(GetTransientPackage());
	Wall->Length = 12000.0;
	TArray<FVector4> Marks;
	Wall->GetPlanArrows(Marks);
	TestEqual(TEXT("no ramp, no mark"), Marks.Num(), 0);
	Wall->Params.bRamp = true;
	const double B = Wall->Params.GetBaseWidth();
	for (const EHutongBaySide Outer : { EHutongBaySide::MinusY, EHutongBaySide::PlusY })
	{
		for (const bool bTowardStart : { false, true })
		{
			Wall->OuterSide = Outer;
			Wall->Params.bRampRisesTowardStart = bTowardStart;
			Wall->Params.RampPosition = bTowardStart ? 0.15 : 0.85;
			Marks.Reset();
			Wall->GetPlanArrows(Marks);
			if (!TestEqual(TEXT("one mark"), Marks.Num(), 1)) continue;
			const FVector4 M = Marks[0];
			const FString Case = FString::Printf(TEXT("outer %d, toward start %d"), (int32)Outer, bTowardStart ? 1 : 0);
			TestTrue(Case + TEXT(": inside the footprint"), M.X > 0.0 && M.X < Wall->Length && M.Y > 0.0 && M.Y < B);
			TestEqual(Case + TEXT(": on the inner side"), M.Y > 0.5 * B, Outer == EHutongBaySide::MinusY);
			TestEqual(Case + TEXT(": points the way it rises"), M.Z > 0.0, !bTowardStart);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallShortLegTest,
	"HutongLayout.CityWall.ShortLegMitre",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallShortLegTest::RunTest(const FString& Parameters)
{
	// A leg shorter than the wall is wide (12 m against 20 m) still runs along X: its mitre lengthens the
	// outer face and shortens the inner, and only the end zone moves. Read off the proportions, the run was
	// taken along Y and the battlements were pushed sideways along the whole leg.
	auto Build = [](bool bSkew)
	{
		UHutongCityWallBuildingComponent* Wall = NewObject<UHutongCityWallBuildingComponent>(GetTransientPackage());
		Wall->Length = 1200.0;
		Wall->Params.bBastions = false;
		Wall->bBuildLODChain = false;
		if (bSkew)
		{
			Wall->FootprintSkew.Corner10 = FVector2D(300.0, 0.0);
			Wall->FootprintSkew.Corner11 = FVector2D(-300.0, 0.0);
		}
		TArray<FDynamicMesh3> LODs;
		Wall->BuildLODs(LODs);
		return LODs.Num() > 0 ? LODs[0] : FDynamicMesh3();
	};
	const FDynamicMesh3 Square = Build(false), Mitred = Build(true);
	const double B = FHutongCityWallParams().GetBaseWidth();

	double OuterEnd = -1.0e9, InnerEnd = -1.0e9;
	for (const int32 V : Mitred.VertexIndicesItr())
	{
		const FVector3d P = Mitred.GetVertex(V);
		if (P.Z > 1.0) continue;
		if (P.Y < 1.0) OuterEnd = FMath::Max(OuterEnd, P.X);
		if (P.Y > B - 1.0) InnerEnd = FMath::Max(InnerEnd, P.X);
	}
	TestTrue(FString::Printf(TEXT("outer face runs to the mitre (%.0f)"), OuterEnd), FMath::IsNearlyEqual(OuterEnd, 1500.0, 1.0));
	TestTrue(FString::Printf(TEXT("inner face stops short (%.0f)"), InnerEnd), FMath::IsNearlyEqual(InnerEnd, 900.0, 1.0));

	// Away from the mitred end (the zone is at most half the leg), every point is where the square leg has it.
	TSet<FIntVector> Built;
	auto Key = [](const FVector3d& P) { return FIntVector(FMath::RoundToInt32(P.X * 10.0), FMath::RoundToInt32(P.Y * 10.0), FMath::RoundToInt32(P.Z * 10.0)); };
	for (const int32 V : Square.VertexIndicesItr()) Built.Add(Key(Square.GetVertex(V)));
	int32 Moved = 0, Checked = 0;
	for (const int32 V : Mitred.VertexIndicesItr())
	{
		const FVector3d P = Mitred.GetVertex(V);
		if (P.X > 500.0) continue;
		++Checked;
		if (!Built.Contains(Key(P))) ++Moved;
	}
	TestTrue(FString::Printf(TEXT("battlements checked (%d)"), Checked), Checked > 50);
	TestEqual(TEXT("nothing moves away from the mitred end"), Moved, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallStepBearingTest,
	"HutongLayout.Snap.StepBearing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallStepBearingTest::RunTest(const FString& Parameters)
{
	// The 15° step the run tools apply on hover and again on the click: nearest step off the base bearing,
	// same distance, height kept.
	const FVector Start(100.0, 200.0, 50.0);
	const double R = FMath::DegreesToRadians(37.0);
	const FVector Cursor = Start + FVector(FMath::Cos(R), FMath::Sin(R), 0.0) * 1000.0 + FVector(0, 0, 7.0);
	auto Bearing = [&](const FVector& P) { return FMath::RadiansToDegrees(FMath::Atan2(P.Y - Start.Y, P.X - Start.X)); };
	const FVector A = HutongSnap::StepBearing(Start, Cursor, 0.0, 15.0);
	TestTrue(FString::Printf(TEXT("37° steps to 30° (%.2f)"), Bearing(A)), FMath::IsNearlyEqual(Bearing(A), 30.0, 1.0e-6));
	TestTrue(TEXT("same distance"), FMath::IsNearlyEqual(FVector::Dist2D(Start, A), 1000.0, 1.0e-6));
	TestEqual(TEXT("height kept"), A.Z, Cursor.Z);
	const FVector B = HutongSnap::StepBearing(Start, Cursor, 10.0, 15.0);
	TestTrue(FString::Printf(TEXT("off a 10° frame, 40° (%.2f)"), Bearing(B)), FMath::IsNearlyEqual(Bearing(B), 40.0, 1.0e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallChooseRankTest,
	"HutongLayout.CityWall.ChooseRank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallChooseRankTest::RunTest(const FString& Parameters)
{
	// A wall whose height was set by hand still takes the outer city's figures when City is changed in Details.
	UHutongCityWallBuildingComponent* Wall = NewObject<UHutongCityWallBuildingComponent>(GetTransientPackage());
	Wall->SetEditHeight(1300.0);
	TestEqual(TEXT("hand height held"), Wall->Params.GetHeight(), 1300.0);
	Wall->Params.Rank = EHutongCityWallRank::Outer;
	FProperty* RankProp = FindFProperty<FProperty>(FHutongCityWallParams::StaticStruct(), GET_MEMBER_NAME_CHECKED(FHutongCityWallParams, Rank));
	if (!TestNotNull(TEXT("the City field"), RankProp)) return false;
	FPropertyChangedEvent Event(RankProp, EPropertyChangeType::ValueSet);
	Wall->PostEditChangeProperty(Event);
	TestEqual(TEXT("outer city height"), Wall->Params.GetHeight(), HutongCanon::CityWall::OuterHeightCm);
	TestEqual(TEXT("outer city base"), Wall->Params.GetBaseWidth(), HutongCanon::CityWall::OuterBaseWidthCm);
	return true;
}

#endif
