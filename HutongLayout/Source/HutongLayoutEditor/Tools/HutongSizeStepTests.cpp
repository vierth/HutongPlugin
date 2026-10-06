#include "HutongLayoutModeSettings.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSizeStepTest,
	"HutongLayout.Snap.SizeStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSizeStepTest::RunTest(const FString& Parameters)
{
	// Two hands drawing one kind of building a few centimetres apart land on one size (so one mesh); the
	// steps are the 營造尺's; a length never rounds to nothing; a negative (leftward) drag keeps its side.
	const double Step = HutongSizeStep::Cm(EHutongSizeStep::FiveCun);
	TestEqual(TEXT("five cun is 16 cm"), Step, 16.0);
	TestEqual(TEXT("1004 cm"), HutongSizeStep::Round(1004.0, Step), 1008.0);
	TestEqual(TEXT("1011 cm, the same building"), HutongSizeStep::Round(1011.0, Step), 1008.0);
	TestEqual(TEXT("never under one step"), HutongSizeStep::Round(3.0, Step), 16.0);
	TestEqual(TEXT("leftward drag"), HutongSizeStep::Round(-1011.0, Step), -1008.0);
	TestEqual(TEXT("off leaves it"), HutongSizeStep::Round(1011.0, HutongSizeStep::Cm(EHutongSizeStep::Off)), 1011.0);
	TestEqual(TEXT("one chi"), HutongSizeStep::Cm(EHutongSizeStep::Chi), 32.0);
	TestEqual(TEXT("one zhang"), HutongSizeStep::Cm(EHutongSizeStep::Zhang), 320.0);
	TestEqual(TEXT("default five cun"), GetDefault<UHutongLayoutModeSettings>()->SizeStep, EHutongSizeStep::FiveCun);
	return true;
}

#endif
