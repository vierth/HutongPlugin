#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongMeshInspect.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Generation/HallGenerator.h"
#include "Generation/PavilionGenerator.h"
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
#include "Generation/HutongCanon.h"
#include "Generation/FrameGenerator.h"
#include "Generation/HutongUrban.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongRidge.h"
#include "Misc/AutomationTest.h"
#include "Generation/HutongPalette.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

#if WITH_DEV_AUTOMATION_TESTS

// Roof primitives must be closed, two-manifold and wound outward.
namespace
{
	using UE::Geometry::FDynamicMesh3;

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
		// Square plan, flare pushed into its clamps on the shorter eave.
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
		// Odd segment counts, so the 收山 row falls off a natural division.
		FXieshanRoofSpec S = Base();
		S.SlopeSegments = 7; S.EaveSegments = 3; S.ShouInset = 61.0;
		Cases.Add({ TEXT("odd segments"), S });
	}
	{
		// 捲棚歇山: rakes reach the crown horizontally and meet in an arc.
		FXieshanRoofSpec S = Base();
		S.Section.ApexRoll = 0.35;
		S.FlareLength = 130.0; S.FlareRun = 55.0; S.FlareRise = 40.0;
		Cases.Add({ TEXT("捲棚 crown"), S });
	}
	{
		// A roll deep enough to pass the 收山 line and round the hipped skirt's top too.
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

	// 勾頭 round a flared eave: ~100 separate closed solids in the fascia band, leaving the shell undisturbed.
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

	// Strips are separate closed solids touching the shell.
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
	// Not a closure test: a building is deliberately a heap of overlapping solids.
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
		// 100 x 0.5 + 100 x 0.7: the sum the rise comes from.
		TestEqual(TEXT("五檁: rise is 舉 x run, summed"), S.Rise(), 120.0);
		TestEqual(TEXT("五檁: eave sits on the eave line"), S.HeightAtDistanceFromRidge(200.0), 0.0);
		// Crease between 檐步 and 脊步 at half the span: 100 cm run at 五舉.
		TestEqual(TEXT("五檁: the 步架 crease is at 五舉"), S.HeightAtDistanceFromRidge(100.0), 50.0);
		TestEqual(TEXT("五檁: ridge is the full rise"), S.HeightAtDistanceFromRidge(0.0), 120.0);
		// Halfway up the straight eave step: exactly half its rise (a curve would not be).
		TestEqual(TEXT("五檁: the 檐步 is straight"), S.HeightAtDistanceFromRidge(150.0), 25.0);
	}

	// The eave overhang is a leading segment at the eave step's 舉.
	{
		const FHutongRoofSection S = Jiajia::MakeSectionWithRatios({ 0.5, 0.7 }, 200.0, 60.0, 0.0);
		TestEqual(TEXT("overhang: adds to the span"), S.HalfSpan(), 260.0);
		TestEqual(TEXT("overhang: adds its own run at 五舉"), S.Rise(), 150.0);
		TestEqual(TEXT("overhang: the 檐檁 sits at the overhang's rise"),
			S.HeightAtDistanceFromRidge(200.0), 30.0);
	}

	// Depth = (檁數 - 1) x 步架, which is why purlin count is the input.
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
		// Sharp: crown is the fold. Rolled: a fillet below it, still above the band join.
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
	// 勾頭 on 筒瓦, not 合瓦: the rank rule must show in geometry, not an unread flag.
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
	// Logged, not asserted.
	UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(TEXT("合瓦 %d tris, 筒瓦 %d tris (+%d for the 勾頭)"), He, Tong, Tong - He));

	// The rank rule is wired to the gate styles.
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

	// Wider bay, taller column.
	FHutongSiheyuanParams Wide = P;
	Wide.Width = 1300.0;
	Wide.BayCountOverride = 3;
	TestTrue(TEXT("a wider bay raises the eave"), Wide.GetEaveHeight() > P.GetEaveHeight());

	FHutongSiheyuanParams More = P;
	More.Width = 1300.0;                            // more bays, not wider ones
	TestTrue(TEXT("a longer row of the same bays does not"),
		More.GetEaveHeight() < Wide.GetEaveHeight());

	// 柱高 floor holds the smallest type up; the ratio governs the rest.
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

	// Proportions are not overridden.
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

		// Doorway result, logged.
		const double Clear = Preset.GetDoorLeafTopHeight() - Preset.GetFloorHeight()
			- Preset.GetLowerSillHeight();
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
	// An eave shows two courses: round 檐椽 tipped with square 飛椽. It once drew one.
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
	// Every slot named, no two alike.
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

	// A mesh carries only the slots it wears: the converter packs material IDs densely into polygon groups up to the highest used.
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

		// IDs now run 0..N-1, so the section list is the slot list.
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

	// 甬路: three boxes, two surfaces.
	{
		FHutongPathParams Path;
		UE::Geometry::FDynamicMesh3 M;
		UHutongPathBuildingComponent::BuildPathMesh(Path, 700.0, 150.0, false, M);
		const int32 Slots = Compacted(M, TEXT("a 甬路"));
		TestTrue(FString::Printf(TEXT("a 甬路 wears few slots (%d)"), Slots),
			Slots > 0 && Slots < HutongGen::MatSlot_Count);
	}

	// A 正房 wears every slot, so compacting must change nothing.
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
	// An untagged slot is an empty section and a useless swatch.
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

	// The 下鹼 must not swallow the wall above, nor the leaves the frame.
	TestTrue(TEXT("the body is still the largest surface"),
		Count[HutongGen::MatSlot_Body] > Count[HutongGen::MatSlot_BaseCourse]);

	// 門漆 is the leaves and nothing else.
	TestTrue(FString::Printf(TEXT("門漆 covers the leaves only (%d vs %d wood)"),
			Count[HutongGen::MatSlot_DoorPaint], Count[HutongGen::MatSlot_Wood]),
		Count[HutongGen::MatSlot_DoorPaint] < Count[HutongGen::MatSlot_Wood]);

	// The 殿 also has a door mid bay loop; it once tagged its brick 檻牆 as timber and used neither window slot.
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
	// Gates open inward: leaves add depth courtyard-side only.
	auto Reach = [](bool bOpen, double& OutStreet, double& OutCourt)
	{
		FHutongWallParams P;
		P.Length = 700.0;
		P.bHasGate = true;
		P.bGateLeavesOpen = bOpen;
		// Walls carry no 散水, so nothing outreaches the leaves; this measures the doors.

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

	// The swung leaf clears its pivot stone; the pivot stands inboard of the leaf edge.
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

		// Stone reveal reaches X = Reveal from the left jamb.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRuyiDoorHeadTest,
	"HutongLayout.Doors.RuyiDoorHead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRuyiDoorHeadTest::RunTest(const FString& Parameters)
{
	// The 如意門's 門頭 is a 朝天欄杆: ledges stepping out to the 蓋板, the 欄板 standing back on it, the
	// dentils and top course out again under the roof. Read off rays from the lane at mid-span, carved and plain.
	for (const bool bCarved : { true, false })
	{
		FHutongGateHouseParams P;
		P.Style = EHutongGateStyle::Ruyi;
		P.bCarvedDoorHead = bCarved;
		const FHutongGateHouseParams::FSizeRange R = P.GetSizeRange();
		P.Width = 0.5 * (R.FrontageMin + R.FrontageMax);
		P.Depth = 0.5 * (R.DepthMin + R.DepthMax);
		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildGateHouse(Mesh, P);
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);

		TArray<double> Faces;
		// Short of the roof base: the eave's fascia hangs below it.
		for (double Z = P.GetDoorHeadHeight() + 2.0; Z < P.GetRoofBaseHeight() - 10.0; Z += 1.0)
		{
			const FRay3d Ray(FVector3d(0.5 * P.Width, -500.0, Z), FVector3d(0.0, 1.0, 0.0));
			double T = 0.0;
			int32 Tri = -1;
			if (Tree.FindNearestHitTriangle(Ray, T, Tri)) Faces.Add(Ray.PointAt(T).Y);
		}
		const TCHAR* Kind = bCarved ? TEXT("carved") : TEXT("plain");
		if (!TestTrue(FString::Printf(TEXT("%s: the frieze is hit"), Kind), Faces.Num() > 10)) return false;
		// Going up: the most proud face so far is the 蓋板 until one stands back from it (the 欄板), then out again.
		const double Step = 0.3 * P.DoorHeadProjection;
		int32 Cover = 0, Back = INDEX_NONE, Out = INDEX_NONE;
		for (int32 i = 1; i < Faces.Num() && Out == INDEX_NONE; ++i)
		{
			if (Back == INDEX_NONE)
			{
				if (Faces[i] < Faces[Cover]) Cover = i;
				else if (Faces[i] > Faces[Cover] + Step) Back = i;
			}
			else if (Faces[i] < Faces[Back] - Step) Out = i;
		}
		TestTrue(FString::Printf(TEXT("%s: the 欄板 stands back on the 蓋板, the dentils out again (cm %d, %d, %d)"), Kind, Cover, Back, Out),
			Back != INDEX_NONE && Out != INDEX_NONE);
		const double Reach = FMath::Max(Faces) - FMath::Min(Faces);
		TestTrue(FString::Printf(TEXT("%s: the courses stay within the head's projection (%.1f cm)"), Kind, Reach),
			Reach <= P.DoorHeadProjection + 1.5);
	}
	FHutongGateHouseParams Carved, Plain;
	Carved.Style = Plain.Style = EHutongGateStyle::Ruyi;
	Plain.bCarvedDoorHead = false;
	UE::Geometry::FDynamicMesh3 A, B;
	HutongGen::BuildGateHouse(A, Carved);
	HutongGen::BuildGateHouse(B, Plain);
	TestTrue(FString::Printf(TEXT("carving adds geometry (%d vs %d tris)"), A.TriangleCount(), B.TriangleCount()),
		A.TriangleCount() > B.TriangleCount());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGatePassableTest,
	"HutongLayout.Doors.GatePassable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGatePassableTest::RunTest(const FString& Parameters)
{
	// The mesh is its own collision: every gate, leaves ajar, must let a body Min Clear Width wide walk
	// straight through from the lane to the court. Rays along the passage at the clear width's edges and
	// centre, from above the 門檻 to shoulder height.
	struct FCase { EHutongGateStyle Style; bool bNarrowest; };
	for (const FCase Case : { FCase{ EHutongGateStyle::Guangliang, false }, FCase{ EHutongGateStyle::Jinzhu, false },
		FCase{ EHutongGateStyle::Manzi, false }, FCase{ EHutongGateStyle::Ruyi, false }, FCase{ EHutongGateStyle::Ruyi, true } })
	{
		FHutongGateHouseParams P;
		P.Style = Case.Style;
		const FHutongGateHouseParams::FSizeRange R = P.GetSizeRange();
		P.Width = Case.bNarrowest ? R.FrontageMin : 0.5 * (R.FrontageMin + R.FrontageMax);
		P.Depth = 0.5 * (R.DepthMin + R.DepthMax);
		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildGateHouse(Mesh, P);
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);

		const double Floor = P.FloorHeight + FMath::Max(P.ThresholdHeight, 0.0);
		const double Top = FMath::Min(P.FloorHeight + 150.0, P.GetDoorHeadHeight() - 5.0);
		const double Half = 0.5 * P.MinClearWidth - 1.0;
		int32 Blocked = 0;
		for (const double DX : { -Half, 0.0, Half })
		{
			for (double Z = Floor + 10.0; Z <= Top; Z += 20.0)
			{
				const FRay3d Ray(FVector3d(0.5 * P.Width + DX, -300.0, Z), FVector3d(0.0, 1.0, 0.0));
				double T = 0.0; int32 Tid = -1; FVector3d Bary;
				if (Tree.FindNearestHitTriangle(Ray, T, Tid, Bary) && T < P.Depth + 600.0)
				{
					if (Blocked++ == 0)
					{
						const FVector3d Hit = Ray.PointAt(T);
						const int32 Slot = Mesh.HasAttributes() && Mesh.Attributes()->GetMaterialID() ? Mesh.Attributes()->GetMaterialID()->GetValue(Tid) : -1;
						double LeftLeaf = -BIG_NUMBER, RightLeaf = BIG_NUMBER;
						for (const int32 T2 : Mesh.TriangleIndicesItr())
						{
							if (Mesh.Attributes()->GetMaterialID()->GetValue(T2) != Slot) continue;
							const UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(T2);
							for (int32 c = 0; c < 3; ++c)
							{
								const FVector3d V = Mesh.GetVertex(Tri[c]);
								if (V.X < 0.5 * P.Width) LeftLeaf = FMath::Max(LeftLeaf, V.X); else RightLeaf = FMath::Min(RightLeaf, V.X);
							}
						}
						AddWarning(FString::Printf(TEXT("%s: first blocked at x %.1f of %.0f, z %.0f, y %.1f, slot %d; leaves reach %.1f and %.1f"),
							*UEnum::GetDisplayValueAsText(Case.Style).ToString(), Hit.X, P.Width, Hit.Z, Hit.Y, Slot, LeftLeaf, RightLeaf));
					}
				}
			}
		}
		TestEqual(FString::Printf(TEXT("a %s%s lets a %.0f cm body through"), *UEnum::GetDisplayValueAsText(Case.Style).ToString(),
			Case.bNarrowest ? TEXT(" at its narrowest") : TEXT(""), P.MinClearWidth), Blocked, 0);
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
		// Short of the wall's inner face, not flush.
		TestTrue(FString::Printf(TEXT("the 枕 stops well short of the inner face (%.1f vs %.1f)"),
			MaxY, BackY), MaxY < BackY - 5.0);
		TestTrue(FString::Printf(TEXT("and it projects forward of it (%.0f cm)"), FrontY - MinY),
			FrontY - MinY > 20.0);
	}

	// 抱鼓石: big disc, modest base.
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
	// Drum faces look along the lane, so its diameter shows in Y.
	const double Diameter = MaxY - MinY;

	TestTrue(FString::Printf(TEXT("the drum is most of the stone's height (%.0f of %.0f)"),
		Diameter, H), Diameter > 0.5 * H);
	// So it overhangs its base, as a drum does.
	TestTrue(FString::Printf(TEXT("it stands proud of its base (%.0f vs %.0f deep)"), Diameter, Dp),
		Diameter > Dp);
	TestTrue(TEXT("and does not grow past the stone's top"), MaxZ <= H + 0.01);

	// The gate house has 門墩 too, sharing this code.
	{
		FHutongGateHouseParams G;
		G.DoorStones.Style = EHutongDoorStone::Drum;

		G.Style = EHutongGateStyle::Guangliang;
		TestTrue(TEXT("廣亮大門 may carry a 抱鼓石"),
			G.GetDoorStones().Style == EHutongDoorStone::Drum);
		G.Style = EHutongGateStyle::Manzi;
		TestTrue(TEXT("so may a 蠻子門: every gate takes either form"),
			G.GetDoorStones().Style == EHutongDoorStone::Drum);

		// Built: a gate with stones has more geometry than one without.
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

		// 反八字影壁 (圖5-1-2): a 廣亮大門's wings splay out in front of the piers; other ranks ignore the flag.
		auto Bounds = [](EHutongGateStyle Style, bool bWings)
		{
			FHutongGateHouseParams P;
			P.Style = Style;
			P.Width = 380.0; P.Depth = 450.0;
			P.bSplayedScreens = bWings;
			UE::Geometry::FDynamicMesh3 M;
			HutongGen::BuildGateHouse(M, P);
			FBox B(ForceInit);
			for (const FVector3d& V : M.VerticesItr()) B += FVector(V);
			return B;
		};
		const FBox Plain = Bounds(EHutongGateStyle::Guangliang, false);
		const FBox Wings = Bounds(EHutongGateStyle::Guangliang, true);
		const double Reach = HutongCanon::Gate::WingLengthShare * 380.0 * FMath::Sin(FMath::DegreesToRadians(HutongCanon::Gate::WingAngleDeg));
		TestTrue(FString::Printf(TEXT("the wings reach out past each pier (%.0f, %.0f)"), Wings.Min.X, Wings.Max.X - 380.0),
			Wings.Min.X < -0.9 * Reach && Wings.Max.X > 380.0 + 0.9 * Reach);
		TestTrue(FString::Printf(TEXT("and forward to the street, past the steps (%.0f vs %.0f)"), Wings.Min.Y, Plain.Min.Y),
			Wings.Min.Y < -0.9 * Reach && Wings.Min.Y < Plain.Min.Y);
		TestTrue(TEXT("below the gate's roof"), Wings.Max.Z <= Plain.Max.Z + 0.01);
		// Every diagonal 方磚 field (the wings' and the 廊心) has a whole brick at its middle: each field is one box,
		// its authored UVs symmetric about the field's centre.
		{
			FHutongGateHouseParams P;
			P.Style = EHutongGateStyle::Guangliang;
			P.Width = 380.0; P.Depth = 450.0;
			P.bSplayedScreens = true;
			UE::Geometry::FDynamicMesh3 M;
			HutongGen::BuildGateHouse(M, P);
			if (!TestTrue(TEXT("the gate has UVs and slots"), M.HasAttributes() && M.Attributes()->PrimaryUV() && M.Attributes()->GetMaterialID())) return false;
			const UE::Geometry::FDynamicMeshUVOverlay* UV = M.Attributes()->PrimaryUV();
			const UE::Geometry::FDynamicMeshMaterialAttribute* Mat = M.Attributes()->GetMaterialID();
			const double Brick = HutongGen::FloorPaverCm / 100.0;
			int32 Fields = 0;
			// Each field is one run of Floor-slot triangles with authored UVs; borders and caps lie between.
			auto IsField = [&](int32 t) { return M.IsTriangle(t) && Mat->GetValue(t) == HutongGen::MatSlot_Floor && UV->IsSetTriangle(t); };
			for (int32 t = 0; t < M.MaxTriangleID(); )
			{
				if (!IsField(t)) { ++t; continue; }
				FBox2D Box(ForceInit);
				for (; t < M.MaxTriangleID() && IsField(t); ++t)
				{
					const UE::Geometry::FIndex3i E = UV->GetTriangle(t);
					for (int32 c = 0; c < 3; ++c) Box += FVector2D(UV->GetElement(E[c]));
				}
				const FVector2D Mid = Box.GetCenter() / Brick - FVector2D(0.5, 0.5);
				++Fields;
				TestTrue(FString::Printf(TEXT("field %d is centred on a brick (%.3f, %.3f bricks off)"), Fields,
					Mid.X - FMath::RoundToDouble(Mid.X), Mid.Y - FMath::RoundToDouble(Mid.Y)),
					FMath::Abs(Mid.X - FMath::RoundToDouble(Mid.X)) < 0.01 && FMath::Abs(Mid.Y - FMath::RoundToDouble(Mid.Y)) < 0.01);
			}
			TestEqual(TEXT("two wing fields and four 廊心"), Fields, 6);
		}

		TestTrue(TEXT("a 蠻子門 builds none"),
			Bounds(EHutongGateStyle::Manzi, true).Equals(Bounds(EHutongGateStyle::Manzi, false)));
	}

	// 垂花門 too: 板門 on pivots.
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
	// Shut and unswung: only a peg stands in front of the door plane.
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
	// Centred on the head: a peg may stand slightly proud of the rail, never a full diameter above.
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
	// 圖5-1-7.1, 5-1-5.1: a 蠍子尾 is a blade leaning out at about 50° from a root well inside the ridge, rising
	// about 2.5 ridge courses, its tip barely past the gable. It was a slab leaning out past the gable, then
	// a curl hooking straight up from it.
	FHutongSiheyuanParams P;
	P.Width = 900.0; P.Depth = 450.0;

	UE::Geometry::FDynamicMesh3 With, Without;
	HutongGen::BuildSiheyuan(With, P);
	FHutongSiheyuanParams Plain = P;
	Plain.bHasRidgeCourse = false;
	HutongGen::BuildSiheyuan(Without, Plain);
	auto Max = [](const UE::Geometry::FDynamicMesh3& M, int32 Axis)
	{
		double V = -BIG_NUMBER;
		for (int32 v : M.VertexIndicesItr()) V = FMath::Max(V, M.GetVertex(v)[Axis]);
		return V;
	};
	UE::Geometry::FDynamicMesh3 NoTail;
	FHutongSiheyuanParams Untailed = P;
	Untailed.RidgeEndKick = 0.0;
	HutongGen::BuildSiheyuan(NoTail, Untailed);
	const double RidgeTop = Max(NoTail, 2);
	const double Rise = Max(With, 2) - RidgeTop;
	const double Past = Max(With, 0) - Max(Without, 0);
	TestTrue(FString::Printf(TEXT("the tail rises about %.1f ridge courses (%.0f cm)"), HutongCanon::Roof::TailRiseInCourses, Rise),
		FMath::Abs(Rise - P.RidgeEndKick) < 3.0 && Rise > 2.0 * P.RidgeCourseHeight);
	TestTrue(FString::Printf(TEXT("its tip barely passes the gable (%.0f cm)"), Past), Past < 15.0);

	// Halfway up, the blade stands well inside the gable: it leans, it is not a hook off the gable's face.
	double SumX = 0.0;
	int32 Count = 0;
	for (int32 v : With.VertexIndicesItr())
	{
		const FVector3d V = With.GetVertex(v);
		if (V.X > 0.5 * P.Width && V.Z > RidgeTop + 0.4 * Rise && V.Z < RidgeTop + 0.6 * Rise) { SumX += V.X; ++Count; }
	}
	const double Inside = Count > 0 ? P.Width - SumX / Count : 0.0;
	TestTrue(FString::Printf(TEXT("halfway up it stands %.0f cm inside the gable"), Inside), Count > 0 && Inside > 0.25 * Rise);
	return true;
}

// 博縫 and 排山勾滴 stand a few cm proud of each 山牆 and hug the roof edge eave to ridge, so the
// gable reads as brick with a rim, not a slab.
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

	const double Eave = P.GetRoofBaseHeight();
	const double Apex = Eave + P.GetRoofRise();
	double MinX = BIG_NUMBER, MaxX = -BIG_NUMBER;
	int32 Proud = 0, OffTheRake = 0;
	for (int32 vid : With.VertexIndicesItr())
	{
		const FVector3d V = With.GetVertex(vid);
		MinX = FMath::Min(MinX, V.X);
		MaxX = FMath::Max(MaxX, V.X);
		// Past the 下鹼's 3 cm projection.
		if (V.X < -3.5 || V.X > P.Width + 3.5)
		{
			++Proud;
			// Along the gable wall; the 墀頭 盤頭 step out under the rake foot, in front of it.
			const bool bAlongWall = V.Y > 0.5 && V.Y < P.Depth - 0.5;
			if (bAlongWall && (V.Z < Eave - 40.0 || V.Z > Apex + 12.0)) ++OffTheRake;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongTileCourseTest,
	"HutongLayout.Roofs.TileCourses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongTileCourseTest::RunTest(const FString& Parameters)
{
	using namespace HutongGen;
	constexpr double W = 600.0, D = 500.0, Eave = 300.0;

	// Everything after the courses off, so they are appended last and can be lifted out.
	auto Build = [&](EHutongRoofTile Tile, bool bRuns, FDynamicMesh3& Out)
	{
		Shell::FRoofParams Roof;
		Roof.Section = Jiajia::MakeSection(EHutongPurlins::Five, 0.5 * D, Roof.FrontOverhang, 0.0);
		Roof.Tile = Tile;
		Roof.bTileRuns = bRuns;
		Roof.RakeDepth = 0.0;
		Roof.RafterSection = 0.0;
		Shell::AppendGableRoof(Out, W, D, Eave, Roof);
	};

	double Crown[2] = {};
	const EHutongRoofTile Tiles[2] = { EHutongRoofTile::He, EHutongRoofTile::Tong };
	for (int32 t = 0; t < 2; ++t)
	{
		const TCHAR* Name = Tiles[t] == EHutongRoofTile::Tong ? TEXT("筒瓦") : TEXT("合瓦");
		FDynamicMesh3 With, Without;
		Build(Tiles[t], true, With);
		Build(Tiles[t], false, Without);
		if (!TestTrue(FString::Printf(TEXT("%s: the courses add geometry"), Name),
			With.TriangleCount() > Without.TriangleCount())) continue;

		FDynamicMesh3 Courses;
		TMap<int32, int32> Map;
		auto Vert = [&](int32 V) -> int32
		{
			if (const int32* Found = Map.Find(V)) return *Found;
			return Map.Add(V, Courses.AppendVertex(With.GetVertex(V)));
		};
		for (int32 tid = Without.MaxTriangleID(); tid < With.MaxTriangleID(); ++tid)
		{
			if (!With.IsTriangle(tid)) continue;
			const UE::Geometry::FIndex3i Tri = With.GetTriangle(tid);
			Courses.AppendTriangle(Vert(Tri.A), Vert(Tri.B), Vert(Tri.C));
		}
		const FShellReport R = InspectShell(Courses);
		TestTrue(FString::Printf(TEXT("%s: every course is a closed solid (%d unmatched, %d duplicated, volume %.0f)"),
			Name, R.Unmatched, R.Duplicated, R.Volume), R.IsSolid());

		// Course standoff from its slope, measured square to it.
		Shell::FRoofParams Probe;
		Probe.Section = Jiajia::MakeSection(EHutongPurlins::Five, 0.5 * D, Probe.FrontOverhang, 0.0);
		const double O = Probe.FrontOverhang;
		const TArray<FVector2d> Profile = HutongMeshUtils::GableRoofProfile(
			D + O, Shell::RoofRise(Probe), Probe.Section, Probe.SlopeSegments);
		double Top = -BIG_NUMBER;
		for (int32 vid : Courses.VertexIndicesItr())
		{
			const FVector3d V = Courses.GetVertex(vid);
			const FVector2d P(V.Y + O, V.Z - Eave);
			double Best = BIG_NUMBER, Above = 0.0;
			for (int32 i = 0; i + 1 < Profile.Num(); ++i)
			{
				const FVector2d AB = Profile[i + 1] - Profile[i];
				const double L = AB.Length();
				if (L <= UE_SMALL_NUMBER) continue;
				const FVector2d Dir = AB / L, AP = P - Profile[i];
				const double Dist = (AP - Dir * FMath::Clamp(AP.Dot(Dir), 0.0, L)).Length();
				if (Dist < Best) { Best = Dist; Above = (Dir.X * AP.Y - Dir.Y * AP.X) > 0.0 ? Dist : -Dist; }
			}
			Top = FMath::Max(Top, Above);
		}
		const double Bottom = 0.0;
		Crown[t] = Top - Bottom;
		TestTrue(FString::Printf(TEXT("%s: the courses stand proud of the slope (%.1f cm)"), Name, Crown[t]),
			Crown[t] > 1.5);
	}
	TestTrue(FString::Printf(TEXT("a 筒瓦 course stands higher than a 合瓦 one (%.1f against %.1f)"), Crown[1], Crown[0]),
		Crown[1] > Crown[0]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallCapCoplanarTest,
	"HutongLayout.Walls.CapFacesDoNotFight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallCapCoplanarTest::RunTest(const FString& Parameters)
{
	// Drip course once lay on the top corbel face and z-fought along every wall.
	for (const EHutongWallRole Role : { EHutongWallRole::Perimeter, EHutongWallRole::Courtyard })
	{
		FHutongWallParams P;
		P.Role = Role;
		P.bHasWindows = false;
		FDynamicMesh3 M;
		UHutongWallBuildingComponent::BuildWallMesh(P, 600.0, P.GetThickness(), false, M, EHutongDetail::Near);
		const double CapBase = P.GetHeight() - 1.0;
		TArray<FString> Where;
		const int32 Fights = HutongMeshInspect::CountSameFacingOverlaps(M, CapBase, &Where);
		for (const FString& W : Where) UE_LOG(LogTemp, Display, TEXT("cap fight: %s"), *W);
		TestEqual(FString::Printf(TEXT("%s: no two cap faces share a plane and a facing"),
			Role == EHutongWallRole::Perimeter ? TEXT("院牆") : TEXT("隔牆")), Fights, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongInnerGateHallAndRollTest,
	"HutongLayout.Roofs.InnerGateHallAndRoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongInnerGateHallAndRollTest::RunTest(const FString& Parameters)
{
	// 一殿一卷: 殿 in front with the ridge, 卷 lower behind, meeting eave to eave at the 天溝; neither
	// dips below the eave there or crosses the line.
	FHutongInnerGateParams P;
	P.Style = EHutongInnerGateStyle::OneHallOneRoll;
	P.Width = 360.0;
	P.Depth = 420.0;
	FDynamicMesh3 M;
	HutongGen::BuildInnerGate(M, P);
	if (!TestTrue(TEXT("the gate builds"), M.TriangleCount() > 0)) return false;

	const double Eave = P.GetEaveHeight();
	const double Valley = P.GetValleyY();
	double FrontTop = 0.0, RearTop = 0.0, LowestAtValley = BIG_NUMBER;
	for (int32 vid : M.VertexIndicesItr())
	{
		const FVector3d V = M.GetVertex(vid);
		if (V.Z <= Eave + 1.0) continue;
		if (V.Y < Valley - 1.0) FrontTop = FMath::Max(FrontTop, V.Z); else if (V.Y > Valley + 1.0) RearTop = FMath::Max(RearTop, V.Z);
	}
	for (int32 vid : M.VertexIndicesItr())
	{
		const FVector3d V = M.GetVertex(vid);
		if (FMath::Abs(V.Y - Valley) < 0.5 && V.Z > Eave - 20.0) LowestAtValley = FMath::Min(LowestAtValley, V.Z);
	}
	TestTrue(FString::Printf(TEXT("the 殿 stands over the 卷 (%.0f against %.0f)"), FrontTop, RearTop), FrontTop > RearTop + 10.0);
	TestNearlyEqual(TEXT("the 天溝 is at the eave"), LowestAtValley, P.GetRoofBaseHeight(), 1.0);
	TestTrue(FString::Printf(TEXT("the ridge estimate is the 殿's (%.0f, built %.0f)"), HutongGen::Ridge::InnerGate(P), FrontTop),
		FrontTop >= HutongGen::Ridge::InnerGate(P) - 1.0 && FrontTop <= HutongGen::Ridge::InnerGate(P) + P.RidgeCourseHeight + P.RidgeEndKick + 1.0);
	TestTrue(TEXT("the doors stand in the wall line, the 垂蓮柱 ahead of it"), P.GetWallLineY() > 20.0 && P.GetWallLineY() < Valley);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRoofUVTest,
	"HutongLayout.Roofs.UVs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRoofUVTest::RunTest(const FString& Parameters)
{
	using namespace HutongMeshUtils;

	// Whatever the roof authored, the fallback leaves nothing unmapped.
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

	// The roof's own UVs run in tile rows, undistorted.
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

			// One edge suffices: roof length vs UV length times tile size.
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
	// Stated, not derived: the eave is this opening-stack test's input.
	P.bDeriveEaveFromBays = false;
	P.EaveHeight = 365.0;

	const double Floor = P.GetFloorHeight();
	const double Col = P.GetColumnHeight();
	TestTrue(TEXT("柱高 is 11/13 of the eave"), FMath::IsNearlyEqual(Col, 365.0 * 11.0 / 13.0, 0.5));
	TestTrue(TEXT("額枋 underside is 柱高 less one 柱徑"),
		FMath::IsNearlyEqual(P.GetArchitraveBottom(), Floor + Col * (10.0 / 11.0), 0.5));

	// Unified: one 中檻 across the bay.
	TestTrue(TEXT("window head and door leaf head are the same member"),
		FMath::IsNearlyEqual(P.GetWindowTopHeight(), P.GetDoorLeafTopHeight(), 0.01));
	TestTrue(TEXT("the 中檻 sits under the 額枋"),
		P.GetWindowTopHeight() < P.GetArchitraveBottom());

	// The sill follows the 隔扇 (表十四, p.104): 下檻 and the leaf's lower run, 1.2 of a leaf's width.
	FHutongSiheyuanParams Small = P;
	Small.EaveHeight = 316.0;                       // the 耳房 preset, stated as the presets do
	for (const FHutongSiheyuanParams* H : { &P, &Small })
	{
		TestNearlyEqual(TEXT("the sill is the 下檻 and the 隔扇's lower run"),
			H->GetWindowSillHeight() - H->GetFloorHeight(),
			H->GetLowerSillHeight() + HutongCanon::Joinery::GeshanLowerOfWidth * H->GetGeshanWidth(), 0.01);
		TestTrue(TEXT("and stands under the 中檻"), H->GetWindowSillHeight() < H->GetWindowTopHeight() - 40.0);
	}

	// The small building clears the doorway rule without the eave being pushed up.
	const double Clear = Small.GetDoorLeafTopHeight() - Small.GetFloorHeight() - Small.GetLowerSillHeight();
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

	// 步 × 6, 12, 24 widths and the pitch are HutongCanon.h constants; this tests their use.
	// Classification covers the whole range.
	TestTrue(TEXT("3 m is an alley"), Classify(300.0) == EHutongStreetClass::Alley);
	TestTrue(TEXT("9.1 m is a 胡同"), Classify(910.0) == EHutongStreetClass::Hutong);
	TestTrue(TEXT("18 m is a 小街"), Classify(1800.0) == EHutongStreetClass::MinorStreet);
	TestTrue(TEXT("36 m is a 大街"), Classify(3600.0) == EHutongStreetClass::MajorStreet);
	TestTrue(TEXT("90 m is off the top"), Classify(9000.0) == EHutongStreetClass::Open);

	// In band vs nearest: a 12 m lane classes as 胡同 but is a poor example.
	TestTrue(TEXT("9.4 m is in band"), IsInBand(940.0));
	TestFalse(TEXT("12 m is not"), IsInBand(1200.0));

	// Snap-to-band is deliberately tight: it corrects a hand, not a measurement.
	TestTrue(TEXT("within tolerance snaps"), NearestCanonicalWidth(940.0, 30.0) > 0.0);
	TestEqual(TEXT("outside tolerance does not"), NearestCanonicalWidth(1200.0, 30.0), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryTest,
	"HutongLayout.Gallery.EverythingBuilds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryTest::RunTest(const FString& Parameters)
{
	// The gallery touches every generator.
	UHutongGalleryToolProperties* Settings = NewObject<UHutongGalleryToolProperties>();
	const TArray<TPair<FString, int32>> Built = HutongGallery::BuildAll(Settings);

	TestTrue(TEXT("the gallery has pieces in it"), Built.Num() >= 15);
	for (const TPair<FString, int32>& It : Built)
	{
		// Logged and asserted.
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
		// 捲棚廡殿: ridge becomes a rounded crown.
		FHipRoofSpec S = Base;
		S.Section.ApexRoll = 0.4;
		Cases.Add({ TEXT("捲棚廡殿"), S });
	}
	{
		// A rolled 攢尖 has no ridge to round.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRoundRoofTest,
	"HutongLayout.Roofs.RoundRoof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 圓攢尖 (陸柱圓亭): the roof of revolution is one closed solid with a flat ceiling or a shell, and its
// sector panels land every course on the slope, the thinned bands on the full set's lines.
bool FHutongRoundRoofTest::RunTest(const FString& Parameters)
{
	using namespace HutongMeshUtils;
	FRoundRoofSpec Spec;
	Spec.Radius = 240.0; Spec.Rise = 138.0; Spec.FasciaDrop = 9.0; Spec.EaveOverhang = 77.0; Spec.UndersideRise = 28.0;
	Spec.Section = HutongGen::Jiajia::MakeSectionWithRatios({ 0.5, 0.75 }, 160.0, 77.0, 0.0);
	for (const double Cover : { 0.0, 10.0 })
	{
		Spec.ShellCover = Cover;
		FDynamicMesh3 Mesh;
		UE::Geometry::FIndex2i Under(-1, -1);
		AppendRoundRoof(Mesh, FVector3d(0.0, 0.0, 300.0), Spec, &Under);
		const FShellReport R = InspectShell(Mesh);
		const TCHAR* Name = Cover > 0.0 ? TEXT("shell") : TEXT("ceiling");
		TestEqual(FString::Printf(TEXT("%s: every edge is matched"), Name), R.Unmatched, 0);
		TestEqual(FString::Printf(TEXT("%s: consistent winding"), Name), R.Duplicated, 0);
		TestTrue(FString::Printf(TEXT("%s: positive volume"), Name), R.Volume > 0.0);
		TestTrue(FString::Printf(TEXT("%s: an underside to tag wood"), Name), Under.B > Under.A && Under.A > 0);
	}

	TestEqual(TEXT("one panel per sector"), MakeRoundRoofPanels(FVector3d::Zero(), Spec).Num(), Spec.Panels);

	// Every course runs eave to top, narrowing with the circle: as many as the slope's lines, each closed.
	FDynamicMesh3 Courses;
	HutongGen::Shell::AppendRoundRoofCourses(Courses, FVector3d(0.0, 0.0, 300.0), Spec, EHutongRoofTile::Tong, 9.0, 30.0);
	const FShellReport C = InspectShell(Courses);
	TestEqual(TEXT("courses: every edge is matched"), C.Unmatched, 0);
	TestTrue(TEXT("courses: positive volume"), C.Volume > 0.0);
	double TopZ = -1e9;
	for (const int32 Vid : Courses.VertexIndicesItr()) TopZ = FMath::Max(TopZ, Courses.GetVertex(Vid).Z);
	TestTrue(TEXT("courses reach the top, under the 寶頂"), TopZ > 300.0 + RoundRoofHeight(Spec, 40.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRoundFloorTest,
	"HutongLayout.Pavilion.RoundFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 尺二方磚車輞砍墁: the round 亭's floor is paved in rings whose UVs the generator writes (none collapsed,
// none left to the box projection), round a stone at the centre.
bool FHutongRoundFloorTest::RunTest(const FString& Parameters)
{
	FHutongPavilionParams P;
	P.Plan = EHutongPavilionPlan::Round;
	const double Side = HutongCanon::Pavilion::FigureBayCm * (1.0 + HutongCanon::Pavilion::ColumnPerBay);
	P.Width = P.Depth = Side;
	FDynamicMesh3 M;
	HutongGen::BuildPavilion(M, P);
	const auto* Mat = M.Attributes()->GetMaterialID();
	const auto* UV = M.Attributes()->PrimaryUV();
	int32 Floor = 0, Unset = 0, Collapsed = 0, CentreStone = 0;
	for (const int32 tid : M.TriangleIndicesItr())
	{
		const int32 Slot = Mat->GetValue(tid);
		const UE::Geometry::FIndex3i T = M.GetTriangle(tid);
		const FVector3d C = (M.GetVertex(T.A) + M.GetVertex(T.B) + M.GetVertex(T.C)) / 3.0;
		if (Slot == HutongGen::MatSlot_Stone && FVector2d(C.X - 0.5 * Side, C.Y - 0.5 * Side).Length() < 30.0
			&& FMath::IsNearlyEqual(C.Z, P.GetFloorHeight(), 0.5)) ++CentreStone;
		if (Slot != HutongGen::MatSlot_Floor) continue;
		++Floor;
		if (!UV->IsSetTriangle(tid)) { ++Unset; continue; }
		FVector2f A, B, Cc;
		UV->GetTriElements(tid, A, B, Cc);
		if (FMath::Abs((B - A) ^ (Cc - A)) < 1e-8f) ++Collapsed;
	}
	TestTrue(FString::Printf(TEXT("the floor is paved (%d tris)"), Floor), Floor > 0);
	TestEqual(TEXT("every floor triangle has the generator's UVs"), Unset, 0);
	TestEqual(TEXT("no floor UV collapsed"), Collapsed, 0);
	TestTrue(TEXT("a round stone at the centre"), CentreStone > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRearHighWindowTest,
	"HutongLayout.Openings.RearHighWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRearHighWindowTest::RunTest(const FString& Parameters)
{
	// 高窗 in the 封護檐 back wall: a lane-backed row's only light from that side; their height is what makes them allowable.
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

	const double T = FMath::Clamp(P.GetRearWallThickness(), 1.0, FMath::Min(SizeX, SizeY) * 0.2);
	const double Eave = FMath::Max(P.GetEaveHeight(), 10.0);
	const double Head = Eave - P.RearWindowHeadDrop;
	const double Sill = Head - P.RearWindowHeight;
	const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);

	// Above head height, the point of placing them there.
	TestTrue(FString::Printf(TEXT("the sill stands above head height (%.0f cm)"), Sill),
		Sill >= 170.0);
	TestTrue(TEXT("the head stays under the eave"), Head < Eave - 1.0);

	// One per bay, each a true hole: no brick or plaster in the opening.
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
			// Back wall depth only, well inside the opening.
			const int32 Slot = Open.Attributes()->GetMaterialID()->GetValue(tid);
			if (Slot == HutongGen::MatSlot_Paper || Slot == HutongGen::MatSlot_Lattice) continue;
			if (C.Y < SizeY - T + 0.5 || C.Y > SizeY - 0.5) continue;
			if (C.Z < Sill + 3.0 || C.Z > Head - 3.0) continue;
			if (FMath::Abs(C.X - Cx) < Half - 3.0) ++Blocking;
		}
		TestEqual(FString::Printf(TEXT("bay %d's 高窗 is open through the wall"), i), Blocking, 0);
	}

	// Independent of the rear eave: a 正房 backing onto its 後院 has an overhanging eave and still wants the light.
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
	// Derived values below depend on the footprint and bay count, which the tool fills at spawn (the post's
	// width is ⅔ of the column, which follows the 明間).
	P.Width = SizeX;
	P.Depth = SizeY;
	P.BayCountOverride = N;

	UE::Geometry::FDynamicMesh3 Mesh;
	UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
		P, EHutongBaySide::MinusY, N, SizeX, SizeY, Mesh);

	const double T = FMath::Clamp(P.GetFacadeWallThickness(), 1.0, FMath::Min(SizeX, SizeY) * 0.2);
	const double PT = FMath::Clamp(P.GetPostThickness(), 1.0, T * 1.5);
	const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);
	const double Eave = FMath::Max(P.GetEaveHeight(), 10.0);
	const double Floor = FMath::Clamp(P.GetFloorHeight(), 0.0, Eave * 0.5);
	const double Head = FMath::Clamp(P.GetDoorTopHeight(), Floor + 10.0, Eave - 10.0);

	// The post reaches the wall's inner face; the column only its radius past the facade plane.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHouseDoorwayTest,
	"HutongLayout.Openings.HouseDoorway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHouseDoorwayTest::RunTest(const FString& Parameters)
{
	// 圖5-4-5 / 5-4-6: the 明間 is four 隔扇 with a 簾架 before the middle two. With the leaves open a walker
	// passes the 風門's 門口 into the room; the fixed outer 隔扇 close the rest of the bay.
	using namespace HutongCanon::House;
	for (const TPair<const TCHAR*, FHouse>& Case : { TPair<const TCHAR*, FHouse>(TEXT("正房"), MainHall),
		TPair<const TCHAR*, FHouse>(TEXT("廂房"), SideHouse), TPair<const TCHAR*, FHouse>(TEXT("倒座房"), FrontRow),
		TPair<const TCHAR*, FHouse>(TEXT("耳房"), EarRoom) })
	{
		FHutongSiheyuanParams P = HutongPresets::MakeHouse(Case.Value);
		const double SizeX = Case.Value.FrontageCm;
		P.Width = SizeX;
		const double SizeY = P.GetCanonicalDepth();
		P.Depth = SizeY;
		const int32 N = HutongGen::ComputeBayCount(SizeX, P.MinBayWidth, P.MaxBayWidth);
		UE::Geometry::FDynamicMesh3 Mesh;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, EHutongBaySide::MinusY, N, SizeX, SizeY, Mesh);
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh);

		const double T = FMath::Clamp(P.GetFacadeWallThickness(), 1.0, FMath::Min(SizeX, SizeY) * 0.2);
		double FY = 0.0, RY = 0.0;
		P.GetBuiltVerandaDepths(SizeY, T, FY, RY);
		const double ColR = P.GetColumnRadius();
		const int32 DoorIdx = P.GetDoorBayIndex(N);
		// The door bay's clear opening: post faces, or a gable wall's inner face in an end bay.
		const double PT = FMath::Clamp(P.GetPostThickness(), 1.0, T * 1.5);
		const double TG = FMath::Clamp(P.GetGableWallThickness(), 1.0, FMath::Min(SizeX, SizeY) * 0.2);
		const double B0 = (DoorIdx == 0) ? TG : P.GetBayBoundary(DoorIdx, N, SizeX, ColR) + 0.5 * PT;
		const double B1 = (DoorIdx == N - 1) ? SizeX - TG : P.GetBayBoundary(DoorIdx + 1, N, SizeX, ColR) - 0.5 * PT;
		const double Mid = 0.5 * (B0 + B1);
		const double Floor = P.GetFloorHeight();

		auto Blocked = [&](double X, double Z, double Reach)
		{
			const FRay3d Ray(FVector3d(X, -300.0, Z), FVector3d(0.0, 1.0, 0.0));
			double Hit = 0.0; int32 Tri = -1;
			return Tree.FindNearestHitTriangle(Ray, Hit, Tri) && Hit < 300.0 + Reach;
		};
		// A walker's width through the 門口, from over the 啞吧檻 (stepped over, like the 下檻) to a crouch's height.
		const double Half = HutongCanon::Openings::WalkerRadiusCm - 1.0;
		const double StepOver = Floor + P.GetLowerSillHeight() + HutongCanon::Joinery::CurtainDumbSill * (P.GetDoorLeafTopHeight() - Floor) + 1.0;
		int32 Stopped = 0;
		for (const double DX : { -Half, 0.0, Half })
		{
			for (double Z = StepOver; Z <= Floor + HutongGen::Passage::MinClearHeight; Z += 10.0)
			{
				Stopped += Blocked(Mid + DX, Z, FY + T + 40.0) ? 1 : 0;
			}
		}
		TestEqual(FString::Printf(TEXT("%s: a walker passes the 風門 into the room"), Case.Key), Stopped, 0);
		UE_LOG(LogTemp, Display, TEXT("%s"), *FString::Printf(TEXT("%s: 隔扇 %.0f wide, sill %.0f above the floor, 中檻 %.0f"),
			Case.Key, P.GetGeshanWidth(), P.GetWindowSillHeight() - Floor, P.GetWindowTopHeight() - Floor));
		// The outer 隔扇 stand shut in the door plane.
		const double LeafW = (B1 - B0) / HutongCanon::Joinery::GeshanPerBay;
		TestTrue(FString::Printf(TEXT("%s: the fixed 隔扇 close the bay's sides"), Case.Key),
			Blocked(B0 + 0.6 * LeafW, Floor + 60.0, FY + T) && Blocked(B1 - 0.6 * LeafW, Floor + 60.0, FY + T));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSillWallAndCorniceTest,
	"HutongLayout.Detailing.SillWallAndCornice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSillWallAndCorniceTest::RunTest(const FString& Parameters)
{
	auto Build = [](FHutongSiheyuanParams P, UE::Geometry::FDynamicMesh3& M)
	{
		const double SX = P.SuggestedFrontage, SY = P.GetSuggestedDepth();
		P.Width = SX; P.Depth = SY;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, EHutongBaySide::MinusY, 0, SX, SY, M);
	};
	auto SlotTris = [](const UE::Geometry::FDynamicMesh3& M, int32 Slot)
	{
		int32 N = 0;
		const UE::Geometry::FDynamicMeshMaterialAttribute* Mat = M.Attributes() ? M.Attributes()->GetMaterialID() : nullptr;
		for (int32 t : M.TriangleIndicesItr()) N += (Mat && Mat->GetValue(t) == Slot);
		return N;
	};

	// p.90 / 圖5-3-10.1: a 海棠池子 lays one diagonal 方磚 field on each window bay's 檻牆.
	{
		const FHutongSiheyuanParams Plain = HutongPresets::MakeHouse(HutongCanon::House::SideHouse);
		FHutongSiheyuanParams Pool = Plain;
		Pool.SillWallFinish = EHutongSillWall::Pool;
		UE::Geometry::FDynamicMesh3 A, B;
		Build(Plain, A);
		Build(Pool, B);
		const int32 Bays = HutongGen::ComputeBayCount(Plain.SuggestedFrontage, Plain.MinBayWidth, Plain.MaxBayWidth);
		TestEqual(TEXT("one 墻心 box per window bay"),
			SlotTris(B, HutongGen::MatSlot_Floor) - SlotTris(A, HutongGen::MatSlot_Floor), 12 * (Bays - 1));
	}

	// 圖5-3-9: each 封護檐 cornice builds, and the drip course sits on its top course.
	{
		const FHutongSiheyuanParams Row = HutongPresets::MakeHouse(HutongCanon::House::FrontRow);
		auto Reach = [&](EHutongSealedCornice Form, int32& OutTris)
		{
			FHutongSiheyuanParams P = Row;
			P.RearCornice = Form;
			UE::Geometry::FDynamicMesh3 M;
			Build(P, M);
			OutTris = M.TriangleCount();
			double MaxY = -BIG_NUMBER;
			for (int32 v : M.VertexIndicesItr()) MaxY = FMath::Max(MaxY, M.GetVertex(v).Y);
			return MaxY;
		};
		int32 BaseTris = 0;
		const double Base = Reach(EHutongSealedCornice::IceTray, BaseTris);
		for (const TPair<EHutongSealedCornice, double>& Form : {
			TPair<EHutongSealedCornice, double>(EHutongSealedCornice::Rounded, 13.0),
			TPair<EHutongSealedCornice, double>(EHutongSealedCornice::Drawer, 13.0),
			TPair<EHutongSealedCornice, double>(EHutongSealedCornice::SevenCourse, 17.0) })
		{
			int32 Tris = 0;
			const double Y = Reach(Form.Key, Tris);
			TestEqual(FString::Printf(TEXT("cornice %d: the drip course moves with the top course"), int32(Form.Key)),
				Y - Base, Form.Value - 14.0, 0.5);
			TestTrue(FString::Printf(TEXT("cornice %d builds its courses (%d tris)"), int32(Form.Key), Tris), Tris != BaseTris);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGoldColumnHeightTest,
	"HutongLayout.Proportions.GoldColumnHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGoldColumnHeightTest::RunTest(const FString& Parameters)
{
	// 圖5-3-1/5-3-2: behind a 前廊 the 金柱 rise past the 檐柱 by what the roof climbs over the 廊, ceilinged
	// house or 徹上明造 alike; the 橫陂 fill that height (p.104).
	for (const bool bExposed : { false, true })
	{
		FHutongSiheyuanParams P = HutongPresets::MakeHouse(HutongCanon::House::SideHouse);
		P.bExposedFrame = bExposed;
		const double SX = P.SuggestedFrontage, SY = P.GetSuggestedDepth();
		P.Width = SX; P.Depth = SY;
		UE::Geometry::FDynamicMesh3 M;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, EHutongBaySide::MinusY, 0, SX, SY, M);

		const double T = FMath::Clamp(P.GetFacadeWallThickness(), 1.0, FMath::Min(SX, SY) * 0.2);
		double FY = 0.0, RY = 0.0;
		P.GetBuiltVerandaDepths(SY, T, FY, RY);
		const int32 N = HutongGen::ComputeBayCount(SX, P.MinBayWidth, P.MaxBayWidth);
		const double ColR = P.GetColumnRadius();
		const double B = P.GetBayBoundary(1, N, SX, ColR);
		// Highest point of the 金柱 at the first interior boundary: vertices on its circle.
		double Top = -BIG_NUMBER;
		for (int32 v : M.VertexIndicesItr())
		{
			const FVector3d V = M.GetVertex(v);
			const double R = FVector2d(V.X - B, V.Y - FY).Length();
			if (R > 0.6 * ColR && R < ColR + 1.5) Top = FMath::Max(Top, V.Z);
		}
		const double Climb = 0.5 * FY;   // the 廊's first 步架 at 五舉, near enough
		TestTrue(FString::Printf(TEXT("%s: 金柱 top %.0f stands over the 檐柱's %.0f by about the 廊's climb"),
			bExposed ? TEXT("徹上明造") : TEXT("ceilinged"), Top, P.GetEaveHeight()),
			Top > P.GetEaveHeight() + 0.6 * Climb);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongReviewFixesTest,
	"HutongLayout.Detailing.ReviewFixes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongReviewFixesTest::RunTest(const FString& Parameters)
{
	// A roof run open at its high end (carried on over the 耳房過道) with a rear eave that shows rafters: the rear
	// 墀頭 closes the low corner, and none stands at the open end, inside the roof carried on.
	{
		FHutongSiheyuanParams P = HutongPresets::MakeHouse(HutongCanon::House::SideHouseSmall);
		P.RearEave = EHutongRearEave::Courtyard;
		P.Width = 600.0;
		P.Depth = P.GetSuggestedDepth();
		UE::Geometry::FDynamicMesh3 M;
		HutongGen::AppendHouseRoofRun(M, P, 0.0, 600.0, /*bOpenLow*/ false, /*bOpenHigh*/ true, 0.0);
		int32 Low = 0, High = 0;
		for (int32 v : M.VertexIndicesItr())
		{
			const FVector3d V = M.GetVertex(v);
			// Only a pier reaches the ground behind the rear wall.
			if (V.Y > P.Depth + 1.0 && V.Z < 50.0) { Low += V.X < 100.0; High += V.X > 500.0; }
		}
		TestTrue(FString::Printf(TEXT("the rear 墀頭 closes the low corner (%d vertices)"), Low), Low > 0);
		TestEqual(TEXT("and none stands at the open end"), High, 0);
	}

	// A pavilion's eave derives from its bay, so from the component's footprint.
	{
		UHutongPavilionBuildingComponent* C = NewObject<UHutongPavilionBuildingComponent>();
		C->Width = C->Depth = 500.0;
		FHutongPavilionParams Sized = C->Params;
		Sized.Width = Sized.Depth = 500.0;
		TestNearlyEqual(TEXT("the pavilion reports the eave of the size it stands at"), C->GetEaveHeight(), Sized.GetEaveHeight(), 0.01);
		TestTrue(TEXT("which is not the params' default size's"), FMath::Abs(C->GetEaveHeight() - C->Params.GetEaveHeight()) > 1.0);
	}

	// A house held on a raised walk floor keeps its doorway: the headroom is bought above the held floor.
	{
		FHutongSiheyuanParams P = HutongPresets::MakeHouse(HutongCanon::House::SideHouse);
		P.Width = P.SuggestedFrontage;
		P.bDeriveEaveFromBays = false;
		P.EaveHeight = 150.0;                 // asked far too low
		P.FloorHeightHeldAt = 90.0;
		const double Clear = P.GetMiddleRailHeight() - P.GetFloorHeight() - P.GetLowerSillHeight();
		TestTrue(FString::Printf(TEXT("the doorway under the 中檻 clears a crouch on the held floor (%.0f cm)"), Clear),
			Clear >= HutongGen::Passage::MinClearHeight - 0.5);
	}

	// A 徹上明造 house dragged off its 步架: the 金柱 stand on the frame's line, where its beams land.
	{
		FHutongSiheyuanParams P = HutongPresets::MakeHouse(HutongCanon::House::SideHouse);
		P.bExposedFrame = true;
		const double SX = P.SuggestedFrontage, SY = 1.2 * P.GetSuggestedDepth();
		P.Width = SX; P.Depth = SY;
		UE::Geometry::FDynamicMesh3 M;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, EHutongBaySide::MinusY, 0, SX, SY, M);
		const HutongGen::FrameLayout::FLayout L = HutongGen::FrameLayout::Make(P);
		const int32 N = HutongGen::ComputeBayCount(SX, P.MinBayWidth, P.MaxBayWidth);
		const double ColR = P.GetColumnRadius();
		const double B = P.GetBayBoundary(1, N, SX, ColR);
		auto RingAt = [&](double Y)
		{
			int32 Hits = 0;
			for (int32 v : M.VertexIndicesItr())
			{
				const FVector3d V = M.GetVertex(v);
				const double R = FVector2d(V.X - B, V.Y - Y).Length();
				Hits += (V.Z < 50.0 && R > 0.6 * ColR && R < ColR + 1.5);
			}
			return Hits;
		};
		TestTrue(TEXT("the frame has its 前廊"), L.bFrontVeranda);
		TestTrue(TEXT("a 金柱 stands on the frame's 廊 line"), RingAt(L.Y[L.Front]) > 0);
		TestEqual(TEXT("and none on the preset's 步架"), RingAt(P.GetVerandaDepth()), 0);

		// One source: the plan, the tool preview and the compound read the same line the mesh stands on.
		const double T = FMath::Clamp(P.GetFacadeWallThickness(), 1.0, FMath::Min(SX, SY) * 0.2);
		double FY = 0.0, RY = 0.0;
		P.GetBuiltVerandaDepths(SY, T, FY, RY);
		TestNearlyEqual(TEXT("GetBuiltVerandaDepths gives the frame's line"), FY, L.Y[L.Front], 0.01);

		// A shallow 前後廊 house: each 廊 a whole step or none, the two never more than the room holds.
		FHutongSiheyuanParams Shallow = HutongPresets::MakeHouse(HutongCanon::House::MainHall);
		Shallow.bExposedFrame = true;
		Shallow.Width = Shallow.SuggestedFrontage;
		const double SD = 300.0;
		Shallow.Depth = SD;
		double SF = 0.0, SR = 0.0;
		Shallow.GetBuiltVerandaDepths(SD, T, SF, SR);
		const double Step = SD / (HutongGen::Jiajia::PurlinCount(Shallow.Purlins) - 1);
		const double Room = SD - 2.0 * T - 100.0;
		TestTrue(FString::Printf(TEXT("shallow: front %.0f and rear %.0f are whole steps (%.0f) or none"), SF, SR, Step),
			(SF == 0.0 || FMath::IsNearlyEqual(SF, Step, 0.01)) && (SR == 0.0 || FMath::IsNearlyEqual(SR, Step, 0.01)));
		TestTrue(FString::Printf(TEXT("shallow: together within the room (%.0f of %.0f)"), SF + SR, Room), SF + SR <= Room + 0.01);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallGardenDoorwayTest,
	"HutongLayout.Walls.GardenDoorway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallGardenDoorwayTest::RunTest(const FString& Parameters)
{
	// 月亮門 and relatives: the 什錦窗 idea carried to the ground.
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

		// A 月亮門 is a circle: width comes from height, whatever the width field says.
		if (Shape == EHutongWallDoorway::Moon)
		{
			TestEqual(TEXT("a 月亮門 is as wide as its outline is tall"),
				P.GetDoorwayWidth(), P.GetDoorwaySpan());
		}

		// The wall grows to carry it, as for a too-short 牆垣式門.
		TestTrue(Name + TEXT(" leaves the wall tall enough to hold it"),
			P.GetHeight() >= P.GetDoorwayHeight() + P.DoorwaySurroundWidth + 10.0 - 0.01);

		// Nothing inside the outline between sill and head.
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

		// A 院牆 refuses it while the role derives.
		UE::Geometry::FDynamicMesh3 Lane;
		Build(Shape, EHutongWallRole::Perimeter, Lane);
		UE::Geometry::FDynamicMesh3 LaneBlank;
		Build(EHutongWallDoorway::None, EHutongWallRole::Perimeter, LaneBlank);
		TestEqual(Name + TEXT(" is refused on a 院牆"),
			Lane.TriangleCount(), LaneBlank.TriangleCount());
	}

	// A flush 下鹼 is a zero-projection course, not a missing one. The band beside the opening was once
	// emitted only when proud, so zero projection cut a full-depth hole either side of the doorway.
	{
		auto WallAt = [](double Projection, UE::Geometry::FDynamicMesh3& M)
		{
			FHutongWallParams P;
			P.Role = EHutongWallRole::Courtyard;
			P.Doorway = EHutongWallDoorway::Moon;
			P.Length = 900.0;
			P.DoorwayPosition = 0.5;
			P.BaseCourseProjection = Projection;
			// No surround: its rows lap over the reveal and would answer the probe for absent masonry.
			P.DoorwaySurroundWidth = 0.0;
			HutongGen::BuildWall(M, P);
			return P;
		};

		UE::Geometry::FDynamicMesh3 Flush, Proud;
		const FHutongWallParams P = WallAt(0.0, Flush);
		WallAt(8.0, Proud);

		// Reveal: inside the opening's span, in the course's band. Only doorway rows put masonry there; wall
		// runs stop at the opening.
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

	// 牆垣式垂花門: 垂花門 ornament without its frame; unlike the 牆垣式門 it leaves the roof line untouched.
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

		// It hangs below the cap, never into it.
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
	// 院牆 keeps the lane out, 隔牆 divides one household's courts; the 隔牆 must be the lower.
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

	// The role's core claim: the inner gate rises clear of its wall.
	const double GateEave = FHutongInnerGateParams().GetEaveHeight();
	TestTrue(FString::Printf(TEXT("垂花門 clears its 隔牆 (eave %.0f over wall top %.0f)"),
			GateEave, CourtTop),
		GateEave > CourtTop);

	// A 院牆 stays private: clearly above eye height (240 cm was not).
	TestTrue(FString::Printf(TEXT("院牆 is not seen over (%.0f cm)"), PerimTop), PerimTop > 300.0);

	// 什錦窗 belong to walls inside the household.
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
	// Count follows the run: a longer wall gains windows, not wider gaps.
	TestTrue(TEXT("a longer 隔牆 carries more of them"),
		LatticeTris(EHutongWallRole::Courtyard, 2800.0)
			> LatticeTris(EHutongWallRole::Courtyard, 1400.0));

	// The opening must fit between 下鹼 and cap, and is silently skipped when it cannot.
	TestTrue(TEXT("隔牆 with a third-height 下鹼 still carries them"),
		LatticeTris(EHutongWallRole::Courtyard, 1400.0, 80.0) > 0);

	// Derivation off: both roles use the same authored numbers.
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
	// Pure plot arithmetic: no world, no mesh.
	auto Corridors = [](EHutongCompoundPlan Plan, double W, double D)
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		In.Width = W;
		In.Depth = D;
		// Asked for explicitly: the plan's default is 前廊 on the 廂房.
		In.CourtWalk = EHutongCourtWalk::Corridor;

		TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
		return Slots.FilterByPredicate([](const FHutongCompoundSlot& S)
			{ return S.Piece == EHutongCompoundPiece::Corridor; });
	};

	// No 抄手遊廊 in a 一進四合院 (open yard); it needs a proper inner court, the same 二進 plan as the 垂花門.
	const TArray<FHutongCompoundSlot> One = Corridors(EHutongCompoundPlan::OneCourtyard, 3200.0, 5200.0);
	TestEqual(TEXT("一進 carries no 遊廊"), One.Num(), 0);

	// 抄手 folds right round the court.
	const TArray<FHutongCompoundSlot> Two = Corridors(EHutongCompoundPlan::TwoCourtyards, 3200.0, 5200.0);
	if (!TestEqual(TEXT("二進 carries a ring, not two runs"), Two.Num(), 6)) return false;

	int32 AlongY = 0;
	for (const FHutongCompoundSlot& S : Two) if (S.bLengthAlongY) ++AlongY;
	TestEqual(TEXT("two of the five run down the court"), AlongY, 2);

	// Every run benches toward the court, so flips depend on the ring side.
	int32 Flipped = 0;
	for (const FHutongCompoundSlot& S : Two) if (S.bFlipOpenSide) ++Flipped;
	TestTrue(TEXT("the ring is not uniformly oriented"), Flipped > 0 && Flipped < Two.Num());

	// Sides abut at the corners, never share plan area.
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

	// The 正房 frontage stays clear.
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
			// North band = whatever reaches the hall's front line; the returns at the 垂花門 pass the hall's X range far from it.
			if (S.Min.Y + S.Size.Y < Hall->Min.Y - 0.01) continue;
			++North;
			const bool bClearOfHall = S.Min.X + S.Size.X <= Hall->Min.X + 0.01
				|| S.Min.X >= Hall->Min.X + Hall->Size.X - 0.01;
			TestTrue(TEXT("no 遊廊 stands across the 正房's frontage"), bClearOfHall);
		}
		TestEqual(TEXT("a link in front of each 耳房"), North, 2);

		// The 甬路 reaches the steps it aims at.
		const TArray<FHutongCompoundSlot> Spine = All.FilterByPredicate(
			[](const FHutongCompoundSlot& S)
				{ return S.Piece == EHutongCompoundPiece::Path && S.Size.Y > S.Size.X; });
		if (!TestEqual(TEXT("one spine"), Spine.Num(), 1)) return false;
		TestTrue(TEXT("the 甬路 runs up to the hall's platform"),
			Spine[0].Min.Y + Spine[0].Size.Y >= Hall->Min.Y - 1.0);
	}

	// The ring still leaves a courtyard.
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
		// At minimum the hall's 耳房 are narrowest.
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

	// 正房三間兩耳: hall frontage centred, a shallower 耳房 each flank.
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

		// The hall's two back onto the plot's north edge; the other four are 廂耳房 in the inner court's corners.
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

		// 廂耳房 close the inner court's south corners, one per wing, against the cross wall.
		const TArray<FHutongCompoundSlot> Wings = Slots.FilterByPredicate(
			[](const FHutongCompoundSlot& S) { return S.Piece == EHutongCompoundPiece::SideHouse; });
		if (!TestEqual(TEXT("two 廂房"), Wings.Num(), 2)) return false;
		TestEqual(TEXT("四耳: two on the hall, one at the south end of each wing"), Ears.Num(), 4);

		for (const FHutongCompoundSlot& E : WingEars)
		{
			TestTrue(TEXT("a 廂耳房 is shallower than its 廂房"), E.Size.X < Wings[0].Size.X - 1.0);
		}

		// The 廂房 does not span the court's length.
		for (const FHutongCompoundSlot& Wing : Wings)
		{
			const bool bEarBefore = WingEars.ContainsByPredicate([&](const FHutongCompoundSlot& E)
				{ return E.Min.Y + E.Size.Y <= Wing.Min.Y + 0.01; });
			const bool bEarAfter = WingEars.ContainsByPredicate([&](const FHutongCompoundSlot& E)
				{ return E.Min.Y >= Wing.Min.Y + Wing.Size.Y - 0.01; });
			TestTrue(TEXT("a 廂房 has its ear at the south end and nothing at the north"),
				bEarBefore && !bEarAfter);
		}

		// 小天井: pocket between 耳房 front and 廂房 north end, closed from the court by a 隔牆 as a light well.
		if (WingEars.Num() > 0)
		{
			const double EarFront = HallEars[0].Min.Y;
			// The well starts at the 廂房 north gable.
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
					// On the 廂房 inner face.
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

	// 十字甬路: spine to the hall steps, an arm to each 廂房.
	HutongGen::FCompoundInput Paths;
	Paths.Plan = EHutongCompoundPlan::TwoCourtyards;
	Paths.Width = 3200.0;
	Paths.Depth = 5200.0;
	const TArray<FHutongCompoundSlot> P = HutongGen::LayOutCompound(Paths)
		.FilterByPredicate([](const FHutongCompoundSlot& S)
			{ return S.Piece == EHutongCompoundPiece::Path; });
	if (!TestEqual(TEXT("甬路 is a cross, not a single run"), P.Num(), 3)) return false;

	// Arms abut the spine, never cross it.
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

	// Compartment: a 隔牆 each side, on the street row's inner face.
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
		// They run the 外院's full depth to the screen's wall.
		TestTrue(TEXT("a cheek reaches the wall the screen backs onto"),
			C->Min.Y + C->Size.Y >= Screen->Min.Y + Screen->Size.Y - 15.0);
	}
	TestEqual(TEXT("both cheeks carry a doorway"), Doors, 2);

	// The 外院's far end is a service yard with its own doorway.
	if (!TestEqual(TEXT("the 外院 is divided once"), Partitions.Num(), 1)) return false;
	TestTrue(TEXT("the partition carries a doorway"), Partitions[0]->WallGateAt >= 0.0);
	TestTrue(TEXT("the partition stands beyond the gate court"),
		In.bGateAtEastEnd ? Partitions[0]->Min.X > Screen->Min.X + Screen->Size.X
			: Partitions[0]->Min.X < Screen->Min.X);
	TestTrue(TEXT("the partition runs from the street row inward"),
		FMath::IsNearlyEqual(Partitions[0]->Min.Y, RowFace, 0.5)
			&& Partitions[0]->Size.Y > 150.0);
	// Slots inside the plot: see HutongLayout.Compound.Enclosure (three plot sizes).
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundScreenBackingTest,
	"HutongLayout.Compound.ScreenBacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundScreenBackingTest::RunTest(const FString& Parameters)
{
	// 座山影壁: the screen's back is the 垂花門's cross wall.
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

	// Cross wall: a 隔牆 across the plot either side of the 垂花門.
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

	// The footprint reaches past that face.
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

	// 花池: kerb retaining earth.
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		FHutongFlowerBedParams P;
		UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(P, 220.0, 150.0, Mesh);
		if (!TestTrue(TEXT("花池 builds"), Mesh.TriangleCount() > 0)) return false;

		TArray<int32> Counts;
		SlotCounts(Mesh, Counts);
		TestTrue(TEXT("花池 retains earth"), Counts[HutongGen::MatSlot_Earth] > 0);
		TestTrue(TEXT("花池 has a kerb"), Counts[HutongGen::MatSlot_BaseCourse] > 0);

		// Soil below the kerb top, else the two share a plane across the bed.
		double TopKerb = -BIG_NUMBER;
		for (int32 vid : Mesh.VertexIndicesItr()) TopKerb = FMath::Max(TopKerb, Mesh.GetVertex(vid).Z);
		TestTrue(TEXT("the kerb stands above the soil"),
			TopKerb >= P.KerbHeight - 0.01);
	}

	// 魚缸: a solid of revolution, so the check is that the sweep closed.
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		FHutongWaterJarParams P;
		UHutongWaterJarBuildingComponent::BuildWaterJarMesh(P, Mesh);
		if (!TestTrue(TEXT("魚缸 builds"), Mesh.TriangleCount() > 0)) return false;

		// Within its declared footprint.
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
	// A line-like slot states its run direction, matching its shape.
	for (EHutongCompoundPlan Plan :
		{ EHutongCompoundPlan::OneCourtyard, EHutongCompoundPlan::TwoCourtyards,
		  EHutongCompoundPlan::ThreeCourtyards })
	{
		HutongGen::FCompoundInput In;
		In.Plan = Plan;
		// Sized off the plan's minimum.
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

			// A run is longer than thick by construction; otherwise the slot came out too square for "whichever is longer" to be safe.
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
	// 三進: a 後院 behind the 正房, closed by the 後罩房, reached by a plot-edge 過道 past the gate-side 耳房.
	HutongGen::FCompoundInput In;
	In.Plan = EHutongCompoundPlan::ThreeCourtyards;
	double MinW, MinD;
	In.GetMinimumPlot(MinW, MinD);
	In.Width = MinW + 400.0;
	In.Depth = MinD + 400.0;

	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);
	if (!TestTrue(TEXT("the 三進 plan lays out"), Slots.Num() > 0)) return true;

	// 後罩房 closes the back: full width, back on the plot's north edge.
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

	// Hall row north face and the court to the 後罩房.
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

	// Both 耳房 run to the boundary; the way to the 後院 cuts through the gate-side one, that type's job
	// (四合院: the hall keeps the axis).
	int32 Ears = 0, Passages = 0;
	double PassageAt = -1.0;
	for (const FHutongCompoundSlot& S : Slots)
	{
		const bool bEar = S.Piece == EHutongCompoundPiece::EarRoom
			|| S.Piece == EHutongCompoundPiece::EarPassage;
		if (!bEar || S.Facing != EHutongBaySide::MinusY) continue;   // the wing's ears face across the court
		if (FMath::Abs(S.Min.Y + S.Size.Y - HallNorth) > 1.0) continue;

		++Ears;
		const bool bLow = S.Min.X < 0.5 * In.Width;
		const double Outer = bLow ? S.Min.X : (In.Width - (S.Min.X + S.Size.X));
		TestTrue(TEXT("an 耳房 runs out to its boundary"), Outer < 0.01);
		if (S.Piece == EHutongCompoundPiece::EarPassage)
		{
			++Passages;
			PassageAt = bLow ? 0.0 : In.Width;
			// Wide enough for room and passage; the layout refuses the ears otherwise.
			TestTrue(TEXT("the 耳房過道 seats its room beside the way"),
				S.Size.X >= In.MinEarRoomFrontage + In.PassageWidth - 1.0);
		}
	}
	TestEqual(TEXT("an 耳房 on each flank"), Ears, 2);
	TestEqual(TEXT("exactly one of them carries the 過道"), Passages, 1);
	// East is local -X: gate at east means gate side is the low-X end.
	TestTrue(TEXT("the 過道 is on the gate's own side"),
		In.bGateAtEastEnd ? (PassageAt == 0.0) : (PassageAt == In.Width));

	// The hall keeps the plot axis on either flank.
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::MainHall) continue;
		TestNearlyEqual(TEXT("the 正房 stands on the plot's centre line"),
			S.Min.X + 0.5 * S.Size.X, 0.5 * In.Width, 1.0);
	}

	// No strip beside the ear room: the 耳房過道 builds its own roof and walls.
	for (const FHutongCompoundSlot& S : Slots)
	{
		TestTrue(TEXT("no separate 過道 roof"), S.Piece != EHutongCompoundPiece::Passage);
	}

	// No other plan has one: the 後罩房 is 三進 only.
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
	// An internal wall meeting the perimeter stops inside it, never on the plot boundary.
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

		// An end stops at or before the perimeter's inner face.
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

			// Ends only: a run's thickness never reaches the boundary alone.
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

// Slots reserve GetFootprintDepth, so a deeper platform intrudes into its neighbour, asymmetrically,
// since a flipped run yaws about half the reported depth.
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

			// 臺基 and everything on it, not the roof: eaves oversail the footprint by design.
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

	// Round trip: footprint to walk and back is the footprint.
	{
		FHutongCorridorParams P;
		P.bClosedSide = true;
		P.bBuildBackWall = true;
		// Inside the walk's band, where the conversion does not clamp.
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
	// The 甬路 meets a 廂房 door; with the 坐凳楣子 unbroken the paving ends at a seat and the building cannot be entered.
	HutongGen::FCompoundInput In;
	In.Plan = EHutongCompoundPlan::TwoCourtyards;
	In.Width = 3200.0;
	In.Depth = 5200.0;
	In.CourtWalk = EHutongCourtWalk::Corridor;
	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(In);

	// The two runs down the court, where 甬路 arms meet.
	TArray<const FHutongCompoundSlot*> Sides;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece == EHutongCompoundPiece::Corridor && S.bLengthAlongY) Sides.Add(&S);
	}
	if (!TestEqual(TEXT("two 遊廊 run down the court"), Sides.Num(), 2)) return false;

	// Arms: 甬路 runs wider than deep; the spine the other way.
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
		// The fraction is in footprint terms on both runs.
		const double GapY = S->Min.Y + S->CorridorBenchGapAt * S->Size.Y;
		TestTrue(FString::Printf(
				TEXT("the break is where the 甬路 arrives (%.0f vs arm at %.0f)"), GapY, ArmY),
			FMath::Abs(GapY - ArmY) <= In.CorridorWalkWidth);
	}
	return true;
}


// Plotted wall thickness comes from the footprint: taken when narrower than the role asks, dropped
// otherwise, so a narrowed run can widen again.
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
	// The corner 檐柱 stands on the wall plane, foot a radius forward; the wrapping 墀頭 must reach
	// further or the column shows through as a wedge. Probed between pier face and column centre.
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
		Probe(TEXT("gate"), M, P.Width, ColR, HutongGen::Shell::ChitouBodyProjection(P.GetChitouProjection(), P.GetRoofOverhang()), P.FloorHeight);
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
		Probe(TEXT("house"), M, P.Width, ColR, HutongGen::Shell::ChitouBodyProjection(P.GetChitouProjection(), P.GetRoofOverhang()), P.GetFloorHeight());
	}
	{
		FHutongShopfrontParams P;
		P.ChitouProjection = 2.0;
		const double ColR = P.GetColumnRadiusFor(P.Width, P.Depth);
		TestTrue(TEXT("shop: the projection is floored at the column radius plus clearance"),
			P.GetChitouProjectionFor(P.Width, P.Depth) >= ColR + HutongCanon::Wall::ChitouColumnClearanceCm - 0.01);
		UE::Geometry::FDynamicMesh3 M;
		HutongGen::BuildShopfront(M, P);
		Probe(TEXT("shop"), M, P.Width, ColR, HutongGen::Shell::ChitouBodyProjection(P.GetChitouProjectionFor(P.Width, P.Depth), P.RoofOverhang), P.FloorHeight);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongEaveUndersideTest,
	"HutongLayout.Materials.EaveUndersideIsWood",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongEaveUndersideTest::RunTest(const FString& Parameters)
{
	// Through the component build: roof top stays tile; the soffit behind the front eave course is wood,
	// tagged by the roof itself, and no tile faces down there. Tested by face position and direction.
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

	// What a ray straight up from the ground first meets, between the eave course and the wall: timber
	// (soffit, 椽), never tile. Buried faces never answer.
	const double CourseW = B->Params.EaveFasciaWidth;
	double EdgeY = 0.0, TopZ = -1e9;
	for (int32 tid : M.TriangleIndicesItr())
	{
		if (MatIDs->GetValue(tid) != HutongGen::MatSlot_Roof) continue;
		const FVector3d C = M.GetTriCentroid(tid);
		TopZ = FMath::Max(TopZ, C.Z);
		EdgeY = FMath::Min(EdgeY, C.Y);
	}
	auto FirstHitUp = [&](const FVector3d& From) -> int32
	{
		int32 Best = -1;
		double BestZ = 1e9;
		for (int32 tid : M.TriangleIndicesItr())
		{
			FVector3d A, Bv, C;
			M.GetTriVertices(tid, A, Bv, C);
			const FVector2d P(From.X, From.Y);
			const double D = (Bv.X - A.X) * (C.Y - A.Y) - (Bv.Y - A.Y) * (C.X - A.X);
			if (FMath::Abs(D) < 1e-9) continue;
			const double U = ((P.X - A.X) * (C.Y - A.Y) - (P.Y - A.Y) * (C.X - A.X)) / D;
			const double V = ((Bv.X - A.X) * (P.Y - A.Y) - (Bv.Y - A.Y) * (P.X - A.X)) / D;
			if (U < 0.0 || V < 0.0 || U + V > 1.0) continue;
			const double Z = A.Z + U * (Bv.Z - A.Z) + V * (C.Z - A.Z);
			if (Z > From.Z && Z < BestZ) { BestZ = Z; Best = tid; }
		}
		return Best;
	};
	int32 Timber = 0, Tile = 0;
	for (int32 i = 0; i < 12; ++i)
	{
		for (const double F : { 0.25, 0.5, 0.75 })
		{
			const double X = 150.0 + i * 61.0;
			const double Y = FMath::Lerp(EdgeY + CourseW + 3.0, -3.0, F);
			const int32 Hit = FirstHitUp(FVector3d(X, Y, 100.0));
			if (Hit < 0) continue;
			const int32 Slot = MatIDs->GetValue(Hit);
			if (Slot == HutongGen::MatSlot_Wood || Slot == HutongGen::MatSlot_Paint) ++Timber;
			if (Slot == HutongGen::MatSlot_Roof) ++Tile;
		}
	}
	TestTrue(TEXT("the ridge is still tile"), TopZ > B->Params.GetEaveHeight());
	TestTrue(FString::Printf(TEXT("under the front eave, timber shows from below (%d timber, %d tile)"), Timber, Tile),
		Timber > 0 && Tile == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRolledXieshanCrownTest,
	"HutongLayout.Roofs.RolledXieshanCrown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRolledXieshanCrownTest::RunTest(const FString& Parameters)
{
	// 捲棚歇山 has no ridge: the crown is a fillet below the sharp fold, and nothing reaches the fold;
	// rakes ride a course over the crown, still under the sharp roof's ridge course.
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

	// Rolled roof's top vertices run along the whole crown, not two end points (a 山花 or rake above the roll).
	double MinX = 1e9, MaxX = -1e9;
	for (int32 vid : Rolled.VertexIndicesItr())
	{
		const FVector3d V = Rolled.GetVertex(vid);
		if (V.Z > RolledTop - 2.0) { MinX = FMath::Min(MinX, V.X); MaxX = FMath::Max(MaxX, V.X); }
	}
	TestTrue(FString::Printf(TEXT("the crown runs along the ridge line (%.0f to %.0f of 1000)"), MinX, MaxX), MaxX - MinX > 300.0);
	// Rolled and sharp close the same way; solidity is the shell test's job.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongBaseCourseLineTest,
	"HutongLayout.Detailing.BaseCourseLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongBaseCourseLineTest::RunTest(const FString& Parameters)
{
	// Every kind with a 下鹼 tops it at one canon height above ground by default, whatever its plinth,
	// so pieces along a frontage form one band; a courtyard wall keeps its lower third. The line can be
	// set from a neighbour and read back.
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

// 七檁前後廊 per 四合院建築及其構造 p.84: four column rows, figures in the source's ranges, ridge over mid-plan.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMainHallFrameTest,
	"HutongLayout.Proportions.MainHallFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMainHallFrameTest::RunTest(const FString& Parameters)
{
	FHutongSiheyuanParams P = HutongPresets::MakeHouse(HutongCanon::House::MainHall);
	P.Width = P.SuggestedFrontage;
	P.Depth = P.GetSuggestedDepth();

	const int32 N = P.GetBayCount();
	TestEqual(TEXT("three bays"), N, 3);
	const double Central = P.GetCentralBayWidth();
	const double Side = Central * P.SideBayWidthRatio;
	TestTrue(FString::Printf(TEXT("明間 %.0f in 390-420"), Central), Central >= 390.0 && Central <= 420.0);
	TestTrue(FString::Printf(TEXT("次間 %.0f about 330"), Side), FMath::Abs(Side - 330.0) <= 15.0);
	TestTrue(FString::Printf(TEXT("檐柱 %.0f in 330-350"), P.GetColumnHeight()),
		P.GetColumnHeight() >= 330.0 && P.GetColumnHeight() <= 350.0);
	TestTrue(FString::Printf(TEXT("進深 %.0f at least 700"), P.Depth), P.Depth >= 699.0);

	double Front, Rear;
	P.GetBuiltVerandaDepths(P.Depth, P.WallThickness, Front, Rear);
	TestTrue(TEXT("a 前廊 and a 後廊, one 步架 each"),
		FMath::IsNearlyEqual(Front, P.StepRun, 0.01) && FMath::IsNearlyEqual(Rear, P.StepRun, 0.01));

	// Ridge course position in depth: mean Y of everything within 1 cm of the top.
	auto RidgeY = [](const FHutongSiheyuanParams& Q)
	{
		FDynamicMesh3 M;
		HutongGen::BuildSiheyuan(M, Q);
		double Top = -TNumericLimits<double>::Max();
		for (const int32 V : M.VertexIndicesItr()) Top = FMath::Max(Top, M.GetVertex(V).Z);
		double Sum = 0.0;
		int32 Count = 0;
		for (const int32 V : M.VertexIndicesItr())
		{
			const FVector3d X = M.GetVertex(V);
			if (X.Z > Top - 1.0) { Sum += X.Y; ++Count; }
		}
		return Count ? Sum / Count : 0.0;
	};

	const double Centred = RidgeY(P);
	TestTrue(FString::Printf(TEXT("前後廊 ridge at %.0f, over the middle of %.0f"), Centred, P.Depth),
		FMath::Abs(Centred - 0.5 * P.Depth) < 5.0);

	// 前廊後無廊 on a 鑽金柱 frame: ridge still mid-plan, no 撅尾巴.
	FHutongSiheyuanParams Small = HutongPresets::MakeHouse(HutongCanon::House::MainHallSmall);
	Small.Width = Small.SuggestedFrontage;
	Small.Depth = Small.GetSuggestedDepth();
	Small.GetBuiltVerandaDepths(Small.Depth, Small.WallThickness, Front, Rear);
	TestTrue(TEXT("前廊後無廊: a 前廊 of one 步架 and no 後廊"),
		FMath::IsNearlyEqual(Front, Small.StepRun, 0.01) && Rear == 0.0);
	const double SmallRidge = RidgeY(Small);
	TestTrue(FString::Printf(TEXT("前廊後無廊 ridge at %.0f, over the middle of %.0f"), SmallRidge, Small.Depth),
		FMath::Abs(SmallRidge - 0.5 * Small.Depth) < 5.0);
	TestTrue(TEXT("and its roof is lower than the 七檁 hall's"),
		Small.GetRoofRise() < P.GetRoofRise());
	return true;
}

// 構架: 圖5-3-1's four column rows and stacked beams for 七檁前後廊; 圖5-3-2's 鑽金柱 frame for the small court.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFrameTest,
	"HutongLayout.Frame.MainHall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFrameTest::RunTest(const FString& Parameters)
{
	auto Placed = [](const HutongCanon::House::FHouse& Row)
	{
		FHutongFrameParams P = HutongPresets::MakeFrame(Row);
		P.House.Width = P.House.SuggestedFrontage;
		P.House.Depth = P.House.GetSuggestedDepth();
		return P;
	};

	FHutongFrameParams Big = Placed(HutongCanon::House::MainHall);
	TestFalse(TEXT("no roof until it is asked for"), Big.bHasRafters);
	// The rest tests the frame under a roof, so the roof goes on.
	Big.bHasRafters = true;
	const HutongGen::FrameLayout::FLayout L = HutongGen::FrameLayout::Make(Big.House);
	TestEqual(TEXT("七檁"), L.Num(), 7);
	TestTrue(TEXT("前後廊: the main beams span 金柱 to 金柱"), L.bRearVeranda && L.Front == 1 && L.Rear == 5);
	TestTrue(TEXT("both eaves level"), FMath::IsNearlyEqual(L.Support[0], L.Support[6]));
	TestTrue(TEXT("the 檐柱 is the house's column"),
		FMath::IsNearlyEqual(L.Support[0] - L.Floor, Big.House.GetColumnHeight(), 0.5));
	TestTrue(TEXT("each line higher toward the ridge"),
		L.Support[0] < L.Support[1] && L.Support[1] < L.Support[2] && L.Support[2] < L.Support[3]);
	TestTrue(TEXT("the 金柱 is thicker than the 檐柱"), L.GoldD > L.D);

	const HutongGen::FrameLayout::FLayout S = HutongGen::FrameLayout::Make(Placed(HutongCanon::House::MainHallSmall).House);
	TestTrue(TEXT("前廊後無廊: 五檁, a 前廊, no 後廊, the main beams ending on the 插梁's 瓜柱"),
		S.Num() == 5 && S.bFrontVeranda && !S.bRearVeranda && S.Front == 1 && S.Rear == 3);

	auto Build = [](const FHutongFrameParams& P)
	{
		FDynamicMesh3 M;
		HutongGen::BuildFrame(M, P);
		return M;
	};
	const FDynamicMesh3 Whole = Build(Big);
	const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Whole);
	// Not IsSolid: edges match by position and the 階條 share theirs with the platform body, reading as
	// duplicates. Every edge still has its partner.
	TestTrue(FString::Printf(TEXT("every member closed and wound outward (%d unmatched)"), R.Unmatched),
		R.Triangles > 0 && R.Unmatched == 0 && R.Volume > 0.0);
	const UE::Geometry::FAxisAlignedBox3d B = Whole.GetBounds();
	TestTrue(TEXT("stands on the ground"), FMath::Abs(B.Min.Z) < 1.0);
	TestTrue(TEXT("reaches past the ridge purlin"), B.Max.Z >= L.PurlinTop(L.Ridge()) - 1.0);
	TestTrue(TEXT("the eaves reach out to 上檐出 front and back"),
		B.Min.Y <= -Big.House.GetRoofOverhang() + 1.0 && B.Max.Y >= Big.House.Depth + Big.House.GetRearRoofOverhang() - 1.0);

	FHutongFrameParams Bare = Big;
	Bare.bHasPlatform = false;
	Bare.bHasRafters = false;
	const FDynamicMesh3 Skeleton = Build(Bare);

	// 檐枋, 墊板, 檁 and the 金/脊 sets pass the gable column by one reach, so with platform and rafters
	// off nothing reaches past that line in X.
	const double EndColumn = Big.House.GetBayBoundary(0, Big.House.GetBayCount(),
		Big.House.Width, Big.House.GetColumnRadius());
	const double RunEnd = EndColumn - HutongCanon::Frame::RunProjection * L.D;
	TestNearlyEqual(TEXT("the members along the frontage end on one line past the column"),
		Skeleton.GetBounds().Min.X, RunEnd, 0.01);
	TestTrue(TEXT("the platform and the rafters are what their toggles take off"),
		Skeleton.TriangleCount() < Whole.TriangleCount());
	TestTrue(TEXT("without the platform the columns stand at floor height"),
		FMath::Abs(Skeleton.GetBounds().Min.Z - L.Floor) < 2.0);

	UE_LOG(LogTemp, Display, TEXT("構架 正房: %d triangles, ridge purlin top %.0f, eave %.0f"),
		R.Triangles, L.PurlinTop(L.Ridge()), L.Support[0]);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRidgeSlotTest,
	"HutongLayout.Appearance.RidgeSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The 正脊 has its own slot: coursed, not tiled in 壟 like the slopes.
bool FHutongRidgeSlotTest::RunTest(const FString& Parameters)
{
	auto RidgeTris = [](const UE::Geometry::FDynamicMesh3& M)
	{
		int32 N = 0;
		if (const UE::Geometry::FDynamicMeshMaterialAttribute* Mat = M.HasAttributes() ? M.Attributes()->GetMaterialID() : nullptr)
		{
			for (int32 tid : M.TriangleIndicesItr()) N += Mat->GetValue(tid) == HutongGen::MatSlot_Ridge;
		}
		return N;
	};

	FHutongSiheyuanParams House = HutongPresets::MakeHouse(HutongCanon::House::MainHall);
	House.bHasRidgeCourse = true;
	UE::Geometry::FDynamicMesh3 A;
	UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(House, EHutongBaySide::MinusY, 0, 1060.0, House.GetSuggestedDepth(), A);
	TestTrue(TEXT("a gable roof's 正脊 is on the ridge slot"), RidgeTris(A) >= 12);

	House.bHasRidgeCourse = false;
	UE::Geometry::FDynamicMesh3 B;
	UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(House, EHutongBaySide::MinusY, 0, 1060.0, House.GetSuggestedDepth(), B);
	TestEqual(TEXT("and no ridge, no ridge slot"), RidgeTris(B), 0);

	FHutongHallParams Hall;
	Hall.RoofType = EHutongRoofType::Xieshan;
	UE::Geometry::FDynamicMesh3 C;
	UHutongHallBuildingComponent::BuildHallMesh(Hall, EHutongBaySide::MinusY, 0, 1500.0, 900.0, C);
	TestTrue(TEXT("a 歇山's 正脊 is on the ridge slot"), RidgeTris(C) > 0);
	return true;
}
