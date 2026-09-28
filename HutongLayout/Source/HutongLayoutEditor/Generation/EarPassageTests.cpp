#include "Misc/AutomationTest.h"
#include "Generation/EarPassageGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

// The closing wall is cut at any width asked, the preview matches the built doorway, and a narrow
// frontage never runs the wall through the room.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongEarPassageDoorwayTest,
	"HutongLayout.EarPassage.Doorway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongEarPassageDoorwayTest::RunTest(const FString&)
{
	for (const double Asked : { 80.0, 120.0, 150.0, 180.0, 240.0, 400.0 })
	{
		FHutongEarPassageParams P;
		P.PassageWidth = Asked;
		TestTrue(FString::Printf(TEXT("closing wall cut through at %.0f cm asked"), Asked), P.HasClosingDoorway());
		TestTrue(TEXT("clear way at least what was asked"), P.GetClearWidth() >= Asked - 1e-6);
		const FHutongWallParams C = P.ClosingWallParams();
		TestTrue(TEXT("doorway inside the run"), C.GetDoorwayCentre(C.Length) - 0.5 * C.GetDoorwayWidth() > 0.0
			&& C.GetDoorwayCentre(C.Length) + 0.5 * C.GetDoorwayWidth() < C.Length);
	}

	// Doorway position and shape are user-set.
	{
		FHutongEarPassageParams P;
		P.ClosingWall.DoorwayPosition = 0.25;
		const FHutongWallParams C = P.ClosingWallParams();
		TestTrue(TEXT("doorway off centre where asked"), C.GetDoorwayCentre(C.Length) < 0.5 * C.Length - 1.0);
		P.ClosingWall.Doorway = EHutongWallDoorway::None;
		TestFalse(TEXT("no doorway when none was asked"), P.HasClosingDoorway());
		TestEqual(TEXT("a solid front takes no extra width"), P.GetClearWidth(), 240.0);
	}

	// Room's roof carried on: walls stand to the ceiling under it.
	{
		FHutongEarPassageParams P;
		TestTrue(TEXT("the room's roof runs over the passage by default"), P.bRoofOverPassage);
		TestNearlyEqual(TEXT("closing wall to the ceiling under the room's roof"), P.ClosingWallParams().GetHeight(),
			P.RoomParams().GetRoofBaseHeight() + P.RoomParams().GetUndersideRise(), 0.01);
		const FHutongWallParams C = P.ClosingWallParams();
		TestTrue(TEXT("closing wall has no cap to pierce the roof"), C.CapSlabHeight <= 0.0 && C.CapRidgeHeight <= 0.0);
	}

	// Own low roof: its eave drives the wall.
	{
		FHutongEarPassageParams P;
		P.bRoofOverPassage = false;
		P.Passage.EaveHeight = 300.0;
		TestEqual(TEXT("closing wall at the eave"), P.ClosingWallParams().GetHeight(), 300.0);
		TestEqual(TEXT("passage roof at the eave"), P.PassageParams().GetEaveHeight(), 300.0);
		// An eave below the doorway head lifts to it, roof included.
		P.Passage.EaveHeight = 150.0;
		TestTrue(TEXT("eave lifted over the doorway"), P.GetPassageEaveHeight() > 150.0);
		TestEqual(TEXT("wall and roof still meet"), P.ClosingWallParams().GetHeight(), P.PassageParams().GetEaveHeight());
	}

	// A frontage narrower than the strip still lays out in order.
	for (const bool bFar : { false, true })
	{
		FHutongEarPassageParams P;
		P.Width = 300.0;
		P.bPassageAtFarEnd = bFar;
		TestTrue(TEXT("room keeps its minimum"), P.GetRoomWidth() >= FHutongEarPassageParams::MinRoomWidthCm - 1e-6);
		TestTrue(TEXT("room and clear way do not overlap"),
			bFar ? P.GetRoomX0() + P.GetRoomWidth() <= P.GetClearX0() + 1e-6
			     : P.GetClearX0() + P.GetClearWidth() <= P.GetRoomX0() + 1e-6);
		const FHutongPlanBays Bays = UHutongEarPassageBuildingComponent::PlanBaysInBuildFrame(P);
		for (int32 i = 1; i < Bays.Boundaries.Num(); ++i)
		{
			TestTrue(FString::Printf(TEXT("plan bays ascend (%d, far end %d)"), i, bFar), Bays.Boundaries[i] > Bays.Boundaries[i - 1]);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
