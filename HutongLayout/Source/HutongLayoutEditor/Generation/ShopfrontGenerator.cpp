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
		// Roof on the frame's line; walls to the ceiling under it; columns at the column top.
		const double RoofZ = Eave + P.GetRoofLift();
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		const double Floor = P.GetFloorHeight();
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

		// The open bays, which the steps, counter and signs gather round.
		const int32 Open = P.GetOpenBayCount(N);
		int32 OpenLo = 0, OpenHi = -1;
		ShopBay::OpenBayRange(N, Open, OpenLo, OpenHi);
		const int32 Mid = N / 2;
		const int32 SignLo = Open > 0 ? FMath::Clamp(OpenLo, 0, N - 1) : Mid;
		const int32 SignHi = Open > 0 ? FMath::Clamp(OpenHi, 0, N - 1) : Mid;
		const int32 SignBay = (SignLo + SignHi) / 2;

		// 1) 臺基; a raised one has broad 踏跺 the width of the open bays.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		// Flush; the 下鹼 runs down past it.
		const double PlatSide = 0.0;
		const double StepTread = 30.0;
		const int32 Steps = (P.bRaisedPlatform && Open > 0) ? FMath::CeilToInt32(Floor / 16.0) : 0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide, Steps, StepTread,
			BayBoundaryX(SignLo), BayBoundaryX(SignHi + 1));
		// Street-side reach of what stands on the ground: platform and steps.
		const double GroundReach = PlatO + Steps * StepTread;

		// 2) 下鹼 and walls on the three closed sides.
		{
			FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP,
				/*bIncludeRear*/ true, /*bToGround*/ true);
			BaseTag.Close();
		}

		AppendBox(Mesh, FVector3d(T, D - T, WallBottom), FVector3d(W - T, D, CeilZ));
		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, CeilZ));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, CeilZ));

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		const double ColTopR = Frame::TaperedTopRadius(ColR, Eave - Floor, P.ColumnTaperRatio);

		// Facade columns, header from opening head to eave.
		Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, Floor, Eave, 1.0, 24);
		if (CeilZ > OpenTop)
		{
			AppendBox(Mesh, FVector3d(T, 0.0, OpenTop), FVector3d(W - T, T, CeilZ));
		}

		// 5) Bays: the shop itself, shared with a 樓 built over one.

		{
			ShopBay::FFront Front;
			// A hair proud of the wall plane: boards start inside the side walls and z-fought the wall at each corner.
			Front.FaceY = -0.5;
			Front.BoardWidth = P.BoardWidth;
			Front.BoardThickness = P.BoardThickness;
			Front.SillZ = Floor;
			Front.HeadZ = OpenTop;
			Front.OpenBayCount = Open;
			Front.Counter = static_cast<ShopBay::FFront::ECounter>(P.Counter);
			Front.InsideDepth = D - T - Front.FaceY;
			Front.CounterHeight = P.CounterHeight;
			Front.CounterDepth = P.CounterDepth;
			ShopBay::AppendFront(Mesh, BayBoundaryX, N, ColR, Front);
		}

		// A sign: a painted frame with the face panel a centimetre proud of it, the panel's front mapped
		// once (U0..U1 across, top to bottom), facing the street (-Y). The street viewer's right is -X.
		auto AppendSignFace = [&](int32 Slot, double X0, double X1, double Z0, double Z1, double FaceY, double Depth,
			double U0, double U1)
		{
			const double Border = FMath::Min(6.0, 0.12 * FMath::Min(X1 - X0, Z1 - Z0));
			{
				FSlotScope FrameTag(Mesh, MatSlot_Paint);
				AppendBox(Mesh, FVector3d(X0, FaceY, Z0), FVector3d(X1, FaceY + Depth, Z1));
			}
			FSlotScope FaceTag(Mesh, Slot);
			const int32 First = Mesh.MaxTriangleID();
			AppendBox(Mesh, FVector3d(X0 + Border, FaceY - 1.0, Z0 + Border), FVector3d(X1 - Border, FaceY, Z1 - Border));
			FaceTag.Close();
			SetFaceUVs(Mesh, First, Mesh.MaxTriangleID(), FVector3d(0, -1, 0),
				FVector3d(X1 - Border, FaceY, Z1 - Border), FVector3d(-1, 0, 0), X1 - X0 - 2.0 * Border,
				FVector3d(0, 0, -1), Z1 - Z0 - 2.0 * Border, U0, U1);
		};
		// 招牌 pair either side of the sign bays, on posts whose front face is at FaceY: one texture
		// across both, its left half on the left board (larger X).
		auto AppendUprightPair = [&](double XLeft, double XRight, double Top, double FaceY)
		{
			if (P.UprightSign != EHutongUprightSign::OnPosts) return;
			const double SW = FMath::Max(P.UprightSignWidth, 10.0);
			const double SH = FMath::Clamp(P.UprightSignHeight, 30.0, Top - Floor - 60.0);
			if (SH < 30.0) return;
			// Inside the side walls: at the end posts of an all-open front, centred on them, the boards stood
			// half out past the footprint, onto a neighbour's in a street row.
			XLeft = FMath::Clamp(XLeft, T + 0.5 * SW, W - T - 0.5 * SW);
			XRight = FMath::Clamp(XRight, T + 0.5 * SW, W - T - 0.5 * SW);
			AppendSignFace(MatSlot_Signboard, XLeft - 0.5 * SW, XLeft + 0.5 * SW, Top - SH, Top, FaceY - 4.0, 4.0, 0.0, 0.5);
			AppendSignFace(MatSlot_Signboard, XRight - 0.5 * SW, XRight + 0.5 * SW, Top - SH, Top, FaceY - 4.0, 4.0, 0.5, 1.0);
		};

		const EHutongShopFront Kind = P.Front;
		const double BoardProj = FMath::Max(P.HangingBoardProjection, 1.0);
		// Never lower than a walker's head above the floor: a raised platform shortens the opening under it.
		const double HeadRoom = HutongCanon::Openings::WalkerHeightCm + 10.0;
		const double BoardDrop = FMath::Min(
			FMath::Max(Kind == EHutongShopFront::Carved ? FMath::Max(P.HangingBoardDrop, 55.0) : P.HangingBoardDrop, 4.0),
			OpenTop - Floor - HeadRoom);
		const bool bBoard = P.bHasHangingBoard && Kind != EHutongShopFront::Plain && Kind != EHutongShopFront::Platform
			&& Kind != EHutongShopFront::Shed && BoardDrop >= 8.0;
		const double SignH = FMath::Max(P.SignboardHeight, 10.0);
		// Under the trade sign's arm: the arm meets the column below whatever spans the front.
		double ArmZ = Eave - 25.0;
		// The lanterns hang from the eave, or from the front of a canopy or shed.
		double LanternTop = Eave - 10.0, LanternY = -0.5 * FMath::Max(P.RoofOverhang, 40.0);
		// What a freestanding 招牌 must stand clear of, out in the street.
		double FrontReach = FMath::Max(GroundReach, FMath::Max(P.RoofOverhang, 0.0) + FMath::Max(P.EaveFasciaWidth, 0.0));

		// 6) 掛檐板 across the front, 花牙子 in each bay corner under it: painted.
		if (bBoard)
		{
			FSlotScope PaintTag(Mesh, MatSlot_Paint);
			AppendBox(Mesh,
				FVector3d(T,     -BoardProj, OpenTop - BoardDrop),
				FVector3d(W - T,  0.0,       OpenTop + 0.3 * BoardDrop));

			if (P.bHasSpandrels)
			{
				// Stepped, not scrolled: a scroll is carving; these are the courses it would be cut from.
				const double Reach = FMath::Max(P.SpandrelReach, 4.0);
				const double Top = OpenTop - BoardDrop;
				const int32 Courses = 3;
				for (int32 i = 0; i <= N; ++i)
				{
					const double CX = BayBoundaryX(i);
					for (int32 s = 0; s < Courses; ++s)
					{
						const double A = Reach * (Courses - s) / double(Courses);
						const double Drop = 0.42 * Reach * (s + 1) / double(Courses);
						// A block each side of the column; end columns only inward.
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

		// 7) The street face proper.
		const double SX0 = BayBoundaryX(SignBay), SX1 = BayBoundaryX(SignBay + 1);
		const double PlaqueInset = 0.12 * (SX1 - SX0);
		if (Kind == EHutongShopFront::Platform)
		{
			// 拍子: a flat canopy on a row of posts, its top under the eave, the railing out past the eave tip.
			const double CD = FMath::Max(P.CanopyDepth, 40.0);
			const double SlabTop = Eave - 25.0;
			const double SlabBot = SlabTop - 18.0;
			const double Post = 16.0;
			const double PostY0 = -CD, PostY1 = -CD + Post;
			const double FasciaBot = SlabBot - 40.0;
			ArmZ = SlabBot - 12.0;
			LanternTop = FasciaBot - 2.0;
			LanternY = -CD + 20.0;
			FrontReach = FMath::Max(FrontReach, CD + 6.0);
			for (int32 i = 0; i <= N; ++i)
			{
				const double X = FMath::Clamp(BayBoundaryX(i), 0.5 * Post, W - 0.5 * Post);
				AppendBox(Mesh, FVector3d(X - 0.5 * Post, PostY0, 0.0), FVector3d(X + 0.5 * Post, PostY1, SlabBot));
			}
			AppendBox(Mesh, FVector3d(T, -CD, SlabBot), FVector3d(W - T, 0.0, SlabTop));
			{
				FSlotScope PaintTag(Mesh, MatSlot_Paint);
				AppendBox(Mesh, FVector3d(T, -CD - 6.0, FasciaBot), FVector3d(W - T, -CD, SlabTop));
			}
			// 朝天欄杆: posts at the bay lines, two rails, balusters between.
			const double RH = FMath::Max(P.CanopyRailHeight, 10.0);
			const double RY0 = -CD + 1.0, RY1 = -CD + 9.0;
			for (int32 i = 0; i <= N; ++i)
			{
				const double X = FMath::Clamp(BayBoundaryX(i), T + 5.0, W - T - 5.0);
				AppendBox(Mesh, FVector3d(X - 5.0, RY0 - 1.0, SlabTop), FVector3d(X + 5.0, RY1 + 1.0, SlabTop + RH + 8.0));
			}
			AppendBox(Mesh, FVector3d(T, RY0, SlabTop + RH - 7.0), FVector3d(W - T, RY1, SlabTop + RH));
			AppendBox(Mesh, FVector3d(T, RY0 + 1.0, SlabTop + 8.0), FVector3d(W - T, RY1 - 1.0, SlabTop + 14.0));
			const int32 Balusters = FMath::Max(FMath::RoundToInt32((W - 2.0 * T) / 30.0), 2);
			for (int32 b = 1; b < Balusters; ++b)
			{
				const double X = T + (W - 2.0 * T) * b / double(Balusters);
				AppendBox(Mesh, FVector3d(X - 2.0, RY0 + 2.0, SlabTop + 14.0), FVector3d(X + 2.0, RY1 - 2.0, SlabTop + RH - 7.0));
			}
			if (P.bHasSignboard)
			{
				const double H = FMath::Min(SignH, SlabTop - FasciaBot - 4.0);
				const double Zc = 0.5 * (SlabTop + FasciaBot);
				AppendSignFace(MatSlot_Plaque, SX0 + PlaqueInset, SX1 - PlaqueInset, Zc - 0.5 * H, Zc + 0.5 * H, -CD - 11.0, 5.0, 0.0, 1.0);
			}
			AppendUprightPair(BayBoundaryX(SignHi + 1), BayBoundaryX(SignLo), FasciaBot - 6.0, PostY0);
		}
		else if (Kind == EHutongShopFront::Shed)
		{
			// 涼棚 as in the 萬壽圖: thin posts out at the street, a front beam, and an open lattice top
			// laid back to a ledger on the columns, under the eave.
			const double SD = FMath::Max(P.ShedDepth, 40.0);
			const double Top = Eave - 25.0;
			const double Post = 10.0;
			const double BeamBot = Top - 22.0;
			ArmZ = BeamBot - 12.0;
			LanternTop = BeamBot - 2.0;
			LanternY = -SD + 20.0;
			FrontReach = FMath::Max(FrontReach, SD + 6.0);
			for (int32 i = 0; i <= N; ++i)
			{
				const double X = FMath::Clamp(BayBoundaryX(i), 0.5 * Post, W - 0.5 * Post);
				AppendBox(Mesh, FVector3d(X - 0.5 * Post, -SD, 0.0), FVector3d(X + 0.5 * Post, -SD + Post, BeamBot));
			}
			const double X0 = T, X1 = W - T;
			AppendBox(Mesh, FVector3d(X0, -SD, BeamBot), FVector3d(X1, -SD + Post, Top - 8.0));
			AppendBox(Mesh, FVector3d(X0, -ColR - 8.0, BeamBot + 8.0), FVector3d(X1, -ColR, Top - 8.0));
			// Lattice: slats along the run under slats across it, a hand apart.
			const double Pitch = 30.0;
			const int32 Along = FMath::Max(FMath::RoundToInt32((SD - ColR) / Pitch), 2);
			for (int32 j = 0; j <= Along; ++j)
			{
				const double Y = -SD + (SD - ColR - 4.0) * j / double(Along);
				AppendBox(Mesh, FVector3d(X0, Y, Top - 8.0), FVector3d(X1, Y + 4.0, Top - 4.0));
			}
			const int32 Across = FMath::Max(FMath::RoundToInt32((X1 - X0) / Pitch), 2);
			for (int32 k = 0; k <= Across; ++k)
			{
				const double X = X0 + (X1 - X0 - 4.0) * k / double(Across);
				AppendBox(Mesh, FVector3d(X, -SD, Top - 4.0), FVector3d(X + 4.0, -ColR, Top));
			}
			// The plaque on the shop's header behind, as on a plain front.
			if (P.bHasSignboard && N > 0)
			{
				const double SignTop = FMath::Min(OpenTop + SignH + 6.0, CeilZ - 4.0);
				if (SX1 - SX0 > 3.0 * PlaqueInset && SignTop - SignH > Floor)
				{
					AppendSignFace(MatSlot_Plaque, SX0 + PlaqueInset, SX1 - PlaqueInset, SignTop - SignH, SignTop, -5.0, 5.0, 0.0, 1.0);
				}
			}
			AppendUprightPair(BayBoundaryX(SignHi + 1), BayBoundaryX(SignLo), BeamBot - 10.0, -SD);
		}
		else
		{
			// 匾額 over the sign bay: on the header of a plain front, hung proud of the 掛檐板 on a carved one.
			if (P.bHasSignboard && N > 0)
			{
				const double SignTop = bBoard ? FMath::Min(OpenTop + 0.25 * BoardDrop, Eave - 4.0) : FMath::Min(OpenTop + SignH + 6.0, CeilZ - 4.0);
				// Hung in the doorway, it keeps the walker's head clear too.
				const double PlaqueH = FMath::Min(SignH, SignTop - Floor - HeadRoom);
				const double FaceY = bBoard ? -1.55 * BoardProj : -5.0;
				const double Depth = bBoard ? 0.75 * BoardProj : 5.0;
				if (SX1 - SX0 > 3.0 * PlaqueInset && PlaqueH >= 20.0)
				{
					AppendSignFace(MatSlot_Plaque, SX0 + PlaqueInset, SX1 - PlaqueInset, SignTop - PlaqueH, SignTop, FaceY, Depth, 0.0, 1.0);
				}
			}
			const double UprightTop = (bBoard ? OpenTop - BoardDrop : OpenTop) - 10.0;
			AppendUprightPair(BayBoundaryX(SignHi + 1), BayBoundaryX(SignLo), UprightTop, -ColR);
		}

		// 沖天招牌, as in the 萬壽圖: one tall board on its own post out in the street beside the way in
		// (the open bays' high-X side, clear of their steps and any front posts), its face to the street.
		if (P.UprightSign == EHutongUprightSign::Freestanding && N > 0)
		{
			const double SW = FMath::Max(P.UprightSignWidth, 10.0);
			const double SH = FMath::Max(P.UprightSignHeight, 30.0);
			const double Post = 12.0;
			const double PX = FMath::Min(BayBoundaryX(SignHi + 1) + 0.5 * SW + 25.0, W - 0.5 * SW);
			const double PY = -(FrontReach + 30.0 + 0.5 * Post);
			const double Top = FMath::Max(Eave, Floor + SH + 60.0);
			AppendBox(Mesh, FVector3d(PX - 0.5 * Post, PY - 0.5 * Post, 0.0), FVector3d(PX + 0.5 * Post, PY + 0.5 * Post, Top + 6.0));
			{
				FSlotScope BaseTag(Mesh, MatSlot_Stone);
				AppendBox(Mesh, FVector3d(PX - 22.0, PY - 22.0, 0.0), FVector3d(PX + 22.0, PY + 22.0, 30.0));
			}
			{
				// A little hood over the board.
				FSlotScope CapTag(Mesh, MatSlot_Paint);
				AppendBox(Mesh, FVector3d(PX - 0.5 * SW - 6.0, PY - 0.5 * Post - 12.0, Top + 6.0), FVector3d(PX + 0.5 * SW + 6.0, PY + 0.5 * Post + 4.0, Top + 12.0));
			}
			AppendSignFace(MatSlot_Signboard, PX - 0.5 * SW, PX + 0.5 * SW, Top - SH, Top, PY - 0.5 * Post - 4.0, 4.0, 0.0, 1.0);
		}

		// Round paper lanterns over each open bay.
		if (P.bHasLanterns && Open > 0)
		{
			const double R = 16.0, H = 34.0;
			const double Bottom = FMath::Max(LanternTop - 12.0 - H, Floor + HeadRoom);
			if (LanternTop - Bottom >= H + 4.0)
			{
				for (int32 i = OpenLo; i <= OpenHi; ++i)
				{
					const double X = 0.5 * (BayBoundaryX(i) + BayBoundaryX(i + 1));
					AppendBox(Mesh, FVector3d(X - 0.6, LanternY - 0.6, Bottom + H), FVector3d(X + 0.6, LanternY + 0.6, LanternTop + 20.0));
					{
						FSlotScope CapTag(Mesh, MatSlot_Paint);
						AppendCylinder(Mesh, FVector3d(X, LanternY, Bottom - 3.0), 0.6 * R, 3.0, 12);
						AppendCylinder(Mesh, FVector3d(X, LanternY, Bottom + H), 0.6 * R, 3.0, 12);
					}
					FSlotScope PaperTag(Mesh, MatSlot_Paper);
					AppendCylinder(Mesh, FVector3d(X, LanternY, Bottom), R, H, 16);
				}
			}
		}

		// 柵欄: a low picket fence along the platform's edge before the shut bays.
		if (P.bHasFence)
		{
			const double FY0 = -PlatO + 2.0, FY1 = -PlatO + 8.0;
			const double FH = 85.0;
			for (int32 i = 0; i < N; ++i)
			{
				if (Open > 0 && i >= OpenLo && i <= OpenHi) continue;
				const double X0 = FMath::Max(BayBoundaryX(i) + ColR, T), X1 = FMath::Min(BayBoundaryX(i + 1) - ColR, W - T);
				if (X1 - X0 < 40.0) continue;
				AppendBox(Mesh, FVector3d(X0, FY0, Floor + 18.0), FVector3d(X1, FY1, Floor + 24.0));
				AppendBox(Mesh, FVector3d(X0, FY0, Floor + FH - 14.0), FVector3d(X1, FY1, Floor + FH - 8.0));
				const int32 Pickets = FMath::Max(FMath::RoundToInt32((X1 - X0) / 14.0), 2);
				for (int32 k = 0; k <= Pickets; ++k)
				{
					const double X = X0 + (X1 - X0 - 4.0) * k / double(Pickets);
					AppendBox(Mesh, FVector3d(X, FY0 + 1.0, Floor), FVector3d(X + 4.0, FY1 - 1.0, Floor + FH));
				}
			}
		}

		// 幌子: an arm out from the end column, the sign hung under its tip edge-on to the facade so the
		// street sees it from both ways; each face mapped once, unmirrored to its own viewer.
		if (P.bHasTradeSign && N > 0)
		{
			const double X = FMath::Max(BayBoundaryX(0), T + 2.0);
			const double Reach = FMath::Max(P.TradeSignReach, 20.0);
			const double TW = FMath::Clamp(P.TradeSignWidth, 10.0, Reach - 10.0);
			const double TH = FMath::Max(P.TradeSignHeight, 15.0);
			const double Top = ArmZ - 14.0;
			const double Bot = FMath::Max(Top - TH, Floor + HeadRoom);
			if (Top - Bot > 15.0)
			{
				AppendBox(Mesh, FVector3d(X - 3.0, -Reach - 4.0, ArmZ), FVector3d(X + 3.0, -ColR, ArmZ + 6.0));
				const double Y0 = -Reach, Y1 = -Reach + TW;
				AppendBox(Mesh, FVector3d(X - 0.6, Y0 + 3.0, Top), FVector3d(X + 0.6, Y0 + 4.2, ArmZ));
				AppendBox(Mesh, FVector3d(X - 0.6, Y1 - 4.2, Top), FVector3d(X + 0.6, Y1 - 3.0, ArmZ));
				FSlotScope SignTag(Mesh, MatSlot_TradeSign);
				const int32 First = Mesh.MaxTriangleID();
				AppendBox(Mesh, FVector3d(X - 1.5, Y0, Bot), FVector3d(X + 1.5, Y1, Top));
				SignTag.Close();
				const int32 End = Mesh.MaxTriangleID();
				// Seen from +X the viewer's right is -Y; from -X it is +Y.
				SetFaceUVs(Mesh, First, End, FVector3d(1, 0, 0), FVector3d(X, Y1, Top), FVector3d(0, -1, 0), TW, FVector3d(0, 0, -1), Top - Bot);
				SetFaceUVs(Mesh, First, End, FVector3d(-1, 0, 0), FVector3d(X, Y0, Top), FVector3d(0, 1, 0), TW, FVector3d(0, 0, -1), Top - Bot);
			}
		}

		WoodTag.Close();

		// 8) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = P.GetRearRoofOverhang();
		Roof.UndersideRise = P.GetUndersideRise();
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

		// 封護檐: the roof builds the wall carried up under it, cornice and drip course.
		Roof.bTileRuns = P.bHasTileRuns;
		Roof.RafterEndInset = P.bHasChitou ? T : 0.0;

		// 墀頭 at the front corners; structural on a shop, reach the eave.
		if (P.bHasChitou)
		{
			FSlotScope PierTag(Mesh, MatSlot_Body);
			Shell::AppendChitou(Mesh, W, D, T, Floor, RoofZ,
				P.GetChitouProjectionFor(W, D), P.ChitouCorbelSteps, Roof, bBaseCourse ? BaseH : 0.0, BaseP);
			PierTag.Close();
		}

		if (RoofZ > Eave) Frame::AppendEaveStack(Mesh, 0.5 * T, W - 0.5 * T, 0.0, FMath::Max(P.ColumnDiameter, 2.0), Eave);
		Shell::AppendGableRoof(Mesh, W, D, RoofZ, Roof);
	}
}
