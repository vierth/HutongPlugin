#include "Generation/HutongGateRow.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/GateHouseGenerator.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// A gate set into a traced street row takes the row's depth and stands above its eave.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGateMatchRowTest,
	"HutongLayout.Gates.MatchRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGateMatchRowTest::RunTest(const FString& Parameters)
{
	using namespace HutongGen::GateRow;

	// A 12 x 6 m row: its depth is behind its own facade, whichever side that is, and never its run.
	const FVector2D Row(1200.0, 600.0);
	TestEqual(TEXT("facade along X: depth is Y"), RowDepth(Row, true, true), 600.0);
	TestEqual(TEXT("facade along Y: depth is X"), RowDepth(FVector2D(600.0, 1200.0), true, false), 600.0);
	TestEqual(TEXT("no facade: the shorter side"), RowDepth(Row, false, false), 600.0);
	TestEqual(TEXT("the run bearing is the actor's when the facade runs along X"), RunYawDeg(30.0, true), 30.0);
	TestEqual(TEXT("and a quarter turn on when it runs along Y"), RunYawDeg(30.0, false), 120.0);

	// A wall met end-on is not a row, whatever it reports for an eave.
	TestTrue(TEXT("a house is a row"), IsRow(279.0, 600.0));
	TestFalse(TEXT("a wall run is not"), IsRow(0.0, 37.0));
	TestFalse(TEXT("nor a piece with no eave"), IsRow(0.0, 600.0));

	// The gate's ridge is lifted clear of the row's, past the style's own eave ceiling, and a
	// gate already clear is left where it stands.
	{
		FHutongGateHouseParams Ruyi;
		const double Before = Ruyi.GetEaveHeight();
		LiftGateAboveRidge(Ruyi, 600.0, 520.0);
		TestNearlyEqual(TEXT("ridge clears the row's by the canon figure"),
			HutongGen::Ridge::Gate(Ruyi, 600.0), 520.0 + HutongCanon::Gate::RidgeAboveRowCm, 0.01);
		TestTrue(TEXT("past the 如意門 band's ceiling"), Ruyi.GetEaveHeight() > HutongCanon::Gate::RuyiSize.EaveMax);
		FHutongGateHouseParams Tall = Ruyi;
		LiftGateAboveRidge(Tall, 600.0, 100.0);
		TestNearlyEqual(TEXT("a gate already clear is not lowered"), Tall.GetEaveHeight(), Ruyi.GetEaveHeight(), 0.01);
		FHutongGateHouseParams Alone;
		LiftGateAboveRidge(Alone, 600.0, 0.0);
		TestNearlyEqual(TEXT("no row leaves the gate alone"), Alone.GetEaveHeight(), Before, 0.01);
	}

	// The components answer for the row: a house reports the eave its footprint derives, a wall reports none.
	UHutongSiheyuanBuildingComponent* House =
		NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	House->FootprintX = 1200.0;
	House->FootprintY = 600.0;
	House->BaySide = EHutongBaySide::MinusY;
	TestTrue(TEXT("a house reports an eave"), House->GetEaveHeight() > 200.0);
	FHutongSiheyuanParams P = House->Params;
	P.Width = 1200.0;
	P.Depth = 600.0;
	TestNearlyEqual(TEXT("and it is the eave its footprint derives"),
		House->GetEaveHeight(), P.GetEaveHeight(), 0.01);
	TestTrue(TEXT("its ridge stands above its eave"), House->GetRidgeHeight() > House->GetEaveHeight() + 50.0);
	TestNearlyEqual(TEXT("and is the ridge its footprint derives"),
		House->GetRidgeHeight(), HutongGen::Ridge::House(P, 1200.0, 600.0), 0.01);

	UHutongWallBuildingComponent* Wall = NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	TestEqual(TEXT("a wall reports none"), Wall->GetEaveHeight(), 0.0);
	TestEqual(TEXT("nor a ridge"), Wall->GetRidgeHeight(), 0.0);

	return true;
}

#endif
