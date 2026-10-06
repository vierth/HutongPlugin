#include "Generation/ShopfrontGenerator.h"
#include "Generation/HutongShopBay.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAABBTree3;

	// Widest run of X (cm) along which a walker can come in off the street onto the customers' floor:
	// level rays from well out in the street, knee to the top of a walker's head, all reach past the setback.
	double WidestWayIn(const FDynamicMesh3& Mesh, const FHutongShopfrontParams& P)
	{
		FDynamicMeshAABBTree3 Tree(&Mesh);
		const double Floor = P.GetFloorHeight();
		const double StartY = -400.0;
		const double Reach = -StartY + HutongGen::ShopBay::InsideSetbackCm - 10.0;
		double Best = 0.0, Run = 0.0;
		for (double X = 0.0; X <= P.Width; X += 5.0)
		{
			bool bClear = true;
			for (const double Z : { Floor + 25.0, Floor + 60.0, Floor + 100.0, Floor + 150.0, Floor + 185.0 })
			{
				double T = 0.0;
				int32 Tid = -1;
				if (Tree.FindNearestHitTriangle(FRay3d(FVector3d(X, StartY, Z), FVector3d(0, 1, 0)), T, Tid) && T < Reach)
				{
					bClear = false;
					break;
				}
			}
			Run = bClear ? Run + 5.0 : 0.0;
			Best = FMath::Max(Best, Run);
		}
		return Best;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongShopfrontWayInTest,
	"HutongLayout.Shopfront.WayIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongShopfrontWayInTest::RunTest(const FString& Parameters)
{
	// An open shop can be walked into, whatever its front and wherever its counter; boarded up, it cannot.
	const double Need = 2.0 * HutongCanon::Openings::WalkerRadiusCm;
	for (const EHutongShopFront Front : { EHutongShopFront::Plain, EHutongShopFront::Carved, EHutongShopFront::Platform, EHutongShopFront::Shed })
	{
	for (const bool bRaised : { false, true })
	{
		for (const EHutongShopCounter Counter : { EHutongShopCounter::None, EHutongShopCounter::Inside, EHutongShopCounter::Street })
		{
			// -1: the default, every bay open.
			for (const int32 Open : { 1, 2, -1 })
			{
				FHutongShopfrontParams P;
				P.Width = 1000.0;
				P.Depth = 520.0;
				P.Front = Front;
				P.Counter = Counter;
				P.bAllBaysOpen = Open < 0;
				P.OpenBayCount = FMath::Max(Open, 0);
				P.bRaisedPlatform = bRaised;
				P.bHasFence = bRaised;
				FDynamicMesh3 Mesh;
				HutongGen::BuildShopfront(Mesh, P);
				const double Way = WidestWayIn(Mesh, P);
				TestTrue(FString::Printf(TEXT("front %d, counter %d, %d open, raised %d: a way in %.0f cm wide (need %.0f)"),
					(int32)Front, (int32)Counter, Open, bRaised ? 1 : 0, Way, Need), Way >= Need);
			}
		}
	}
	}
	FHutongShopfrontParams Shut;
	Shut.Width = 1000.0;
	Shut.Depth = 520.0;
	Shut.bAllBaysOpen = false;
	Shut.OpenBayCount = 0;
	FDynamicMesh3 Mesh;
	HutongGen::BuildShopfront(Mesh, Shut);
	TestTrue(TEXT("boarded up, no way in"), WidestWayIn(Mesh, Shut) < Need);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongShopfrontCounterTest,
	"HutongLayout.Shopfront.Counter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongShopfrontCounterTest::RunTest(const FString& Parameters)
{
	// The inside counter stands behind the customers' floor; the street one on the front line; none, nothing.
	// Each ray's first hit at waist height across the middle of the front, past any column.
	auto Hits = [](EHutongShopCounter Counter)
	{
		FHutongShopfrontParams P;
		P.Width = 1000.0;
		P.Depth = 520.0;
		P.Counter = Counter;
		P.bAllBaysOpen = false;
		P.OpenBayCount = 1;
		P.bHasTradeSign = false;
		P.UprightSign = EHutongUprightSign::None;
		FDynamicMesh3 Mesh;
		HutongGen::BuildShopfront(Mesh, P);
		FDynamicMeshAABBTree3 Tree(&Mesh);
		TArray<double> Out;
		for (double X = 250.0; X <= 750.0; X += 10.0)
		{
			double T = 0.0;
			int32 Tid = -1;
			if (Tree.FindNearestHitTriangle(FRay3d(FVector3d(X, -400.0, P.FloorHeight + 60.0), FVector3d(0, 1, 0)), T, Tid))
			{
				Out.Add(T - 400.0);
			}
		}
		return Out;
	};
	const double Back = 520.0 - 30.0 - 1.0;
	auto Any = [](const TArray<double>& H, double Lo, double Hi) { return H.ContainsByPredicate([&](double Y) { return Y > Lo && Y < Hi; }); };
	const double CD = FHutongShopfrontParams().CounterDepth;
	TestTrue(TEXT("inside counter behind the customers' floor"),
		Any(Hits(EHutongShopCounter::Inside), HutongGen::ShopBay::InsideSetbackCm - 5.0, HutongGen::ShopBay::InsideSetbackCm + 5.0));
	TestTrue(TEXT("street counter on the front line"), Any(Hits(EHutongShopCounter::Street), -0.55 * CD - 3.0, -0.55 * CD + 2.0));
	TestFalse(TEXT("no counter: nothing between the front and the back wall"), Any(Hits(EHutongShopCounter::None), 5.0, Back));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongShopfrontOpenRangeTest,
	"HutongLayout.Shopfront.OpenBayRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongShopfrontOpenRangeTest::RunTest(const FString& Parameters)
{
	// However many bays and however many open, the open run lies inside the frontage and is that long.
	for (int32 N = 1; N <= 9; ++N)
	{
		for (int32 Open = 0; Open <= N; ++Open)
		{
			int32 Lo = 0, Hi = -1;
			HutongGen::ShopBay::OpenBayRange(N, Open, Lo, Hi);
			TestEqual(FString::Printf(TEXT("%d of %d open: run length"), Open, N), Hi - Lo + 1, Open);
			if (Open > 0)
			{
				TestTrue(FString::Printf(TEXT("%d of %d open: inside the frontage (%d..%d)"), Open, N, Lo, Hi), Lo >= 0 && Hi <= N - 1);
			}
		}
	}
	return true;
}

#endif
