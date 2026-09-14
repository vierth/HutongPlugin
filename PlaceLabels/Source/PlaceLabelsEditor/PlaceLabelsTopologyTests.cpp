#include "PlaceLabelsTopology.h"
#include "PlaceLabelGeometry.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The weld is the one piece here that reshapes a level without being watched.

namespace
{
	// A square with its lower-left corner at (X, Y).
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
	// Two squares meant to abut along x = 100, drawn 3 cm apart.
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

	// Well beyond tolerance, the same pair is simply two separate places.
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
	// The same fault the other way: the second square starts 4 cm inside the first.
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

	// And it welds shut, leaving nothing to report.
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

	// Corner count is unchanged: this is corner-to-corner welding, and nothing needed splitting.
	TestEqual(TEXT("first square still has four corners"), Polygons[0].Num(), 4);
	TestEqual(TEXT("second square still has four corners"), Polygons[1].Num(), 4);

	// The two pairs that were 3 cm apart now sit on the shared midline at x = 101.5.
	TestTrue(TEXT("shared corner at the foot of the seam"),
		AnyVertexNear(Polygons[0], FVector2D(101.5, 0.0), 0.01)
		&& AnyVertexNear(Polygons[1], FVector2D(101.5, 0.0), 0.01));
	TestTrue(TEXT("shared corner at the head of the seam"),
		AnyVertexNear(Polygons[0], FVector2D(101.5, 100.0), 0.01)
		&& AnyVertexNear(Polygons[1], FVector2D(101.5, 100.0), 0.01));

	// And the whole point: the seam detector now finds nothing.
	TArray<PlaceLabelsTopology::FPolygonSeam> Seams;
	PlaceLabelsTopology::FindPolygonSeams(Polygons, 25.0, Seams);
	TestEqual(TEXT("no seam survives the weld"), Seams.Num(), 0);

	// Idempotent: welding an already-welded pair must be a no-op, or the panel would report work done every time somebody pressed the button.
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
	// A small square abutting the middle of a long one's right-hand edge, 2 cm short of it.
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 200.0));
	Polygons.Add(Square(202.0, 50.0, 50.0));

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	// The long square gains a corner where each of the small square's corners landed.
	TestEqual(TEXT("two corners inserted into the long square"), Report.CornersInserted, 2);
	TestEqual(TEXT("long square now has six corners"), Polygons[0].Num(), 6);
	TestEqual(TEXT("small square still has four"), Polygons[1].Num(), 4);

	// Both sides now carry a corner at exactly the same two points on x = 200.
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
	// Three regions meeting at one courtyard corner, each a few centimetres off.
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));            // corner at (100, 100)
	Polygons.Add(Square(104.0, 0.0, 100.0));          // corner at (104, 100)
	Polygons.Add(Square(0.0, 103.0, 100.0));          // corner at (100, 103)

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 30.0, Changed, Report);

	TestEqual(TEXT("no cluster refused"), Report.ClustersRefused, 0);

	// All three ended up on one point rather than three.
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
	// Proximity is transitive and distance is not.
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
	// The other side of the refusal.
	TArray<TArray<FVector2D>> Polygons;
	Polygons.Add(Square(0.0, 0.0, 100.0));            // corner at (100, 100)
	Polygons.Add(Square(103.0, 0.0, 100.0));          // corner at (103, 100)
	Polygons.Add(Square(0.0, 102.0, 100.0));          // corner at (100, 102)
	Polygons.Add(Square(104.0, 103.0, 100.0));        // corner at (104, 103)

	TArray<bool> Changed;
	PlaceLabelsTopology::FWeldReport Report;
	PlaceLabelsTopology::WeldPolygons(Polygons, 25.0, Changed, Report);

	TestEqual(TEXT("nothing refused"), Report.ClustersRefused, 0);
	TestTrue(TEXT("all four moved"), Changed[0] && Changed[1] && Changed[2] && Changed[3]);

	// All four now meet at one point rather than four.
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
	// A gap somebody drew on purpose — a lane between two compounds — has to survive the weld untouched.
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

// A cluster is refused when it spans more than one corner of a polygon it cannot afford to lose.
// The "confined to one polygon" guard passed a cluster holding two corners of A and one of B, and
// both of A's went to the same centroid: a triangle then had three vertices with two identical,
// zero signed area, no containment and no fill — and the weld reported success.
bool FPlaceLabelsWeldKeepsTrianglesTest::RunTest(const FString& Parameters)
{
	// Two corners of the triangle within tolerance of each other and of the square's corner.
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

	// A polygon with corners to spare still welds, and the cleanup pass takes the duplicate out.
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
