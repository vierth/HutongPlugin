#include "Generation/HutongBuildingComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExposedGableClosedTest, "HutongLayout.Roofs.ExposedGableClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExposedGableClosedTest::RunTest(const FString& Parameters)
{
	for (const EHutongGateStyle Style : { EHutongGateStyle::Guangliang, EHutongGateStyle::Jinzhu, EHutongGateStyle::Manzi, EHutongGateStyle::Ruyi })
	for (const double D : { 500.0, 600.0, 700.0 })
	for (double Eave = 300.0; Eave <= 480.0; Eave += 15.0)
	{
		const double W = 380.0;
		FHutongGateHouseParams Gate;
		Gate.Style = Style;
		Gate.bConstrainToHistoricalSize = false;
		Gate.bExposedFrame = true;
		Gate.EaveHeight = Eave;
		UE::Geometry::FDynamicMesh3 Mesh;
		UHutongGateHouseBuildingComponent::BuildGateHouseMesh(Gate, EHutongBaySide::MinusY, W, D, Mesh);
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh, true);
		int32 Open = 0;
		for (const double Side : { -1.0, 1.0 })
		for (int32 iy = 1; iy < 10; ++iy)
		for (double Z = 0.0; Z < 1000.0; Z += 10.0)
		{
			const double Y = D * iy / 10.0;
			double T; int32 Tid;
			if (!Tree.FindNearestHitTriangle(FRay3d(FVector3d(0.5 * W, Y, Z), FVector3d(0, 0, 1)), T, Tid)) continue;
			const FVector3d Dir(-Side, 0, 0);
			const bool bHit = Tree.FindNearestHitTriangle(FRay3d(FVector3d(Side < 0 ? -300.0 : W + 300.0, Y, Z), Dir), T, Tid);
			// Normals point inward until the bake reverses the mesh: a face seen from outside has N along the ray.
			if (!bHit || T - 300.0 > 80.0 || Mesh.GetTriNormal(Tid).Dot(Dir) <= 0.0) ++Open;
		}
		TestEqual(FString::Printf(TEXT("style %d, %.0f deep, eave %.0f: both gables closed"), (int32)Style, D, Eave), Open, 0);
	}
	return true;
}

#endif
