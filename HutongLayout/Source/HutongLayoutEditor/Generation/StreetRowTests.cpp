#include "Generation/StreetRowLayout.h"
#include "Generation/HutongGateRow.h"
#include "Generation/HutongBuildingComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// A street row partitions its length into buildings and gates, and a gate built at a bay's width still builds.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongStreetRowLayoutTest,
	"HutongLayout.Row.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongStreetRowLayoutTest::RunTest(const FString& Parameters)
{
	using namespace HutongGen::StreetRow;

	auto Tiles = [&](const TArray<FPiece>& Pieces, double Length, int32 N, const TCHAR* What)
	{
		double Reached = 0.0;
		int32 Bays = 0;
		for (const FPiece& P : Pieces)
		{
			TestNearlyEqual(FString::Printf(TEXT("%s: piece starts where the last ended"), What), P.From, Reached, 0.01);
			TestTrue(FString::Printf(TEXT("%s: piece has extent"), What), P.To > P.From);
			TestEqual(FString::Printf(TEXT("%s: piece's first bay follows"), What), P.FirstBay, Bays);
			Reached = P.To;
			Bays += P.BayCount;
		}
		TestNearlyEqual(FString::Printf(TEXT("%s: the last piece ends on the row's end"), What), Reached, Length, 0.01);
		TestEqual(FString::Printf(TEXT("%s: every bay is in a piece"), What), Bays, N);
	};

	// One gate in the middle: house, gate, house.
	{
		const TArray<FPiece> P = LayOut(1800.0, 6, { 2 });
		TestEqual(TEXT("three pieces"), P.Num(), 3);
		if (P.Num() == 3)
		{
			TestEqual(TEXT("house first"), P[0].Piece, EPiece::Building);
			TestEqual(TEXT("two bays"), P[0].BayCount, 2);
			TestEqual(TEXT("gate second"), P[1].Piece, EPiece::Gate);
			TestNearlyEqual(TEXT("gate is one bay wide"), P[1].To - P[1].From, 300.0, 0.01);
			TestEqual(TEXT("house last"), P[2].Piece, EPiece::Building);
			TestEqual(TEXT("three bays"), P[2].BayCount, 3);
		}
		Tiles(P, 1800.0, 6, TEXT("middle gate"));
	}
	// No gate: one building the whole length.
	{
		const TArray<FPiece> P = LayOut(1800.0, 6, {});
		TestEqual(TEXT("one piece"), P.Num(), 1);
		if (P.Num() == 1) TestEqual(TEXT("with every bay"), P[0].BayCount, 6);
		Tiles(P, 1800.0, 6, TEXT("no gate"));
	}
	// Gates at both ends and two adjacent: each gate is its own piece, and the two house runs between them.
	{
		const TArray<FPiece> P = LayOut(2100.0, 7, { 0, 3, 4, 6 });
		TestEqual(TEXT("six pieces"), P.Num(), 6);
		int32 Gates = 0;
		for (const FPiece& X : P) if (X.Piece == EPiece::Gate) ++Gates;
		TestEqual(TEXT("four gates"), Gates, 4);
		Tiles(P, 2100.0, 7, TEXT("ends and adjacent"));
	}
	// A gate bay off the row is ignored.
	{
		const TArray<FPiece> P = LayOut(900.0, 3, { 7 });
		TestEqual(TEXT("ignored"), P.Num(), 1);
	}

	TestEqual(TEXT("bay under the start"), BayAt(0.0, 900.0, 3), 0);
	TestEqual(TEXT("bay under the middle"), BayAt(450.0, 900.0, 3), 1);
	TestEqual(TEXT("bay under the end"), BayAt(900.0, 900.0, 3), 2);
	TestEqual(TEXT("nothing before the row"), BayAt(-1.0, 900.0, 3), INDEX_NONE);
	TestEqual(TEXT("nothing after it"), BayAt(901.0, 900.0, 3), INDEX_NONE);

	// Every house in a row shares one eave: a one-bay piece and a three-bay piece built from
	// RowHouse derive the same figure, where each on its own derives a different one.
	{
		FHutongSiheyuanParams House;
		House.bDeriveProportions = true;
		House.bDeriveEaveFromBays = true;
		auto EaveOf = [](FHutongSiheyuanParams P, double W, int32 N)
		{
			P.Width = W; P.Depth = 600.0; P.BayCountOverride = N;
			return P.GetEaveHeight();
		};
		TestTrue(TEXT("on their own the pieces disagree"),
			!FMath::IsNearlyEqual(EaveOf(House, 300.0, 1), EaveOf(House, 900.0, 3), 1.0));
		const FHutongSiheyuanParams Row = RowHouse(House, 1500.0, 600.0, 5);
		TestNearlyEqual(TEXT("from the row they agree"), EaveOf(Row, 300.0, 1), EaveOf(Row, 900.0, 3), 0.01);
		TestNearlyEqual(TEXT("and it is the whole run's eave"), EaveOf(Row, 300.0, 1), EaveOf(House, 1500.0, 5), 0.01);
		FHutongSiheyuanParams Fixed = House;
		Fixed.bDeriveEaveFromBays = false;
		Fixed.EaveHeight = 333.0;
		TestNearlyEqual(TEXT("a fixed eave is left alone"), RowHouse(Fixed, 1500.0, 600.0, 5).EaveHeight, 333.0, 0.01);
	}

	// A gate at a house bay's width, off the band, with its ridge held above the row's.
	{
		FHutongGateHouseParams Gate;
		Gate.bConstrainToHistoricalSize = false;
		FHutongSiheyuanParams House;
		const double RowRidge = HutongGen::Ridge::House(House, 1200.0, 600.0);
		HutongGen::GateRow::LiftGateAboveRidge(Gate, 600.0, RowRidge, 35.0);
		TestTrue(TEXT("the gate's ridge clears the row's"),
			HutongGen::Ridge::Gate(Gate, 600.0) >= RowRidge + 35.0 - 0.01);

		UE::Geometry::FDynamicMesh3 Mesh;
		UHutongGateHouseBuildingComponent::BuildGateHouseMesh(Gate, EHutongBaySide::MinusY, 380.0, 600.0, Mesh);
		TestTrue(TEXT("a gate builds at a 380 cm bay"), Mesh.TriangleCount() > 0);

		// Measured off the meshes: the estimate the lift is computed from and the roof the
		// generator builds have to agree, or the gate stands with its ridge below the row's. A
		// gable roof has vertices only at its two ends, so the ridge is read by casting down onto
		// the roof surface across the depth at mid-run, where the 蠍子尾 at the gable ends cannot be hit.
		auto RidgeZ = [](const UE::Geometry::FDynamicMesh3& M, double Width, double Depth)
		{
			UE::Geometry::FDynamicMeshAABBTree3 Tree(&M, true);
			double Top = -TNumericLimits<double>::Max();
			for (int32 i = 0; i <= 40; ++i)
			{
				const double Y = -100.0 + (Depth + 200.0) * i / 40.0;
				const FRay3d Ray(FVector3d(0.5 * Width, Y, 2000.0), FVector3d(0.0, 0.0, -1.0));
				double T; int32 TID;
				if (Tree.FindNearestHitTriangle(Ray, T, TID)) Top = FMath::Max(Top, 2000.0 - T);
			}
			return Top;
		};
		UE::Geometry::FDynamicMesh3 HouseMesh;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(House, EHutongBaySide::MinusY, 0, 1200.0, 600.0, HouseMesh);
		const double HouseTop = RidgeZ(HouseMesh, 1200.0, 600.0);
		const double GateTop = RidgeZ(Mesh, 380.0, 600.0);
		// Estimate against built fold is HutongLayout.Roofs.RidgeEstimates' job, for every type.
		TestTrue(TEXT("the built gate ridge stands clear of the built row ridge"), GateTop >= HouseTop + 30.0);
	}

	return true;
}

#endif
