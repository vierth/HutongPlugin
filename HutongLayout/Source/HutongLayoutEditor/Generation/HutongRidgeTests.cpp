#include "Generation/HutongRidge.h"
#include "Generation/HutongBuildingComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Every roofed type's ridge estimate is measured against the roof its generator builds and
// against its massing block. The block carries no ridge course, so it lands on the estimate;
// the full building carries one, so it stands on the estimate by the course's height.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRidgeEstimatesTest,
	"HutongLayout.Roofs.RidgeEstimates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	// The highest point of the roof at mid-run, read by casting down across the whole depth
	// (with room for the eaves), so the 蠍子尾 and 翼角 at the ends cannot be hit.
	double RidgeOf(const UE::Geometry::FDynamicMesh3& M, double Width, double Depth)
	{
		if (M.TriangleCount() == 0) return -1.0;
		UE::Geometry::FDynamicMeshAABBTree3 Tree(&M, true);
		double Top = -TNumericLimits<double>::Max();
		const int32 N = 200;
		for (int32 i = 0; i <= N; ++i)
		{
			const double Y = -200.0 + (Depth + 400.0) * i / N;
			const FRay3d Ray(FVector3d(0.5 * Width, Y, 3000.0), FVector3d(0.0, 0.0, -1.0));
			double T; int32 TID;
			if (Tree.FindNearestHitTriangle(Ray, T, TID)) Top = FMath::Max(Top, 3000.0 - T);
		}
		return Top;
	}
}

bool FHutongRidgeEstimatesTest::RunTest(const FString& Parameters)
{
	using UE::Geometry::FDynamicMesh3;

	struct FCase
	{
		FString Name;
		double Estimate;
		double Width, Depth;
		TFunction<void(FDynamicMesh3&, EHutongDetail)> Build;
	};
	TArray<FCase> Cases;

	{
		FHutongSiheyuanParams P;
		Cases.Add({ TEXT("house"), HutongGen::Ridge::House(P, 1200.0, 600.0), 1200.0, 600.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, EHutongBaySide::MinusY, 0, 1200.0, 600.0, M, L); } });
	}
	{
		FHutongGateHouseParams P;
		P.bConstrainToHistoricalSize = false;
		Cases.Add({ TEXT("gate house"), HutongGen::Ridge::Gate(P, 600.0), 380.0, 600.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongGateHouseBuildingComponent::BuildGateHouseMesh(P, EHutongBaySide::MinusY, 380.0, 600.0, M, L); } });
	}
	{
		FHutongShopfrontParams P;
		Cases.Add({ TEXT("shopfront"), HutongGen::Ridge::Shop(P), 900.0, 500.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongShopfrontBuildingComponent::BuildShopfrontMesh(P, EHutongBaySide::MinusY, 0, 900.0, 500.0, M, L); } });
	}
	{
		FHutongStoreyParams P;
		Cases.Add({ TEXT("storey"), HutongGen::Ridge::Storey(P), 900.0, 620.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongStoreyBuildingComponent::BuildStoreyMesh(P, EHutongBaySide::MinusY, 0, 900.0, 620.0, M, L); } });
	}
	{
		FHutongHallParams P;
		Cases.Add({ TEXT("hall"), HutongGen::Ridge::Hall(P, 620.0), 900.0, 620.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongHallBuildingComponent::BuildHallMesh(P, EHutongBaySide::MinusY, 0, 900.0, 620.0, M, L); } });
	}
	{
		FHutongPavilionParams P;
		// The 寶頂 stands on the apex and is not the ridge.
		P.bHasFinial = false;
		Cases.Add({ TEXT("pavilion"), HutongGen::Ridge::Pavilion(P, 300.0), 300.0, 300.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongPavilionBuildingComponent::BuildPavilionMesh(P, 300.0, 300.0, M, L); } });
	}
	{
		FHutongCorridorParams P;
		P.Width = 150.0;
		Cases.Add({ TEXT("corridor"), HutongGen::Ridge::Corridor(P), 800.0, P.GetFootprintDepth(),
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongCorridorBuildingComponent::BuildCorridorMesh(P, 800.0, false, false, M, L); } });
	}
	{
		FHutongInnerGateParams P;
		Cases.Add({ TEXT("inner gate"), HutongGen::Ridge::InnerGate(P), 330.0, 140.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongInnerGateBuildingComponent::BuildInnerGateMesh(P, EHutongBaySide::MinusY, 330.0, 140.0, M, L); } });
	}
	{
		FHutongScreenWallParams P;
		Cases.Add({ TEXT("screen wall"), HutongGen::Ridge::ScreenWall(P), 400.0, P.GetFootprintDepth(),
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongScreenWallBuildingComponent::BuildScreenWallMesh(P, 400.0, false, M, L); } });
	}
	{
		FHutongPassageParams P;
		P.Width = 200.0;
		Cases.Add({ TEXT("passage"), HutongGen::Ridge::Passage(P), 300.0, 200.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongPassageBuildingComponent::BuildPassageMesh(P, 300.0, 200.0, false, M, L); } });
	}
	{
		FHutongEarPassageParams P;
		Cases.Add({ TEXT("ear room with passage"), HutongGen::Ridge::EarPassage(P, 760.0, 340.0), 760.0, 340.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongEarPassageBuildingComponent::BuildEarPassageMesh(P, EHutongBaySide::MinusY, 760.0, 340.0, M, L); } });
	}
	{
		FHutongPaifangParams P;
		Cases.Add({ TEXT("paifang"), HutongGen::Ridge::Paifang(P), 900.0, 200.0,
			[P](FDynamicMesh3& M, EHutongDetail L) { UHutongPaifangBuildingComponent::BuildPaifangMesh(P, 900.0, 200.0, false, M, L); } });
	}

	for (const FCase& C : Cases)
	{
		FDynamicMesh3 Near, Block;
		C.Build(Near, EHutongDetail::Near);
		C.Build(Block, EHutongDetail::Massing);
		const double NearTop = RidgeOf(Near, C.Width, C.Depth);
		const double BlockTop = RidgeOf(Block, C.Width, C.Depth);

		TestTrue(FString::Printf(TEXT("%s: the estimate is the built fold, under its ridge course (est %.1f, built %.1f)"), *C.Name, C.Estimate, NearTop),
			NearTop >= C.Estimate - 2.0 && NearTop <= C.Estimate + 60.0);
		TestTrue(FString::Printf(TEXT("%s: the massing block's ridge is the estimate (est %.1f, block %.1f)"), *C.Name, C.Estimate, BlockTop),
			FMath::Abs(BlockTop - C.Estimate) <= 6.0);
	}
	return true;
}

#endif
