#include "Misc/AutomationTest.h"
#include "Generation/EarPassageGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Tools/HutongPresets.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongPanelFilter.h"
#include "Tools/HutongContextMenu.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

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

// The ear room tool's three presets, and a one-bay room taken whole as the passage.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongEarRoomKindsTest,
	"HutongLayout.EarPassage.Kinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongEarRoomKindsTest::RunTest(const FString&)
{
	const TArray<FString>& Names = HutongPresets::BuiltInEarRoomNames();
	TestEqual(TEXT("three ear room presets"), Names.Num(), 3);
	TestFalse(TEXT("the house tool no longer lists the ear room"),
		HutongPresets::BuiltInSiheyuanNames().Contains(TEXT("Ear Room (耳房)")));
	TestTrue(TEXT("the ear room tool opens on a preset it has"), Names.Contains(HutongPresets::DefaultEarRoomName()));

	int32 NoPassage = 0, Passage = 0, NoDoor = 0;
	for (const FString& Name : Names)
	{
		FHutongEarPassageParams P;
		if (!TestTrue(FString::Printf(TEXT("%s loads"), *Name), UHutongPresetLibrary::Get()->LoadPreset(
				TEXT("EarPassage"), Name, FHutongEarPassageParams::StaticStruct(), &P))) continue;
		NoPassage += P.Passageway == EHutongEarPassage::None ? 1 : 0;
		Passage += P.Passageway == EHutongEarPassage::AtEnd ? 1 : 0;
		NoDoor += P.Room.bHasFrontDoorCenter ? 0 : 1;
		P.Width = P.GetSuggestedWidth();
		P.Depth = P.Room.GetSuggestedDepth();
		FDynamicMesh3 Mesh;
		HutongGen::BuildEarPassage(Mesh, P);
		TestTrue(FString::Printf(TEXT("%s builds"), *Name), Mesh.TriangleCount() > 0);
		const FHutongPlanBays Bays = UHutongEarPassageBuildingComponent::PlanBaysInBuildFrame(P);
		TestEqual(FString::Printf(TEXT("%s: a door bay on the plan only with a door"), *Name),
			Bays.DoorBay != INDEX_NONE, P.Room.bHasFrontDoorCenter || P.HasClosingDoorway());
	}
	TestEqual(TEXT("one without a passage, with its door"), NoPassage - NoDoor, 1);
	TestEqual(TEXT("one with the passage"), Passage, 1);
	TestEqual(TEXT("one shut to the court"), NoDoor, 1);

	// No passage: the room takes the whole frontage, nothing else is built.
	{
		FHutongEarPassageParams P;
		P.Passageway = EHutongEarPassage::None;
		P.Width = 600.0;
		TestEqual(TEXT("room is the frontage"), P.GetRoomWidth(), 600.0);
		TestEqual(TEXT("room at the origin"), P.GetRoomX0(), 0.0);
		TestFalse(TEXT("no closing doorway"), P.HasClosingDoorway());
		TestFalse(TEXT("the room's roof does not run on"), P.RoomParams().bRoofRunsOnLow || P.RoomParams().bRoofRunsOnHigh);
	}

	// Whole frontage: the passage, walled both sides, is the building; the drag sets its clear way.
	for (const bool bFar : { false, true })
	{
		FHutongEarPassageParams P;
		P.Passageway = EHutongEarPassage::Whole;
		P.bPassageAtFarEnd = bFar;
		P.Width = 320.0;
		const double T = P.OuterWall.GetThickness();
		TestEqual(TEXT("frontage as dragged"), P.GetWidth(), 320.0);
		TestEqual(TEXT("no room"), P.GetRoomWidth(), 0.0);
		TestNearlyEqual(TEXT("clear way between the two walls"), P.GetClearWidth(), 320.0 - 2.0 * T, 1e-6);
		TestNearlyEqual(TEXT("outer wall at its end"), P.GetOuterWallX0(), bFar ? 320.0 - T : 0.0, 1e-6);
		TestNearlyEqual(TEXT("inner wall at the other"), P.GetInnerWallX0(), bFar ? 0.0 : 320.0 - T, 1e-6);
		TestNearlyEqual(TEXT("clear way between them"), P.GetClearX0(), T, 1e-6);
		TestTrue(TEXT("its doorway is cut"), P.HasClosingDoorway());
		TestEqual(TEXT("eave is a one-bay ear room's"), P.RoomParams().GetBayCount(), 1);
		const FHutongPlanBays Bays = UHutongEarPassageBuildingComponent::PlanBaysInBuildFrame(P);
		TestEqual(TEXT("one bay on the plan"), Bays.Boundaries.Num(), 2);
		// Narrower than the doorway needs: held open, not walled shut.
		P.Width = 50.0;
		TestTrue(TEXT("a narrow drag keeps the doorway"), P.HasClosingDoorway() && P.GetWidth() > 50.0);
		for (const bool bOwnRoof : { false, true })
		{
			P.Width = 320.0;
			P.Depth = 340.0;
			P.bRoofOverPassage = !bOwnRoof;
			FDynamicMesh3 Mesh;
			HutongGen::BuildEarPassage(Mesh, P);
			TestTrue(TEXT("whole-frontage passage builds"), Mesh.TriangleCount() > 0);
			TestTrue(TEXT("stays inside its frontage"), Mesh.GetBounds().Min.X >= -60.0 && Mesh.GetBounds().Max.X <= 320.0 + 60.0);
		}
	}

	// One or two bays: a wide drag holds at two, and the bay keys step a placed one within that.
	{
		UHutongEarPassageBuildingComponent* Ear = NewObject<UHutongEarPassageBuildingComponent>(GetTransientPackage());
		Ear->Params.Passageway = EHutongEarPassage::None;
		Ear->FootprintX = 1100.0;
		Ear->FootprintY = 340.0;
		TestEqual(TEXT("a wide drag holds at two bays"), Ear->GetDrawnBayCount(), 2);
		TestTrue(TEXT("the bay keys reach it"), HutongDetailOps::AdjustBays({ Ear }, -1));
		TestEqual(TEXT("down to one"), Ear->GetDrawnBayCount(), 1);
		HutongDetailOps::AdjustBays({ Ear }, 1);
		HutongDetailOps::AdjustBays({ Ear }, 1);
		TestEqual(TEXT("never past two"), Ear->GetDrawnBayCount(), 2);
		TestFalse(TEXT("an ear room neither divides nor fuses"), Ear->CanDivideOrFuse());
	}

	// The room's house settings that ear rooms do not have stay out of every view.
	{
		const FProperty* RoomProp = FHutongEarPassageParams::StaticStruct()->FindPropertyByName(TEXT("Room"));
		const FProperty* Veranda = FHutongSiheyuanParams::StaticStruct()->FindPropertyByName(TEXT("bHasFrontVeranda"));
		const FProperty* Door = FHutongSiheyuanParams::StaticStruct()->FindPropertyByName(TEXT("bHasFrontDoorCenter"));
		const TArray<const FProperty*> Parents = { RoomProp };
		TestFalse(TEXT("no veranda on an ear room, even advanced"), HutongPanel::IsVisible(*Veranda, Parents, true));
		TestTrue(TEXT("its front door shows in the simple view"), HutongPanel::IsVisible(*Door, Parents, false));
	}

	// Right-click Options: the room's and the passage's choices, none of the walls'.
	{
		const TArray<FString> Labels = HutongContextMenu::OptionLabels(NewObject<UHutongEarPassageBuildingComponent>(GetTransientPackage()));
		TestTrue(TEXT("the room's front door is offered"), Labels.ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("Has Front Door")); }));
		TestTrue(TEXT("the passage is offered"), Labels.ContainsByPredicate([](const FString& L) { return L.StartsWith(TEXT("Passage")); }));
		TestFalse(TEXT("no wall's choices"), Labels.ContainsByPredicate([](const FString& L) { return L.StartsWith(TEXT("Outer Wall")) || L.StartsWith(TEXT("Closing Wall")); }));
		TestFalse(TEXT("no veranda"), Labels.ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("Veranda")); }));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
