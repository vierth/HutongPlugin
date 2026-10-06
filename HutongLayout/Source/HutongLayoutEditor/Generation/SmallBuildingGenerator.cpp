#include "Generation/SmallBuildingGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongShopBay.h"
#include "Generation/HutongJiajia.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildSmallBuilding(FDynamicMesh3& Mesh, const FHutongSmallBuildingParams& P, bool bDetail)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 60.0);
		const double D = FMath::Max(P.Depth, 60.0);
		const double T = FMath::Clamp(P.WallThickness, 8.0, 0.25 * FMath::Min(W, D));
		const double Floor = P.GetFloorHeight();
		const double Eave = P.GetEaveHeight();
		const double Rise = P.GetRoofRise();
		const double Ov = FMath::Max(P.RoofOverhang, 0.0);
		const bool bLeanTo = P.Roof == EHutongSmallRoof::LeanTo;
		// A lean-to's underside: the eave at the front wall, rising to the back.
		const double Slope = Rise / D;
		auto Under = [&](double Y) { return Eave + Slope * Y; };
		// Top of the front wall: the eave, or under a lean-to the underside at the wall's back face.
		const double FrontTop = bLeanTo ? Under(T) : Eave;

		// 臺基, low and plain.
		if (Floor > 1.0)
		{
			Shell::AppendPlatform(Mesh, W, D, Floor, 20.0, 0.0, 0, 30.0, 0.0, 0.0, bDetail);
		}

		// Side and back walls: to the eave, or under a lean-to, following the slope.
		{
			FSlotScope Tag(Mesh, MatSlot_Body);
			auto Block = [&](double X0, double X1, double Y0, double Y1)
			{
				if (!bLeanTo)
				{
					AppendBox(Mesh, FVector3d(X0, Y0, Floor), FVector3d(X1, Y1, Eave));
					return;
				}
				const FVector3d C[8] = {
					{X0, Y0, Floor}, {X1, Y0, Floor}, {X1, Y1, Floor}, {X0, Y1, Floor},
					{X0, Y0, Under(Y0)}, {X1, Y0, Under(Y0)}, {X1, Y1, Under(Y1)}, {X0, Y1, Under(Y1)} };
				AppendHexahedron(Mesh, C);
			};
			Block(0.0, T, 0.0, D);
			Block(W - T, W, 0.0, D);
			Block(T, W - T, D - T, D);
		}

		// The front, bay by bay inside the side walls.
		const int32 N = FMath::Max(P.GetBayCount(), 1);
		auto Bound = [&](int32 i) { return FMath::Clamp(P.GetBayBoundary(i, N), T, W - T); };
		if (!bDetail)
		{
			FSlotScope Tag(Mesh, MatSlot_Body);
			AppendBox(Mesh, FVector3d(T, 0.0, Floor), FVector3d(W - T, T, FrontTop));
		}
		else if (P.Front == EHutongSmallFront::Open)
		{
			// Posts at the bay lines and a beam under the eave.
			FSlotScope Tag(Mesh, MatSlot_Wood);
			const double Post = 14.0;
			for (int32 i = 0; i <= N; ++i)
			{
				const double X = FMath::Clamp(Bound(i), T + 0.5 * Post, W - T - 0.5 * Post);
				AppendBox(Mesh, FVector3d(X - 0.5 * Post, 0.0, Floor), FVector3d(X + 0.5 * Post, Post, FrontTop - 20.0));
			}
			AppendBox(Mesh, FVector3d(T, 0.0, FrontTop - 20.0), FVector3d(W - T, Post, FrontTop));
		}
		else if (P.Front == EHutongSmallFront::Boarded)
		{
			// Loose boards between the bay lines, as a shop's 排板門.
			FSlotScope Tag(Mesh, MatSlot_Wood);
			ShopBay::FFront F;
			F.FaceY = 0.5 * T - 2.5;
			F.BoardThickness = 5.0;
			F.SillZ = Floor;
			F.HeadZ = FrontTop;
			F.OpenBayCount = 0;
			F.Counter = ShopBay::FFront::ECounter::None;
			ShopBay::AppendFront(Mesh, Bound, N, 0.0, F);
		}
		else
		{
			// Brick front: a door in the door bay, a window in each other.
			const int32 DoorBay = P.GetDoorBay(N);
			for (int32 i = 0; i < N; ++i)
			{
				const double X0 = Bound(i), X1 = Bound(i + 1);
				const double C = 0.5 * (X0 + X1);
				FSlotScope Tag(Mesh, MatSlot_Body);
				if (i == DoorBay)
				{
					const double Ow = FMath::Clamp(P.DoorWidth, 40.0, X1 - X0 - 40.0);
					const double Threshold = 10.0;
					// Two metres where the wall allows, lower under a low eave (a shrine's), never under the crouch floor.
					const double LeafTop = FMath::Max(FMath::Min(Floor + 200.0, FrontTop - 25.0), Passage::MinHeadZ(Floor, Threshold));
					const double JambTop = FMath::Min(LeafTop + 15.0, FrontTop - 10.0);
					if (Ow < 40.0 || JambTop <= LeafTop)
					{
						AppendBox(Mesh, FVector3d(X0, 0.0, Floor), FVector3d(X1, T, FrontTop));
						continue;
					}
					AppendBox(Mesh, FVector3d(X0, 0.0, Floor), FVector3d(C - 0.5 * Ow - 8.0, T, FrontTop));
					AppendBox(Mesh, FVector3d(C + 0.5 * Ow + 8.0, 0.0, Floor), FVector3d(X1, T, FrontTop));
					AppendBox(Mesh, FVector3d(C - 0.5 * Ow - 8.0, 0.0, JambTop), FVector3d(C + 0.5 * Ow + 8.0, T, FrontTop));
					Tag.Close();
					FSlotScope WoodTag(Mesh, MatSlot_Wood);
					FHutongDoorAssembly Door;
					Door.OpeningX0 = C - 0.5 * Ow;
					Door.OpeningX1 = C + 0.5 * Ow;
					Door.FrontY = 0.0;
					Door.BackY = T;
					Door.BottomZ = Floor;
					Door.LeafTopZ = LeafTop;
					Door.JambTopZ = JambTop;
					Door.FrameThickness = 8.0;
					Door.ThresholdHeight = Threshold;
					AppendDoorAssembly(Mesh, Door);
					continue;
				}
				const double Ww = FMath::Min(90.0, X1 - X0 - 60.0);
				const double Sill = Floor + 90.0, Head = FMath::Min(Floor + 185.0, FrontTop - 20.0);
				if (!P.bWindows || Ww < 30.0 || Head - Sill < 40.0)
				{
					AppendBox(Mesh, FVector3d(X0, 0.0, Floor), FVector3d(X1, T, FrontTop));
					continue;
				}
				const double WX0 = C - 0.5 * Ww, WX1 = C + 0.5 * Ww;
				AppendBox(Mesh, FVector3d(X0, 0.0, Floor), FVector3d(WX0, T, FrontTop));
				AppendBox(Mesh, FVector3d(WX1, 0.0, Floor), FVector3d(X1, T, FrontTop));
				AppendBox(Mesh, FVector3d(WX0, 0.0, Floor), FVector3d(WX1, T, Sill));
				AppendBox(Mesh, FVector3d(WX0, 0.0, Head), FVector3d(WX1, T, FrontTop));
				Tag.Close();
				// Lattice of two bars each way over a paper pane, mid-wall.
				const double Y = 0.5 * T;
				{
					FSlotScope LatticeTag(Mesh, MatSlot_Lattice);
					for (int32 k = 1; k <= 2; ++k)
					{
						const double BX = FMath::Lerp(WX0, WX1, k / 3.0), BZ = FMath::Lerp(Sill, Head, k / 3.0);
						AppendBox(Mesh, FVector3d(BX - 1.5, Y - 3.0, Sill), FVector3d(BX + 1.5, Y, Head));
						AppendBox(Mesh, FVector3d(WX0, Y - 3.0, BZ - 1.5), FVector3d(WX1, Y, BZ + 1.5));
					}
				}
				FSlotScope PaperTag(Mesh, MatSlot_Paper);
				AppendBox(Mesh, FVector3d(WX0, Y, Sill), FVector3d(WX1, Y + 1.0, Head));
			}
		}

		// Roof.
		if (bLeanTo)
		{
			// One slab, its underside on the walls' slope, out past the front and a little past the back and sides.
			const double Slab = FHutongSmallBuildingParams::LeanToSlab;
			const double Ro = FHutongSmallBuildingParams::LeanToRearOverhang;
			const double Side = 15.0;
			const double Y0 = -Ov, Y1 = D + Ro;
			const FVector3d C[8] = {
				{-Side, Y0, Under(Y0)}, {W + Side, Y0, Under(Y0)}, {W + Side, Y1, Under(Y1)}, {-Side, Y1, Under(Y1)},
				{-Side, Y0, Under(Y0) + Slab}, {W + Side, Y0, Under(Y0) + Slab}, {W + Side, Y1, Under(Y1) + Slab}, {-Side, Y1, Under(Y1) + Slab} };
			FSlotScope Tag(Mesh, MatSlot_Roof);
			AppendHexahedron(Mesh, C);
			return;
		}
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = Ov;
		Roof.RearOverhang = Ov;
		Roof.Rise = Rise;
		Roof.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * D, Ov, 0.0);
		Roof.Tile = P.RoofTile;
		Roof.bTileRuns = bDetail && P.bHasTileRuns;
		Roof.bHasRidgeCourse = bDetail;
		Roof.RidgeCourseHeight = 14.0;
		Roof.RidgeCourseWidth = 14.0;
		Roof.RidgeEndKick = 0.0;
		Roof.SlopeSegments = 6;
		Roof.FasciaDepth = 6.0;
		Roof.FasciaWidth = 8.0;
		Roof.RafterSection = 0.0;
		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
}
