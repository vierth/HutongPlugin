#include "Generation/CityWallGenerator.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongMeshInspect.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongJiajia.h"
#include "Tools/HutongWallChain.h"
#include "Generation/GateComplexLayout.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using UE::Geometry::FDynamicMesh3;

	// Per slot: triangle count, and how many face +Y and -Y (pre-bake normals point inward).
	struct FPalaceSlotFaces { int32 Count = 0, PlusY = 0, MinusY = 0; };

	TMap<int32, FPalaceSlotFaces> PalaceSlotFaces(const FDynamicMesh3& Mesh)
	{
		TMap<int32, FPalaceSlotFaces> Out;
		const UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T : Mesh.TriangleIndicesItr())
		{
			FPalaceSlotFaces& F = Out.FindOrAdd(MatIDs->GetValue(T));
			++F.Count;
			const double Ny = -Mesh.GetTriNormal(T).Y;
			if (Ny > 0.5) ++F.PlusY;
			if (Ny < -0.5) ++F.MinusY;
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPalaceWallTest,
	"HutongLayout.CityWall.Palace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPalaceWallTest::RunTest(const FString& Parameters)
{
	FHutongCityWallParams P;
	P.ChooseRank(EHutongCityWallRank::Palace);
	P.bBastions = false;
	P.bRamp = true;
	P.Length = 6000.0;
	TestEqual(TEXT("walk at 7.9 m"), P.GetHeight(), 790.0);
	TestEqual(TEXT("8.62 m at the foot"), P.GetBaseWidth(), 862.0);
	TestEqual(TEXT("6.66 m at the top"), P.GetTopWidth(), 666.0);

	for (const EHutongDetail Level : { EHutongDetail::Massing, EHutongDetail::Near })
	{
		FDynamicMesh3 Mesh;
		UHutongCityWallBuildingComponent::BuildCityWallMesh(P, P.Length, EHutongBaySide::MinusY, Mesh, Level);
		const TMap<int32, FPalaceSlotFaces> Faces = PalaceSlotFaces(Mesh);
		const FPalaceSlotFaces Red = Faces.FindRef(HutongGen::MatSlot_RedWall);
		const FPalaceSlotFaces Brick = Faces.FindRef(HutongGen::MatSlot_CityBrick);
		TestTrue(FString::Printf(TEXT("level %d: red faces the palace (%d tris)"), (int32)Level, Red.Count), Red.Count > 0 && Red.PlusY == Red.Count);
		TestTrue(FString::Printf(TEXT("level %d: grey brick on the outer face"), (int32)Level), Brick.MinusY > 0);

		// The half turn puts the palace side on -Y.
		FDynamicMesh3 Turned;
		UHutongCityWallBuildingComponent::BuildCityWallMesh(P, P.Length, EHutongBaySide::PlusY, Turned, Level);
		const FPalaceSlotFaces TurnedRed = PalaceSlotFaces(Turned).FindRef(HutongGen::MatSlot_RedWall);
		TestTrue(FString::Printf(TEXT("level %d: turned, red faces -Y"), (int32)Level), TurnedRed.Count == Red.Count && TurnedRed.MinusY == Red.Count);
	}

	// Each 垛 an upside-down T: the top step half the merlon, a step under the top, the crenel two steps down.
	{
		FDynamicMesh3 Mesh;
		HutongGen::BuildCityWall(Mesh, P, true, false);
		const double Top = P.GetHeight() + P.GetParapetHeight();
		TestEqual(TEXT("top step half the merlon"), P.GetMerlonTopWidth(), 0.5 * P.GetMerlonWidth());
		TestEqual(TEXT("crenel two steps deep"), P.GetCrenelDepth(), 2.0 * P.GetMerlonTopHeight());
		TArray<double> TopXs;
		for (int32 V : Mesh.VertexIndicesItr())
		{
			const FVector3d X = Mesh.GetVertex(V);
			if (FMath::IsNearlyEqual(X.Z, Top, 0.01) && X.Y < 0.5 * P.GetBaseWidth()) TopXs.AddUnique(FMath::RoundToDouble(X.X));
		}
		TopXs.Sort();
		const double FirstTop = TopXs.Num() >= 2 ? TopXs[1] - TopXs[0] : 0.0;
		TestTrue(FString::Printf(TEXT("the parapet top is the narrow step (%.0f wide)"), FirstTop),
			FMath::Abs(FirstTop - P.GetMerlonTopWidth()) < 15.0);
		const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);
		TestTrue(FString::Printf(TEXT("closed (%d unmatched)"), R.Unmatched), R.Unmatched == 0 && R.Volume > 0.0);
	}

	FHutongCityWallParams Inner;
	FDynamicMesh3 InnerMesh;
	HutongGen::BuildCityWall(InnerMesh, Inner, true, true);
	TestFalse(TEXT("the inner city wall carries no red"), PalaceSlotFaces(InnerMesh).Contains(HutongGen::MatSlot_RedWall));

	UHutongCityWallBuildingComponent* C = NewObject<UHutongCityWallBuildingComponent>();
	C->Params = P;
	TestEqual(TEXT("labelled as the palace wall"), C->GetTypeLabel().ToString(), FString(TEXT("Palace Wall (紫禁城城牆)")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongImperialWallTest,
	"HutongLayout.Walls.ImperialCity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongImperialWallTest::RunTest(const FString& Parameters)
{
	using namespace HutongCanon::Wall;
	FHutongWallParams P;
	P.Role = EHutongWallRole::ImperialCity;
	P.bHasWindows = false;
	TestEqual(TEXT("2 m at the foot"), P.GetThickness(), ImperialThicknessCm);
	TestEqual(TEXT("1.7 m at the top"), P.GetTopThickness(), ImperialTopThicknessCm);
	TestEqual(TEXT("no base course"), P.GetBaseCourseHeight(), 0.0);
	TestEqual(TEXT("no windows"), P.GetWindowCount(2000.0), 0);

	FDynamicMesh3 Mesh;
	UHutongWallBuildingComponent::BuildWallMesh(P, 2000.0, P.GetThickness(), false, Mesh, EHutongDetail::Near);
	const UE::Geometry::FAxisAlignedBox3d Box = Mesh.GetBounds();
	TestTrue(FString::Printf(TEXT("about 6 m overall (%.0f)"), Box.Max.Z), FMath::Abs(Box.Max.Z - ImperialOverallHeightCm) < 15.0);

	const TMap<int32, FPalaceSlotFaces> Faces = PalaceSlotFaces(Mesh);
	const FPalaceSlotFaces Red = Faces.FindRef(HutongGen::MatSlot_RedWall);
	TestTrue(TEXT("red plaster on both faces"), Red.PlusY > 0 && Red.MinusY > 0);
	TestTrue(TEXT("a glazed cap"), Faces.FindRef(HutongGen::MatSlot_GlazedTile).Count > 0);
	TestFalse(TEXT("no plain roof tile left"), Faces.Contains(HutongGen::MatSlot_Roof));
	TestTrue(TEXT("grey brick cornice"), Faces.FindRef(HutongGen::MatSlot_Body).Count > 0);

	// Battered: the red faces meet the cornice a batter in from the foot.
	const UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
	double TopMinY = BIG_NUMBER, TopMaxY = -BIG_NUMBER, FootMinY = BIG_NUMBER, FootMaxY = -BIG_NUMBER;
	for (int32 T : Mesh.TriangleIndicesItr())
	{
		if (MatIDs->GetValue(T) != HutongGen::MatSlot_RedWall) continue;
		const UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(T);
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector3d V = Mesh.GetVertex(Tri[k]);
			if (V.Z < 1.0) { FootMinY = FMath::Min(FootMinY, V.Y); FootMaxY = FMath::Max(FootMaxY, V.Y); }
			if (V.Z > P.GetHeight() - 1.0) { TopMinY = FMath::Min(TopMinY, V.Y); TopMaxY = FMath::Max(TopMaxY, V.Y); }
		}
	}
	TestTrue(FString::Printf(TEXT("foot 200 wide (%.1f)"), FootMaxY - FootMinY), FMath::Abs(FootMaxY - FootMinY - ImperialThicknessCm) < 0.5);
	TestTrue(FString::Printf(TEXT("top 170 wide (%.1f)"), TopMaxY - TopMinY), FMath::Abs(TopMaxY - TopMinY - ImperialTopThicknessCm) < 0.5);

	UHutongWallBuildingComponent* C = NewObject<UHutongWallBuildingComponent>();
	C->Params = P;
	TestEqual(TEXT("labelled as the imperial city wall"), C->GetTypeLabel().ToString(), FString(TEXT("Imperial City Wall (皇城牆)")));
	return true;
}


// A run carried on from a placed leg's free end: the leg's join end and the new leg's are cut as one run drawn in one go.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityWallCarryOnTest,
	"HutongLayout.CityWall.CarryOn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityWallCarryOnTest::RunTest(const FString& Parameters)
{
	using namespace HutongWallChain;
	const double B = 862.0;
	const FVector2D A(0.0, 0.0), J(4000.0, 0.0), C(6000.0, 2500.0);
	auto Run = [&](const TArray<FVector2D>& Points)
	{
		TArray<FSegment> Out;
		Build(Points, B, 0.5 * B, {}, {}, Out);
		return Out;
	};
	auto Same = [](const FHutongFootprintSkew& X, const FHutongFootprintSkew& Y)
	{
		for (int32 k = 0; k < 4; ++k) if (!X.Get(k).Equals(Y.Get(k), 0.01)) return false;
		return true;
	};
	const TArray<FSegment> Whole = Run({ A, J, C });
	if (!TestEqual(TEXT("two legs"), Whole.Num(), 2)) return true;

	// The leg A→J placed alone, then carried on toward C (join at its end).
	const TArray<FSegment> Leg = Run({ A, J });
	const FHutongFootprintSkew AtEnd = CarryJoinSkew(Leg[0].Skew, false, Whole[0].Skew);
	TestTrue(TEXT("join at the leg's end: cut as the whole run's"), Same(AtEnd, Whole[0].Skew));

	// The leg J→A placed alone (join at its start): the whole run drawn C→J→A gives its cut directly.
	const TArray<FSegment> Back = Run({ J, A });
	const FHutongFootprintSkew AtStart = CarryJoinSkew(Back[0].Skew, true, Whole[0].Skew);
	TestTrue(TEXT("join at the leg's start: cut as the whole run's"), Same(AtStart, Run({ C, J, A }).Last().Skew));

	// A run ending on the leg: built J→A last, read from A.
	TestTrue(TEXT("reversed reads from the other end"), Same(Reversed(Run({ C, J, A }).Last().Skew), Whole[0].Skew));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallCornerTest,
	"HutongLayout.CityWall.Corner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallCornerTest::RunTest(const FString& Parameters)
{
	for (int32 Kind = 0; Kind < 6; ++Kind)
	{
		const EHutongCityWallRank Rank = Kind == 5 ? EHutongCityWallRank::Palace : Kind >= 3 ? EHutongCityWallRank::Inner : (EHutongCityWallRank)Kind;
		UHutongWallCornerBuildingComponent* C = NewObject<UHutongWallCornerBuildingComponent>();
		C->Params.ChooseRank(Rank);
		C->Shape = Kind == 3 ? EHutongWallCornerShape::Rounded : EHutongWallCornerShape::Platform;
		C->Tower = Kind == 4 ? EHutongCornerTower::ArrowTower : Kind == 5 ? EHutongCornerTower::PalaceTower : EHutongCornerTower::None;
		const double S = C->GetPlatformSize(), B = C->Params.GetBaseWidth();
		TestTrue(FString::Printf(TEXT("kind %d: the corner is wider than the wall"), Kind), S > B);
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			C->OuterCorner = Corner;
			const FString Case = FString::Printf(TEXT("kind %d, corner %d"), Kind, Corner);
			for (const EHutongDetail Level : { EHutongDetail::Massing, EHutongDetail::Near })
			{
				FDynamicMesh3 Mesh;
				UHutongWallCornerBuildingComponent::BuildCornerMesh(C->Params, C->Shape, S, Corner, Mesh, Level, C->Tower);
				const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);
				TestTrue(FString::Printf(TEXT("%s, level %d: closed (%d unmatched, volume %.0f)"), *Case, (int32)Level, R.Unmatched, R.Volume),
					R.Unmatched == 0 && R.Volume > 0.0);
				const UE::Geometry::FAxisAlignedBox3d Box = Mesh.GetBounds();
				// A rounded corner's mitres stand a little past the arc they are cut on.
				const double Slack = C->Shape == EHutongWallCornerShape::Rounded ? 0.01 * S : 0.5;
				TestTrue(*FString::Printf(TEXT("%s: inside the footprint (%.0f %.0f .. %.0f %.0f of %.0f)"), *Case, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, S),
					Box.Min.X > -Slack && Box.Min.Y > -Slack && Box.Max.X < S + Slack && Box.Max.Y < S + Slack);

				// Red only on what faces away from the outside corner.
				const FVector3d Outer(C->GetOuterCornerWorld());
				const FVector3d Inward = (FVector3d(0.5 * S, 0.5 * S, 0.0) - Outer).GetSafeNormal();
				const UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
				int32 Red = 0, RedOutward = 0;
				// A tower's own walls are red all round: the platform below the walk is what faces the palace.
				const double Below = C->Tower == EHutongCornerTower::None ? BIG_NUMBER : C->Params.GetHeight();
				for (int32 T : Mesh.TriangleIndicesItr())
				{
					if (MatIDs->GetValue(T) != HutongGen::MatSlot_RedWall || Mesh.GetTriCentroid(T).Z > Below) continue;
					++Red;
					if ((-Mesh.GetTriNormal(T)).Dot(Inward) < 0.0) ++RedOutward;
				}
				TestTrue(*FString::Printf(TEXT("%s: red only on the palace's side (%d of %d facing out)"), *Case, RedOutward, Red),
					RedOutward == 0 && (Rank == EHutongCityWallRank::Palace) == (Red > 0));
			}

			// Each port on the footprint's edge, leaving it, its centre line a half-base from the outside face.
			TArray<UHutongBuildingComponent::FWallPort> Ports;
			C->GetWallPorts(Ports);
			TestEqual(*FString::Printf(TEXT("%s: two ports"), *Case), Ports.Num(), 2);
			const FVector2D Centre(0.5 * S, 0.5 * S), Outer(C->GetOuterCornerWorld());
			for (const UHutongBuildingComponent::FWallPort& Port : Ports)
			{
				const FVector2D P(Port.Point);
				TestTrue(*FString::Printf(TEXT("%s: port on the edge, leading out"), *Case),
					(P - Centre).Dot(Port.Out) > 0.49 * S && FMath::IsNearlyEqual(FMath::Abs((P - Centre).Dot(Port.Out)), 0.5 * S, 0.01));
				const FVector2D Across(-Port.Out.Y, Port.Out.X);
				TestTrue(*FString::Printf(TEXT("%s: port centred on the leg"), *Case),
					FMath::IsNearlyEqual(FMath::Abs((P - Outer).Dot(Across)), 0.5 * B, 0.01));
			}
		}
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongArrowTowerTest,
	"HutongLayout.CityWall.ArrowTower",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongArrowTowerTest::RunTest(const FString& Parameters)
{
	UHutongArrowTowerBuildingComponent* C = NewObject<UHutongArrowTowerBuildingComponent>();
	const FHutongArrowTowerParams& P = C->Params;
	for (const EHutongBaySide Side : { EHutongBaySide::MinusY, EHutongBaySide::PlusX, EHutongBaySide::PlusY, EHutongBaySide::MinusX })
	{
		C->Facade = Side;
		const FVector2D S = C->GetFootprintSize();
		const FString Case = FString::Printf(TEXT("front %d"), (int32)Side);
		for (const EHutongDetail Level : { EHutongDetail::Massing, EHutongDetail::Far, EHutongDetail::Near })
		{
			FDynamicMesh3 Mesh;
			UHutongArrowTowerBuildingComponent::BuildArrowTowerMesh(P, Side, Mesh, Level);
			const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);
			TestTrue(FString::Printf(TEXT("%s, level %d: closed (%d unmatched, volume %.0f)"), *Case, (int32)Level, R.Unmatched, R.Volume),
				R.Unmatched == 0 && R.Volume > 0.0);
			const UE::Geometry::FAxisAlignedBox3d Box = Mesh.GetBounds();
			TestTrue(*FString::Printf(TEXT("%s, level %d: inside the footprint"), *Case, (int32)Level),
				Box.Min.X > -0.5 && Box.Min.Y > -0.5 && Box.Max.X < S.X + 0.5 && Box.Max.Y < S.Y + 0.5);
		}

		TArray<UHutongBuildingComponent::FWallPort> Ports;
		C->GetWallPorts(Ports);
		if (!TestEqual(*FString::Printf(TEXT("%s: two ports"), *Case), Ports.Num(), 2)) continue;
		const FVector2D Centre = 0.5 * S;
		for (const UHutongBuildingComponent::FWallPort& Port : Ports)
		{
			TestEqual(*FString::Printf(TEXT("%s: a leg of the city's wall"), *Case), Port.BaseWidth, P.GetLegBaseWidth());
			TestTrue(*FString::Printf(TEXT("%s: port on a side, leading out"), *Case),
				FMath::IsNearlyEqual((FVector2D(Port.Point) - Centre).Dot(Port.Out), 0.5 * P.GetPlatformWidth(), 0.01));
			// The outside is ahead of the front, square to the legs.
			const FVector2D Ahead = FVector2D(Port.Outside) - FVector2D(Port.Point);
			TestTrue(*FString::Printf(TEXT("%s: outside square to the legs"), *Case), FMath::Abs(Ahead.GetSafeNormal().Dot(Port.Out)) < 0.2);
		}
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCityGateTest,
	"HutongLayout.CityWall.Gate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCityGateTest::RunTest(const FString& Parameters)
{
	UHutongCityGateBuildingComponent* C = NewObject<UHutongCityGateBuildingComponent>();
	for (const EHutongCityGateTower Tower : { EHutongCityGateTower::None, EHutongCityGateTower::Zhalou, EHutongCityGateTower::Chenglou })
	{
		C->Params.Tower = Tower;
		const FHutongCityGateParams& P = C->Params;
		for (const EHutongBaySide Side : { EHutongBaySide::MinusY, EHutongBaySide::PlusX, EHutongBaySide::PlusY, EHutongBaySide::MinusX })
		{
			C->Facade = Side;
			const FVector2D S = C->GetFootprintSize();
			const FString Case = FString::Printf(TEXT("tower %d, front %d"), (int32)Tower, (int32)Side);
			for (const EHutongDetail Level : { EHutongDetail::Massing, EHutongDetail::Near })
			{
				FDynamicMesh3 Mesh;
				UHutongCityGateBuildingComponent::BuildCityGateMesh(P, Side, Mesh, Level);
				const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);
				TestTrue(FString::Printf(TEXT("%s, level %d: closed (%d unmatched)"), *Case, (int32)Level, R.Unmatched), R.Unmatched == 0 && R.Volume > 0.0);
				const UE::Geometry::FAxisAlignedBox3d Box = Mesh.GetBounds();
				TestTrue(*FString::Printf(TEXT("%s, level %d: inside the footprint"), *Case, (int32)Level),
					Box.Min.X > -0.5 && Box.Min.Y > -0.5 && Box.Max.X < S.X + 0.5 && Box.Max.Y < S.Y + 0.5);

				// Walk straight through the passage at a man's height.
				UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);
				const FVector2D Mid = 0.5 * S;
				const bool bAlongX = HutongGen::BaySide::IsAlongX((HutongGen::EBaySide)Side);
				const FVector3d Through = bAlongX ? FVector3d(0.0, 1.0, 0.0) : FVector3d(1.0, 0.0, 0.0);
				const FVector3d From = FVector3d(Mid.X, Mid.Y, 180.0) - Through * (FMath::Max(S.X, S.Y) + 100.0);
				double T = 0.0; int32 Tid = -1;
				TestFalse(*FString::Printf(TEXT("%s, level %d: the passage is open"), *Case, (int32)Level),
					Tree.FindNearestHitTriangle(FRay3d(From, Through), T, Tid));
			}
			TArray<UHutongBuildingComponent::FWallPort> Ports;
			C->GetWallPorts(Ports);
			if (!TestEqual(*FString::Printf(TEXT("%s: two ports"), *Case), Ports.Num(), 2)) continue;
			for (const UHutongBuildingComponent::FWallPort& Port : Ports)
			{
				TestTrue(*FString::Printf(TEXT("%s: port at an end"), *Case),
					FMath::IsNearlyEqual((FVector2D(Port.Point) - 0.5 * S).Dot(Port.Out), 0.5 * P.GetLength(), 0.01));
			}
		}
	}
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongXieshanHipEndTest,
	"HutongLayout.Roofs.XieshanHipEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongXieshanHipEndTest::RunTest(const FString& Parameters)
{
	for (int32 Case = 0; Case < 4; ++Case)
	{
		HutongMeshUtils::FXieshanRoofSpec Xie;
		Xie.Width = 2000.0;
		Xie.Depth = 900.0;
		Xie.Section = HutongGen::Jiajia::MakeSection(EHutongPurlins::Seven, 0.5 * Xie.Depth, 150.0, 0.0);
		Xie.Rise = Xie.Section.Rise();
		Xie.ShouInset = 140.0;
		Xie.FlareRise = 40.0; Xie.FlareRun = 30.0; Xie.FlareLength = 250.0;
		Xie.bHipStart = (Case & 1) != 0;
		Xie.bHipEnd = (Case & 2) != 0;
		FDynamicMesh3 Mesh;
		HutongMeshUtils::AppendXieshanRoof(Mesh, FVector3d::ZeroVector, Xie);
		const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);
		TestTrue(FString::Printf(TEXT("hips %d: closed (%d unmatched, volume %.0f)"), Case, R.Unmatched, R.Volume), R.Unmatched == 0 && R.Volume > 0.0);

		// A hipped end reaches the ridge half the depth in from its eave; a 山花 stands at the 收山 line.
		double MaxZNearStart = 0.0;
		for (int32 V : Mesh.VertexIndicesItr())
		{
			const FVector3d X = Mesh.GetVertex(V);
			if (X.X < 0.4 * Xie.Depth) MaxZNearStart = FMath::Max(MaxZNearStart, X.Z);
		}
		const bool bHip = Xie.bHipStart;
		TestTrue(FString::Printf(TEXT("hips %d: the start end %s"), Case, bHip ? TEXT("slopes up to the ridge") : TEXT("stands to the ridge")),
			bHip ? MaxZNearStart < 0.9 * Xie.Rise : MaxZNearStart > 0.95 * Xie.Rise);
	}
	return true;
}


// Every joining face of the gate, the arrow tower and the corners has a wall leg ending on it, and the pieces' footprints
// do not overlap.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGateComplexTest,
	"HutongLayout.CityWall.GateComplex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGateComplexTest::RunTest(const FString& Parameters)
{
	using namespace HutongGen;
	for (const EHutongBarbicanGateSide Side : { EHutongBarbicanGateSide::East, EHutongBarbicanGateSide::West })
	{
		FHutongGateComplexParams P;
		P.SideGate = Side;
		TArray<FGateComplexPiece> Pieces;
		LayOutGateComplex(P, Pieces);
		const FString Case = Side == EHutongBarbicanGateSide::East ? TEXT("east") : TEXT("west");

		TArray<FVector2D> LegEnds;
		TArray<FVector2D> Ports;
		int32 Gates = 0, Towers = 0, Corners = 0;
		for (const FGateComplexPiece& Piece : Pieces)
		{
			const FTransform Xf(FRotator(0.0, Piece.YawDeg, 0.0), FVector(Piece.Origin.X, Piece.Origin.Y, 0.0));
			UHutongBuildingComponent* C = nullptr;
			switch (Piece.Kind)
			{
			case FGateComplexPiece::EKind::Gate: { auto* G = NewObject<UHutongCityGateBuildingComponent>(); G->Params = Piece.Gate; G->Facade = Piece.Facade; C = G; ++Gates; break; }
			case FGateComplexPiece::EKind::ArrowTower: { auto* T = NewObject<UHutongArrowTowerBuildingComponent>(); T->Params = Piece.Tower; T->Facade = Piece.Facade; C = T; ++Towers; break; }
			case FGateComplexPiece::EKind::Corner:
			{
				auto* K = NewObject<UHutongWallCornerBuildingComponent>();
				K->Params = Piece.Wall; K->Shape = EHutongWallCornerShape::Rounded; K->PlatformSize = Piece.CornerSize; K->OuterCorner = Piece.OuterCorner;
				C = K; ++Corners;
				break;
			}
			default:
			{
				const double B = Piece.Wall.GetBaseWidth();
				LegEnds.Add(FVector2D(Xf.TransformPosition(FVector(0.0, 0.5 * B, 0.0))));
				LegEnds.Add(FVector2D(Xf.TransformPosition(FVector(Piece.Length, 0.5 * B, 0.0))));
				break;
			}
			}
			if (!C) continue;
			TArray<UHutongBuildingComponent::FWallPort> Local;
			C->GetWallPorts(Local);
			TestEqual(*FString::Printf(TEXT("%s: the footprint the layout reserved"), *Case), C->GetFootprintSize(), Piece.Size);
			for (const UHutongBuildingComponent::FWallPort& Port : Local) Ports.Add(FVector2D(Xf.TransformPosition(Port.Point)));
		}
		TestEqual(*FString::Printf(TEXT("%s: two gates"), *Case), Gates, 2);
		TestEqual(*FString::Printf(TEXT("%s: one arrow tower"), *Case), Towers, 1);
		TestEqual(*FString::Printf(TEXT("%s: two corners"), *Case), Corners, 2);
		for (const FVector2D& Port : Ports)
		{
			const bool bJoined = LegEnds.ContainsByPredicate([&](const FVector2D& E) { return FVector2D::Distance(E, Port) < 1.0; });
			TestTrue(*FString::Printf(TEXT("%s: a wall joins the face at (%.0f, %.0f)"), *Case, Port.X, Port.Y), bJoined);
		}

		// No two pieces other than walls overlap.
		for (int32 i = 0; i < Pieces.Num(); ++i)
		{
			for (int32 j = i + 1; j < Pieces.Num(); ++j)
			{
				if (Pieces[i].Kind == FGateComplexPiece::EKind::Wall || Pieces[j].Kind == FGateComplexPiece::EKind::Wall) continue;
				FVector2D A[4], Bq[4];
				GateComplexPieceCorners(Pieces[i], A);
				GateComplexPieceCorners(Pieces[j], Bq);
				const FBox2D Ba(A, 4), Bb(Bq, 4);
				const bool bOverlap = Ba.Min.X < Bb.Max.X - 1.0 && Bb.Min.X < Ba.Max.X - 1.0 && Ba.Min.Y < Bb.Max.Y - 1.0 && Bb.Min.Y < Ba.Max.Y - 1.0;
				TestFalse(*FString::Printf(TEXT("%s: pieces %d and %d apart"), *Case, i, j), bOverlap);
			}
		}
	}
	return true;
}

#endif
