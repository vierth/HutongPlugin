#include "Tools/HutongGroupTransform.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGroupTransformTest,
	"HutongLayout.Footprint.GroupTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGroupTransformTest::RunTest(const FString& Parameters)
{
	using namespace HutongGroupTransform;

	TestEqual(TEXT("one bearing"), FrameYaw({ 30.0, 120.0, -60.0 }), 30.0, 1e-6);
	TestEqual(TEXT("mixed"), FrameYaw({ 0.0, 30.0 }), 0.0);
	TestEqual(TEXT("near 0 and near 90 agree"), FrameYaw({ 0.2, 89.9 }), 0.2, 1e-6);

	const FVector2D Size(900.0, 600.0);
	const FTransform House(FRotator(0.0, 0.0, 0.0), FVector(1000.0, 2000.0, 50.0));
	const FVector Corner = House.TransformPosition(FVector(Size.X, 0.0, 0.0));
	const FTransform Turned = RotateAbout(House, Corner, 90.0);
	TestTrue(TEXT("pivot corner stays"), Turned.TransformPosition(FVector(Size.X, 0.0, 0.0)).Equals(Corner, 1e-6));
	TestEqual(TEXT("yaw added"), Turned.Rotator().Yaw, 90.0, 1e-6);
	TestEqual(TEXT("height kept"), Turned.GetLocation().Z, 50.0, 1e-6);

	const FVector Centre = FootprintCentre(House, Size);
	const FVector Doubled = ScaledCentre(Centre, 0.0, FVector2D(1000.0, 2000.0), FVector2D(2.0, 2.0));
	TestTrue(TEXT("centre offset doubles"), Doubled.Equals(FVector(1900.0, 2600.0, Centre.Z), 1e-6));
	TestTrue(TEXT("sides double"), SideFactors(0.0, 0.0, FVector2D(2.0, 2.0), Size, false).Equals(FVector2D(2.0, 2.0), 1e-9));
	const FVector Origin = OriginForCentre(House, Doubled, Size * 2.0);
	TestTrue(TEXT("origin keeps the min corner"), Origin.Equals(FVector(1000.0, 2000.0, 50.0), 1e-6));

	TestTrue(TEXT("quarter turn swaps"), SideFactors(90.0, 0.0, FVector2D(3.0, 1.0), Size, false).Equals(FVector2D(1.0, 3.0), 1e-9));
	const FVector2D Wall(2000.0, 40.0);
	TestTrue(TEXT("wall thickness kept"), SideFactors(0.0, 0.0, FVector2D(2.0, 2.0), Wall, true).Equals(FVector2D(2.0, 1.0), 1e-9));

	FHutongFootprintSkew Skew;
	Skew.Corner10 = FVector2D(30.0, -10.0);
	TestTrue(TEXT("skew stretches"), ScaledSkew(Skew, FVector2D(2.0, 0.5)).Corner10.Equals(FVector2D(60.0, -5.0), 1e-9));
	return true;
}

#endif
