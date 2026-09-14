#include "Generation/ShopfrontGenerator.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongShopBay.h"
#include "Generation/SiheyuanGenerator.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildShopfront(FDynamicMesh3& Mesh, const FHutongShopfrontParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = P.GetEaveHeight();
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.25);
		const double ColR = P.GetColumnRadiusFor(W, D);

		const int32 N = (P.BayCountOverride > 0)
			? P.BayCountOverride
			: ComputeBayCount(W, P.MinBayWidth, P.MaxBayWidth);
		auto BayBoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, W, ColR); };

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);
		const double WallBottom = bBaseCourse ? Floor + BaseH : Floor;

		const double OpenTop = FMath::Clamp(P.OpeningTopRatio, 0.3, 0.95) * Eave;

		// 1) 臺基.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		// Flush, with the 下鹼 running down past it.
		const double PlatSide = 0.0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide, 0, 30.0, 0.0, 0.0);

		// 2) 下鹼 and walls on the three closed sides.
		{
			const int32 BaseFirstTri = Mesh.MaxTriangleID();
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP,
				/*bIncludeRear*/ true, /*bToGround*/ true);
			SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_BaseCourse);
		}

		AppendBox(Mesh, FVector3d(T, D - T, WallBottom), FVector3d(W - T, D, Eave));
		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, Eave));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, Eave));

		// 3) 墀頭 at the front corners, which on a shop are doing real work.
		if (P.bHasChitou)
		{
			Shell::AppendChitou(Mesh, W, T, Floor, Eave,
				P.GetChitouProjectionFor(W, D), P.ChitouCorbelSteps);
		}

		// --- The front ---
		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		const double ColTopR = Frame::TaperedTopRadius(ColR, Eave - Floor, P.ColumnTaperRatio);

		// Columns on the facade line, and the header from the opening head to the eave.
		Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, Floor, Eave, 1.0, 16);
		if (Eave > OpenTop)
		{
			AppendBox(Mesh, FVector3d(T, 0.0, OpenTop), FVector3d(W - T, T, Eave));
		}

		// 5) The bays themselves, which are the shop and are shared with the 樓 built over one.
		const int32 Open = FMath::Clamp(P.OpenBayCount, 0, N);
		int32 OpenLo = 0, OpenHi = -1;
		ShopBay::OpenBayRange(N, Open, OpenLo, OpenHi);
		const int32 Mid = N / 2;

		{
			ShopBay::FFront Front;
			// A hair proud of the wall plane: the boards' run starts inside the side walls, and on
			// the wall's own front plane their faces fought it for the strip at each corner.
			Front.FaceY = -0.5;
			Front.BoardWidth = P.BoardWidth;
			Front.BoardThickness = P.BoardThickness;
			Front.SillZ = Floor;
			Front.HeadZ = OpenTop;
			Front.OpenBayCount = Open;
			Front.bHasCounter = P.bHasCounter;
			Front.CounterHeight = P.CounterHeight;
			Front.CounterDepth = P.CounterDepth;
			ShopBay::AppendFront(Mesh, BayBoundaryX, N, ColR, Front);
		}

		// 6) 掛檐板 across the whole front, and 花牙子 in the corner of each bay under it.
		const double BoardDrop = FMath::Max(P.HangingBoardDrop, 4.0);
		const double BoardProj = FMath::Max(P.HangingBoardProjection, 1.0);
		if (P.bHasHangingBoard && OpenTop > Floor + BoardDrop)
		{
			AppendBox(Mesh,
				FVector3d(T,     -BoardProj, OpenTop - BoardDrop),
				FVector3d(W - T,  0.0,       OpenTop + 0.3 * BoardDrop));

			if (P.bHasSpandrels)
			{
				// Stepped, not scrolled: a scroll is carving, and these are the courses it would be cut from.
				const double Reach = FMath::Max(P.SpandrelReach, 4.0);
				const double Top = OpenTop - BoardDrop;
				const int32 Steps = 3;
				for (int32 i = 0; i <= N; ++i)
				{
					const double CX = BayBoundaryX(i);
					for (int32 s = 0; s < Steps; ++s)
					{
						const double A = Reach * (Steps - s) / double(Steps);
						const double Drop = 0.42 * Reach * (s + 1) / double(Steps);
						// One block each side of the column, except at the ends where the bay is only on the inside.
						if (i > 0)
						{
							AppendBox(Mesh,
								FVector3d(CX - A, -0.7 * BoardProj, Top - Drop),
								FVector3d(CX,      0.0,             Top));
						}
						if (i < N)
						{
							AppendBox(Mesh,
								FVector3d(CX,      -0.7 * BoardProj, Top - Drop),
								FVector3d(CX + A,   0.0,             Top));
						}
					}
				}
			}
		}

		// 7) 匾額 over the open bay, standing proud of the 掛檐板 so it reads as hung on it rather than cut into it.
		if (P.bHasSignboard && N > 0)
		{
			const int32 SignBay = (Open > 0) ? FMath::Clamp((OpenLo + OpenHi) / 2, 0, N - 1) : Mid;
			const double SX0 = BayBoundaryX(SignBay);
			const double SX1 = BayBoundaryX(SignBay + 1);
			const double SH = FMath::Max(P.SignboardHeight, 10.0);
			const double SignTop = FMath::Min(OpenTop + 0.25 * BoardDrop, Eave - 4.0);
			const double Inset = 0.12 * (SX1 - SX0);
			if (SX1 - SX0 > 3.0 * Inset && SignTop - SH > Floor)
			{
				AppendBox(Mesh,
					FVector3d(SX0 + Inset, -BoardProj - 0.55 * BoardProj, SignTop - SH),
					FVector3d(SX1 - Inset, -BoardProj + 0.2 * BoardProj,  SignTop));
			}
		}

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);

		// 8) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = P.GetRearRoofOverhang();
		Roof.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
			? FMath::Max(Roof.FrontOverhang - Roof.RearOverhang, 0.0)
			: 0.0;
		Roof.Rise = P.GetRoofRise();
		// 五檁, as an ordinary street building.
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Five, 0.5 * D, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.Tile = P.RoofTile;
		Roof.TileRowSpacing = P.TileRowSpacing;
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;

		// 封護檐 on the lane variant, exactly as on a house.
		const double RearLift = Shell::RearEaveLift(Roof, D);
		if (RearLift > 0.0)
		{
			const double BandP = FMath::Max(PlatSide, 2.0);
			AppendBox(Mesh,
				FVector3d(-BandP,    D - T,     Eave),
				FVector3d(W + BandP, D + BandP, Eave + RearLift));
		}

		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
}
