#include "Generation/HutongMeshInspect.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"
#include "Tools/GalleryTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPresets.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "StaticMeshResources.h"

#if WITH_DEV_AUTOMATION_TESTS

// What the plugin costs, as a number.

using UE::Geometry::FDynamicMesh3;

namespace
{
	// A house is the unit the city is counted in, so this is the number that decides whether 40 km² is reachable at all.
	constexpr int32 HouseBudgetTris = 7000;

	// The ToolKey is the string "Siheyuan".
	FHutongSiheyuanParams PresetParams(const FString& Name)
	{
		HutongPresets::RegisterBuiltInPresets();

		FHutongSiheyuanParams P;
		if (UHutongPresetLibrary* Lib = UHutongPresetLibrary::Get())
		{
			Lib->LoadPreset(TEXT("Siheyuan"), Name, FHutongSiheyuanParams::StaticStruct(), &P);
		}
		return P;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCostReportTest,
	"HutongLayout.Cost.Report",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCostReportTest::RunTest(const FString& Parameters)
{
	UHutongGalleryToolProperties* Settings = NewObject<UHutongGalleryToolProperties>();
	const TArray<HutongGallery::FHutongCostRow> Rows = HutongGallery::BuildCostReport(Settings);

	TestTrue(TEXT("the gallery has pieces in it"), Rows.Num() >= 15);

	// Sorted heaviest first: the point of a cost report is that the expensive thing is at the top where somebody will read it, not buried in declaration order.
	TArray<HutongGallery::FHutongCostRow> Sorted = Rows;
	Sorted.Sort([](const HutongGallery::FHutongCostRow& A, const HutongGallery::FHutongCostRow& B)
	{
		return A.NearTriangles() > B.NearTriangles();
	});

	int64 Totals[HutongGallery::FHutongCostRow::NumLevels] = {};
	double TotalMs = 0.0;

	UE_LOG(LogTemp, Display, TEXT("cost: %-40s %8s %8s %8s %8s %6s %8s"),
		TEXT("piece"), TEXT("塊"), TEXT("遠"), TEXT("近"), TEXT("精"),
		TEXT("slots"), TEXT("ms"));

	for (const HutongGallery::FHutongCostRow& R : Sorted)
	{
		UE_LOG(LogTemp, Display, TEXT("cost: %-40s %8d %8d %8d %8d %6d %8.2f"),
			*R.Label,
			R.Triangles[0], R.Triangles[1], R.Triangles[2], R.Triangles[3],
			R.MaterialSlots[(int32)EHutongDetail::Near],
			R.BuildMs[(int32)EHutongDetail::Near]);

		for (int32 L = 0; L < HutongGallery::FHutongCostRow::NumLevels; ++L)
		{
			Totals[L] += R.Triangles[L];
			TotalMs += R.BuildMs[L];
		}

		TestTrue(FString::Printf(TEXT("%s builds geometry at every level"), *R.Label),
			R.Triangles[0] > 0 && R.Triangles[1] > 0 && R.Triangles[2] > 0 && R.Triangles[3] > 0);

		// The property the whole scheme rests on. A level that is not cheaper than the one above it is not a level, and the failure is silent otherwise.
		TestTrue(FString::Printf(TEXT("%s: 塊 <= 遠"), *R.Label),
			R.Triangles[(int32)EHutongDetail::Massing] <= R.Triangles[(int32)EHutongDetail::Far]);
		TestTrue(FString::Printf(TEXT("%s: 遠 <= 近"), *R.Label),
			R.Triangles[(int32)EHutongDetail::Far] <= R.Triangles[(int32)EHutongDetail::Near]);
		TestTrue(FString::Printf(TEXT("%s: 近 <= 精"), *R.Label),
			R.Triangles[(int32)EHutongDetail::Near] <= R.Triangles[(int32)EHutongDetail::Hero]);

		// A section is a draw call.
		const int32 NearSlots = R.MaterialSlots[(int32)EHutongDetail::Near];
		TestTrue(FString::Printf(TEXT("%s wears a sane number of material slots"), *R.Label),
			NearSlots >= 1 && NearSlots <= HutongGen::MatSlot_Count);
	}

	UE_LOG(LogTemp, Display, TEXT("cost: %-40s %8lld %8lld %8lld %8lld %6s %8.2f"),
		TEXT("TOTAL"), Totals[0], Totals[1], Totals[2], Totals[3], TEXT(""), TotalMs);

	// The whole gallery in massing is what a first-pass city costs per piece.
	UE_LOG(LogTemp, Display, TEXT("cost: 塊 is %.0f%% of 近; 遠 is %.0f%%"),
		100.0 * (double)Totals[0] / FMath::Max((double)Totals[2], 1.0),
		100.0 * (double)Totals[1] / FMath::Max((double)Totals[2], 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCostHouseBudgetTest,
	"HutongLayout.Cost.HouseBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCostHouseBudgetTest::RunTest(const FString& Parameters)
{
	// The five shipped presets are what a city is actually made of, so they are what carries a budget.
	for (const FString& Name : HutongPresets::BuiltInSiheyuanNames())
	{
		FHutongSiheyuanParams P = PresetParams(Name);
		const double W = P.SuggestedFrontage > 0.0 ? P.SuggestedFrontage : 900.0;
		const double D = P.GetSuggestedDepth() > 0.0 ? P.GetSuggestedDepth() : 450.0;

		FDynamicMesh3 Mesh;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
			P, EHutongBaySide::MinusY, 0, W, D, Mesh);

		const int32 Tris = Mesh.TriangleCount();

		UE_LOG(LogTemp, Display, TEXT("cost: preset %-24s %5.0f×%-5.0f %6d tris"),
			*Name, W, D, Tris);

		TestTrue(FString::Printf(TEXT("%s builds geometry"), *Name), Tris > 0);
		TestTrue(FString::Printf(TEXT("%s stays inside the per-house budget (%d)"),
			*Name, HouseBudgetTris), Tris <= HouseBudgetTris);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDetailSavingsTest,
	"HutongLayout.Detail.Savings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDetailSavingsTest::RunTest(const FString& Parameters)
{
	// The premise of the detail system, asserted.
	auto Tris = [](auto Build) { FDynamicMesh3 M; Build(M); return M.TriangleCount(); };

	{
		const FHutongSiheyuanParams P = PresetParams(TEXT("Side House (廂房)"));
		const int32 Near = Tris([&](FDynamicMesh3& M)
		{
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				P, EHutongBaySide::MinusY, 0, 900.0, 450.0, M, EHutongDetail::Near);
		});
		const int32 Far = Tris([&](FDynamicMesh3& M)
		{
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				P, EHutongBaySide::MinusY, 0, 900.0, 450.0, M, EHutongDetail::Far);
		});

		UE_LOG(LogTemp, Display, TEXT("cost: 廂房 近 %d → 遠 %d (%.0f%%)"),
			Near, Far, 100.0 * Far / FMath::Max(Near, 1));
		TestTrue(TEXT("遠 takes a 廂房 to 60% of 近 or under"), Far * 10 <= Near * 6);
	}

	{
		FHutongHallParams P;   // 歇山 in 筒瓦, the heaviest thing the plugin builds
		const int32 Near = Tris([&](FDynamicMesh3& M)
		{
			UHutongHallBuildingComponent::BuildHallMesh(
				P, EHutongBaySide::MinusY, 0, 900.0, 620.0, M, EHutongDetail::Near);
		});
		const int32 Far = Tris([&](FDynamicMesh3& M)
		{
			UHutongHallBuildingComponent::BuildHallMesh(
				P, EHutongBaySide::MinusY, 0, 900.0, 620.0, M, EHutongDetail::Far);
		});

		UE_LOG(LogTemp, Display, TEXT("cost: 殿 近 %d → 遠 %d (%.0f%%)"),
			Near, Far, 100.0 * Far / FMath::Max(Near, 1));
		TestTrue(TEXT("遠 takes a 殿 to 60% of 近 or under"), Far * 10 <= Near * 6);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDetailMassingTest,
	"HutongLayout.Detail.Massing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDetailMassingTest::RunTest(const FString& Parameters)
{
	// A block has to be cheap, and it has to stand where the building stands.
	constexpr int32 MassingCeilingTris = 400;

	for (const FString& Name : HutongPresets::BuiltInSiheyuanNames())
	{
		FHutongSiheyuanParams P = PresetParams(Name);
		const double W = P.SuggestedFrontage > 0.0 ? P.SuggestedFrontage : 900.0;
		const double D = P.GetSuggestedDepth() > 0.0 ? P.GetSuggestedDepth() : 450.0;
		// The eave and the section below are read off the footprint, which is the tool's to fill in.
		P.Width = W;
		P.Depth = D;

		FDynamicMesh3 Mesh;
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
			P, EHutongBaySide::MinusY, 0, W, D, Mesh, EHutongDetail::Massing);

		const HutongMeshInspect::FShellReport R = HutongMeshInspect::InspectShell(Mesh);

		UE_LOG(LogTemp, Display, TEXT("cost: 塊 %-24s %5.0f×%-5.0f %4d tris"),
			*Name, W, D, R.Triangles);

		TestTrue(FString::Printf(TEXT("%s builds a block"), *Name), R.Triangles > 0);
		TestTrue(FString::Printf(TEXT("%s block is under the ceiling (%d)"),
			*Name, MassingCeilingTris), R.Triangles <= MassingCeilingTris);
		TestTrue(FString::Printf(TEXT("%s block is not inside out"), *Name), R.Volume > 0.0);

		// The silhouette test: the block must fill the footprint it stands on and rise past the eave to the ridge 舉架 puts there.
		const UE::Geometry::FAxisAlignedBox3d Bounds = Mesh.GetBounds();
		const double Over = P.GetRoofOverhang();

		TestTrue(FString::Printf(TEXT("%s block reaches its own eave"), *Name),
			Bounds.Max.Z > P.GetEaveHeight());
		TestTrue(FString::Printf(TEXT("%s block stops at its own ridge"), *Name),
			Bounds.Max.Z <= P.GetEaveHeight() + P.GetRoofSection().Rise() + 5.0);
		TestTrue(FString::Printf(TEXT("%s block sits on the ground"), *Name),
			Bounds.Min.Z >= -1.0);
		TestTrue(FString::Printf(TEXT("%s block spans its frontage"), *Name),
			Bounds.Min.X <= 1.0 && Bounds.Max.X >= W - 1.0);
		// The roof oversails the footprint front and back, and by no more than the eave it is.
		TestTrue(FString::Printf(TEXT("%s block keeps inside its own eaves"), *Name),
			Bounds.Min.Y >= -(Over + P.PlatformOverhang + 1.0) && Bounds.Max.Y <= D + Over + 1.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDetailPlanOnlyTest,
	"HutongLayout.Detail.PlanOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDetailPlanOnlyTest::RunTest(const FString& Parameters)
{
	using HutongGen::Detail::BuildPlacementLODs;

	// A laid-out building has no geometry at all: the outline component draws it.
	int32 Calls = 0;
	TArray<FDynamicMesh3> LODs;
	LODs.Emplace();
	BuildPlacementLODs(true, 600.0, 300.0, EHutongDetail::Hero, true,
		[&Calls](FDynamicMesh3&, EHutongDetail) { ++Calls; }, LODs);
	TestEqual(TEXT("plan-only builds nothing"), LODs.Num(), 0);
	TestEqual(TEXT("plan-only never calls the generator"), Calls, 0);

	BuildPlacementLODs(false, 600.0, 300.0, EHutongDetail::Massing, false,
		[&Calls](FDynamicMesh3& M, EHutongDetail)
		{
			++Calls;
			HutongMeshUtils::AppendBox(M, FVector3d::Zero(), FVector3d(10.0, 10.0, 10.0));
		}, LODs);
	TestEqual(TEXT("otherwise the generator runs"), Calls, 1);
	TestEqual(TEXT("and its mesh is the LOD"), LODs.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPlanBaysTest,
	"HutongLayout.Detail.PlanBays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPlanBaysTest::RunTest(const FString& Parameters)
{
	// A laid-out rectangle has to say how many 間 it is, and the divisions it draws must be the
	// ones the generator will build — which is why they come out of the params' own BayBoundary.
	UHutongSiheyuanBuildingComponent* House =
		NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	House->FootprintX = 960.0;
	House->FootprintY = 500.0;
	House->BayCountOverride = 3;
	// Deliberately off-centre: a turned facade that dropped the door bay's index would land on a
	// 次間 and nothing about a symmetrical house would show it.
	House->Params.DoorBayIndex = 0;

	for (int32 s = 0; s < 4; ++s)
	{
		const EHutongBaySide Side = (EHutongBaySide)s;
		House->BaySide = Side;
		const FString Name = StaticEnum<EHutongBaySide>()->GetNameStringByValue(s);

		FHutongPlanBays Bays;
		House->GetPlanBays(Bays);
		TestEqual(*(Name + TEXT(": three bays are four boundaries")), Bays.Boundaries.Num(), 4);
		if (Bays.Boundaries.Num() != 4) continue;

		bool bAscending = true;
		for (int32 i = 1; i < Bays.Boundaries.Num(); ++i)
		{
			bAscending &= Bays.Boundaries[i] > Bays.Boundaries[i - 1];
		}
		TestTrue(*(Name + TEXT(": the boundaries run up the facade")), bAscending);

		// Measured along the facade edge, and inset from each end by the corner column's radius.
		const bool bAlongX = House->ArePlanBaysAlongX();
		TestEqual(*(Name + TEXT(": counted along the facade")),
			bAlongX, HutongGen::BaySide::IsAlongX(Side));
		const double Span = bAlongX ? House->FootprintX : House->FootprintY;
		TestTrue(*(Name + TEXT(": inside the footprint")),
			Bays.Boundaries[0] > 0.0 && Bays.Boundaries.Last() < Span);
		TestTrue(*(Name + TEXT(": both ends inset by the same radius")),
			FMath::Abs(Bays.Boundaries[0] - (Span - Bays.Boundaries.Last())) < 0.01);

		// The door bay is the 明間, whichever way the facade has been turned.
		int32 Widest = 0;
		double Best = 0.0;
		for (int32 b = 0; b + 1 < Bays.Boundaries.Num(); ++b)
		{
			const double BayWidth = Bays.Boundaries[b + 1] - Bays.Boundaries[b];
			if (BayWidth > Best) { Best = BayWidth; Widest = b; }
		}
		TestEqual(*(Name + TEXT(": the door is in the widest bay")), Bays.DoorBay, Widest);
	}

	// A wall is not divided into bays, and the readout must not invent one.
	UHutongWallBuildingComponent* Wall =
		NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	FHutongPlanBays None;
	Wall->GetPlanBays(None);
	TestEqual(TEXT("a wall reports no bays"), None.Boundaries.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPlanColumnsTest,
	"HutongLayout.Detail.PlanColumns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPlanColumnsTest::RunTest(const FString& Parameters)
{
	// A frame and the house it is the frame of, on one footprint, put their column footings on the
	// plan in the same places: the frame tool's Full House stamps the one beside the other.
	for (const HutongCanon::House::FHouse& Canon : { HutongCanon::House::MainHall,
			HutongCanon::House::MainHallSmall, HutongCanon::House::MainHallFiveBay })
	{
		const FHutongFrameParams FrameParams = HutongPresets::MakeFrame(Canon);
		UHutongFrameBuildingComponent* Frame = NewObject<UHutongFrameBuildingComponent>(GetTransientPackage());
		UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		Frame->Params = FrameParams;
		House->Params = FrameParams.House;
		const FString Label = FString::Printf(TEXT("%.0f cm, %d rows expected"), Canon.FrontageCm, Canon.bRearVeranda ? 4 : 3);

		for (int32 s = 0; s < 4; ++s)
		{
			const EHutongBaySide Side = (EHutongBaySide)s;
			const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
			const double W = Canon.FrontageCm, D = FrameParams.House.GetSuggestedDepth();
			for (UHutongBuildingComponent* C : { (UHutongBuildingComponent*)Frame, (UHutongBuildingComponent*)House })
			{
				C->SetFootprintSize(bAlongX ? FVector2D(W, D) : FVector2D(D, W));
				C->SetFacade(Side);
			}
			FHutongPlanBays F, H;
			Frame->GetPlanBays(F);
			House->GetPlanBays(H);
			const FString Name = Label + TEXT(" · ") + StaticEnum<EHutongBaySide>()->GetNameStringByValue(s);

			TestEqual(*(Name + TEXT(": frame rows")), F.ColumnRows.Num(), Canon.bRearVeranda ? 4 : 3);
			TestEqual(*(Name + TEXT(": house rows")), H.ColumnRows.Num(), F.ColumnRows.Num());
			TestEqual(*(Name + TEXT(": boundaries")), H.Boundaries.Num(), F.Boundaries.Num());
			if (F.ColumnRows.Num() != H.ColumnRows.Num() || F.Boundaries.Num() != H.Boundaries.Num()) continue;
			for (int32 i = 0; i < F.ColumnRows.Num(); ++i)
			{
				TestTrue(*FString::Printf(TEXT("%s: row %d where the frame's is (%.1f vs %.1f)"), *Name, i, H.ColumnRows[i], F.ColumnRows[i]),
					FMath::Abs(H.ColumnRows[i] - F.ColumnRows[i]) < 0.5);
			}
			for (int32 i = 0; i < F.Boundaries.Num(); ++i)
			{
				TestTrue(*FString::Printf(TEXT("%s: column line %d where the frame's is"), *Name, i),
					FMath::Abs(H.Boundaries[i] - F.Boundaries[i]) < 0.5);
			}
			TestTrue(*(Name + TEXT(": footings drawn")), F.FootingSize > 2.0 * F.ColumnRadius && H.FootingSize > 0.0);
		}
		if (Canon.FrontageCm > 1500.0)
		{
			FHutongPlanBays F;
			Frame->GetPlanBays(F);
			TestEqual(TEXT("the five-bay hall is five bays"), F.Boundaries.Num(), 6);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDetailLODChainTest,
	"HutongLayout.Detail.LODChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDetailLODChainTest::RunTest(const FString& Parameters)
{
	using HutongGen::Detail::LODChain;
	using HutongGen::Detail::LODScreenSize;
	using HutongGen::Detail::BuildLODChain;

	// The four levels are also the four LODs of one mesh.

	// The chain is the placed level and everything below it, cheapest last.
	{
		const TArray<EHutongDetail> Near = LODChain(EHutongDetail::Near);
		TestEqual(TEXT("近 chains three levels"), Near.Num(), 3);
		TestEqual(TEXT("LOD0 is the placed level"), Near[0], EHutongDetail::Near);
		TestEqual(TEXT("the bottom of every chain is 塊"), Near.Last(), EHutongDetail::Massing);
		TestEqual(TEXT("精 chains four"), LODChain(EHutongDetail::Hero).Num(), 4);
		// Placed at the bottom there is nothing below to fall back to.
		TestEqual(TEXT("塊 chains one"), LODChain(EHutongDetail::Massing).Num(), 1);
	}

	// A LOD taking over further away than the one above it would never be drawn at all.
	for (int32 i = 1; i < 4; ++i)
	{
		TestTrue(FString::Printf(TEXT("LOD %d takes over further out than LOD %d"), i, i - 1),
			LODScreenSize(i) < LODScreenSize(i - 1));
	}

	// A house at 近: three levels, each genuinely cheaper than the one above, and LOD0 identical to what the placement would have baked with no chain at all.
	HutongPresets::RegisterBuiltInPresets();
	const TArray<FString>& Names = HutongPresets::BuiltInSiheyuanNames();
	if (Names.Num() > 0)
	{
		const FHutongSiheyuanParams P = PresetParams(Names[0]);
		const double W = P.SuggestedFrontage > 0.0 ? P.SuggestedFrontage : 900.0;
		const double D = P.GetSuggestedDepth() > 0.0 ? P.GetSuggestedDepth() : 450.0;

		auto Build = [&P, W, D](FDynamicMesh3& M, EHutongDetail Level)
		{
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				P, EHutongBaySide::MinusY, 0, W, D, M, Level);
		};

		TArray<FDynamicMesh3> LODs;
		BuildLODChain(EHutongDetail::Near, /*bChain*/ true, Build, LODs);
		TestEqual(TEXT("a house bakes three LODs"), LODs.Num(), 3);

		FDynamicMesh3 Alone;
		Build(Alone, EHutongDetail::Near);
		TestEqual(TEXT("LOD0 is the placed level's own mesh"),
			LODs.Num() > 0 ? LODs[0].TriangleCount() : 0, Alone.TriangleCount());

		for (int32 i = 1; i < LODs.Num(); ++i)
		{
			UE_LOG(LogTemp, Display, TEXT("cost: %s LOD%d %d tris"),
				*Names[0], i, LODs[i].TriangleCount());
			TestTrue(FString::Printf(TEXT("LOD %d is cheaper than LOD %d"), i, i - 1),
				LODs[i].TriangleCount() < LODs[i - 1].TriangleCount());
			TestTrue(FString::Printf(TEXT("LOD %d is not empty"), i), LODs[i].TriangleCount() > 0);
		}

		// Off, a placement is one mesh at one level, exactly as it was before the chain existed.
		TArray<FDynamicMesh3> Single;
		BuildLODChain(EHutongDetail::Near, /*bChain*/ false, Build, Single);
		TestEqual(TEXT("the chain can be turned off"), Single.Num(), 1);
	}

	// The drop rule, on the two cases that prove it is needed. A wall has no block form and reads 塊 as 遠.
	{
		auto Chain = [&](const FHutongWallParams& From)
		{
			auto Build = [&From](FDynamicMesh3& M, EHutongDetail Level)
			{
				UHutongWallBuildingComponent::BuildWallMesh(
					From, 800.0, From.GetThickness(), false, M, Level);
			};
			TArray<FDynamicMesh3> LODs;
			BuildLODChain(EHutongDetail::Near, /*bChain*/ true, Build, LODs);
			return LODs.Num();
		};

		// A 隔牆 carries 什錦窗, which is the whole of what a wall's levels touch.
		FHutongWallParams Inner;
		Inner.Role = EHutongWallRole::Courtyard;
		TestEqual(TEXT("a 隔牆's 塊 is its 遠 and is not baked twice"), Chain(Inner), 2);

		// A 院牆 is deliberately blank.
		FHutongWallParams Perimeter;
		Perimeter.Role = EHutongWallRole::Perimeter;
		TestEqual(TEXT("a blank 院牆 is one LOD at every range"), Chain(Perimeter), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDetailLODBakeTest,
	"HutongLayout.Detail.LODBake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDetailLODBakeTest::RunTest(const FString& Parameters)
{
	// The chain arithmetic is pinned next door; this is the half of it that only the asset can answer.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to spawn into"), World)) return false;

	HutongPresets::RegisterBuiltInPresets();
	const TArray<FString>& Names = HutongPresets::BuiltInSiheyuanNames();
	if (Names.Num() == 0) { World->DestroyWorld(false); return false; }

	const FHutongSiheyuanParams P = PresetParams(Names[0]);
	const double W = P.SuggestedFrontage > 0.0 ? P.SuggestedFrontage : 900.0;
	const double D = P.GetSuggestedDepth() > 0.0 ? P.GetSuggestedDepth() : 450.0;

	TArray<FDynamicMesh3> LODs;
	HutongGen::Detail::BuildLODChain(EHutongDetail::Near, /*bChain*/ true,
		[&P, W, D](FDynamicMesh3& M, EHutongDetail Level)
		{
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				P, EHutongBaySide::MinusY, 0, W, D, M, Level);
		}, LODs);

	const int32 Expected = LODs.Num();
	TArray<int32> SourceTris;
	for (const FDynamicMesh3& M : LODs) SourceTris.Add(M.TriangleCount());

	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
	if (!TestNotNull(TEXT("an actor to bake onto"), Actor)) { World->DestroyWorld(false); return false; }

	HutongGen::BuildAndAssignStaticMesh(Actor, LODs, FHutongPalette());

	UStaticMesh* Baked = Actor->GetStaticMeshComponent()
		? Actor->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
	if (!TestNotNull(TEXT("the actor carries a baked mesh"), Baked))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("one source model per level"), Baked->GetNumSourceModels(), Expected);

	if (const FStaticMeshRenderData* RD = Baked->GetRenderData())
	{
		TestEqual(TEXT("one built LOD per level"), RD->LODResources.Num(), Expected);

		const int32 Slots = Baked->GetStaticMaterials().Num();
		TestTrue(TEXT("the asset carries the chain's slots"), Slots > 0);

		for (int32 L = 0; L < RD->LODResources.Num(); ++L)
		{
			const FStaticMeshLODResources& R = RD->LODResources[L];
			// Built, not decimated: the LOD is the level's own mesh, triangle for triangle.
			TestEqual(FString::Printf(TEXT("LOD %d is the level's own mesh"), L),
				(int32)R.GetNumTriangles(), SourceTris.IsValidIndex(L) ? SourceTris[L] : -1);

			for (int32 Sec = 0; Sec < R.Sections.Num(); ++Sec)
			{
				const int32 Mat = R.Sections[Sec].MaterialIndex;
				TestTrue(FString::Printf(TEXT("LOD %d section %d lands on a real slot"), L, Sec),
					Mat >= 0 && Mat < Slots);
			}
		}
	}

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
