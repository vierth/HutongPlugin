#include "Generation/HutongBuildingComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallGatesTest, "HutongLayout.Walls.MultipleGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallGatesTest::RunTest(const FString& Parameters)
{
	const double L = 2000.0;
	FHutongWallParams P;
	P.bHasGate = true;
	P.GatePosition = 0.25;
	P.MoreGatePositions = { 0.75 };
	const TArray<FHutongWallGateSpan> Spans = P.GetGateSpans(L);
	TestEqual(TEXT("two gates fit"), Spans.Num(), 2);

	UE::Geometry::FDynamicMesh3 Mesh;
	UHutongWallBuildingComponent::BuildWallMesh(P, L, P.GetThickness(), false, Mesh, EHutongDetail::Near);
	UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh, true);
	auto Through = [&](double X)
	{
		double T; int32 Tid;
		return !Tree.FindNearestHitTriangle(FRay3d(FVector3d(X, -500.0, 100.0), FVector3d(0, 1, 0)), T, Tid);
	};
	for (const FHutongWallGateSpan& G : Spans) TestTrue(FString::Printf(TEXT("open through the gate at %.0f"), G.Centre), Through(G.Centre));
	TestFalse(TEXT("solid between the gates"), Through(0.5 * L));

	P.MoreGatePositions.Add(0.26);
	const TArray<FHutongWallGateSpan> Crowded = P.GetGateSpans(L);
	TestEqual(TEXT("a crowding gate is dropped"), Crowded.Num(), 2);
	if (Crowded.Num() == 2) TestNearlyEqual(TEXT("the first stays where it was"), Crowded[0].Centre, Spans[0].Centre, 0.01);

	FHutongWallParams One;
	One.bHasGate = true;
	TestEqual(TEXT("one gate"), One.GetGateSpans(L).Num(), 1);
	One.bHasGate = false;
	One.MoreGatePositions = { 0.3 };
	TestEqual(TEXT("no gate without the first"), One.GetGateSpans(L).Num(), 0);
	return true;
}

#endif
