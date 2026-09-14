#include "Generation/CompoundLayout.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/CompoundTool.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Is the compound closed?
namespace
{
	// A run along one axis that some slot occupies.
	struct FSpan
	{
		double Min = 0.0;
		double Max = 0.0;
	};

	// The stretches of [EdgeMin, EdgeMax] that no span covers.
	TArray<FSpan> FindUncovered(TArray<FSpan> Spans, double EdgeMin, double EdgeMax,
		double Tolerance = 1.0)
	{
		TArray<FSpan> Holes;

		Spans.Sort([](const FSpan& A, const FSpan& B) { return A.Min < B.Min; });

		double Reached = EdgeMin;
		for (const FSpan& Span : Spans)
		{
			if (Span.Min > Reached + Tolerance)
			{
				Holes.Add({ Reached, Span.Min });
			}
			Reached = FMath::Max(Reached, Span.Max);
		}
		if (Reached < EdgeMax - Tolerance)
		{
			Holes.Add({ Reached, EdgeMax });
		}
		return Holes;
	}

	// Does this slot reach the given plot edge?
	bool TouchesEdge(const FHutongCompoundSlot& Slot, double EdgeCoord, bool bAlongX,
		double Tolerance = 1.0)
	{
		const double Near = bAlongX ? Slot.Min.Y : Slot.Min.X;
		const double Far = bAlongX ? Slot.Min.Y + Slot.Size.Y : Slot.Min.X + Slot.Size.X;
		return Near <= EdgeCoord + Tolerance && Far >= EdgeCoord - Tolerance;
	}

	FString DescribeHoles(const TArray<FSpan>& Holes)
	{
		FString Out;
		for (const FSpan& Hole : Holes)
		{
			Out += FString::Printf(TEXT("[%.0f..%.0f = %.0f cm] "), Hole.Min, Hole.Max,
				Hole.Max - Hole.Min);
		}
		return Out;
	}

	void CheckEdge(FAutomationTestBase& Test, const TArray<FHutongCompoundSlot>& Slots,
		const TCHAR* EdgeName, double EdgeCoord, bool bAlongX, double EdgeMin, double EdgeMax)
	{
		TArray<FSpan> Spans;
		for (const FHutongCompoundSlot& Slot : Slots)
		{
			// The 甬路 is paving and the furnishing is furniture; neither encloses anything.
			if (Slot.Piece == EHutongCompoundPiece::Path
				|| Slot.Piece == EHutongCompoundPiece::FlowerBed
				|| Slot.Piece == EHutongCompoundPiece::WaterJar
				|| Slot.Piece == EHutongCompoundPiece::Corridor)
			{
				continue;
			}
			if (!TouchesEdge(Slot, EdgeCoord, bAlongX))
			{
				continue;
			}
			Spans.Add(bAlongX
				? FSpan{ Slot.Min.X, Slot.Min.X + Slot.Size.X }
				: FSpan{ Slot.Min.Y, Slot.Min.Y + Slot.Size.Y });
		}

		const TArray<FSpan> Holes = FindUncovered(Spans, EdgeMin, EdgeMax);
		Test.TestTrue(
			FString::Printf(TEXT("the %s edge is continuous — holes: %s"), EdgeName,
				Holes.Num() ? *DescribeHoles(Holes) : TEXT("none")),
			Holes.Num() == 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundEnclosureTest,
	"HutongLayout.Compound.Enclosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundEnclosureTest::RunTest(const FString& Parameters)
{
	// Every plan, at a plot comfortably above the minimum so nothing is being dropped for space.
	for (const EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		// Off the plan's own minimum.
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		In.Width = MinW + 400.0;
		In.Depth = MinD + 400.0;

		const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		if (!TestTrue(TEXT("the plan produced something"), Slots.Num() > 0))
		{
			continue;
		}

		const TCHAR* PlanName =
			Plan == EHutongCompoundPlan::OneCourtyard ? TEXT("一進")
			: Plan == EHutongCompoundPlan::TwoCourtyards ? TEXT("二進") : TEXT("三進");
		AddInfo(FString::Printf(TEXT("--- %s plan, %.0f x %.0f, %d slots ---"),
			PlanName, In.Width, In.Depth, Slots.Num()));

		CheckEdge(*this, Slots, *FString::Printf(TEXT("%s street (south)"), PlanName),
			0.0, /*bAlongX*/ true, 0.0, In.Width);
		CheckEdge(*this, Slots, *FString::Printf(TEXT("%s north"), PlanName),
			In.Depth, /*bAlongX*/ true, 0.0, In.Width);
		CheckEdge(*this, Slots, *FString::Printf(TEXT("%s east"), PlanName),
			0.0, /*bAlongX*/ false, 0.0, In.Depth);
		CheckEdge(*this, Slots, *FString::Printf(TEXT("%s west"), PlanName),
			In.Width, /*bAlongX*/ false, 0.0, In.Depth);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundCourtWalkTest,
	"HutongLayout.Compound.CourtWalk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundCourtWalkTest::RunTest(const FString& Parameters)
{
	// A compound the size this plan lays out ordinarily sheltered its inner court with 前廊 on the 廂房.
	auto Plot = [](EHutongCourtWalk Walk, double& OutW, double& OutD)
	{
		HutongGen::FCompoundInput In;
		In.Plan = EHutongCompoundPlan::TwoCourtyards;
		In.CourtWalk = Walk;
		In.GetMinimumPlot(OutW, OutD);
		return In;
	};

	double VerW, VerD, RingW, RingD, NoneW, NoneD;
	Plot(EHutongCourtWalk::WingVerandas, VerW, VerD);
	Plot(EHutongCourtWalk::Corridor, RingW, RingD);
	Plot(EHutongCourtWalk::None, NoneW, NoneD);

	// The ring costs a band off all four sides; the verandas cost only their own depth, and only across the court.
	TestTrue(TEXT("the ring wants a wider plot than the verandas"), RingW > VerW + 1.0);
	TestTrue(TEXT("the ring wants a deeper plot than the verandas"), RingD > VerD + 1.0);
	TestTrue(TEXT("the verandas want no more depth than a bare court"), VerD <= NoneD + 0.01);

	// The wing itself: 前出廊 is 七檩 and the extra 步架 goes in front of the rooms.
	{
		FHutongSiheyuanParams Base;
		Base.Purlins = EHutongPurlins::Five;
		Base.StepRun = 112.0;
		Base.SuggestedDepth = 0.0;
		Base.SuggestedFrontage = 900.0;
		Base.Width = Base.SuggestedFrontage;

		const double Rooms = Base.GetSuggestedDepth();
		const FHutongSiheyuanParams Wing =
			HutongCompound::CourtWing(Base, EHutongCourtWalk::WingVerandas);

		TestTrue(TEXT("a verandaed wing has a 前廊"), Wing.bHasFrontVeranda);
		TestEqual(TEXT("a 前出廊 wing is 七檩"), int32(Wing.Purlins), int32(EHutongPurlins::Seven));
		TestEqual(TEXT("the veranda is one 步架"),
			Wing.GetVerandaDepth(), Wing.StepRun, 0.01);
		TestEqual(TEXT("the wing grows by exactly that step"),
			Wing.GetSuggestedDepth() - Wing.GetVerandaDepth(), Rooms, 0.5);

		// And the other two walks leave the preset alone.
		for (const EHutongCourtWalk Walk : { EHutongCourtWalk::Corridor, EHutongCourtWalk::None })
		{
			const FHutongSiheyuanParams Plain = HutongCompound::CourtWing(Base, Walk);
			TestFalse(TEXT("only the veranda walk gives the wing a 前廊"), Plain.bHasFrontVeranda);
			TestEqual(TEXT("and leaves its depth alone"),
				Plain.GetSuggestedDepth(), Rooms, 0.01);
		}
	}

	for (const EHutongCourtWalk Walk :
		{ EHutongCourtWalk::WingVerandas, EHutongCourtWalk::Corridor, EHutongCourtWalk::None })
	{
		HutongGen::FCompoundInput In;
		In.Plan = EHutongCompoundPlan::TwoCourtyards;
		In.CourtWalk = Walk;
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		In.Width = MinW + 400.0;
		In.Depth = MinD + 400.0;

		const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		if (!TestTrue(TEXT("the plan produced something"), Slots.Num() > 0)) continue;

		int32 Corridors = 0;
		for (const FHutongCompoundSlot& S : Slots)
		{
			if (S.Piece == EHutongCompoundPiece::Corridor) ++Corridors;
		}
		TestEqual(FString::Printf(TEXT("walk %d builds the ring only when asked"), int32(Walk)),
			Corridors > 0, Walk == EHutongCourtWalk::Corridor);
	}

	return true;
}

// The screen wall's length is the panel's, not the generator's drag default.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundScreenLengthTest,
	"HutongLayout.Compound.ScreenLength",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundScreenLengthTest::RunTest(const FString& Parameters)
{
	UHutongCompoundToolProperties* S = NewObject<UHutongCompoundToolProperties>();
	if (!TestNotNull(TEXT("the compound has settings"), S)) return false;

	auto ScreenRun = [&](double Length)
	{
		S->ScreenWallLength = Length;
		double W, D;
		UHutongCompoundTool::MakeInputFrom(S, 1.0, 1.0).GetSuggestedPlot(W, D);
		for (const FHutongCompoundSlot& Slot : HutongGen::LayOutCompound(UHutongCompoundTool::MakeInputFrom(S, W, D)))
		{
			if (Slot.Piece == EHutongCompoundPiece::ScreenWall)
			{
				return Slot.bLengthAlongY ? Slot.Size.Y : Slot.Size.X;
			}
		}
		return -1.0;
	};
	const double Short = ScreenRun(300.0);
	const double Long = ScreenRun(800.0);
	TestTrue(TEXT("the screen is laid out"), Short > 0.0 && Long > 0.0);
	TestTrue(TEXT("and the setting reaches it"), Long > Short + 1.0);
	// The drag field the generator carries is not what the compound reads.
	S->ScreenWall.Length = 1200.0;
	TestNearlyEqual(TEXT("the generator's own Length is ignored"), ScreenRun(300.0), Short, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundRearWindowTest,
	"HutongLayout.Compound.RearWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundRearWindowTest::RunTest(const FString& Parameters)
{
	// 高窗 belong to the rows whose backs are the street.
	UHutongCompoundToolProperties* S = NewObject<UHutongCompoundToolProperties>();
	if (!TestNotNull(TEXT("the compound has settings"), S)) return false;

	// The 門房 is built from the 倒座房's parameters, so it is covered by the same assertion.
	TestTrue(TEXT("the 正房 carries 高窗"), S->MainHall.bHasRearHighWindows);
	TestTrue(TEXT("the 倒座房 carries 高窗"), S->FrontRow.bHasRearHighWindows);
	TestTrue(TEXT("the 後罩房 carries 高窗"), S->RearRow.bHasRearHighWindows);

	// And the two whose backs are the neighbour's plot.
	TestFalse(TEXT("the 廂房 stays blank"), S->SideHouse.bHasRearHighWindows);
	TestFalse(TEXT("the 耳房 stays blank"), S->EarRoom.bHasRearHighWindows);

	// 後檐 follows the plan for the hall row.
	for (const EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		const bool bRearCourt = (Plan == EHutongCompoundPlan::ThreeCourtyards);
		const FHutongSiheyuanParams Hall =
			HutongCompound::CourtRow(S->MainHall, Plan, EHutongBaySide::MinusY);
		const FHutongSiheyuanParams HallEar =
			HutongCompound::CourtRow(S->EarRoom, Plan, EHutongBaySide::MinusY);
		// The 廂耳房 share the 耳房's parameters and face sideways onto the plot's own edge.
		const FHutongSiheyuanParams WingEar =
			HutongCompound::CourtRow(S->EarRoom, Plan, EHutongBaySide::PlusX);

		const EHutongRearEave Want =
			bRearCourt ? EHutongRearEave::Courtyard : EHutongRearEave::Lane;
		TestEqual(TEXT("the 正房's rear eave follows the plan"), int32(Hall.RearEave), int32(Want));
		TestEqual(TEXT("the hall's 耳房 follow it too"), int32(HallEar.RearEave), int32(Want));
		TestEqual(TEXT("the 廂耳房 keep their 封護檐"),
			int32(WingEar.RearEave), int32(EHutongRearEave::Lane));
	}

	// And in the mesh: a courtyard back oversails its own wall the way a front does, where a lane back stops on it.
	{
		const double SX = 1040.0;
		const double SY = FMath::Max(S->MainHall.GetSuggestedDepth(), 200.0);

		auto RearReach = [&](EHutongCompoundPlan Plan)
		{
			FHutongSiheyuanParams P =
				HutongCompound::CourtRow(S->MainHall, Plan, EHutongBaySide::MinusY);
			P.Width = SX;
			P.Depth = SY;

			UE::Geometry::FDynamicMesh3 M;
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				P, EHutongBaySide::MinusY, 0, SX, SY, M);

			double Far = -BIG_NUMBER;
			for (int32 vid : M.VertexIndicesItr()) Far = FMath::Max(Far, M.GetVertex(vid).Y);
			return Far - SY;
		};

		const double Lane = RearReach(EHutongCompoundPlan::TwoCourtyards);
		const double Court = RearReach(EHutongCompoundPlan::ThreeCourtyards);
		TestTrue(FString::Printf(
			TEXT("the 三進 hall's eave oversails its back wall (%.0f cm against %.0f)"), Court, Lane),
			Court > Lane + 30.0);
	}

	// In the mesh, not only in the flag: built at the footprint the plan gives it, the row's back wall really is pierced.
	{
		const double SX = 1300.0;
		const double SY = FMath::Max(S->FrontRow.GetSuggestedDepth(), 200.0);

		auto Build = [&](bool bWindows, UE::Geometry::FDynamicMesh3& M)
		{
			FHutongSiheyuanParams P = S->FrontRow;
			P.bHasRearHighWindows = bWindows;
			P.Width = SX;
			P.Depth = SY;
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				P, EHutongBaySide::PlusY, 0, SX, SY, M);
		};

		UE::Geometry::FDynamicMesh3 Open, Blank;
		Build(true, Open);
		Build(false, Blank);
		TestTrue(TEXT("the laid-out 倒座房's back wall is pierced"),
			Open.TriangleCount() > Blank.TriangleCount());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundPlotSizeTest,
	"HutongLayout.Compound.PlotSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundPlotSizeTest::RunTest(const FString& Parameters)
{
	// The minimum is a floor, not a default. Laid out on it every court comes out the least it can be and the compound reads as cramped.
	auto Measure = [&](EHutongCompoundPlan Plan)
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;

		double MinW, MinD, WantW, WantD;
		In.GetMinimumPlot(MinW, MinD);
		In.GetSuggestedPlot(WantW, WantD);

		UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(
			TEXT("compound: plan %d — minimum %.1f x %.1f m, ordinary %.1f x %.1f m"),
			int32(Plan), MinW * 0.01, MinD * 0.01, WantW * 0.01, WantD * 0.01));

		TestTrue(TEXT("the ordinary plot is at least the minimum"),
			WantW >= MinW - 0.01 && WantD >= MinD - 0.01);
		TestTrue(TEXT("the ordinary plot is bigger than the minimum"),
			WantD > MinD + 100.0);

		In.Width = WantW;
		In.Depth = WantD;
		return In;
	};

	Measure(EHutongCompoundPlan::OneCourtyard);
	Measure(EHutongCompoundPlan::TwoCourtyards);
	const HutongGen::FCompoundInput In = Measure(EHutongCompoundPlan::ThreeCourtyards);

	// The figure this is all measured against.
	TestTrue(FString::Printf(TEXT("a 三進 stands 50–60 m deep (%.1f m)"), In.Depth * 0.01),
		In.Depth >= 4700.0 && In.Depth <= 6200.0);

	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
	if (!TestTrue(TEXT("the plan produced something"), Slots.Num() > 0)) return false;

	auto Find = [&Slots](EHutongCompoundPiece Piece) -> const FHutongCompoundSlot*
	{
		return Slots.FindByPredicate([Piece](const FHutongCompoundSlot& S)
			{ return S.Piece == Piece; });
	};

	const FHutongCompoundSlot* Hall = Find(EHutongCompoundPiece::MainHall);
	const FHutongCompoundSlot* Gate = Find(EHutongCompoundPiece::InnerGate);
	const FHutongCompoundSlot* Rear = Find(EHutongCompoundPiece::RearRow);
	const FHutongCompoundSlot* Wing = Find(EHutongCompoundPiece::SideHouse);
	if (!TestTrue(TEXT("the pieces the courts are measured between are all there"),
		Hall && Gate && Rear && Wing))
	{
		return false;
	}

	// The three courts, each measured between the buildings that face it.
	const double InnerDepth = Hall->Min.Y - (Gate->Min.Y + Gate->Size.Y);
	const double RearDepth = Rear->Min.Y - (Hall->Min.Y + Hall->Size.Y);
	const double InnerWidth = In.Width - 2.0 * Wing->Size.X;

	UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(
		TEXT("compound: 內院 %.1f x %.1f m, 後院 %.1f m deep, 廂房 %.1f m long"),
		InnerWidth * 0.01, InnerDepth * 0.01, RearDepth * 0.01, Wing->Size.Y * 0.01));

	// A court is a room.
	TestTrue(FString::Printf(TEXT("the 內院 is a court, not a passage (%.1f m wide)"),
		InnerWidth * 0.01), InnerWidth >= 1200.0);
	TestTrue(FString::Printf(TEXT("the 內院 is not a canyon (%.1f deep by %.1f wide)"),
		InnerDepth * 0.01, InnerWidth * 0.01), InnerDepth <= 2.2 * InnerWidth);
	TestTrue(FString::Printf(TEXT("the 後院 keeps a real depth (%.1f m)"), RearDepth * 0.01),
		RearDepth >= In.RearCourtDepth - 1.0);

	// And the wing has first claim on the range it stands in.
	const TArray<FHutongCompoundSlot> Ears = Slots.FilterByPredicate(
		[](const FHutongCompoundSlot& S)
		{
			return S.Piece == EHutongCompoundPiece::EarRoom && S.Size.Y < S.Size.X;
		});
	for (const FHutongCompoundSlot& Ear : Ears)
	{
		TestTrue(FString::Printf(TEXT("a 廂耳房 (%.1f m) stays shorter than its 廂房 (%.1f m)"),
			Ear.Size.Y * 0.01, Wing->Size.Y * 0.01), Ear.Size.Y <= Wing->Size.Y + 1.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundDoorwayTest,
	"HutongLayout.Compound.Doorways",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundDoorwayTest::RunTest(const FString& Parameters)
{
	// Every doorway the plan asks for has to be a doorway in the mesh.
	for (const EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		// The tight case is the one worth testing.
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		In.Width = MinW;
		In.Depth = MinD;

		const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		if (!TestTrue(TEXT("the plan produced something"), Slots.Num() > 0))
		{
			continue;
		}

		const TCHAR* PlanName =
			Plan == EHutongCompoundPlan::OneCourtyard ? TEXT("一進")
			: Plan == EHutongCompoundPlan::TwoCourtyards ? TEXT("二進") : TEXT("三進");

		int32 Doorways = 0;
		for (const FHutongCompoundSlot& Slot : Slots)
		{
			if (Slot.Piece != EHutongCompoundPiece::Wall || Slot.WallGateAt < 0.0) continue;
			++Doorways;

			const double Run = Slot.bLengthAlongY ? Slot.Size.Y : Slot.Size.X;
			FHutongWallParams P;
			P.Role = Slot.WallRole;
			P.Length = Run;
			HutongGen::ApplySlotDoorway(P, Slot, Run);

			const FString Where = FString::Printf(
				TEXT("%s doorway on the %.0f cm run at (%.0f, %.0f)"),
				PlanName, Run, Slot.Min.X, Slot.Min.Y);

			TestFalse(Where + TEXT(" carries no raised hood"), P.bHasGate);
			TestTrue(Where + TEXT(" is actually cut"), P.HasDecorativeDoorway(Run));
			TestTrue(Where + TEXT(" is wide enough to walk through"),
				P.GetDoorwayWidth() >= 89.0);

			// And in the mesh, not only in the params: a blank run and a run with a doorway in it cannot come out the same size.
			UE::Geometry::FDynamicMesh3 Open, Blank;
			HutongGen::BuildWall(Open, P);
			FHutongWallParams Q = P;
			Q.Doorway = EHutongWallDoorway::None;
			HutongGen::BuildWall(Blank, Q);
			TestNotEqual(Where + TEXT(" changes the mesh"),
				Open.TriangleCount(), Blank.TriangleCount());
		}

		UE_LOG(LogTemp, Display, TEXT("%s"),
			*FString::Printf(TEXT("compound: %s cuts %d internal doorways at its minimum plot"),
				PlanName, Doorways));
		TestTrue(FString::Printf(TEXT("%s cuts at least one internal doorway"), PlanName),
			Doorways > 0 || Plan == EHutongCompoundPlan::OneCourtyard);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundNoOverlapTest,
	"HutongLayout.Compound.NoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The plan degrades by dropping pieces, never by overlapping them — and the 門道院 is where that
// was broken: with no cross wall to back onto, the 影壁 compartment was laid through the 廂房.
bool FHutongCompoundNoOverlapTest::RunTest(const FString& Parameters)
{
	auto IsBuilding = [](EHutongCompoundPiece P)
	{
		switch (P)
		{
		case EHutongCompoundPiece::MainHall:
		case EHutongCompoundPiece::EarRoom:
		case EHutongCompoundPiece::SideHouse:
		case EHutongCompoundPiece::FrontRow:
		case EHutongCompoundPiece::RearRow:
		case EHutongCompoundPiece::GateHouse:
		case EHutongCompoundPiece::GateLodge:
		case EHutongCompoundPiece::InnerGate:
		case EHutongCompoundPiece::ScreenWall:
			return true;
		default:
			return false;
		}
	};

	for (const EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		const TCHAR* PlanName =
			Plan == EHutongCompoundPlan::OneCourtyard ? TEXT("一進")
			: Plan == EHutongCompoundPlan::TwoCourtyards ? TEXT("二進") : TEXT("三進");

		// The tight case and a roomy one: the compartment moves with the plot.
		for (const double Extra : { 0.0, 400.0, 1200.0 })
		{
			HutongGen::FCompoundInput In;
			In.Plan = Plan;
			double MinW, MinD;
			In.GetMinimumPlot(MinW, MinD);
			In.Width = MinW + Extra;
			In.Depth = MinD + Extra;

			const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
			if (!TestTrue(TEXT("the plan produced something"), Slots.Num() > 0)) continue;

			// The compartment giving way to the gate must not be how it stops overlapping it.
			int32 Screens = 0;
			for (const FHutongCompoundSlot& S : Slots)
			{
				if (S.Piece == EHutongCompoundPiece::ScreenWall) ++Screens;
			}
			UE_LOG(LogTemp, Display, TEXT("compound overlap: %s at +%.0f (%.0f x %.0f), %d 影壁"),
				PlanName, Extra, In.Width, In.Depth, Screens);
			TestEqual(FString::Printf(TEXT("%s at +%.0f still builds its 影壁"), PlanName, Extra),
				Screens, 1);

			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				if (!IsBuilding(Slots[i].Piece)) continue;
				for (int32 j = i + 1; j < Slots.Num(); ++j)
				{
					if (!IsBuilding(Slots[j].Piece)) continue;

					// Abutting face to face is the safe arrangement; only real area is a fault.
					const double OverX = FMath::Min(Slots[i].Min.X + Slots[i].Size.X,
						Slots[j].Min.X + Slots[j].Size.X) - FMath::Max(Slots[i].Min.X, Slots[j].Min.X);
					const double OverY = FMath::Min(Slots[i].Min.Y + Slots[i].Size.Y,
						Slots[j].Min.Y + Slots[j].Size.Y) - FMath::Max(Slots[i].Min.Y, Slots[j].Min.Y);

					TestTrue(FString::Printf(
						TEXT("%s at +%.0f: piece %d and piece %d do not overlap (%.0f x %.0f cm)"),
						PlanName, Extra, (int32)Slots[i].Piece, (int32)Slots[j].Piece, OverX, OverY),
						OverX <= 1.0 || OverY <= 1.0);
				}
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
