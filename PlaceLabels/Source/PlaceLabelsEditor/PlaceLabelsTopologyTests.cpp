#include "PlaceLabelsTopology.h"
#include "PlaceLabelGeometry.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<FVector2D> Square(double X, double Y, double Size)
	{
		return { FVector2D(X, Y), FVector2D(X + Size, Y),
				 FVector2D(X + Size, Y + Size), FVector2D(X, Y + Size) };
	}

	bool AnyVertexNear(const TArray<FVector2D>& Poly, const FVector2D& P, double Tol)
	{
		for (const FVector2D& V : Poly)
		{
			if (FVector2D::Distance(V, P) <= Tol)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsSeamGapTest,
	"PlaceLabels.Topology.SeamGap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsSeamGapTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));
	Polygons.Add(Square(103.0, 0.0, 100.0));

	TArray<PlaceLabelsTopology::FPolygonSeam> Seams;
	PlaceLabelsTopology::FindPolygonSeams(Polygons, 25.0, Seams);

	TestEqual(TEXT("one seam reported"), Seams.Num(), 1);
	if (Seams.Num() == 1)
	{
		TestEqual(TEXT("worst offset is the 3 cm gap"), Seams[0].WorstOffsetCm, 3.0, 0.001);
		TestFalse(TEXT("a gap is not an overlap"), Seams[0].bOverlapping);
		TestTrue(TEXT("near misses found along the seam"), Seams[0].NearMissCount > 0);
	}

	TArray<TArray<FVector2D>> FarApart;
	FarApart.Add(Square(0.0, 0.0, 100.0));
	FarApart.Add(Square(500.0, 0.0, 100.0));

	PlaceLabelsTopology::FindPolygonSeams(FarApart, 25.0, Seams);
	TestEqual(TEXT("a real gap is not a seam"), Seams.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsSeamOverlapTest,
	"PlaceLabels.Topology.SeamOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsSeamOverlapTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));
	Polygons.Add(Square(96.0, 0.0, 100.0));

	TArray<PlaceLabelsTopology::FPolygonSeam> Seams;
	PlaceLabelsTopology::FindPolygonSeams(Polygons, 25.0, Seams);

	TestEqual(TEXT("one seam reported"), Seams.Num(), 1);
	if (Seams.Num() == 1)
	{
		TestEqual(TEXT("worst offset is the 4 cm overlap"), Seams[0].WorstOffsetCm, 4.0, 0.001);
		TestTrue(TEXT("reported as an overlap"), Seams[0].bOverlapping);
	}

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);
	PlaceLabelsTopology::FindPolygonSeams(Polygons, 25.0, Seams);
	TestEqual(TEXT("no seam survives the weld"), Seams.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldGapTest,
	"PlaceLabels.Topology.WeldGap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldGapTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));
	Polygons.Add(Square(103.0, 0.0, 100.0));

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestTrue(TEXT("both polygons moved"), Changed[0] && Changed[1]);
	TestTrue(TEXT("corners were moved"), Report.CornersMoved > 0);
	TestEqual(TEXT("no cluster refused"), Report.ClustersRefused, 0);

	TestEqual(TEXT("first square still has four corners"), Polygons[0].Num(), 4);
	TestEqual(TEXT("second square still has four corners"), Polygons[1].Num(), 4);

	TestTrue(TEXT("shared corner at the foot of the seam"),
		AnyVertexNear(Polygons[0], FVector2D(101.5, 0.0), 0.01)
		&& AnyVertexNear(Polygons[1], FVector2D(101.5, 0.0), 0.01));
	TestTrue(TEXT("shared corner at the head of the seam"),
		AnyVertexNear(Polygons[0], FVector2D(101.5, 100.0), 0.01)
		&& AnyVertexNear(Polygons[1], FVector2D(101.5, 100.0), 0.01));

	TArray<PlaceLabelsTopology::FPolygonSeam> Seams;
	PlaceLabelsTopology::FindPolygonSeams(Polygons, 25.0, Seams);
	TestEqual(TEXT("no seam survives the weld"), Seams.Num(), 0);

	TArray<bool> SecondPass;
	PlaceLabelsTopology::FWeldReport SecondReport;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, SecondPass, SecondReport);
	TestFalse(TEXT("second weld changes nothing"), SecondPass[0] || SecondPass[1]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldTJunctionTest,
	"PlaceLabels.Topology.WeldTJunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldTJunctionTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 200.0));
	Polygons.Add(Square(202.0, 50.0, 50.0));

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestEqual(TEXT("two corners inserted into the long square"), Report.CornersInserted, 2);
	TestEqual(TEXT("long square now has six corners"), Polygons[0].Num(), 6);
	TestEqual(TEXT("small square still has four"), Polygons[1].Num(), 4);

	TestTrue(TEXT("shared corner at y = 50"),
		AnyVertexNear(Polygons[0], FVector2D(200.0, 50.0), 0.01)
		&& AnyVertexNear(Polygons[1], FVector2D(200.0, 50.0), 0.01));
	TestTrue(TEXT("shared corner at y = 100"),
		AnyVertexNear(Polygons[0], FVector2D(200.0, 100.0), 0.01)
		&& AnyVertexNear(Polygons[1], FVector2D(200.0, 100.0), 0.01));

	TArray<PlaceLabelsTopology::FPolygonSeam> Seams;
	PlaceLabelsTopology::FindPolygonSeams(Polygons, 25.0, Seams);
	TestEqual(TEXT("no seam survives the weld"), Seams.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldThreeWayTest,
	"PlaceLabels.Topology.WeldThreeWay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldThreeWayTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));
	Polygons.Add(Square(104.0, 0.0, 100.0));
	Polygons.Add(Square(0.0, 103.0, 100.0));

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 30.0, Changed, Report);

	TestEqual(TEXT("no cluster refused"), Report.ClustersRefused, 0);

	const FVector2D Shared(101.3333333, 101.0);
	TestTrue(TEXT("first region meets at the shared corner"),
		AnyVertexNear(Polygons[0], Shared, 0.01));
	TestTrue(TEXT("second region meets at the shared corner"),
		AnyVertexNear(Polygons[1], Shared, 0.01));
	TestTrue(TEXT("third region meets at the shared corner"),
		AnyVertexNear(Polygons[2], Shared, 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldRefusesChainTest,
	"PlaceLabels.Topology.WeldRefusesChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldRefusesChainTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	for (int32 i = 0; i < 6; ++i)
	{
		const double X = i * 20.0;
		Polygons.Add({ FVector2D(X, 0.0), FVector2D(X, 100.0), FVector2D(X + 100.0, 200.0 + i) });
	}

	const TArray<TArray<FVector2D>> Before = Polygons;

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestTrue(TEXT("the runaway cluster was refused"), Report.ClustersRefused > 0);
	TestEqual(TEXT("the first corner was left where it was"),
		Polygons[0][0].X, Before[0][0].X, 0.001);
	TestEqual(TEXT("and so was the last"),
		Polygons[5][0].X, Before[5][0].X, 0.001);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldFourWayTest,
	"PlaceLabels.Topology.WeldFourWay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldFourWayTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));
	Polygons.Add(Square(103.0, 0.0, 100.0));
	Polygons.Add(Square(0.0, 102.0, 100.0));
	Polygons.Add(Square(104.0, 103.0, 100.0));

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestEqual(TEXT("nothing refused"), Report.ClustersRefused, 0);
	TestTrue(TEXT("all four moved"), Changed[0] && Changed[1] && Changed[2] && Changed[3]);

	const FVector2D Shared((100.0 + 103.0 + 100.0 + 104.0) / 4.0,
						   (100.0 + 100.0 + 102.0 + 103.0) / 4.0);
	for (int32 i = 0; i < 4; ++i)
	{
		TestTrue(*FString::Printf(TEXT("region %d meets at the shared corner"), i),
			AnyVertexNear(Polygons[i], Shared, 0.01));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldLeavesRealGapsTest,
	"PlaceLabels.Topology.WeldLeavesRealGaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldLeavesRealGapsTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));
	Polygons.Add(Square(950.0, 0.0, 100.0));

	const TArray<TArray<FVector2D>> Before = Polygons;

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestFalse(TEXT("nothing moved"), Changed[0] || Changed[1]);
	TestEqual(TEXT("no corners moved"), Report.CornersMoved, 0);
	TestEqual(TEXT("no corners inserted"), Report.CornersInserted, 0);
	TestTrue(TEXT("geometry is byte-identical"), Polygons == Before);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsWeldKeepsTrianglesTest,
	"PlaceLabels.Topology.WeldKeepsTriangles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsWeldKeepsTrianglesTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add({ FVector2D(0.0, 0.0), FVector2D(10.0, 6.0), FVector2D(200.0, 300.0) });
	Polygons.Add({ FVector2D(5.0, 3.0), FVector2D(-200.0, 3.0),
		FVector2D(-200.0, -300.0), FVector2D(5.0, -300.0) });

	const TArray<TArray<FVector2D>> Before = Polygons;

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestEqual(TEXT("the triangle keeps three corners"), Polygons[0].Num(), 3);
	TestTrue(TEXT("and they are still three distinct corners"),
		!Polygons[0][0].Equals(Polygons[0][1], 0.01));
	TestTrue(TEXT("so it still has area"),
		FMath::Abs(PlaceLabelsGeo::SignedArea2D(Polygons[0])) > 1.0);
	TestTrue(TEXT("and the cluster that would have collapsed it was reported"),
		Report.ClustersRefused > 0);
	TestEqual(TEXT("the triangle was left exactly as it was"),
		Polygons[0][1].X, Before[0][1].X, 0.001);

	{
		TArray<TArray<FVector2D>> Wide;
		Wide.Add({ FVector2D(0.0, 0.0), FVector2D(10.0, 6.0), FVector2D(200.0, 300.0),
			FVector2D(-200.0, 300.0), FVector2D(-200.0, 0.0) });
		Wide.Add({ FVector2D(5.0, 3.0), FVector2D(-400.0, 3.0),
			FVector2D(-400.0, -300.0), FVector2D(5.0, -300.0) });

		TArray<bool> WideChanged;
		PlaceLabelsTopology::FWeldReport WideReport;
		PlaceLabelsTopology::WeldPolygons(Wide, 25.0, WideChanged, WideReport);

		TestTrue(TEXT("a polygon with corners to spare still welds"), WideReport.CornersMoved > 0);
		TestTrue(TEXT("and comes out with no duplicate corner"),
			!Wide[0][0].Equals(Wide[0][1], 0.01) || Wide[0].Num() < 5);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
