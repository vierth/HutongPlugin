#include "Tools/CompoundTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Can the court be walked round under cover? Grid-samples the compound's collision-level meshes: a
// cell is standable with a floor and body height clear above; the walk is standable cells under a
// roof, linked to neighbours by at most a step.
namespace
{
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAABBTree3;

	constexpr double Cell = 10.0;
	constexpr double BodyHeight = 180.0;
	constexpr double BodyRadius = 20.0;
	constexpr double MaxStep = 20.0;
	constexpr double MaxFloor = 150.0;
	constexpr double RoofReach = 700.0;

	struct FWalkCell
	{
		bool bStand = false;
		bool bBlocked = false;
		bool bCovered = false;
		double Floor = 0.0;
	};

	struct FWalkGrid
	{
		int32 NX = 0, NY = 0;
		TArray<FWalkCell> Cells;
		FWalkCell& At(int32 X, int32 Y) { return Cells[Y * NX + X]; }
		const FWalkCell& At(int32 X, int32 Y) const { return Cells[Y * NX + X]; }
		FVector2D Centre(int32 X, int32 Y) const { return FVector2D((X + 0.5) * Cell, (Y + 0.5) * Cell); }
	};

	// These meshes are wound so GetTriNormal points into the solid.
	double OutwardZ(const FDynamicMesh3& M, int32 Tid) { return -M.GetTriNormal(Tid).Z; }

	FWalkGrid Sample(const FDynamicMesh3& Mesh, double W, double D)
	{
		FDynamicMeshAABBTree3 Tree(&Mesh);
		FWalkGrid G;
		G.NX = FMath::CeilToInt32(W / Cell);
		G.NY = FMath::CeilToInt32(D / Cell);
		G.Cells.SetNum(G.NX * G.NY);

		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		for (int32 y = 0; y < G.NY; ++y)
		{
			for (int32 x = 0; x < G.NX; ++x)
			{
				const FVector2D C = G.Centre(x, y);
				const FVector3d Origin(C.X, C.Y, -5.0);
				Hits.Reset();
				Tree.FindAllHitTriangles(FRay3d(Origin, FVector3d(0, 0, 1)), Hits);
				TArray<TPair<double, double>> Z; // height, outward z of the face
				for (const auto& H : Hits) Z.Add({ Origin.Z + H.Distance, OutwardZ(Mesh, H.TriangleId) });
				Z.Sort([](const TPair<double, double>& A, const TPair<double, double>& B) { return A.Key < B.Key; });

				// Ground, then every upward top below the highest standable floor.
				TArray<double> Candidates = { 0.0 };
				for (const auto& H : Z) if (H.Value > 0.5 && H.Key <= MaxFloor) Candidates.Add(H.Key);
				Candidates.Sort([](double A, double B) { return A > B; });

				FWalkCell& Out = G.At(x, y);
				Out.bBlocked = true;
				for (const double F : Candidates)
				{
					bool bClear = true, bInside = false;
					for (const auto& H : Z)
					{
						if (H.Key <= F + 3.0) continue;
						// First face above the feet: if it exits a solid, the feet are inside one.
						if (!bInside && H.Key > F + 3.0) { bInside = H.Value > 0.5; }
						if (H.Key < F + BodyHeight) { bClear = false; }
						break;
					}
					if (bClear && !bInside)
					{
						Out.bStand = true;
						Out.bBlocked = false;
						Out.Floor = F;
						for (const auto& H : Z)
						{
							if (H.Key >= F + BodyHeight && H.Key <= F + RoofReach) { Out.bCovered = true; break; }
						}
						break;
					}
				}
			}
		}
		return G;
	}

	// Standable, covered unless told otherwise, and a body radius clear of anything blocked.
	TArray<bool> WalkableCells(const FWalkGrid& G, bool bNeedCover = true)
	{
		TArray<bool> Ok;
		Ok.SetNumZeroed(G.NX * G.NY);
		const int32 R = FMath::CeilToInt32(BodyRadius / Cell);
		for (int32 y = 0; y < G.NY; ++y)
		{
			for (int32 x = 0; x < G.NX; ++x)
			{
				const FWalkCell& C = G.At(x, y);
				if (!C.bStand || (bNeedCover && !C.bCovered)) continue;
				bool bFree = true;
				for (int32 dy = -R; dy <= R && bFree; ++dy)
				{
					for (int32 dx = -R; dx <= R && bFree; ++dx)
					{
						if (dx * dx + dy * dy > R * R) continue;
						const int32 xx = x + dx, yy = y + dy;
						if (xx < 0 || yy < 0 || xx >= G.NX || yy >= G.NY) continue;
						const FWalkCell& N = G.At(xx, yy);
						// A wall, post, or a floor a step higher that would be walked into.
						if (N.bBlocked || (N.bStand && N.Floor > C.Floor + MaxStep)) bFree = false;
					}
				}
				Ok[y * G.NX + x] = bFree;
			}
		}
		return Ok;
	}

	int32 NearestWalkable(const FWalkGrid& G, const TArray<bool>& Ok, const FVector2D& P, const FBox2D& Within)
	{
		int32 Best = INDEX_NONE;
		double BestD = TNumericLimits<double>::Max();
		for (int32 y = 0; y < G.NY; ++y)
		{
			for (int32 x = 0; x < G.NX; ++x)
			{
				if (!Ok[y * G.NX + x]) continue;
				const FVector2D C = G.Centre(x, y);
				if (!Within.IsInside(C)) continue;
				const double Dd = FVector2D::DistSquared(C, P);
				if (Dd < BestD) { BestD = Dd; Best = y * G.NX + x; }
			}
		}
		return Best;
	}

	// BFS over walkable cells whose centre passes Allowed; returns cells reached.
	TArray<bool> Reach(const FWalkGrid& G, const TArray<bool>& Ok, int32 Start, TFunctionRef<bool(const FVector2D&)> Allowed)
	{
		TArray<bool> Seen;
		Seen.SetNumZeroed(G.NX * G.NY);
		if (Start == INDEX_NONE) return Seen;
		TArray<int32> Queue = { Start };
		Seen[Start] = true;
		for (int32 q = 0; q < Queue.Num(); ++q)
		{
			const int32 I = Queue[q];
			const int32 x = I % G.NX, y = I / G.NX;
			for (int32 dy = -1; dy <= 1; ++dy)
			{
				for (int32 dx = -1; dx <= 1; ++dx)
				{
					const int32 xx = x + dx, yy = y + dy;
					if ((dx == 0 && dy == 0) || xx < 0 || yy < 0 || xx >= G.NX || yy >= G.NY) continue;
					const int32 J = yy * G.NX + xx;
					if (Seen[J] || !Ok[J] || !Allowed(G.Centre(xx, yy))) continue;
					if (FMath::Abs(G.At(xx, yy).Floor - G.At(x, y).Floor) > MaxStep) continue;
					Seen[J] = true;
					Queue.Add(J);
				}
			}
		}
		return Seen;
	}

	// Grid picture for a failing run, written to HUTONG_WALK_DUMP.
	void DumpGrid(const FWalkGrid& G, const TArray<bool>& Ok, const TArray<bool>& A, const TArray<bool>& B)
	{
		const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("HUTONG_WALK_DUMP"));
		if (Path.IsEmpty()) return;
		FString Out = FString::Printf(TEXT("P3\n%d %d\n255\n"), G.NX, G.NY);
		for (int32 y = G.NY - 1; y >= 0; --y)
		{
			for (int32 x = G.NX - 1; x >= 0; --x)   // east (low X) on the right
			{
				const int32 I = y * G.NX + x;
				const FWalkCell& C = G.Cells[I];
				int32 R = 245, Gc = 245, Bc = 245;
				if (C.bBlocked) { R = Gc = Bc = 60; }
				else if (C.bStand && C.bCovered) { const int32 V = 150 + FMath::Clamp((int32)C.Floor, 0, 100); R = Gc = Bc = V; }
				if (Ok[I]) { R = 120; Gc = 180; Bc = 230; }
				if (A[I]) { R = 40; Gc = 170; Bc = 60; }
				if (B[I]) { R = 230; Gc = 140; Bc = 30; }
				Out += FString::Printf(TEXT("%d %d %d "), R, Gc, Bc);
			}
			Out += TEXT("\n");
		}
		FFileHelper::SaveStringToFile(Out, *Path);

		FString Csv = TEXT("x,y,stand,blocked,covered,floor,ok\n");
		for (int32 y = 0; y < G.NY; ++y)
		{
			for (int32 x = 0; x < G.NX; ++x)
			{
				const FWalkCell& C = G.At(x, y);
				Csv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%.1f,%d\n"), x, y, C.bStand, C.bBlocked, C.bCovered, C.Floor, (int32)Ok[y * G.NX + x]);
			}
		}
		FFileHelper::SaveStringToFile(Csv, *(Path + TEXT(".csv")));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCompoundWalkTest,
	"HutongLayout.Compound.WalkRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCompoundWalkTest::RunTest(const FString& Parameters)
{
	UHutongCompoundTool* Tool = NewObject<UHutongCompoundTool>();
	Tool->Settings = NewObject<UHutongCompoundToolProperties>(Tool);
	UHutongCompoundToolProperties* S = Tool->Settings;

	// Default first (大型, 三進, court walked round), then every size and plan with the walk.
	struct FCase { EHutongCompoundSize Size; EHutongCompoundPlan Plan; };
	const FCase Cases[] = {
		{ EHutongCompoundSize::Standard, EHutongCompoundPlan::ThreeCourtyards },
		{ EHutongCompoundSize::Large, EHutongCompoundPlan::ThreeCourtyards },
		{ EHutongCompoundSize::Large, EHutongCompoundPlan::TwoCourtyards },
		{ EHutongCompoundSize::Medium, EHutongCompoundPlan::ThreeCourtyards },
		{ EHutongCompoundSize::Small, EHutongCompoundPlan::TwoCourtyards },
	};
	for (const FCase& Case : Cases)
	{
		S->PlotSize = Case.Size;
		S->ApplyCourtSize();
		S->Plan = Case.Plan;
		S->CourtWalk = EHutongCourtWalk::Linked;

		double W = 0.0, D = 0.0;
		UHutongCompoundTool::GetStampedPlot(S, W, D);
		const FString Name = FString::Printf(TEXT("size %d plan %d"), int32(Case.Size), int32(Case.Plan));

		// 遠: collision cooks from it.
		TArray<HutongCompound::FBuiltSlot> Built;
		Tool->BuildCompound(W, D, EHutongDetail::Far, Built);
		FDynamicMesh3 All;
		FBox2D Gate(ForceInit), Hall(ForceInit);
		for (const HutongCompound::FBuiltSlot& B : Built)
		{
			All.AppendWithOffsets(B.Mesh);
			const FBox2D Box(B.Slot.Min, B.Slot.Min + B.Slot.Size);
			if (B.Slot.Piece == EHutongCompoundPiece::InnerGate) Gate = Box;
			if (B.Slot.Piece == EHutongCompoundPiece::MainHall) Hall = Box;
		}
		if (!TestTrue(*(Name + TEXT(": a 垂花門 and a 正房")), Gate.bIsValid && Hall.bIsValid)) continue;

		const FWalkGrid G = Sample(All, W, D);
		const TArray<bool> Ok = WalkableCells(G);

		// From under the 垂花門 roof, court side of its wall, to the 正房 前廊.
		FBox2D GateIn = Gate;
		GateIn.Min.Y = Gate.GetCenter().Y;
		const int32 Start = NearestWalkable(G, Ok, GateIn.GetCenter(), GateIn.ExpandBy(40.0));
		FBox2D HallFront = Hall;
		HallFront.Max.Y = Hall.Min.Y + 200.0;
		const int32 Goal = NearestWalkable(G, Ok, FVector2D(Hall.GetCenter().X, Hall.Min.Y + 60.0), HallFront);
		TestTrue(*(Name + TEXT(": somewhere to stand under the 垂花門")), Start != INDEX_NONE);
		TestTrue(*(Name + TEXT(": somewhere to stand on the 正房's 前廊")), Goal != INDEX_NONE);

		// Each side alone: the open court between is not part of the walk.
		const double Mid = 0.5 * W;
		const double Slack = 0.5 * Gate.GetSize().X;
		const TArray<bool> Low = Reach(G, Ok, Start, [&](const FVector2D& P) { return P.X <= Mid + Slack; });
		const TArray<bool> High = Reach(G, Ok, Start, [&](const FVector2D& P) { return P.X >= Mid - Slack; });
		// Dump the picture for a failing case, if asked.
		if (Goal == INDEX_NONE || !Low[Goal] || !High[Goal]) DumpGrid(G, Ok, Low, High);
		if (Goal != INDEX_NONE)
		{
			TestTrue(*(Name + TEXT(": the east side walks under cover from the 垂花門 to the 正房")), Low[Goal]);
			TestTrue(*(Name + TEXT(": the west side walks under cover from the 垂花門 to the 正房")), High[Goal]);
		}

		// The gate is walked through upright, 外院 flight to rear flight, within its own width.
		{
			const TArray<bool> Open = WalkableCells(G, /*bNeedCover*/ false);
			const double CX = Gate.GetCenter().X;
			const double Half = 0.5 * Gate.GetSize().X + 30.0;
			const FBox2D Band(FVector2D(CX - Half, Gate.Min.Y - 260.0), FVector2D(CX + Half, Gate.Max.Y + 260.0));
			const int32 Out = NearestWalkable(G, Open, FVector2D(CX, Gate.Min.Y - 200.0), Band);
			const int32 In = NearestWalkable(G, Open, FVector2D(CX, Gate.Max.Y + 200.0), Band);
			const TArray<bool> Through = Reach(G, Open, Out, [&](const FVector2D& P) { return Band.IsInside(P); });
			TestTrue(*(Name + TEXT(": the 垂花門 is walked straight through")), Out != INDEX_NONE && In != INDEX_NONE && Through[In]);
			if (Out == INDEX_NONE || In == INDEX_NONE || !Through[In]) DumpGrid(G, Open, Through, Through);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
