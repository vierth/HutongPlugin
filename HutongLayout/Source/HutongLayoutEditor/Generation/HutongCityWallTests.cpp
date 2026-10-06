#include "Generation/CityWallGenerator.h"
#include "Generation/HutongBuildingComponent.h"
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

#endif
