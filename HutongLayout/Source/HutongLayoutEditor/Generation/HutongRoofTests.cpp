#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongMeshInspect.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Generation/HallGenerator.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/ShopfrontGenerator.h"
#include "Generation/WallGenerator.h"
#include "Generation/InnerGateGenerator.h"
#include "Generation/CompoundLayout.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/GalleryTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPresets.h"
#include "Generation/HutongUrban.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The roof primitives have to be closed, two-manifold and wound outward.
namespace
{
	using UE::Geometry::FDynamicMesh3;

	// Both now live in Generation/HutongMeshInspect.h.
	using HutongMeshInspect::FShellReport;
	using HutongMeshInspect::InspectShell;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongXieshanRoofTest,
	"HutongLayout.Roofs.Xieshan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongXieshanRoofTest::RunTest(const FString& Parameters)
{
	using namespace HutongMeshUtils;

	struct FCase
	{
		const TCHAR* Name;
		FXieshanRoofSpec Spec;
	};

	auto Base = []()
	{
		FXieshanRoofSpec S;
		S.Width = 620.0; S.Depth = 420.0; S.Rise = 190.0;
		S.Section = HutongGen::Jiajia::MakeSection(EHutongPurlins::Five, 210.0, 60.0, 0.0);
		S.ShouInset = 95.0; S.FasciaDrop = 9.0;
		S.SlopeSegments = 6; S.EaveSegments = 6;
		return S;
	};

	TArray<FCase> Cases;
	Cases.Add({ TEXT("plain"), Base() });

	{
		FXieshanRoofSpec S = Base();
		S.FlareLength = 130.0; S.FlareRun = 55.0; S.FlareRise = 40.0;
		Cases.Add({ TEXT("with 翼角"), S });
	}
	{
		// 收山 driven to both ends of its clamp.
		FXieshanRoofSpec S = Base();
		S.ShouInset = 1.0;
		Cases.Add({ TEXT("收山 at the minimum"), S });
	}
	{
		FXieshanRoofSpec S = Base();
		S.ShouInset = 10000.0;
		Cases.Add({ TEXT("收山 at the maximum"), S });
	}
	{
		// Square plan, and a flare pushed into its clamps on the shorter eave.
		FXieshanRoofSpec S = Base();
		S.Width = 400.0; S.Depth = 400.0;
		S.FlareLength = 10000.0; S.FlareRun = 10000.0; S.FlareRise = 60.0;
		Cases.Add({ TEXT("square, flare clamped"), S });
	}
	{
		FXieshanRoofSpec S = Base();
		S.FasciaDrop = 0.0;
		Cases.Add({ TEXT("no fascia"), S });
	}
	{
		// Odd segment counts, so the 收山 row lands off a natural division.
		FXieshanRoofSpec S = Base();
		S.SlopeSegments = 7; S.EaveSegments = 3; S.ShouInset = 61.0;
		Cases.Add({ TEXT("odd segments"), S });
	}
	{
		// 捲棚歇山: the rakes arrive at the crown horizontally and meet in an arc.
		FXieshanRoofSpec S = Base();
		S.Section.ApexRoll = 0.35;
		S.FlareLength = 130.0; S.FlareRun = 55.0; S.FlareRise = 40.0;
		Cases.Add({ TEXT("捲棚 crown"), S });
	}
	{
		// A roll deep enough to reach past the 收山 line and round the top of the hipped skirt too.
		FXieshanRoofSpec S = Base();
		S.Section.ApexRoll = 0.95; S.ShouInset = 40.0;
		Cases.Add({ TEXT("捲棚 past 收山"), S });
	}

	for (const FCase& C : Cases)
	{
		FDynamicMesh3 Mesh;
		AppendXieshanRoof(Mesh, FVector3d::Zero(), C.Spec);
		const FShellReport R = InspectShell(Mesh);

		TestTrue(FString::Printf(TEXT("%s: builds triangles"), C.Name), R.Triangles > 0);
		TestEqual(FString::Printf(TEXT("%s: every edge is matched (closed, two-manifold)"), C.Name),
			R.Unmatched, 0);
		TestEqual(FString::Printf(TEXT("%s: no directed edge emitted twice (consistent winding)"), C.Name),
			R.Duplicated, 0);
		TestTrue(FString::Printf(TEXT("%s: positive volume (wound outward)"), C.Name), R.Volume > 0.0);
	}

	// 勾頭 round a flared eave: a hundred separate closed solids sitting in the fascia band, which must not disturb the shell they hang off.
	{
		FXieshanRoofSpec S = Base();
		S.FlareLength = 130.0; S.FlareRun = 55.0; S.FlareRise = 40.0;
		S.bEaveCaps = true; S.TileRowSpacing = 20.0;

		FDynamicMesh3 Mesh;
		AppendXieshanRoof(Mesh, FVector3d::Zero(), S);
		const FShellReport R = InspectShell(Mesh);

		FXieshanRoofSpec Plain = Base();
		Plain.FlareLength = 130.0; Plain.FlareRun = 55.0; Plain.FlareRise = 40.0;
		FDynamicMesh3 Bare;
		AppendXieshanRoof(Bare, FVector3d::Zero(), Plain);

		TestTrue(TEXT("勾頭: the row adds geometry"), Mesh.TriangleCount() > Bare.TriangleCount());
		TestEqual(TEXT("勾頭: every edge is matched"), R.Unmatched, 0);
		TestEqual(TEXT("勾頭: consistent winding"), R.Duplicated, 0);
		TestTrue(TEXT("勾頭: positive volume"), R.Volume > 0.0);
	}

	// The strips are separate closed solids that merely touch the shell.
	{
		FXieshanRoofSpec S = Base();
		S.FlareLength = 120.0; S.FlareRun = 45.0; S.FlareRise = 35.0;
		S.RidgeWidth = 14.0; S.RidgeHeight = 20.0;
		S.BargeThickness = 6.0; S.BargeDepth = 26.0;

		FDynamicMesh3 Mesh;
		AppendXieshanRoof(Mesh, FVector3d::Zero(), S);
		const FShellReport R = InspectShell(Mesh);

		TestEqual(TEXT("正脊/垂脊/戧脊 + 博風板: every edge is matched"), R.Unmatched, 0);
		TestEqual(TEXT("正脊/垂脊/戧脊 + 博風板: consistent winding"), R.Duplicated, 0);
		TestTrue(TEXT("正脊/垂脊/戧脊 + 博風板: positive volume"), R.Volume > 0.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHallTest,
	"HutongLayout.Roofs.Hall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHallTest::RunTest(const FString& Parameters)
{
	// Not a closure test — a building is deliberately a heap of overlapping solids.
	const double Plans[][2] = { {900.0, 620.0}, {380.0, 380.0}, {2400.0, 700.0}, {320.0, 900.0} };
	const EHutongRoofType Roofs[] = {
		EHutongRoofType::Xieshan, EHutongRoofType::Wudian, EHutongRoofType::Cuanjian };

	for (const double* Plan : Plans)
	{
		for (EHutongRoofType Roof : Roofs)
		{
			FHutongHallParams P;
			P.Width = Plan[0];
			P.Depth = Plan[1];
			P.RoofType = Roof;

			UE::Geometry::FDynamicMesh3 Mesh;
			HutongGen::BuildHall(Mesh, P);
			TestTrue(FString::Printf(TEXT("hall %.0fx%.0f roof %d builds"),
				Plan[0], Plan[1], (int32)Roof), Mesh.TriangleCount() > 100);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongJiajiaTest,
	"HutongLayout.Roofs.Jiajia",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongJiajiaTest::RunTest(const FString& Parameters)
{
	using namespace HutongGen;

	// 五檁小式 at a 100 cm 步架: 檐步五舉 then 脊步七舉.
	{
		const FHutongRoofSection S = Jiajia::MakeSectionWithRatios({ 0.5, 0.7 }, 200.0, 0.0, 0.0);
		TestEqual(TEXT("五檁: half span is the two 步架"), S.HalfSpan(), 200.0);
		// 100 x 0.5 + 100 x 0.7. The rise is a consequence, and this is the sum it comes from.
		TestEqual(TEXT("五檁: rise is 舉 x run, summed"), S.Rise(), 120.0);
		TestEqual(TEXT("五檁: eave sits on the eave line"), S.HeightAtDistanceFromRidge(200.0), 0.0);
		// The crease between 檐步 and 脊步, at half the span: 100 cm of run at 五舉.
		TestEqual(TEXT("五檁: the 步架 crease is at 五舉"), S.HeightAtDistanceFromRidge(100.0), 50.0);
		TestEqual(TEXT("五檁: ridge is the full rise"), S.HeightAtDistanceFromRidge(0.0), 120.0);
		// Halfway up the eave step: straight, so exactly half of it. A smooth curve would not be.
		TestEqual(TEXT("五檁: the 檐步 is straight"), S.HeightAtDistanceFromRidge(150.0), 25.0);
	}

	// The eave overhang is a leading segment carrying the eave step's own 舉.
	{
		const FHutongRoofSection S = Jiajia::MakeSectionWithRatios({ 0.5, 0.7 }, 200.0, 60.0, 0.0);
		TestEqual(TEXT("overhang: adds to the span"), S.HalfSpan(), 260.0);
		TestEqual(TEXT("overhang: adds its own run at 五舉"), S.Rise(), 150.0);
		TestEqual(TEXT("overhang: the 檐檁 sits at the overhang's rise"),
			S.HeightAtDistanceFromRidge(200.0), 30.0);
	}

	// Depth is (檁數 - 1) x 步架 — the whole reason the purlin count is the thing being set.
	TestEqual(TEXT("五檁 depth"), Jiajia::DepthFor(EHutongPurlins::Five, 100.0), 400.0);
	TestEqual(TEXT("七檁 depth"), Jiajia::DepthFor(EHutongPurlins::Seven, 100.0), 600.0);
	TestEqual(TEXT("三檁 is one 步架 a side"), Jiajia::StepsPerSide(EHutongPurlins::Three), 1);

	// Monotonic all the way up, rolled or not.
	for (double Roll : { 0.0, 0.35, 0.9 })
	{
		const FHutongRoofSection S =
			Jiajia::MakeSection(EHutongPurlins::Seven, 300.0, 70.0, Roll);
		double Last = -1.0;
		for (int32 i = 0; i <= 40; ++i)
		{
			const double D = S.HalfSpan() * (1.0 - (double)i / 40.0);   // eave -> ridge
			const double Z = S.HeightAtDistanceFromRidge(D);
			TestTrue(FString::Printf(TEXT("roll %.2f: rises toward the ridge"), Roll), Z >= Last - 1e-9);
			Last = Z;
		}
		// Sharp, the crown is the fold; rolled, it is a fillet below it, still above the band's join.
		const double Crown = S.HeightAtDistanceFromRidge(0.0);
		if (Roll <= 0.0)
		{
			TestTrue(TEXT("roll 0: crown is the full rise"), FMath::IsNearlyEqual(Crown, S.Rise(), 1e-6));
		}
		else
		{
			const double Join = S.HeightAtDistanceFromRidge(Roll * S.HalfSpan());
			TestTrue(FString::Printf(TEXT("roll %.2f: crown is below the fold"), Roll), Crown < S.Rise() - 1e-6);
			TestTrue(FString::Printf(TEXT("roll %.2f: and above the roll's join"), Roll), Crown > Join);
			TestTrue(FString::Printf(TEXT("roll %.2f: CrownFactor says so"), Roll), FMath::IsNearlyEqual(S.CrownFactor(), Crown / S.Rise(), 1e-9));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRoofTileTest,
	"HutongLayout.Roofs.TileRank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRoofTileTest::RunTest(const FString& Parameters)
{
	// 勾頭 exist on 筒瓦 and not on 合瓦, so the rank rule has to show up as geometry rather than as a flag nothing reads.
	auto Build = [](EHutongRoofTile Tile)
	{
		FHutongSiheyuanParams P;
		P.Width = 900.0;
		P.Depth = 450.0;
		P.RoofTile = Tile;
		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildSiheyuan(Mesh, P);
		return Mesh.TriangleCount();
	};

	const int32 He = Build(EHutongRoofTile::He);
	const int32 Tong = Build(EHutongRoofTile::Tong);
	TestTrue(TEXT("合瓦 builds"), He > 0);
	TestTrue(TEXT("筒瓦 adds 勾頭 along the eave"), Tong > He);
	// Logged rather than asserted.
	UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(TEXT("合瓦 %d tris, 筒瓦 %d tris (+%d for the 勾頭)"), He, Tong, Tong - He));

	// And the rank rule is wired to the gate styles rather than left loose.
	FHutongGateHouseParams G;
	G.Style = EHutongGateStyle::Guangliang;
	TestTrue(TEXT("廣亮大門 is entitled to 筒瓦"), G.GetRoofTile() == EHutongRoofTile::Tong);
	G.Style = EHutongGateStyle::Ruyi;
	TestTrue(TEXT("如意門 is not"), G.GetRoofTile() == EHutongRoofTile::He);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongBayProportionTest,
	"HutongLayout.Proportions.BayToHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongBayProportionTest::RunTest(const FString& Parameters)
{
	// 檐柱高 = 8/10 明間面闊.
	FHutongSiheyuanParams P;
	P.Width = 1040.0; P.Depth = 600.0;      // the 正房's suggested frontage
	P.MinBayWidth = 300.0; P.MaxBayWidth = 380.0;

	TestEqual(TEXT("1040 over three bays"), P.GetBayCount(), 3);
	const double Central = P.GetCentralBayWidth();
	TestTrue(FString::Printf(TEXT("明間 is %.0f"), Central),
		FMath::IsNearlyEqual(Central, 385.2, 1.0));
	TestTrue(FString::Printf(TEXT("柱高 is 8/10 of it (%.0f of %.0f)"), P.GetColumnHeight(), Central),
		FMath::IsNearlyEqual(P.GetColumnHeight() / Central, 0.8, 0.01));

	// A wider bay wants a taller column.
	FHutongSiheyuanParams Wide = P;
	Wide.Width = 1300.0;
	Wide.BayCountOverride = 3;
	TestTrue(TEXT("a wider bay raises the eave"), Wide.GetEaveHeight() > P.GetEaveHeight());

	FHutongSiheyuanParams More = P;
	More.Width = 1300.0;                            // more bays, not wider ones
	TestTrue(TEXT("a longer row of the same bays does not"),
		More.GetEaveHeight() < Wide.GetEaveHeight());

	// The 柱高 floor: it holds the smallest type up and leaves everything else to the ratio.
	{
		FHutongSiheyuanParams Ear;
		Ear.Width = 460.0; Ear.MinBayWidth = 220.0; Ear.MaxBayWidth = 270.0;
		TestTrue(FString::Printf(TEXT("the ratio alone would give the 耳房 a %.0f cm column"),
			0.8 * Ear.GetCentralBayWidth()), 0.8 * Ear.GetCentralBayWidth() < Ear.MinColumnHeight);
		TestTrue(TEXT("so the floor holds it up"),
			FMath::IsNearlyEqual(Ear.GetColumnHeight(), Ear.MinColumnHeight, 0.5));

		FHutongSiheyuanParams Side;
		Side.Width = 900.0; Side.MinBayWidth = 280.0; Side.MaxBayWidth = 340.0;
		TestTrue(TEXT("and does not touch a 廂房"),
			Side.GetColumnHeight() > Side.MinColumnHeight + 10.0);
	}

	// The proportions are not overridden any more.
	for (const FString& Name : HutongPresets::BuiltInSiheyuanNames())
	{
		FHutongSiheyuanParams Preset;
		if (!UHutongPresetLibrary::Get()->LoadPreset(
				TEXT("Siheyuan"), Name, FHutongSiheyuanParams::StaticStruct(), &Preset))
		{
			continue;
		}
		Preset.Width = FMath::Max(Preset.SuggestedFrontage, 200.0);

		const double Wanted = Preset.GetEaveHeightFromBays();
		TestTrue(FString::Printf(TEXT("%s builds at the height its bays ask for (%.0f)"), *Name, Wanted),
			FMath::IsNearlyEqual(Preset.GetEaveHeight(), Wanted, 0.5));

		// What the doorway comes out at, logged.
		const double Clear = Preset.GetDoorLeafTopHeight() - Preset.GetFloorHeight()
			- Preset.DoorThresholdHeight;
		UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(
			TEXT("%s: eave %.0f, doorway %.0f cm clear -> %s"), *Name, Wanted, Clear,
			Clear >= 176.0 ? TEXT("stand") : TEXT("duck")));
		TestTrue(FString::Printf(TEXT("%s can be passed at least crouching"), *Name),
			Clear >= HutongGen::Passage::MinClearHeight - 0.5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongEaveRafterTest,
	"HutongLayout.Roofs.Rafters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongEaveRafterTest::RunTest(const FString& Parameters)
{
	// An eave shows two courses: round 檐椽 with square 飛椽 on their ends. It drew one.
	auto Build = [](bool bFlying)
	{
		FHutongSiheyuanParams P;
		P.Width = 900.0; P.Depth = 450.0;
		P.bHasFlyingRafters = bFlying;
		UE::Geometry::FDynamicMesh3 M;
		HutongGen::BuildSiheyuan(M, P);
		return M.TriangleCount();
	};
	const int32 One = Build(false);
	const int32 Two = Build(true);
	TestTrue(TEXT("檐椽 alone still builds"), One > 0);
	TestTrue(FString::Printf(TEXT("飛椽 add a second course (+%d tris)"), Two - One), Two > One);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMaterialSlotNamingTest,
	"HutongLayout.Appearance.SlotNaming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMaterialSlotNamingTest::RunTest(const FString& Parameters)
{
	// Every slot is named, and no two share a name.
	TSet<FName> Names;
	for (int32 Slot = 0; Slot < HutongGen::MatSlot_Count; ++Slot)
	{
		const FName Name = HutongGen::MaterialSlotName(Slot);
		TestFalse(FString::Printf(TEXT("slot %d has a name"), Slot), Name.IsNone());
		TestFalse(FString::Printf(TEXT("slot %d is not the fallback"), Slot),
			Name == FName(TEXT("Unnamed")));
		bool bAlready = false;
		Names.Add(Name, &bAlready);
		TestFalse(FString::Printf(TEXT("slot %d's name is its own"), Slot), bAlready);
	}

	// A mesh carries only the slots it wears. The converter packs material IDs into polygon groups densely up to the highest one used.
	auto Compacted = [&](UE::Geometry::FDynamicMesh3& M, const TCHAR* What)
	{
		TArray<int32> Before;
		if (const UE::Geometry::FDynamicMeshMaterialAttribute* Mat =
			M.HasAttributes() ? M.Attributes()->GetMaterialID() : nullptr)
		{
			for (int32 tid : M.TriangleIndicesItr()) Before.AddUnique(Mat->GetValue(tid));
		}
		Before.Sort();

		const TArray<int32> Used = HutongGen::CompactMaterialSlots(M);
		TestEqual(FString::Printf(TEXT("%s keeps exactly the slots it tagged"), What),
			Used.Num(), Before.Num());
		TestTrue(FString::Printf(TEXT("%s lists them in slot order"), What), Used == Before);

		// And the IDs now run 0..N-1, which is what makes the section list the slot list.
		int32 Highest = -1;
		if (const UE::Geometry::FDynamicMeshMaterialAttribute* Mat =
			M.HasAttributes() ? M.Attributes()->GetMaterialID() : nullptr)
		{
			for (int32 tid : M.TriangleIndicesItr())
			{
				Highest = FMath::Max(Highest, Mat->GetValue(tid));
			}
		}
		TestEqual(FString::Printf(TEXT("%s numbers its slots from zero"), What),
			Highest, Used.Num() - 1);
		return Used.Num();
	};

	// The 甬路: three boxes, two surfaces.
	{
		FHutongPathParams Path;
		UE::Geometry::FDynamicMesh3 M;
		UHutongPathBuildingComponent::BuildPathMesh(Path, 700.0, 150.0, false, M);
		const int32 Slots = Compacted(M, TEXT("a 甬路"));
		TestTrue(FString::Printf(TEXT("a 甬路 wears few slots (%d)"), Slots),
			Slots > 0 && Slots < HutongGen::MatSlot_Count);
	}

	// A 正房 wears one of everything, so it is the case where compacting must change nothing.
	{
		FHutongSiheyuanParams P;
		P.Width = 1040.0; P.Depth = 600.0;
		P.bHasFrontVeranda = true;

		UE::Geometry::FDynamicMesh3 M;
		HutongGen::BuildSiheyuan(M, P);
		Compacted(M, TEXT("a 正房"));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMaterialSlotTest,
	"HutongLayout.Appearance.Slots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMaterialSlotTest::RunTest(const FString& Parameters)
{
	// A slot with nothing tagged into it is an empty section and a swatch that does nothing.
	FHutongSiheyuanParams P;
	P.Width = 1040.0; P.Depth = 600.0;
	P.bHasFrontVeranda = true;

	UE::Geometry::FDynamicMesh3 Mesh;
	HutongGen::BuildSiheyuan(Mesh, P);

	TArray<int32> Count;
	Count.SetNumZeroed(HutongGen::MatSlot_Count);
	const UE::Geometry::FDynamicMeshMaterialAttribute* Mat =
		Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
	if (!TestNotNull(TEXT("the mesh carries material IDs"), (void*)Mat)) return false;

	for (int32 tid = 0; tid < Mesh.MaxTriangleID(); ++tid)
	{
		if (!Mesh.IsTriangle(tid)) continue;
		const int32 Slot = Mat->GetValue(tid);
		if (Count.IsValidIndex(Slot)) ++Count[Slot];
	}

	auto Used = [&](int32 Slot, const TCHAR* What)
	{
		TestTrue(FString::Printf(TEXT("%s has geometry (%d tris)"), What, Count[Slot]),
			Count[Slot] > 0);
	};
	Used(HutongGen::MatSlot_Body,       TEXT("青磚 body"));
	Used(HutongGen::MatSlot_Roof,       TEXT("瓦 roof"));
	Used(HutongGen::MatSlot_Wood,       TEXT("木作 frame"));
	Used(HutongGen::MatSlot_Stone,      TEXT("石作"));
	Used(HutongGen::MatSlot_BaseCourse, TEXT("下鹼"));
	Used(HutongGen::MatSlot_DoorPaint,  TEXT("門漆 leaves"));
	Used(HutongGen::MatSlot_Lattice,    TEXT("窗欞"));
	Used(HutongGen::MatSlot_Paper,      TEXT("窗紙"));
	Used(HutongGen::MatSlot_Plaster,    TEXT("白灰 interior"));

	// 散水 goes round the foot and is tagged into the 下鹼's slot.
	{
		FHutongSiheyuanParams NoApron = P;
		NoApron.Apron.bEnabled = false;
		UE::Geometry::FDynamicMesh3 Bare;
		HutongGen::BuildSiheyuan(Bare, NoApron);

		const UE::Geometry::FDynamicMeshMaterialAttribute* BareMat =
			Bare.HasAttributes() ? Bare.Attributes()->GetMaterialID() : nullptr;
		int32 BareBase = 0;
		for (int32 tid = 0; BareMat && tid < Bare.MaxTriangleID(); ++tid)
		{
			if (Bare.IsTriangle(tid) && BareMat->GetValue(tid) == HutongGen::MatSlot_BaseCourse) ++BareBase;
		}
		TestTrue(FString::Printf(TEXT("散水 adds to the 下鹼 slot (%d vs %d)"),
				Count[HutongGen::MatSlot_BaseCourse], BareBase),
			Count[HutongGen::MatSlot_BaseCourse] > BareBase);
	}

	// And the 下鹼 must not have swallowed the wall above it, nor the leaves the frame.
	TestTrue(TEXT("the body is still the largest surface"),
		Count[HutongGen::MatSlot_Body] > Count[HutongGen::MatSlot_BaseCourse]);

	// 門漆 must be the leaves and nothing else.
	TestTrue(FString::Printf(TEXT("門漆 covers the leaves only (%d vs %d wood)"),
			Count[HutongGen::MatSlot_DoorPaint], Count[HutongGen::MatSlot_Wood]),
		Count[HutongGen::MatSlot_DoorPaint] < Count[HutongGen::MatSlot_Wood]);

	// The 殿 is the other type with a door in the middle of a bay loop, and it was tagging its brick 檻牆 as timber and using neither of the window slots.
	FHutongHallParams H;
	H.Width = 1400.0; H.Depth = 900.0;

	UE::Geometry::FDynamicMesh3 HallMesh;
	HutongGen::BuildHall(HallMesh, H);

	TArray<int32> HallCount;
	HallCount.SetNumZeroed(HutongGen::MatSlot_Count);
	const UE::Geometry::FDynamicMeshMaterialAttribute* HallMat =
		HallMesh.HasAttributes() ? HallMesh.Attributes()->GetMaterialID() : nullptr;
	if (!TestNotNull(TEXT("the hall carries material IDs"), (void*)HallMat)) return false;
	for (int32 tid = 0; tid < HallMesh.MaxTriangleID(); ++tid)
	{
		if (!HallMesh.IsTriangle(tid)) continue;
		const int32 Slot = HallMat->GetValue(tid);
		if (HallCount.IsValidIndex(Slot)) ++HallCount[Slot];
	}

	TestTrue(TEXT("殿 窗欞 has geometry"), HallCount[HutongGen::MatSlot_Lattice] > 0);
	TestTrue(TEXT("殿 窗紙 has geometry"), HallCount[HutongGen::MatSlot_Paper] > 0);
	TestTrue(TEXT("殿 彩畫 has geometry"), HallCount[HutongGen::MatSlot_Paint] > 0);
	TestTrue(FString::Printf(TEXT("殿 門漆 covers the leaves only (%d vs %d wood)"),
			HallCount[HutongGen::MatSlot_DoorPaint], HallCount[HutongGen::MatSlot_Wood]),
		HallCount[HutongGen::MatSlot_DoorPaint] < HallCount[HutongGen::MatSlot_Wood]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGateSwingTest,
	"HutongLayout.Doors.GateSwing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGateSwingTest::RunTest(const FString& Parameters)
{
	// A gate opens inward: the leaves add depth on the courtyard side and nothing on the street side.
	auto Reach = [](bool bOpen, double& OutStreet, double& OutCourt)
	{
		FHutongWallParams P;
		P.Length = 700.0;
		P.bHasGate = true;
		P.bGateLeavesOpen = bOpen;
		// A wall carries no 散水 now, so nothing outreaches the leaves and this measures the doors.

		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildWall(Mesh, P);

		OutStreet = BIG_NUMBER; OutCourt = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			const FVector3d V = Mesh.GetVertex(vid);
			OutStreet = FMath::Min(OutStreet, V.Y);   // the lane is -Y
			OutCourt = FMath::Max(OutCourt, V.Y);
		}
	};

	double ShutStreet, ShutCourt, OpenStreet, OpenCourt;
	Reach(false, ShutStreet, ShutCourt);
	Reach(true, OpenStreet, OpenCourt);

	TestTrue(FString::Printf(TEXT("opening the gate puts nothing over the lane (%.1f vs %.1f)"),
		OpenStreet, ShutStreet), OpenStreet >= ShutStreet - 0.5);
	TestTrue(FString::Printf(TEXT("the leaves stand back into the courtyard (%.0f vs %.0f)"),
		OpenCourt, ShutCourt), OpenCourt > ShutCourt + 10.0);

	// The swung leaf clears the stone it turns on. The pivot stands inboard of the leaf's own edge.
	{
		const double DoorW = 130.0, JambT = 10.0, Depth = 34.0;
		const double Reveal = HutongGen::DoorStoneReveal(JambT, DoorW);

		HutongGen::FHutongDoorAssembly D;
		D.OpeningX0 = 0.0;  D.OpeningX1 = DoorW;
		D.FrontY = 0.0;     D.BackY = Depth;
		D.BottomZ = 0.0;    D.LeafTopZ = 210.0;  D.JambTopZ = 222.0;
		D.FrameThickness = JambT;
		D.StoneReveal = Reveal;
		D.bUseLeafAngles = true;
		D.LeftLeafAngleDeg = -85.0;
		D.RightLeafAngleDeg = -85.0;
		D.MinClearHeight = 0.0;

		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::AppendDoorAssembly(Mesh, D);

		// The stone's reveal reaches to X = Reveal from the left jamb.
		double DeepestIntoStone = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			const FVector3d V = Mesh.GetVertex(vid);
			if (V.Y <= D.BackY + 1.0) continue;              // not swung back into the passage
			if (V.X > 0.5 * DoorW) continue;                  // left leaf only
			DeepestIntoStone = FMath::Max(DeepestIntoStone, Reveal - V.X);
		}
		TestTrue(FString::Printf(
			TEXT("the swung leaf clears the stone's %.1f cm reveal (worst %.1f cm into it)"),
			Reveal, DeepestIntoStone), DeepestIntoStone < 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDrumStoneTest,
	"HutongLayout.Doors.DrumStone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDrumStoneTest::RunTest(const FString& Parameters)
{
	// The stone stands wholly in front of the doorway, not straddling the door plane.
	{
		FHutongDoorStoneParams S;
		const double FrontY = 0.0, BackY = 34.0;

		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::AppendDoorStonePair(Mesh, S, 0.0, 130.0, FrontY, BackY, 0.0, 10.0, 200.0);

		double MinY = BIG_NUMBER, MaxY = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			MinY = FMath::Min(MinY, Mesh.GetVertex(vid).Y);
			MaxY = FMath::Max(MaxY, Mesh.GetVertex(vid).Y);
		}
		// Short of the wall's inner face, not flush with it.
		TestTrue(FString::Printf(TEXT("the 枕 stops well short of the inner face (%.1f vs %.1f)"),
			MaxY, BackY), MaxY < BackY - 5.0);
		TestTrue(FString::Printf(TEXT("and it projects forward of it (%.0f cm)"), FrontY - MinY),
			FrontY - MinY > 20.0);
	}

	// A 抱鼓石 is a big disc on a modest base.
	const double W = 18.0, Dp = 36.0, H = 98.0, Frac = 0.62;

	UE::Geometry::FDynamicMesh3 Mesh;
	HutongGen::AppendDrumStone(Mesh, 0.0, W, 0.0, Dp, 0.0, H, Frac);

	double MinY = BIG_NUMBER, MaxY = -BIG_NUMBER, MaxZ = -BIG_NUMBER;
	for (int32 vid : Mesh.VertexIndicesItr())
	{
		const FVector3d V = Mesh.GetVertex(vid);
		MinY = FMath::Min(MinY, V.Y);
		MaxY = FMath::Max(MaxY, V.Y);
		MaxZ = FMath::Max(MaxZ, V.Z);
	}
	// The drum's faces look along the lane, so its diameter shows in Y.
	const double Diameter = MaxY - MinY;

	TestTrue(FString::Printf(TEXT("the drum is most of the stone's height (%.0f of %.0f)"),
		Diameter, H), Diameter > 0.5 * H);
	// Which necessarily means it overhangs the base it stands on — that is what a drum does.
	TestTrue(FString::Printf(TEXT("it stands proud of its base (%.0f vs %.0f deep)"), Diameter, Dp),
		Diameter > Dp);
	TestTrue(TEXT("and does not grow past the stone's top"), MaxZ <= H + 0.01);

	// The gate house grew 門墩 of its own, sharing this code.
	{
		FHutongGateHouseParams G;
		G.DoorStones.Style = EHutongDoorStone::Drum;

		G.Style = EHutongGateStyle::Guangliang;
		TestTrue(TEXT("廣亮大門 may carry a 抱鼓石"),
			G.GetDoorStones().Style == EHutongDoorStone::Drum);
		G.Style = EHutongGateStyle::Manzi;
		TestTrue(TEXT("蠻子門 is held to a block however it is set"),
			G.GetDoorStones().Style == EHutongDoorStone::Block);

		// And they are built: a gate with stones has more geometry than one without.
		auto Count = [](bool bStones)
		{
			FHutongGateHouseParams P;
			P.Width = 380.0; P.Depth = 400.0;
			P.DoorStones.bEnabled = bStones;
			UE::Geometry::FDynamicMesh3 M;
			HutongGen::BuildGateHouse(M, P);
			return M.TriangleCount();
		};
		TestTrue(TEXT("the gate house builds its 門墩"), Count(true) > Count(false));
	}

	// 垂花門 too: its doors are 板門 on pivots.
	{
		auto Count = [](bool bStones)
		{
			FHutongInnerGateParams P;
			P.Width = 330.0; P.Depth = 150.0;
			P.DoorStones.bEnabled = bStones;
			UE::Geometry::FDynamicMesh3 M;
			HutongGen::BuildInnerGate(M, P);
			return M.TriangleCount();
		};
		TestTrue(TEXT("垂花門 builds its 門枕石"), Count(true) > Count(false));

		FHutongInnerGateParams P;
		TestTrue(TEXT("and they are modest by default"), P.DoorStones.BlockHeight < 50.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDoorPegTest,
	"HutongLayout.Doors.Pegs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDoorPegTest::RunTest(const FString& Parameters)
{
	// 門簪 pin the head.
	HutongGen::FHutongDoorAssembly D;
	D.OpeningX0 = 0.0;   D.OpeningX1 = 130.0;
	D.FrontY = 0.0;      D.BackY = 34.0;
	D.BottomZ = 0.0;     D.LeafTopZ = 250.0;  D.JambTopZ = 262.0;
	D.FrameThickness = 12.0;
	D.PegCount = 4;
	// Shut and unswung, so the only thing in front of the door plane is a peg.
	D.bLeavesOpen = false;
	D.bUseLeafAngles = false;
	D.MinClearHeight = 0.0;

	UE::Geometry::FDynamicMesh3 Mesh;
	HutongGen::AppendDoorAssembly(Mesh, D);

	const double HeadLo = D.LeafTopZ;
	const double HeadHi = D.LeafTopZ + D.FrameThickness;
	const double HeadMid = 0.5 * (HeadLo + HeadHi);

	int32 Proud = 0;
	double WorstOffset = 0.0, Reach = 0.0;
	for (int32 vid : Mesh.VertexIndicesItr())
	{
		const FVector3d V = Mesh.GetVertex(vid);
		if (V.Y >= D.FrontY - 0.5) continue;          // not in front of the door plane
		++Proud;
		WorstOffset = FMath::Max(WorstOffset, FMath::Abs(V.Z - HeadMid));
		Reach = FMath::Max(Reach, D.FrontY - V.Y);
	}

	TestTrue(TEXT("the pegs are built"), Proud > 0);
	// Centred on the head: a peg may stand a little proud of the rail's edges, never a whole diameter above it.
	TestTrue(FString::Printf(TEXT("pegs sit on the head, not above it (worst %.1f cm off centre)"),
		WorstOffset), WorstOffset < D.FrameThickness);
	TestTrue(FString::Printf(TEXT("pegs stand out from the face (%.1f cm)"), Reach), Reach > 8.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRidgeTailTest,
	"HutongLayout.Roofs.RidgeTail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRidgeTailTest::RunTest(const FString& Parameters)
{
	// A 蠍子尾 rises more than it projects. It was a slab leaning out past the gable.
	FHutongSiheyuanParams P;
	P.Width = 900.0; P.Depth = 450.0;

	auto Extents = [](const FHutongSiheyuanParams& Params, double& OutTopZ, double& OutMaxX)
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildSiheyuan(Mesh, Params);
		OutTopZ = -BIG_NUMBER; OutMaxX = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			const FVector3d V = Mesh.GetVertex(vid);
			OutTopZ = FMath::Max(OutTopZ, V.Z);
			OutMaxX = FMath::Max(OutMaxX, V.X);
		}
	};

	double WithZ, WithX, WithoutZ, WithoutX;
	Extents(P, WithZ, WithX);

	FHutongSiheyuanParams Plain = P;
	Plain.bHasRidgeCourse = false;
	Extents(Plain, WithoutZ, WithoutX);

	const double Rose = WithZ - WithoutZ;
	const double Projected = WithX - WithoutX;
	TestTrue(FString::Printf(TEXT("the tail rises (%.0f cm)"), Rose), Rose > 1.0);
	TestTrue(FString::Printf(TEXT("it rises more than it projects (%.0f up vs %.0f out)"),
		Rose, Projected), Rose > Projected);
	// 硬山 means a flush gable: the tail may overhang a little, never half a metre.
	TestTrue(FString::Printf(TEXT("it overhangs the gable modestly (%.0f cm)"), Projected),
		Projected < 30.0);
	UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(
		TEXT("蠍子尾: rises %.0f cm, overhangs %.0f cm (was 40 up / 43 out, as a slab)"),
		Rose, Projected));
	return true;
}

// 博縫 and 排山勾滴 stand a few centimetres proud of each 山牆 and hug the roof edge from eave to
// ridge, so the gable reads as a brick face with a rim rather than a slab.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGableRakeTest,
	"HutongLayout.Roofs.GableRake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGableRakeTest::RunTest(const FString& Parameters)
{
	FHutongSiheyuanParams P;
	P.Width = 900.0; P.Depth = 450.0;
	P.bHasRidgeCourse = false;   // the 蠍子尾 also reaches past the gable; keep it out of the measure

	UE::Geometry::FDynamicMesh3 With, Without;
	HutongGen::BuildSiheyuan(With, P);
	FHutongSiheyuanParams Plain = P;
	Plain.bHasGableRake = false;
	HutongGen::BuildSiheyuan(Without, Plain);

	const double Eave = P.GetEaveHeight();
	const double Apex = Eave + P.GetRoofRise();
	double MinX = BIG_NUMBER, MaxX = -BIG_NUMBER;
	int32 Proud = 0, OffTheRake = 0;
	for (int32 vid : With.VertexIndicesItr())
	{
		const FVector3d V = With.GetVertex(vid);
		MinX = FMath::Min(MinX, V.X);
		MaxX = FMath::Max(MaxX, V.X);
		// Past the 下鹼's own 3 cm projection.
		if (V.X < -3.5 || V.X > P.Width + 3.5)
		{
			++Proud;
			if (V.Z < Eave - 40.0 || V.Z > Apex + 12.0) ++OffTheRake;
		}
	}
	TestTrue(TEXT("the rake adds triangles"), With.TriangleCount() > Without.TriangleCount());
	TestTrue(FString::Printf(TEXT("it stands proud of both gables (%.0f .. %.0f)"), MinX, MaxX - P.Width),
		MinX < -3.0 && MinX > -15.0 && MaxX - P.Width > 3.0 && MaxX - P.Width < 15.0);
	TestTrue(TEXT("something is out past the gable"), Proud > 0);
	TestEqual(TEXT("everything past the gable hugs the roof edge"), OffTheRake, 0);

	double PlainMinX = BIG_NUMBER;
	for (int32 vid : Without.VertexIndicesItr()) PlainMinX = FMath::Min(PlainMinX, Without.GetVertex(vid).X);
	TestTrue(TEXT("off, the gable is flush again"), PlainMinX > -3.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRoofUVTest,
	"HutongLayout.Roofs.UVs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRoofUVTest::RunTest(const FString& Parameters)
{
	using namespace HutongMeshUtils;

	// Whatever the roof authored, the fallback must leave nothing bare.
	auto CheckCovered = [&](FDynamicMesh3& Mesh, const TCHAR* What)
	{
		FillUnsetUVsBoxProjected(Mesh, 100.0);
		const UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		if (!TestNotNull(FString::Printf(TEXT("%s: has a UV layer"), What), (void*)UV)) return;

		int32 Bare = 0, Degenerate = 0;
		for (int32 tid = 0; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			if (!UV->IsSetTriangle(tid)) { ++Bare; continue; }

			const UE::Geometry::FIndex3i E = UV->GetTriangle(tid);
			const FVector2f A = UV->GetElement(E.A), B = UV->GetElement(E.B), C = UV->GetElement(E.C);
			// A UV triangle with no area maps a face to a line.
			const float Area2 = FMath::Abs((B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y));
			if (Area2 < 1e-9f) ++Degenerate;
		}
		TestEqual(FString::Printf(TEXT("%s: no triangle left without UVs"), What), Bare, 0);
		TestEqual(FString::Printf(TEXT("%s: no UV triangle collapsed to a line"), What), Degenerate, 0);
	};

	{
		FDynamicMesh3 Mesh;
		FHutongSiheyuanParams P;
		P.Width = 900.0; P.Depth = 450.0;
		HutongGen::BuildSiheyuan(Mesh, P);
		CheckCovered(Mesh, TEXT("siheyuan"));
	}
	{
		FDynamicMesh3 Mesh;
		FHutongHallParams P;
		P.Width = 1000.0; P.Depth = 660.0;
		HutongGen::BuildHall(Mesh, P);
		CheckCovered(Mesh, TEXT("殿 (歇山, 筒瓦)"));
	}

	// The roof's own mapping is in tile rows and not distorted.
	{
		FXieshanRoofSpec S;
		S.Width = 620.0; S.Depth = 420.0; S.Rise = 190.0;
		S.Section = HutongGen::Jiajia::MakeSection(EHutongPurlins::Five, 210.0, 60.0, 0.0);
		S.TileRowSpacing = 20.0;

		FDynamicMesh3 Mesh;
		AppendXieshanRoof(Mesh, FVector3d::Zero(), S);
		const UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		if (!TestNotNull(TEXT("歇山 roof has UVs"), (void*)UV)) return false;

		double WorstRatio = 1.0;
		for (int32 tid = 0; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid) || !UV->IsSetTriangle(tid)) continue;
			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			const UE::Geometry::FIndex3i E = UV->GetTriangle(tid);

			// One edge is enough: compare its length on the roof with its length in UV space, scaled by the tile size.
			const double World = (Mesh.GetVertex(T.B) - Mesh.GetVertex(T.A)).Length();
			const double Uv = (FVector2d(UV->GetElement(E.B)) - FVector2d(UV->GetElement(E.A))).Length()
				* S.TileRowSpacing;
			if (World < 1.0 || Uv < 1e-6) continue;
			WorstRatio = FMath::Max(WorstRatio, FMath::Max(World / Uv, Uv / World));
		}
		// The flare pushes corners out along their diagonals.
		TestTrue(FString::Printf(TEXT("roof UVs stay near world scale (worst %.2fx)"), WorstRatio),
			WorstRatio < 1.2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongOpeningStackTest,
	"HutongLayout.Proportions.OpeningStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongOpeningStackTest::RunTest(const FString& Parameters)
{
	// A 正房 at its preset eave, checked by hand.
	FHutongSiheyuanParams P;
	// Stated rather than derived: this is a test about the opening stack, and the eave is an input to it.
	P.bDeriveEaveFromBays = false;
	P.EaveHeight = 365.0;

	const double Floor = P.GetFloorHeight();
	const double Col = P.GetColumnHeight();
	TestTrue(TEXT("柱高 is 11/13 of the eave"), FMath::IsNearlyEqual(Col, 365.0 * 11.0 / 13.0, 0.5));
	TestTrue(TEXT("額枋 underside is 柱高 less one 柱徑"),
		FMath::IsNearlyEqual(P.GetArchitraveBottom(), Floor + Col * (10.0 / 11.0), 0.5));

	// The unification: one 中檻 across the bay.
	TestTrue(TEXT("window head and door leaf head are the same member"),
		FMath::IsNearlyEqual(P.GetWindowTopHeight(), P.GetDoorLeafTopHeight(), 0.01));
	TestTrue(TEXT("the 中檻 sits under the 額枋"),
		P.GetWindowTopHeight() < P.GetArchitraveBottom());

	// The sill is an absolute, and this is the claim worth testing.
	FHutongSiheyuanParams Small = P;
	Small.EaveHeight = 316.0;                       // the 耳房 preset, stated as the presets do
	TestTrue(TEXT("正房 sill is 85 above its floor"),
		FMath::IsNearlyEqual(P.GetWindowSillHeight() - P.GetFloorHeight(), 85.0, 0.01));
	TestTrue(TEXT("耳房 sill is the same 85 above its own floor"),
		FMath::IsNearlyEqual(Small.GetWindowSillHeight() - Small.GetFloorHeight(), 85.0, 0.01));

	// And the small building clears the doorway rule on its own now, without the eave being pushed up for it.
	const double Clear = Small.GetDoorLeafTopHeight() - Small.GetFloorHeight() - Small.DoorThresholdHeight;
	TestTrue(FString::Printf(TEXT("耳房 doorway is %.0f cm clear, unforced"), Clear),
		Clear >= HutongGen::Passage::MinClearHeight);
	TestTrue(TEXT("耳房 eave stands on its own"), Small.GetEaveHeight() <= 316.0 + 0.01);

	UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(
		TEXT("正房: 額枋 underside %.0f, 中檻 %.0f, sill %.0f (all above ground)"),
		P.GetArchitraveBottom(), P.GetWindowTopHeight(), P.GetWindowSillHeight()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongUrbanModuleTest,
	"HutongLayout.Urban.Module",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongUrbanModuleTest::RunTest(const FString& Parameters)
{
	using namespace HutongGen::Urban;

	// The 步 × 6, 12, 24 widths and the pitch are the canon's own constants (HutongCanon.h),
	// restated nowhere; what is tested is what the module does with them.
	// Classification covers the whole range — every width lands somewhere.
	TestTrue(TEXT("3 m is an alley"), Classify(300.0) == EHutongStreetClass::Alley);
	TestTrue(TEXT("9.1 m is a 胡同"), Classify(910.0) == EHutongStreetClass::Hutong);
	TestTrue(TEXT("18 m is a 小街"), Classify(1800.0) == EHutongStreetClass::MinorStreet);
	TestTrue(TEXT("36 m is a 大街"), Classify(3600.0) == EHutongStreetClass::MajorStreet);
	TestTrue(TEXT("90 m is off the top"), Classify(9000.0) == EHutongStreetClass::Open);

	// In band vs merely nearest: a 12 m lane is a 胡同 by class and not a good example of one.
	TestTrue(TEXT("9.4 m is in band"), IsInBand(940.0));
	TestFalse(TEXT("12 m is not"), IsInBand(1200.0));

	// Snap-to-band is tight on purpose: it corrects a hand, not a measurement.
	TestTrue(TEXT("within tolerance snaps"), NearestCanonicalWidth(940.0, 30.0) > 0.0);
	TestEqual(TEXT("outside tolerance does not"), NearestCanonicalWidth(1200.0, 30.0), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryTest,
	"HutongLayout.Gallery.EverythingBuilds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryTest::RunTest(const FString& Parameters)
{
	// The gallery is the one place that touches every generator in the plugin.
	UHutongGalleryToolProperties* Settings = NewObject<UHutongGalleryToolProperties>();
	const TArray<TPair<FString, int32>> Built = HutongGallery::BuildAll(Settings);

	TestTrue(TEXT("the gallery has pieces in it"), Built.Num() >= 15);
	for (const TPair<FString, int32>& It : Built)
	{
		// Logged as well as asserted.
		UE_LOG(LogTemp, Display, TEXT("%s"),
			*FString::Printf(TEXT("gallery: %-44s %6d tris"), *It.Key, It.Value));
		TestTrue(FString::Printf(TEXT("%s builds geometry"), *It.Key), It.Value > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHippedRoofControlTest,
	"HutongLayout.Roofs.HippedControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHippedRoofControlTest::RunTest(const FString& Parameters)
{
	using namespace HutongMeshUtils;

	FHipRoofSpec Base;
	Base.Width = 500.0; Base.Depth = 300.0; Base.RidgeLength = 200.0; Base.Rise = 160.0;
	Base.Section = HutongGen::Jiajia::MakeSection(EHutongPurlins::Five, 150.0, 50.0, 0.0);
	Base.bEaveCaps = true;   // 廡殿 is a ranked roof; if it exists at all it is 筒瓦
	Base.FlareLength = 110.0; Base.FlareRun = 40.0; Base.FlareRise = 30.0; Base.FasciaDrop = 8.0;

	struct FCase { const TCHAR* Name; FHipRoofSpec Spec; };
	TArray<FCase> Cases;
	Cases.Add({ TEXT("廡殿"), Base });

	{
		// 捲棚廡殿: the ridge becomes a rounded crown.
		FHipRoofSpec S = Base;
		S.Section.ApexRoll = 0.4;
		Cases.Add({ TEXT("捲棚廡殿"), S });
	}
	{
		// A rolled 攢尖 has no ridge at all to round.
		FHipRoofSpec S = Base;
		S.RidgeLength = 0.0; S.Section.ApexRoll = 0.5;
		Cases.Add({ TEXT("捲棚攢尖"), S });
	}

	for (const FCase& C : Cases)
	{
		FDynamicMesh3 Mesh;
		AppendHippedRoof(Mesh, FVector3d::Zero(), C.Spec);
		const FShellReport R = InspectShell(Mesh);

		TestTrue(FString::Printf(TEXT("%s: builds triangles"), C.Name), R.Triangles > 0);
		TestEqual(FString::Printf(TEXT("%s: every edge is matched"), C.Name), R.Unmatched, 0);
		TestEqual(FString::Printf(TEXT("%s: consistent winding"), C.Name), R.Duplicated, 0);
		TestTrue(FString::Printf(TEXT("%s: positive volume"), C.Name), R.Volume > 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRearHighWindowTest,
	"HutongLayout.Openings.RearHighWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRearHighWindowTest::RunTest(const FString& Parameters)
{
	// 高窗 in the 封護檐 back wall: the only light a row with its back to the lane gets from that side, and the height is what makes them allowable at all.
	const double SizeX = 1300.0;
	const double SizeY = 360.0;
	const int32 N = 4;

	auto Build = [&](bool bWindows, EHutongRearEave Rear, UE::Geometry::FDynamicMesh3& M)
	{
		FHutongSiheyuanParams P;
		P.Width = SizeX;
		P.Depth = SizeY;
		P.RearEave = Rear;
		P.bHasRearHighWindows = bWindows;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
			P, EHutongBaySide::MinusY, N, SizeX, SizeY, M);
		return P;
	};

	UE::Geometry::FDynamicMesh3 Blank, Open;
	Build(false, EHutongRearEave::Lane, Blank);
	const FHutongSiheyuanParams P = Build(true, EHutongRearEave::Lane, Open);

	TestTrue(TEXT("the 高窗 change the wall"), Open.TriangleCount() != Blank.TriangleCount());

	const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(SizeX, SizeY) * 0.2);
	const double Eave = FMath::Max(P.GetEaveHeight(), 10.0);
	const double Head = Eave - P.RearWindowHeadDrop;
	const double Sill = Head - P.RearWindowHeight;
	const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);

	// Above head height, which is the whole point of putting them up there.
	TestTrue(FString::Printf(TEXT("the sill stands above head height (%.0f cm)"), Sill),
		Sill >= 170.0);
	TestTrue(TEXT("the head stays under the eave"), Head < Eave - 1.0);

	// One to a bay, and each really is a hole: nothing of the wall — brick or plaster — may stand in the opening.
	for (int32 i = 0; i < N; ++i)
	{
		const double A = P.GetBayBoundary(i, N, SizeX, ColR);
		const double B = P.GetBayBoundary(i + 1, N, SizeX, ColR);
		const double Cx = 0.5 * (A + B);
		const double Half = FMath::Min(0.5 * P.RearWindowWidth, 0.35 * (B - A));

		int32 Blocking = 0;
		for (int32 tid : Open.TriangleIndicesItr())
		{
			const UE::Geometry::FIndex3i Tri = Open.GetTriangle(tid);
			const FVector3d C = (Open.GetVertex(Tri.A) + Open.GetVertex(Tri.B)
				+ Open.GetVertex(Tri.C)) / 3.0;
			// Only the back wall's own depth, and only well inside the opening.
			const int32 Slot = Open.Attributes()->GetMaterialID()->GetValue(tid);
			if (Slot == HutongGen::MatSlot_Paper || Slot == HutongGen::MatSlot_Lattice) continue;
			if (C.Y < SizeY - T + 0.5 || C.Y > SizeY - 0.5) continue;
			if (C.Z < Sill + 3.0 || C.Z > Head - 3.0) continue;
			if (FMath::Abs(C.X - Cx) < Half - 3.0) ++Blocking;
		}
		TestEqual(FString::Printf(TEXT("bay %d's 高窗 is open through the wall"), i), Blocking, 0);
	}

	// Independent of the rear eave. What the roof does at the back and what the wall carries are two questions, and a 正房 whose back gives onto its own 後院 takes an overhanging eave and still wants its light.
	UE::Geometry::FDynamicMesh3 Court, CourtBlank;
	Build(true, EHutongRearEave::Courtyard, Court);
	Build(false, EHutongRearEave::Courtyard, CourtBlank);
	TestTrue(TEXT("a courtyard back carries them too"),
		Court.TriangleCount() > CourtBlank.TriangleCount());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWindowRevealTest,
	"HutongLayout.Openings.WindowReveals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWindowRevealTest::RunTest(const FString& Parameters)
{
	// 抱框.
	const double SizeX = 1400.0;
	const double SizeY = 700.0;
	const int32 N = 5;

	FHutongSiheyuanParams P;
	// Every derived question below depends on the footprint, and the tool fills it in at spawn.
	P.Width = SizeX;
	P.Depth = SizeY;

	UE::Geometry::FDynamicMesh3 Mesh;
	UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
		P, EHutongBaySide::MinusY, N, SizeX, SizeY, Mesh);

	const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(SizeX, SizeY) * 0.2);
	const double PT = FMath::Clamp(P.PostThickness, 1.0, T * 1.5);
	const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);
	const double Eave = FMath::Max(P.GetEaveHeight(), 10.0);
	const double Floor = FMath::Clamp(P.GetFloorHeight(), 0.0, Eave * 0.5);
	const double Head = FMath::Clamp(P.GetDoorTopHeight(), Floor + 10.0, Eave - 10.0);

	// The post reaches the wall's inner face; the column only ever reaches its own radius past the facade plane.
	TestTrue(TEXT("the column cannot fill the wall's depth by itself"), ColR < T);

	for (int32 i = 1; i < N; ++i)
	{
		const double B = P.GetBayBoundary(i, N, SizeX, ColR);
		for (const double Face : { B - 0.5 * PT, B + 0.5 * PT })
		{
			int32 Found = 0;
			for (int32 vid : Mesh.VertexIndicesItr())
			{
				const FVector3d V = Mesh.GetVertex(vid);
				if (FMath::Abs(V.X - Face) < 0.5 && FMath::Abs(V.Y - T) < 0.5
					&& V.Z > Floor + 1.0 && V.Z < Head + 1.0)
				{
					++Found;
				}
			}
			TestTrue(FString::Printf(
				TEXT("bay boundary %d is closed through the wall's depth at x=%.1f"), i, Face),
				Found > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallGardenDoorwayTest,
	"HutongLayout.Walls.GardenDoorway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallGardenDoorwayTest::RunTest(const FString& Parameters)
{
	// 月亮門 and its relatives are the 什錦窗's trick carried to the ground.
	auto Build = [](EHutongWallDoorway Shape, EHutongWallRole Role, UE::Geometry::FDynamicMesh3& M)
	{
		FHutongWallParams P;
		P.Role = Role;
		P.Doorway = Shape;
		P.Length = 900.0;
		P.DoorwayPosition = 0.5;
		HutongGen::BuildWall(M, P);
	};

	UE::Geometry::FDynamicMesh3 Blank;
	Build(EHutongWallDoorway::None, EHutongWallRole::Courtyard, Blank);

	for (EHutongWallDoorway Shape : { EHutongWallDoorway::Moon, EHutongWallDoorway::Arch,
		EHutongWallDoorway::Octagon, EHutongWallDoorway::Hexagon, EHutongWallDoorway::Rect })
	{
		FHutongWallParams P;
		P.Role = EHutongWallRole::Courtyard;
		P.Doorway = Shape;
		P.Length = 900.0;
		P.DoorwayPosition = 0.5;

		UE::Geometry::FDynamicMesh3 M;
		Build(Shape, EHutongWallRole::Courtyard, M);
		const FString Name = FString::Printf(TEXT("doorway shape %d"), int32(Shape));

		TestTrue(Name + TEXT(" builds geometry"), M.TriangleCount() > Blank.TriangleCount());

		// A 月亮門 is a circle, so it takes its width from its height whatever the width field says.
		if (Shape == EHutongWallDoorway::Moon)
		{
			TestEqual(TEXT("a 月亮門 is as wide as its outline is tall"),
				P.GetDoorwayWidth(), P.GetDoorwaySpan());
		}

		// The wall grows to carry it, exactly as it grows for a 牆垣式門 too short to walk through.
		TestTrue(Name + TEXT(" leaves the wall tall enough to hold it"),
			P.GetHeight() >= P.GetDoorwayHeight() + P.DoorwaySurroundWidth + 10.0 - 0.01);

		// Nothing stands inside the outline between the sill and the head.
		const double Cx = P.DoorwayPosition * P.Length;
		const double HalfW = 0.5 * P.GetDoorwayWidth();
		const double Top = P.GetDoorwayHeight();
		const double Bury = P.GetDoorwayBury();
		const double Sill = P.DoorwaySillHeight;
		int32 Intruding = 0;
		for (int32 vid : M.VertexIndicesItr())
		{
			const FVector3d V = M.GetVertex(vid);
			if (V.Z <= Sill + 1.0 || V.Z >= Top - 1.0) continue;
			const double HwHere =
				P.GetDoorwayHalfWidthFraction(2.0 * (V.Z + Bury) / (Top + Bury) - 1.0) * HalfW;
			if (FMath::Abs(V.X - Cx) < HwHere - 6.0) ++Intruding;
		}
		TestEqual(Name + TEXT(" leaves the opening clear of masonry"), Intruding, 0);

		// And a 院牆 refuses it while the role derives.
		UE::Geometry::FDynamicMesh3 Lane;
		Build(Shape, EHutongWallRole::Perimeter, Lane);
		UE::Geometry::FDynamicMesh3 LaneBlank;
		Build(EHutongWallDoorway::None, EHutongWallRole::Perimeter, LaneBlank);
		TestEqual(Name + TEXT(" is refused on a 院牆"),
			Lane.TriangleCount(), LaneBlank.TriangleCount());
	}

	// A flush 下鹼 is a course with no projection, not an absent course. The band beside the opening
	// used to be emitted only when it stood proud, so setting the projection to zero — which the
	// panel offers and calls flush — cut a full-depth hole through the wall either side of the
	// doorway, from the sill up to the course's own height.
	{
		auto WallAt = [](double Projection, UE::Geometry::FDynamicMesh3& M)
		{
			FHutongWallParams P;
			P.Role = EHutongWallRole::Courtyard;
			P.Doorway = EHutongWallDoorway::Moon;
			P.Length = 900.0;
			P.DoorwayPosition = 0.5;
			P.BaseCourseProjection = Projection;
			// No surround: its own rows lap over the reveal, and they would answer the probe below
			// for masonry that is not there.
			P.DoorwaySurroundWidth = 0.0;
			HutongGen::BuildWall(M, P);
			return P;
		};

		UE::Geometry::FDynamicMesh3 Flush, Proud;
		const FHutongWallParams P = WallAt(0.0, Flush);
		WallAt(8.0, Proud);

		// The reveal: inside the opening's own bounding span, in the band the course occupies.
		// Only the doorway's rows put masonry there — the wall's own runs stop at the opening.
		const double Sill = FMath::Clamp(P.DoorwaySillHeight, 0.0, 40.0);
		const double BaseTop = FMath::Clamp(
			FMath::Clamp(P.GetBaseCourseHeight(), 0.0, P.GetHeight() * 0.6), Sill, P.GetDoorwayHeight());
		const double Cx = P.DoorwayPosition * P.Length;
		const double HalfW = 0.5 * P.GetDoorwayWidth();

		auto RevealVerts = [&](const UE::Geometry::FDynamicMesh3& M)
		{
			int32 N = 0;
			for (int32 vid : M.VertexIndicesItr())
			{
				const FVector3d V = M.GetVertex(vid);
				if (V.Z > Sill + 1.0 && V.Z < BaseTop - 1.0 && FMath::Abs(V.X - Cx) < HalfW - 1.0) ++N;
			}
			return N;
		};

		TestTrue(TEXT("the band beside the opening exists when the 下鹼 stands proud"),
			RevealVerts(Proud) > 0);
		TestTrue(TEXT("and it is still there when the 下鹼 is flush"),
			RevealVerts(Flush) > 0);
	}

	// 牆垣式垂花門: the dressing is the ornament of a 垂花門 with none of its frame, and the whole reason it is a separate thing from the 牆垣式門 is that it does not touch the roof line.
	{
		auto TopAndTris = [](bool bDress, int32& OutTris)
		{
			FHutongWallParams P;
			P.Role = EHutongWallRole::Courtyard;
			P.bDeriveFromRole = false;
			P.Height = 280.0;
			P.Doorway = EHutongWallDoorway::Rect;
			P.bDoorwayChuihua = bDress;
			P.Length = 900.0;

			UE::Geometry::FDynamicMesh3 M;
			HutongGen::BuildWall(M, P);
			OutTris = M.TriangleCount();

			double Top = -BIG_NUMBER;
			for (int32 vid : M.VertexIndicesItr())
			{
				Top = FMath::Max(Top, M.GetVertex(vid).Z);
			}
			return Top;
		};

		int32 PlainTris = 0, DressTris = 0;
		const double PlainTop = TopAndTris(false, PlainTris);
		const double DressTop = TopAndTris(true, DressTris);

		TestTrue(TEXT("the 垂花 dressing builds geometry"), DressTris > PlainTris);
		TestEqual(TEXT("the 垂花 dressing does not raise the wall's top line"),
			DressTop, PlainTop, 0.01);

		// And it hangs below the cap rather than reaching into it.
		auto ProudAboveBody = [](bool bDress)
		{
			FHutongWallParams P;
			P.Role = EHutongWallRole::Courtyard;
			P.bDeriveFromRole = false;
			P.Height = 280.0;
			P.Doorway = EHutongWallDoorway::Rect;
			P.bDoorwayChuihua = bDress;
			P.Length = 900.0;

			UE::Geometry::FDynamicMesh3 M;
			HutongGen::BuildWall(M, P);

			const double H = P.GetHeight();
			const double T = P.GetThickness();
			int32 Count = 0;
			for (int32 vid : M.VertexIndicesItr())
			{
				const FVector3d V = M.GetVertex(vid);
				const bool bProud = (V.Y < -0.5) || (V.Y > T + 0.5);
				if (bProud && V.Z > H + 0.01) ++Count;
			}
			return Count;
		};
		TestEqual(TEXT("the dressing adds nothing above the cap's underside"),
			ProudAboveBody(true), ProudAboveBody(false));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallRoleTest,
	"HutongLayout.Walls.Role",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallRoleTest::RunTest(const FString& Parameters)
{
	// A 院牆 keeps the lane out and a 隔牆 divides one household's own courts, and the whole point of the distinction is that the second is the lower of the two.
	auto TopOf = [](EHutongWallRole Role, bool bDerive, double& OutThickness)
	{
		FHutongWallParams P;
		P.Role = Role;
		P.bDeriveFromRole = bDerive;
		P.Length = 900.0;
		OutThickness = P.GetThickness();

		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildWall(Mesh, P);

		double Top = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			Top = FMath::Max(Top, Mesh.GetVertex(vid).Z);
		}
		return (Mesh.TriangleCount() > 0) ? Top : 0.0;
	};

	double PerimT = 0.0, CourtT = 0.0;
	const double PerimTop = TopOf(EHutongWallRole::Perimeter, true, PerimT);
	const double CourtTop = TopOf(EHutongWallRole::Courtyard, true, CourtT);

	TestTrue(TEXT("both roles build geometry"), PerimTop > 0.0 && CourtTop > 0.0);
	TestTrue(FString::Printf(TEXT("院牆 stands over 隔牆 (%.0f vs %.0f cm)"), PerimTop, CourtTop),
		PerimTop > CourtTop + 40.0);
	TestTrue(FString::Printf(TEXT("院牆 is the thicker (%.0f vs %.0f cm)"), PerimT, CourtT),
		PerimT > CourtT);

	// The claim the whole role rests on: the inner gate rises clear of the wall it stands in.
	const double GateEave = FHutongInnerGateParams().GetEaveHeight();
	TestTrue(FString::Printf(TEXT("垂花門 clears its 隔牆 (eave %.0f over wall top %.0f)"),
			GateEave, CourtTop),
		GateEave > CourtTop);

	// And a 院牆 is still private: above eye height by a clear margin, which 240 cm was not.
	TestTrue(FString::Printf(TEXT("院牆 is not seen over (%.0f cm)"), PerimTop), PerimTop > 300.0);

	// 什錦窗 belong to a wall inside the household.
	auto LatticeTris = [](EHutongWallRole Role, double RunLength, double BaseCourse = -1.0)
	{
		FHutongWallParams P;
		P.Role = Role;
		P.Length = RunLength;
		if (BaseCourse >= 0.0) P.BaseCourseHeight = BaseCourse;

		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildWall(Mesh, P);

		const UE::Geometry::FDynamicMeshMaterialAttribute* Mat =
			Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
		int32 N = 0;
		for (int32 tid = 0; Mat && tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (Mesh.IsTriangle(tid) && Mat->GetValue(tid) == HutongGen::MatSlot_Lattice) ++N;
		}
		return N;
	};

	TestTrue(TEXT("隔牆 carries 什錦窗"), LatticeTris(EHutongWallRole::Courtyard, 1400.0) > 0);
	TestEqual(TEXT("院牆 is blank"), LatticeTris(EHutongWallRole::Perimeter, 1400.0), 0);
	// The count follows the run, so a longer wall gains windows rather than wider gaps.
	TestTrue(TEXT("a longer 隔牆 carries more of them"),
		LatticeTris(EHutongWallRole::Courtyard, 2800.0)
			> LatticeTris(EHutongWallRole::Courtyard, 1400.0));

	// The opening has to fit in the field of brick between the 下鹼 and the cap, and it silently does not get built when it cannot.
	TestTrue(TEXT("隔牆 with a third-height 下鹼 still carries them"),
		LatticeTris(EHutongWallRole::Courtyard, 1400.0, 80.0) > 0);

	// Derivation off puts both roles back on the same authored numbers.
	double OffP = 0.0, OffC = 0.0;
	const double OffPerim = TopOf(EHutongWallRole::Perimeter, false, OffP);
	const double OffCourt = TopOf(EHutongWallRole::Courtyard, false, OffC);
	TestEqual(TEXT("derivation off: the role stops mattering (height)"), OffPerim, OffCourt);
	TestEqual(TEXT("derivation off: the role stops mattering (thickness)"), OffP, OffC);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundCorridorTest,
	"HutongLayout.Compound.Corridors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundCorridorTest::RunTest(const FString& Parameters)
{
	// Pure arithmetic over the plot, so this needs no world and no mesh.
	auto Corridors = [](EHutongCompoundPlan Plan, double W, double D)
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		In.Width = W;
		In.Depth = D;
		// Asked for outright: the plan's own default is 前廊 on the 廂房.
		In.CourtWalk = EHutongCourtWalk::Corridor;

		TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		return Slots.FilterByPredicate([](const FHutongCompoundSlot& S)
			{ return S.Piece == EHutongCompoundPiece::Corridor; });
	};

	// A 抄手遊廊 is not a small compound's. A 一進四合院 has an open yard you cross in the rain; the covered walk belongs to a household with a proper inner court, and it needs the same 二進 plan the 垂花門 does.
	const TArray<FHutongCompoundSlot> One = Corridors(EHutongCompoundPlan::OneCourtyard, 3200.0, 5200.0);
	TestEqual(TEXT("一進 carries no 遊廊"), One.Num(), 0);

	// 抄手 folds right round the court.
	const TArray<FHutongCompoundSlot> Two = Corridors(EHutongCompoundPlan::TwoCourtyards, 3200.0, 5200.0);
	if (!TestEqual(TEXT("二進 carries a ring, not two runs"), Two.Num(), 6)) return false;

	int32 AlongY = 0;
	for (const FHutongCompoundSlot& S : Two) if (S.bLengthAlongY) ++AlongY;
	TestEqual(TEXT("two of the five run down the court"), AlongY, 2);

	// Every run benches toward the court, so which are flipped depends on which side of the ring they are on.
	int32 Flipped = 0;
	for (const FHutongCompoundSlot& S : Two) if (S.bFlipOpenSide) ++Flipped;
	TestTrue(TEXT("the ring is not uniformly oriented"), Flipped > 0 && Flipped < Two.Num());

	// The four sides abut rather than overlap at the corners, so no two runs share plan area.
	for (int32 i = 0; i < Two.Num(); ++i)
	{
		for (int32 j = i + 1; j < Two.Num(); ++j)
		{
			const FVector2D AMin = Two[i].Min, AMax = Two[i].Min + Two[i].Size;
			const FVector2D BMin = Two[j].Min, BMax = Two[j].Min + Two[j].Size;
			const bool bApart = AMax.X <= BMin.X + 0.01 || BMax.X <= AMin.X + 0.01
				|| AMax.Y <= BMin.Y + 0.01 || BMax.Y <= AMin.Y + 0.01;
			TestTrue(TEXT("no two 遊廊 overlap in plan"), bApart);
		}
	}

	// The 正房's frontage is left clear.
	{
		const TArray<FHutongCompoundSlot> All =
			HutongGen::LayOutCompound([]{ HutongGen::FCompoundInput I;
				I.Plan = EHutongCompoundPlan::TwoCourtyards; I.Width = 3200.0; I.Depth = 5200.0;
				I.CourtWalk = EHutongCourtWalk::Corridor;
				return I; }());
		const FHutongCompoundSlot* Hall = All.FindByPredicate([](const FHutongCompoundSlot& S)
			{ return S.Piece == EHutongCompoundPiece::MainHall; });
		if (!TestNotNull(TEXT("there is a 正房"), (void*)Hall)) return false;

		int32 North = 0;
		for (const FHutongCompoundSlot& S : All)
		{
			if (S.Piece != EHutongCompoundPiece::Corridor) continue;
			// The north band is whatever reaches the hall's own front line; the returns down at the 垂花門 pass the hall's X range and are nowhere near it.
			if (S.Min.Y + S.Size.Y < Hall->Min.Y - 0.01) continue;
			++North;
			const bool bClearOfHall = S.Min.X + S.Size.X <= Hall->Min.X + 0.01
				|| S.Min.X >= Hall->Min.X + Hall->Size.X - 0.01;
			TestTrue(TEXT("no 遊廊 stands across the 正房's frontage"), bClearOfHall);
		}
		TestEqual(TEXT("a link in front of each 耳房"), North, 2);

		// And the 甬路 reaches the steps it is aimed at.
		const TArray<FHutongCompoundSlot> Spine = All.FilterByPredicate(
			[](const FHutongCompoundSlot& S)
				{ return S.Piece == EHutongCompoundPiece::Path && S.Size.Y > S.Size.X; });
		if (!TestEqual(TEXT("one spine"), Spine.Num(), 1)) return false;
		TestTrue(TEXT("the 甬路 runs up to the hall's platform"),
			Spine[0].Min.Y + Spine[0].Size.Y >= Hall->Min.Y - 1.0);
	}

	// And the ring still leaves a courtyard.
	{
		HutongGen::FCompoundInput Tight;
		Tight.Plan = EHutongCompoundPlan::TwoCourtyards;
		Tight.CourtWalk = EHutongCourtWalk::Corridor;
		double MinW, MinD;
		Tight.GetMinimumPlot(MinW, MinD);
		Tight.Width = MinW;
		Tight.Depth = MinD;

		const TArray<FHutongCompoundSlot> Ring = HutongGen::LayOutCompound(Tight)
			.FilterByPredicate([](const FHutongCompoundSlot& S)
				{ return S.Piece == EHutongCompoundPiece::Corridor; });
		// At the minimum the hall's 耳房 are as narrow as they go.
		if (!TestEqual(TEXT("the minimum plot seats the ring's four sides"), Ring.Num(), 4)) return false;

		TArray<FHutongCompoundSlot> Sides = Ring.FilterByPredicate(
			[](const FHutongCompoundSlot& S) { return S.bLengthAlongY; });
		if (!TestEqual(TEXT("one run down each side"), Sides.Num(), 2)) return false;
		Sides.Sort([](const FHutongCompoundSlot& A, const FHutongCompoundSlot& B)
			{ return A.Min.X < B.Min.X; });

		const double ClearW = Sides[1].Min.X - (Sides[0].Min.X + Sides[0].Size.X);
		const double ClearD = Sides[0].Size.Y;
		TestTrue(FString::Printf(TEXT("the court survives the ring across (%.0f)"), ClearW),
			ClearW >= Tight.MinCourtyardWidth - 1.0);
		TestTrue(FString::Printf(TEXT("the court survives the ring down (%.0f)"), ClearD),
			ClearD >= Tight.MinCourtyardDepth - 1.0);
	}

	// 正房三間兩耳: the hall's own frontage in the middle with a shallower 耳房 at each flank.
	{
		HutongGen::FCompoundInput Back;
		Back.Plan = EHutongCompoundPlan::TwoCourtyards;
		Back.Width = 3200.0;
		Back.Depth = 5200.0;
		const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(Back);

		const FHutongCompoundSlot* Hall = Slots.FindByPredicate([](const FHutongCompoundSlot& S)
			{ return S.Piece == EHutongCompoundPiece::MainHall; });
		const TArray<FHutongCompoundSlot> Ears = Slots.FilterByPredicate(
			[](const FHutongCompoundSlot& S) { return S.Piece == EHutongCompoundPiece::EarRoom; });

		if (!TestNotNull(TEXT("there is a 正房"), (void*)Hall)) return false;

		// The hall's own two are the ones with their backs on the plot's north edge; the other four are the 廂耳房 filling the inner court's corners.
		const double Back0 = Back.Depth;
		TArray<FHutongCompoundSlot> HallEars, WingEars;
		for (const FHutongCompoundSlot& E : Ears)
		{
			(FMath::IsNearlyEqual(E.Min.Y + E.Size.Y, Back0, 0.01) ? HallEars : WingEars).Add(E);
		}
		if (!TestEqual(TEXT("耳房 at both the hall's flanks"), HallEars.Num(), 2)) return false;

		TestTrue(TEXT("the 正房 does not run the plot's width"), Hall->Size.X < Back.Width - 1.0);
		for (const FHutongCompoundSlot& E : HallEars)
		{
			TestTrue(TEXT("an 耳房 is shallower than the hall"), E.Size.Y < Hall->Size.Y - 1.0);
		}
		TestTrue(TEXT("the hall is centred"),
			FMath::IsNearlyEqual(Hall->Min.X + 0.5 * Hall->Size.X, 0.5 * Back.Width, 0.01));

		// 廂耳房 close the inner court's south corners, one to a wing, hard against the cross wall.
		const TArray<FHutongCompoundSlot> Wings = Slots.FilterByPredicate(
			[](const FHutongCompoundSlot& S) { return S.Piece == EHutongCompoundPiece::SideHouse; });
		if (!TestEqual(TEXT("two 廂房"), Wings.Num(), 2)) return false;
		TestEqual(TEXT("四耳: two on the hall, one at the south end of each wing"), Ears.Num(), 4);

		for (const FHutongCompoundSlot& E : WingEars)
		{
			TestTrue(TEXT("a 廂耳房 is shallower than its 廂房"), E.Size.X < Wings[0].Size.X - 1.0);
		}

		// The 廂房 does not run the court's whole length.
		for (const FHutongCompoundSlot& Wing : Wings)
		{
			const bool bEarBefore = WingEars.ContainsByPredicate([&](const FHutongCompoundSlot& E)
				{ return E.Min.Y + E.Size.Y <= Wing.Min.Y + 0.01; });
			const bool bEarAfter = WingEars.ContainsByPredicate([&](const FHutongCompoundSlot& E)
				{ return E.Min.Y >= Wing.Min.Y + Wing.Size.Y - 0.01; });
			TestTrue(TEXT("a 廂房 has its ear at the south end and nothing at the north"),
				bEarBefore && !bEarAfter);
		}

		// 小天井: the pocket between the 耳房's front and the north end of the 廂房 range, closed off from the court by a 隔牆 so it reads as a light well.
		if (WingEars.Num() > 0)
		{
			const double EarFront = HallEars[0].Min.Y;
			// The well starts at the 廂房's own north gable now that no room stands up there.
			double WingNorth = 0.0;
			for (const FHutongCompoundSlot& Wing : Wings)
			{
				WingNorth = FMath::Max(WingNorth, Wing.Min.Y + Wing.Size.Y);
			}

			TArray<const FHutongCompoundSlot*> Wells;
			for (const FHutongCompoundSlot& S : Slots)
			{
				if (S.Piece != EHutongCompoundPiece::Wall || !S.bLengthAlongY) continue;
				if (S.WallRole != EHutongWallRole::Courtyard) continue;
				if (FMath::IsNearlyEqual(S.Min.Y, WingNorth, 0.5)) Wells.Add(&S);
			}
			if (TestEqual(TEXT("a 小天井 closed at each plot edge"), Wells.Num(), 2))
			{
				for (const FHutongCompoundSlot* Well : Wells)
				{
					TestTrue(TEXT("the well closes on the 耳房's front"),
						FMath::IsNearlyEqual(Well->Min.Y + Well->Size.Y, EarFront, 0.5));
					TestTrue(TEXT("the well can be got into"), Well->WallGateAt >= 0.0);
					// On the 廂房's own inner face.
					const bool bOnWing = Wings.ContainsByPredicate(
						[&](const FHutongCompoundSlot& Wing)
						{
							return FMath::IsNearlyEqual(Wing.Min.X + Wing.Size.X, Well->Min.X, 0.5)
								|| FMath::IsNearlyEqual(Wing.Min.X, Well->Min.X + Well->Size.X, 0.5);
						});
					TestTrue(TEXT("the well's wall stands on the 廂房's inner face"), bOnWing);
				}
			}
		}
	}

	// 十字甬路: a spine to the hall's steps and an arm across to each 廂房.
	HutongGen::FCompoundInput Paths;
	Paths.Plan = EHutongCompoundPlan::TwoCourtyards;
	Paths.Width = 3200.0;
	Paths.Depth = 5200.0;
	const TArray<FHutongCompoundSlot> P = HutongGen::LayOutCompound(Paths)
		.FilterByPredicate([](const FHutongCompoundSlot& S)
			{ return S.Piece == EHutongCompoundPiece::Path; });
	if (!TestEqual(TEXT("甬路 is a cross, not a single run"), P.Num(), 3)) return false;

	// The arms abut the spine rather than crossing it.
	int32 Spines = 0;
	for (const FHutongCompoundSlot& S : P) if (S.Size.Y > S.Size.X) ++Spines;
	TestEqual(TEXT("one spine and two arms"), Spines, 1);
	for (const FHutongCompoundSlot& S : P)
	{
		if (S.Size.Y > S.Size.X) continue;
		const double ArmX0 = S.Min.X, ArmX1 = S.Min.X + S.Size.X;
		for (const FHutongCompoundSlot& Sp : P)
		{
			if (Sp.Size.Y <= Sp.Size.X) continue;
			const double SpX0 = Sp.Min.X, SpX1 = Sp.Min.X + Sp.Size.X;
			TestTrue(TEXT("no arm overlaps the spine"), ArmX1 <= SpX0 + 0.01 || ArmX0 >= SpX1 - 0.01);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundEntryTest,
	"HutongLayout.Compound.Entry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundEntryTest::RunTest(const FString& Parameters)
{
	// 大門 → 門道院 → 外院.
	HutongGen::FCompoundInput In;
	In.Plan = EHutongCompoundPlan::TwoCourtyards;
	In.Width = 3200.0;
	In.Depth = 5200.0;
	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);

	auto First = [&Slots](EHutongCompoundPiece P) -> const FHutongCompoundSlot*
	{
		return Slots.FindByPredicate([P](const FHutongCompoundSlot& S) { return S.Piece == P; });
	};
	const FHutongCompoundSlot* Gate = First(EHutongCompoundPiece::GateHouse);
	const FHutongCompoundSlot* Screen = First(EHutongCompoundPiece::ScreenWall);
	const FHutongCompoundSlot* Row = First(EHutongCompoundPiece::FrontRow);
	if (!TestNotNull(TEXT("there is a 大門"), (void*)Gate)) return false;
	if (!TestNotNull(TEXT("there is a 影壁"), (void*)Screen)) return false;
	if (!TestNotNull(TEXT("there is a 倒座房"), (void*)Row)) return false;

	// It covers the doorway it faces, squarely.
	TestTrue(FString::Printf(TEXT("the 影壁 is at least as wide as the 大門 (%.0f vs %.0f)"),
			Screen->Size.X, Gate->Size.X),
		Screen->Size.X >= Gate->Size.X - 0.01);
	const double Off = FMath::Abs((Screen->Min.X + 0.5 * Screen->Size.X)
		- (Gate->Min.X + 0.5 * Gate->Size.X));
	TestTrue(TEXT("and it stands square in front of it"),
		Off <= 0.5 * (Screen->Size.X - Gate->Size.X) + 1.0);

	// The compartment: a 隔牆 up each side, standing on the street row's inner face.
	const double RowFace = FMath::Max(Row->Size.Y, Gate->Size.Y);
	TArray<const FHutongCompoundSlot*> OnRow;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::Wall) continue;
		if (S.WallRole != EHutongWallRole::Courtyard || !S.bLengthAlongY) continue;
		if (FMath::IsNearlyEqual(S.Min.Y, RowFace, 0.5)) OnRow.Add(&S);
	}

	TArray<const FHutongCompoundSlot*> Cheeks;
	TArray<const FHutongCompoundSlot*> Partitions;
	for (const FHutongCompoundSlot* S : OnRow)
	{
		const bool bAtScreenEnd =
			FMath::IsNearlyEqual(S->Min.X + S->Size.X, Screen->Min.X, 0.5)
			|| FMath::IsNearlyEqual(S->Min.X, Screen->Min.X + Screen->Size.X, 0.5);
		if (bAtScreenEnd) Cheeks.Add(S); else Partitions.Add(S);
	}
	if (!TestEqual(TEXT("the gate court is walled on both sides"), Cheeks.Num(), 2)) return false;

	// Both cheeks carry a doorway.
	int32 Doors = 0;
	for (const FHutongCompoundSlot* C : Cheeks)
	{
		if (C->WallGateAt >= 0.0) ++Doors;
		// They run the full depth of the 外院 to the wall the screen is on.
		TestTrue(TEXT("a cheek reaches the wall the screen backs onto"),
			C->Min.Y + C->Size.Y >= Screen->Min.Y + Screen->Size.Y - 15.0);
	}
	TestEqual(TEXT("both cheeks carry a doorway"), Doors, 2);

	// And the 外院's far end is partitioned off as a service yard, with its own doorway.
	if (!TestEqual(TEXT("the 外院 is divided once"), Partitions.Num(), 1)) return false;
	TestTrue(TEXT("the partition carries a doorway"), Partitions[0]->WallGateAt >= 0.0);
	TestTrue(TEXT("the partition stands beyond the gate court"),
		In.bGateAtEastEnd ? Partitions[0]->Min.X > Screen->Min.X + Screen->Size.X
			: Partitions[0]->Min.X < Screen->Min.X);
	TestTrue(TEXT("the partition runs from the street row inward"),
		FMath::IsNearlyEqual(Partitions[0]->Min.Y, RowFace, 0.5)
			&& Partitions[0]->Size.Y > 150.0);
	// That every slot sits inside the plot is HutongLayout.Compound.Enclosure's, at three plot sizes.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundScreenBackingTest,
	"HutongLayout.Compound.ScreenBacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundScreenBackingTest::RunTest(const FString& Parameters)
{
	// 座山影壁: the screen's back is the cross wall the 垂花門 stands in.
	HutongGen::FCompoundInput In;
	In.Plan = EHutongCompoundPlan::TwoCourtyards;
	In.Width = 3200.0;
	In.Depth = 5200.0;
	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);

	const FHutongCompoundSlot* Screen = Slots.FindByPredicate([](const FHutongCompoundSlot& S)
		{ return S.Piece == EHutongCompoundPiece::ScreenWall; });
	const FHutongCompoundSlot* Inner = Slots.FindByPredicate([](const FHutongCompoundSlot& S)
		{ return S.Piece == EHutongCompoundPiece::InnerGate; });
	if (!TestNotNull(TEXT("there is a 影壁"), (void*)Screen)) return false;
	if (!TestNotNull(TEXT("there is a 垂花門"), (void*)Inner)) return false;

	// The cross wall: the 隔牆 runs across the plot, one either side of the 垂花門.
	TArray<const FHutongCompoundSlot*> Cross;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::Wall) continue;
		if (S.WallRole != EHutongWallRole::Courtyard || S.bLengthAlongY) continue;
		Cross.Add(&S);
	}
	if (!TestEqual(TEXT("the cross wall is two runs and nothing else"), Cross.Num(), 2)) return false;

	const double CrossFace = Cross[0]->Min.Y;
	for (const FHutongCompoundSlot* C : Cross)
	{
		TestTrue(TEXT("both cross runs are on one line"),
			FMath::IsNearlyEqual(C->Min.Y, CrossFace, 0.5));
	}

	// The footprint has to reach that face and go past it.
	const double Back = Screen->Min.Y + Screen->Size.Y;
	const double Over = Back - CrossFace;
	TestTrue(FString::Printf(
			TEXT("the 影壁 reaches the cross wall (back %.1f vs face %.1f)"), Back, CrossFace),
		Over >= -0.01);
	TestTrue(FString::Printf(TEXT("the plinth is buried (%.1f cm past the face)"), Over),
		Over >= In.ScreenPlinthProjection - 0.01);
	TestTrue(FString::Printf(TEXT("and the body with it, not merely tangent (%.1f cm)"), Over),
		Over > In.ScreenPlinthProjection + 0.01);
	TestTrue(FString::Printf(TEXT("but not driven out the far side (%.1f cm into %.1f cm of wall)"),
			Over, In.CourtyardWallThickness),
		Over < In.CourtyardWallThickness);

	// It faces the 大門 across the court.
	const double SX0 = Screen->Min.X, SX1 = Screen->Min.X + Screen->Size.X;
	const double IX0 = Inner->Min.X,  IX1 = Inner->Min.X + Inner->Size.X;
	TestTrue(TEXT("the 影壁 does not stand across the 垂花門"), SX1 <= IX0 + 0.01 || SX0 >= IX1 - 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCourtyardFurnishingTest,
	"HutongLayout.Courtyard.Furnishing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCourtyardFurnishingTest::RunTest(const FString& Parameters)
{
	auto SlotCounts = [](const UE::Geometry::FDynamicMesh3& Mesh, TArray<int32>& Out)
	{
		Out.Init(0, HutongGen::MatSlot_Count);
		const UE::Geometry::FDynamicMeshMaterialAttribute* Mat =
			Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			const int32 ID = Mat ? Mat->GetValue(tid) : 0;
			if (Out.IsValidIndex(ID)) ++Out[ID];
		}
	};

	// 花池: a kerb retaining earth.
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		FHutongFlowerBedParams P;
		UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(P, 220.0, 150.0, Mesh);
		if (!TestTrue(TEXT("花池 builds"), Mesh.TriangleCount() > 0)) return false;

		TArray<int32> Counts;
		SlotCounts(Mesh, Counts);
		TestTrue(TEXT("花池 retains earth"), Counts[HutongGen::MatSlot_Earth] > 0);
		TestTrue(TEXT("花池 has a kerb"), Counts[HutongGen::MatSlot_BaseCourse] > 0);

		// The soil sits below the kerb's top, or the two share a plane across the whole bed.
		double TopKerb = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr()) TopKerb = FMath::Max(TopKerb, Mesh.GetVertex(vid).Z);
		TestTrue(TEXT("the kerb stands above the soil"),
			TopKerb >= P.KerbHeight - 0.01);
	}

	// 魚缸: a solid of revolution, so the check that matters is that the sweep closed.
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		FHutongWaterJarParams P;
		UHutongWaterJarBuildingComponent::BuildWaterJarMesh(P, Mesh);
		if (!TestTrue(TEXT("魚缸 builds"), Mesh.TriangleCount() > 0)) return false;

		// It stands within its own declared footprint.
		const double Span = P.GetFootprint();
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			const FVector3d V = Mesh.GetVertex(vid);
			TestTrue(TEXT("the 魚缸 stays inside its footprint"),
				V.X >= -0.01 && V.X <= Span + 0.01 && V.Y >= -0.01 && V.Y <= Span + 0.01);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundSlotAxisTest,
	"HutongLayout.Compound.SlotAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundSlotAxisTest::RunTest(const FString& Parameters)
{
	// A line-like slot states its own run direction, and it has to match its shape.
	for (EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		// Sized off the plan's own minimum.
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		In.Width = MinW + 400.0;
		In.Depth = MinD + 400.0;

		const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		TestTrue(TEXT("the plan produced something to check"), Slots.Num() > 0);

		for (const FHutongCompoundSlot& S : Slots)
		{
			const bool bLineLike = S.Piece == EHutongCompoundPiece::Wall
				|| S.Piece == EHutongCompoundPiece::Path
				|| S.Piece == EHutongCompoundPiece::Corridor
				|| S.Piece == EHutongCompoundPiece::Passage
				|| S.Piece == EHutongCompoundPiece::ScreenWall;
			if (!bLineLike) continue;

			// A run is by construction longer than it is thick; anything else means the slot came out square enough that "whichever is longer" was never a safe way to ask.
			TestTrue(FString::Printf(
					TEXT("piece %d at (%.0f,%.0f) size (%.0f,%.0f) runs the way it says"),
					int32(S.Piece), S.Min.X, S.Min.Y, S.Size.X, S.Size.Y),
				S.bLengthAlongY ? (S.Size.Y > S.Size.X) : (S.Size.X > S.Size.Y));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundRearCourtTest,
	"HutongLayout.Compound.RearCourt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundRearCourtTest::RunTest(const FString& Parameters)
{
	// 三進: a 後院 behind the 正房 closed at the back by the 後罩房, reached by a 過道 at the plot edge past the 耳房 on the gate's own side.
	HutongGen::FCompoundInput In;
	In.Plan = EHutongCompoundPlan::ThreeCourtyards;
	double MinW, MinD;
	In.GetMinimumPlot(MinW, MinD);
	In.Width = MinW + 400.0;
	In.Depth = MinD + 400.0;

	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
	if (!TestTrue(TEXT("the 三進 plan lays out"), Slots.Num() > 0)) return true;

	// The 後罩房 closes the back: full width, its own back on the plot's north edge.
	int32 RearRows = 0;
	double RearFront = 0.0;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::RearRow) continue;
		++RearRows;
		RearFront = S.Min.Y;
		TestTrue(TEXT("the 後罩房 runs the plot's full width"),
			S.Min.X < 0.01 && FMath::Abs(S.Min.X + S.Size.X - In.Width) < 0.01);
		TestTrue(TEXT("the 後罩房's back is the plot's north edge"),
			FMath::Abs(S.Min.Y + S.Size.Y - In.Depth) < 0.01);
		TestTrue(TEXT("the 後罩房 faces south, onto the 後院"), S.Facing == EHutongBaySide::MinusY);
	}
	TestEqual(TEXT("exactly one 後罩房"), RearRows, 1);

	// The hall row's own north face, and the court between it and the 後罩房.
	double HallNorth = 0.0;
	for (const FHutongCompoundSlot& S : Slots)
	{
		const bool bHallRow = S.Piece == EHutongCompoundPiece::MainHall
			|| (S.Piece == EHutongCompoundPiece::EarRoom && S.Facing == EHutongBaySide::MinusY);
		if (bHallRow) HallNorth = FMath::Max(HallNorth, S.Min.Y + S.Size.Y);
	}
	TestTrue(TEXT("the 正房 comes off the north boundary"), HallNorth < In.Depth - 100.0);
	TestTrue(TEXT("there is a 後院 between the hall row and the 後罩房"),
		RearFront - HallNorth > 150.0);

	// Exactly one of the hall's two 耳房 is held off the boundary, leaving the 過道; the other still runs out to it.
	int32 EarsAtEdge = 0, EarsHeldBack = 0;
	double PassageAt = -1.0;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::EarRoom) continue;
		if (S.Facing != EHutongBaySide::MinusY) continue;   // the wing's ears face across the court

		const bool bLow = S.Min.X < 0.5 * In.Width;
		const double Outer = bLow ? S.Min.X : (In.Width - (S.Min.X + S.Size.X));
		if (Outer < 0.01) ++EarsAtEdge;
		else { ++EarsHeldBack; PassageAt = bLow ? 0.0 : In.Width; }
	}
	TestEqual(TEXT("one 耳房 still runs to the boundary"), EarsAtEdge, 1);
	TestEqual(TEXT("one 耳房 is held back for the 過道"), EarsHeldBack, 1);
	// East is local -X, so the gate's side is the low-X end when the gate is at the east.
	TestTrue(TEXT("the 過道 is on the gate's own side"),
		In.bGateAtEastEnd ? (PassageAt == 0.0) : (PassageAt == In.Width));

	// The 過道 is covered: one roof over the strip, running from the 隔牆 that closes it to the 後罩房 across the back.
	int32 Passages = 0;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::Passage) continue;
		++Passages;
		TestTrue(TEXT("the 過道's roof runs along the plot's depth"), S.bLengthAlongY);
		TestTrue(TEXT("the 過道's roof reaches the 後罩房"),
			FMath::Abs(S.Min.Y + S.Size.Y - RearFront) < 0.01);
		TestTrue(TEXT("the 過道's roof covers the 隔牆 that closes it"), S.Min.Y <= HallNorth);
		// It bears into the wall at each side.
		const bool bLow = S.Min.X < 0.5 * In.Width;
		TestTrue(TEXT("the 過道's roof bears into the 院牆"),
			bLow ? (S.Min.X < In.WallThickness) : (S.Min.X + S.Size.X > In.Width - In.WallThickness));

		// And it builds: a roof and nothing else, so every vertex sits at or above its eave.
		FHutongPassageParams Pass;
		UE::Geometry::FDynamicMesh3 Mesh;
		UHutongPassageBuildingComponent::BuildPassageMesh(
			Pass, S.Size.Y, S.Size.X - 2.0 * Pass.Bearing, /*bAlongY*/ true, Mesh);
		TestTrue(TEXT("the 過道's roof builds geometry"), Mesh.TriangleCount() > 0);
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			TestTrue(TEXT("nothing of the 過道 hangs below its eave"),
				Mesh.GetVertex(vid).Z >= Pass.EaveHeight - 0.01);
		}
	}
	TestEqual(TEXT("exactly one 過道"), Passages, 1);

	// And the strip is closed onto the 內院 by a 隔牆 with a doorway.
	bool bGatedPassage = false;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::Wall) continue;
		if (S.WallRole != EHutongWallRole::Courtyard || S.bLengthAlongY) continue;
		if (S.WallGateAt < 0.0) continue;
		// The run that crosses the passage strip, as opposed to the cross wall further south.
		if (S.Min.Y > HallNorth - 400.0 && S.Min.Y < HallNorth) bGatedPassage = true;
	}
	TestTrue(TEXT("the 過道 is closed onto the 內院 by a gated 隔牆"), bGatedPassage);

	// And no other plan grows one: the 後罩房 belongs to the 三進 alone.
	for (EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards })
	{
		HutongGen::FCompoundInput Other;
		Other.Plan = Plan;
		double W2, D2;
		Other.GetMinimumPlot(W2, D2);
		Other.Width = W2 + 400.0;
		Other.Depth = D2 + 400.0;
		for (const FHutongCompoundSlot& S : HutongGen::LayOutCompound(Other))
		{
			TestTrue(TEXT("only the 三進 plan places a 後罩房"),
				S.Piece != EHutongCompoundPiece::RearRow);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundWallJunctionsTest,
	"HutongLayout.Compound.WallJunctions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundWallJunctionsTest::RunTest(const FString& Parameters)
{
	// An internal wall meeting the perimeter must stop inside it, never on the plot boundary.
	for (EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		In.Width = MinW + 400.0;
		In.Depth = MinD + 400.0;
		const double T = In.WallThickness;

		// An end either stops at or before the perimeter's inner face.
		auto EndIsSound = [&](double End, double Extent)
		{
			const bool bNearLow  = End < T - 0.01;
			const bool bNearHigh = End > Extent - T + 0.01;
			if (bNearLow)  return End > 0.01 && End < T - 0.01;
			if (bNearHigh) return End < Extent - 0.01 && End > Extent - T + 0.01;
			return true;
		};

		const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		TestTrue(TEXT("the plan produced something to check"), Slots.Num() > 0);

		for (const FHutongCompoundSlot& S : Slots)
		{
			if (S.Piece != EHutongCompoundPiece::Wall) continue;
			if (S.WallRole != EHutongWallRole::Courtyard) continue;

			const FString Where = FString::Printf(
				TEXT("隔牆 at (%.1f,%.1f) size (%.1f,%.1f)"),
				S.Min.X, S.Min.Y, S.Size.X, S.Size.Y);

			// Only the ends matter: a run's thickness never reaches the boundary on its own.
			if (S.bLengthAlongY)
			{
				TestTrue(Where + TEXT(" starts clear of the plot's south edge"),
					EndIsSound(S.Min.Y, In.Depth));
				TestTrue(Where + TEXT(" stops clear of the plot's north edge"),
					EndIsSound(S.Min.Y + S.Size.Y, In.Depth));
			}
			else
			{
				TestTrue(Where + TEXT(" starts clear of the plot's east edge"),
					EndIsSound(S.Min.X, In.Width));
				TestTrue(Where + TEXT(" stops clear of the plot's west edge"),
					EndIsSound(S.Min.X + S.Size.X, In.Width));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCorridorFootprintTest,
	"HutongLayout.Compound.CorridorFootprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A slot is reserved from GetFootprintDepth and the mesh is laid in it, so a platform deeper than
// that figure is a run standing in the building it was laid against — and asymmetrically, since a
// flipped run yaws about half the reported depth.
bool FHutongCorridorFootprintTest::RunTest(const FString& Parameters)
{
	for (const bool bClosed : { false, true })
	{
		for (const bool bWall : { false, true })
		{
			FHutongCorridorParams P;
			P.bClosedSide = bClosed;
			P.bBuildBackWall = bWall;
			P.Length = 900.0;

			UE::Geometry::FDynamicMesh3 M;
			UHutongCorridorBuildingComponent::BuildCorridorMesh(
				P, P.Length, /*bAlongY*/ false, /*bFlip*/ false, M);

			// The 臺基 and everything standing on it, not the roof: an eave oversails its own
			// footprint here as it does everywhere, and that is what a footprint excludes.
			const double Floor = FMath::Clamp(P.FloorHeight, 0.0, P.GetEaveHeight() * 0.3);
			double MinY = BIG_NUMBER, MaxY = -BIG_NUMBER;
			for (int32 vid : M.VertexIndicesItr())
			{
				const FVector3d V = M.GetVertex(vid);
				if (V.Z > Floor + 0.01) continue;
				MinY = FMath::Min(MinY, V.Y);
				MaxY = FMath::Max(MaxY, V.Y);
			}

			const FString Name = FString::Printf(TEXT("遊廊 closed=%d wall=%d"),
				bClosed ? 1 : 0, bWall ? 1 : 0);
			TestTrue(FString::Printf(TEXT("%s: the platform starts at the footprint's near edge (%.1f)"),
				*Name, MinY), MinY >= -0.01);
			TestTrue(FString::Printf(
				TEXT("%s: the platform stays inside the reported footprint (%.1f of %.1f cm)"),
				*Name, MaxY, P.GetFootprintDepth()),
				MaxY <= P.GetFootprintDepth() + 0.01);
		}
	}

	// And the round trip holds: a footprint converted to a walk and back is the footprint.
	{
		FHutongCorridorParams P;
		P.bClosedSide = true;
		P.bBuildBackWall = true;
		// Inside the walk's own band, where the conversion is not clamping.
		const double Footprint = 200.0;
		P.Width = P.WalkWidthFromFootprint(Footprint);
		TestEqual(TEXT("footprint → walk → footprint"), P.GetFootprintDepth(), Footprint, 0.01);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundBenchGapTest,
	"HutongLayout.Compound.BenchGap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundBenchGapTest::RunTest(const FString& Parameters)
{
	// The 甬路 arrives at a 廂房's door and the walk is what it arrives at. With the 坐凳楣子 unbroken the paving ends against a seat and the building cannot be entered at all.
	HutongGen::FCompoundInput In;
	In.Plan = EHutongCompoundPlan::TwoCourtyards;
	In.Width = 3200.0;
	In.Depth = 5200.0;
	In.CourtWalk = EHutongCourtWalk::Corridor;
	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);

	// The two runs down the court, which are the ones a 甬路 arm meets.
	TArray<const FHutongCompoundSlot*> Sides;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece == EHutongCompoundPiece::Corridor && S.bLengthAlongY) Sides.Add(&S);
	}
	if (!TestEqual(TEXT("two 遊廊 run down the court"), Sides.Num(), 2)) return false;

	// The arms: 甬路 runs wider than they are deep. The spine runs the other way.
	TArray<const FHutongCompoundSlot*> Arms;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece == EHutongCompoundPiece::Path && S.Size.X > S.Size.Y) Arms.Add(&S);
	}
	if (!TestEqual(TEXT("the 甬路 throws an arm to each 廂房"), Arms.Num(), 2)) return false;

	const double ArmY = Arms[0]->Min.Y + 0.5 * Arms[0]->Size.Y;
	for (const FHutongCompoundSlot* A : Arms)
	{
		TestTrue(TEXT("both arms land on one line"),
			FMath::IsNearlyEqual(A->Min.Y + 0.5 * A->Size.Y, ArmY, 0.5));
	}

	for (const FHutongCompoundSlot* S : Sides)
	{
		if (!TestTrue(TEXT("a side run names where its bench breaks"),
			S->CorridorBenchGapAt >= 0.0 && S->CorridorBenchGapAt <= 1.0))
		{
			continue;
		}
		// The fraction is stated in footprint terms on both runs.
		const double GapY = S->Min.Y + S->CorridorBenchGapAt * S->Size.Y;
		TestTrue(FString::Printf(
				TEXT("the break is where the 甬路 arrives (%.0f vs arm at %.0f)"), GapY, ArmY),
			FMath::Abs(GapY - ArmY) <= In.CorridorWalkWidth);
	}
	return true;
}


// A wall's plotted thickness is a fact about its footprint: taken when the footprint is narrower
// than the role asks, dropped when it is not, so a narrowed run can be widened again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallPlottedThicknessTest,
	"HutongLayout.Walls.PlottedThickness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallPlottedThicknessTest::RunTest(const FString& Parameters)
{
	UHutongWallBuildingComponent* W = NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	W->Length = 500.0;
	W->bLengthAlongY = false;
	const double Asked = W->Params.GetThickness();

	W->SetFootprintSize(FVector2D(500.0, Asked - 10.0));
	TestNearlyEqual(TEXT("a narrower footprint caps the wall"), W->GetBuiltThickness(), Asked - 10.0, 0.01);
	TestNearlyEqual(TEXT("and the footprint reports the cap"), W->GetFootprintSize().Y, Asked - 10.0, 0.01);

	W->SetFootprintSize(FVector2D(500.0, Asked + 20.0));
	TestNearlyEqual(TEXT("a wider one gives the role's figure back"), W->GetBuiltThickness(), Asked, 0.01);
	TestEqual(TEXT("with no cap left standing"), W->FootprintThickness, 0.0);

	const FVector2D Reported = W->GetFootprintSize();
	W->SetFootprintSize(Reported);
	TestTrue(TEXT("the round trip is idempotent"), W->GetFootprintSize().Equals(Reported, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongChitouCoversColumnTest,
	"HutongLayout.Detailing.ChitouCoversColumn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongChitouCoversColumnTest::RunTest(const FString& Parameters)
{
	// The corner 檐柱 stands on the wall plane, so its foot reaches a radius forward of it; the
	// 墀頭 wrapping it must reach further or the column shows through the pier's face as a wedge.
	// Probed inside the column's own footprint, between the pier's face and the column's centre.
	auto Probe = [&](const TCHAR* Label, const UE::Geometry::FDynamicMesh3& M,
		double W, double ColR, double Proj, double Floor)
	{
		double MinY = BIG_NUMBER;
		for (int32 vid : M.VertexIndicesItr())
		{
			const FVector3d V = M.GetVertex(vid);
			const bool bInFoot = (V.X > 0.5 && V.X < ColR - 0.5) || (V.X > W - ColR + 0.5 && V.X < W - 0.5);
			if (!bInFoot || V.Z < Floor + 5.0 || V.Z > Floor + 100.0) continue;
			MinY = FMath::Min(MinY, V.Y);
		}
		TestTrue(FString::Printf(TEXT("%s: nothing at the column's foot stands forward of the 墀頭 face (%.1f vs %.1f)"),
			Label, MinY, -Proj), MinY >= -Proj - 0.01);
	};

	{
		FHutongGateHouseParams P;
		P.Width = 380.0; P.Depth = 400.0;
		P.ChitouProjection = 2.0;
		const double ColR = 0.5 * P.GetColumnDiameter();
		TestTrue(TEXT("gate: the projection is floored at the column radius plus clearance"),
			P.GetChitouProjection() >= ColR + HutongCanon::Wall::ChitouColumnClearanceCm - 0.01);
		P.ChitouProjection = 40.0;
		TestEqual(TEXT("gate: a projection already past the column is left alone"),
			P.GetChitouProjection(), 40.0);
		P.ChitouProjection = 2.0;
		UE::Geometry::FDynamicMesh3 M;
		HutongGen::BuildGateHouse(M, P);
		Probe(TEXT("gate"), M, P.Width, ColR, P.GetChitouProjection(), P.FloorHeight);
	}
	{
		FHutongSiheyuanParams P;
		P.Width = 1040.0; P.Depth = 300.0;
		P.ChitouProjection = 2.0;
		const double ColR = P.GetColumnRadius();
		TestTrue(TEXT("house: the projection is floored at the column radius plus clearance"),
			P.GetChitouProjection() >= ColR + HutongCanon::Wall::ChitouColumnClearanceCm - 0.01);
		UE::Geometry::FDynamicMesh3 M;
		HutongGen::BuildSiheyuan(M, P);
		Probe(TEXT("house"), M, P.Width, ColR, P.GetChitouProjection(), P.GetFloorHeight());
	}
	{
		FHutongShopfrontParams P;
		P.ChitouProjection = 2.0;
		const double ColR = P.GetColumnRadiusFor(P.Width, P.Depth);
		TestTrue(TEXT("shop: the projection is floored at the column radius plus clearance"),
			P.GetChitouProjectionFor(P.Width, P.Depth) >= ColR + HutongCanon::Wall::ChitouColumnClearanceCm - 0.01);
		UE::Geometry::FDynamicMesh3 M;
		HutongGen::BuildShopfront(M, P);
		Probe(TEXT("shop"), M, P.Width, ColR, P.GetChitouProjectionFor(P.Width, P.Depth), P.FloorHeight);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongEaveUndersideTest,
	"HutongLayout.Materials.EaveUndersideIsWood",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongEaveUndersideTest::RunTest(const FString& Parameters)
{
	// Through the component's own build, which is where the retag runs: the top of the roof stays
	// tile and the soffit under the front eave is wood. Pinned on where the faces are, not on
	// which way this mesh's normals point, since that is exactly what went wrong once.
	UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	B->DetailLevel = EHutongDetail::Near;
	B->bBuildLODChain = false;
	B->SetFootprintSize(FVector2D(1040.0, 600.0));
	TArray<UE::Geometry::FDynamicMesh3> LODs;
	B->BuildLODs(LODs);
	if (!TestTrue(TEXT("the house builds"), LODs.Num() == 1 && LODs[0].TriangleCount() > 0)) return false;
	const UE::Geometry::FDynamicMesh3& M = LODs[0];
	const UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = M.Attributes()->GetMaterialID();
	if (!TestNotNull(TEXT("materials are tagged"), MatIDs)) return false;

	const double Eave = B->Params.GetEaveHeight();
	int32 TopTri = -1;
	double TopZ = -1e9;
	int32 SoffitWood = 0, SoffitTile = 0;
	for (int32 tid : M.TriangleIndicesItr())
	{
		const int32 Slot = MatIDs->GetValue(tid);
		if (Slot != HutongGen::MatSlot_Roof && Slot != HutongGen::MatSlot_Wood) continue;
		const FVector3d C = M.GetTriCentroid(tid);
		if (Slot == HutongGen::MatSlot_Roof && C.Z > TopZ) { TopZ = C.Z; TopTri = tid; }
		// Under the front eave: outside the footprint's front edge, below the eave line, and
		// looking down — which in this mesh's winding is a normal pointing up. The tops of the
		// drip tiles hang below the eave line too and are rightly still tile.
		const FVector3d N = M.GetTriNormal(tid);
		if (C.Y < -20.0 && C.Z < Eave - 5.0 && C.Z > 0.5 * Eave && N.Z > 0.3)
		{
			if (Slot == HutongGen::MatSlot_Wood) ++SoffitWood; else ++SoffitTile;
		}
	}
	TestTrue(TEXT("the ridge is still tile"), TopTri >= 0);
	TestTrue(FString::Printf(TEXT("the soffit under the front eave is wood (%d wood, %d tile)"), SoffitWood, SoffitTile), SoffitWood > 0 && SoffitTile == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRolledXieshanCrownTest,
	"HutongLayout.Roofs.RolledXieshanCrown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRolledXieshanCrownTest::RunTest(const FString& Parameters)
{
	// A 捲棚歇山 has no ridge: its crown is a fillet below the sharp fold, and nothing on the roof
	// stands up to where the fold was — the rakes ride over the crown, a course above it, and
	// that is still under the sharp roof's ridge course.
	auto Build = [](double Roll, UE::Geometry::FDynamicMesh3& M)
	{
		FHutongHallParams P;
		P.RoofType = EHutongRoofType::Xieshan;
		P.RoofApexRoll = Roll;
		P.Width = 1000.0; P.Depth = 660.0;
		HutongGen::BuildHall(M, P);
	};
	UE::Geometry::FDynamicMesh3 Sharp, Rolled;
	Build(0.0, Sharp);
	Build(0.35, Rolled);
	if (!TestTrue(TEXT("both build"), Sharp.TriangleCount() > 0 && Rolled.TriangleCount() > 0)) return false;

	auto MaxZ = [](const UE::Geometry::FDynamicMesh3& M)
	{
		double Top = -1e9;
		for (int32 vid : M.VertexIndicesItr()) Top = FMath::Max(Top, M.GetVertex(vid).Z);
		return Top;
	};
	const double SharpTop = MaxZ(Sharp), RolledTop = MaxZ(Rolled);
	TestTrue(FString::Printf(TEXT("the roll lowers the top (%.1f sharp, %.1f rolled)"), SharpTop, RolledTop), RolledTop < SharpTop - 5.0);

	// The highest vertices of the rolled roof lie along the crown across the whole ridge line —
	// not at two points at the ends, which is what a 山花 or a rake standing above the roll looks like.
	double MinX = 1e9, MaxX = -1e9;
	for (int32 vid : Rolled.VertexIndicesItr())
	{
		const FVector3d V = Rolled.GetVertex(vid);
		if (V.Z > RolledTop - 2.0) { MinX = FMath::Min(MinX, V.X); MaxX = FMath::Max(MaxX, V.X); }
	}
	TestTrue(FString::Printf(TEXT("the crown runs along the ridge line (%.0f to %.0f of 1000)"), MinX, MaxX), MaxX - MinX > 300.0);
	// Rolled and sharp are closed the same way; solidity is the shell's own test's business.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongBaseCourseLineTest,
	"HutongLayout.Detailing.BaseCourseLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongBaseCourseLineTest::RunTest(const FString& Parameters)
{
	// Every kind with a 下鹼 tops it out at the one canon line above the ground by default,
	// whatever its own floor or plinth is, so pieces placed one at a time along a frontage read
	// as one band; a courtyard wall keeps its own lower third. And the line can be set from a
	// neighbour's and read back.
	using namespace HutongCanon;
	TArray<UClass*> Classes;
	GetDerivedClasses(UHutongBuildingComponent::StaticClass(), Classes, /*bRecursive*/ true);
	int32 Banded = 0;
	for (UClass* Class : Classes)
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
		UHutongBuildingComponent* Probe = NewObject<UHutongBuildingComponent>(GetTransientPackage(), Class);
		const double Top = Probe->GetBaseCourseTop();
		if (Top <= 0.0) continue;
		++Banded;
		TestTrue(FString::Printf(TEXT("%s tops its 下鹼 out at the canon line (%.1f)"), *Class->GetName(), Top),
			FMath::IsNearlyEqual(Top, BaseCourse::TopCm, 0.5));
		Probe->SetBaseCourseTop(BaseCourse::TopCm + 20.0);
		TestTrue(FString::Printf(TEXT("%s takes a neighbour's line"), *Class->GetName()),
			FMath::IsNearlyEqual(Probe->GetBaseCourseTop(), BaseCourse::TopCm + 20.0, 0.5));
	}
	TestEqual(TEXT("eight kinds carry a 下鹼"), Banded, 8);

	FHutongWallParams Court;
	Court.Role = EHutongWallRole::Courtyard;
	TestTrue(TEXT("a courtyard wall's band is its lower third"),
		FMath::IsNearlyEqual(Court.GetBaseCourseHeight(), Court.GetHeight() / 3.0, 0.5));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
