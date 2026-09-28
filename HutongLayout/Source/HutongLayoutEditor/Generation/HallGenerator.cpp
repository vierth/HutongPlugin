#include "Generation/HallGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/SiheyuanGenerator.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace
	{
		namespace G = HutongCanon::GrandHall;

		// A 斗栱 set, 單翹單昂 五踩, at the origin on its column or 攢 line, projecting to -Y; Dk the 斗口,
		// Z0 the 平板枋 top. 坐斗; 正心瓜栱 and the 翹 to one 踩; 正心萬栱, the 單材瓜栱 across it and the 昂 to
		// the second, its beak dropping; the 廂栱 and 耍頭. A 柱頭科's 翹 and 昂 are two 斗口 wide.
		void AppendBracketSet(FDynamicMesh3& Mesh, double Dk, double Z0, bool bColumnHead)
		{
			auto Box = [&](double X0, double Y0, double Za, double X1, double Y1, double Zb)
			{
				HutongMeshUtils::AppendBox(Mesh, FVector3d(X0 * Dk, Y0 * Dk, Z0 + Za * Dk), FVector3d(X1 * Dk, Y1 * Dk, Z0 + Zb * Dk));
			};
			const double A = bColumnHead ? 1.0 : 0.5;
			Box(-1.5, -1.5, 0.0, 1.5, 1.5, 2.0);                    // 坐斗
			Box(-3.1, -0.62, 1.2, 3.1, 0.62, 3.2);                  // 正心瓜栱
			Box(-A, -3.7, 1.2, A, 3.0, 3.2);                        // 翹
			Box(-0.9, -3.7, 3.0, 0.9, -2.3, 4.0);                   // 十八斗
			Box(-4.6, -0.62, 3.2, 4.6, 0.62, 5.2);                  // 正心萬栱
			Box(-3.1, -3.5, 3.4, 3.1, -2.5, 4.8);                   // 單材瓜栱
			Box(-A, -6.0, 3.2, A, 3.0, 5.2);                        // 昂
			Box(-A, -7.6, 2.4, A, -5.9, 4.0);                       // 昂嘴
			Box(-3.6, -6.5, 5.2, 3.6, -5.5, 6.6);                   // 廂栱
			Box(-A, -7.2, 5.2, A, 3.0, 7.2);                        // 耍頭
		}

		// Per-bay 平身科 counts; a side's own from the 攢檔 of 11 斗口 where the figure gives none.
		int32 SetsBetween(double Span, double Dk) { return FMath::Max(FMath::RoundToInt32(Span / (11.0 * Dk)) - 1, 0); }
	}

	// 大式 九檁歇山轉角前後廊 (則例 卷二): the figure's plan, section and members, every 尺 scaled by
	// GetGrandScale. Facade on -Y. Rows front to back: 檐柱, 金柱, 金柱, 檐柱; the facades at the 金柱 lines,
	// 山牆 at both ends, 斗栱 on all four sides.
	void BuildGrandHall(FDynamicMesh3& Mesh, const FHutongHallParams& P)
	{
		using namespace HutongMeshUtils;
		const double K = P.GetGrandScale();
		const double Dk = G::Doukou * K;
		const double W = FMath::Max(P.Width, 1.0), D = FMath::Max(P.Depth, 1.0);
		const double ColR = 0.5 * G::EaveColumn * K, GoldR = 0.5 * G::GoldColumn * K;
		// The figure's plan centred on the footprint.
		const double OX = 0.5 * (W - (G::Frontage + G::EaveColumn) * K) + ColR;
		const double OY = 0.5 * (D - (G::Depth + G::EaveColumn) * K) + ColR;
		TArray<double> BX = { OX };
		for (const double Bay : G::Bays) BX.Add(BX.Last() + Bay * K);
		const double Y[4] = { OY, OY + G::Veranda * K, OY + (G::Depth - G::Veranda) * K, OY + G::Depth * K };
		const double CX = 0.5 * (BX[0] + BX.Last()), CY = 0.5 * (Y[0] + Y[3]);
		const double HalfX = CX - BX[0], HalfY = CY - Y[0];

		const double Floor = G::PlatformHeight * K;
		const double Eave = P.GetEaveHeight();
		const double PurlinZ = Floor + G::ToPurlin * K;
		const double O = P.GetRoofOverhangBuilt();
		const double RoofZ = P.GetRoofBaseHeight();
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double PlateTop = Eave + G::PlateH * K;

		// Built on a side's line in a frame centred on the plan (line at -E, run along X), then turned onto it:
		// 0 front, 1 right, 2 back, 3 left. A rotation, never a mirror.
		auto Along = [&](int32 Side) { return (Side % 2 == 0) ? HalfX : HalfY; };
		auto Out = [&](int32 Side) { return (Side % 2 == 0) ? HalfY : HalfX; };
		auto PlaceSide = [&](int32 Side, int32 Mark)
		{
			TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, 90.0 * Side, 0.0), FVector(CX, CY, 0.0)));
		};
		// A side's column positions along its run, and the 平身科 between each pair.
		auto SideColumns = [&](int32 Side)
		{
			TArray<double> A;
			if (Side % 2 == 0) for (const double X : BX) A.Add(X - CX);
			else for (const double Yr : Y) A.Add(Yr - CY);
			return A;
		};

		// 1) 臺基 paved inside its 階條石, three flights before the 明間 and 次間 between 垂帶 on the column lines.
		const double PlatO = FMath::Max(G::PlatformReach * K - ColR, 0.0) + FMath::Min(OX, OY) - ColR;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatO, 0, 0.0, 0.0, 0.0);
		{
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			const int32 Risers = FMath::Max(FMath::RoundToInt32(G::PlatformHeight / G::StepRiser), 2);
			const double Tread = G::StepTread * K;
			const double Str = G::EdgeStone * K;
			const double EdgeY = -PlatO;
			for (int32 b = 1; b <= 3; ++b)
			{
				Shell::AppendSteps(Mesh, BX[b] + 0.5 * Str, BX[b + 1] - 0.5 * Str, EdgeY, -1.0, Risers - 1, Tread, Floor);
			}
			const double Run = (Risers - 1) * Tread;
			const double Toe = Floor / Risers;
			const TArray<FVector2d> Stringer = { { EdgeY, 0.0 }, { EdgeY, Floor }, { EdgeY - Run, Toe }, { EdgeY - Run, 0.0 } };
			for (int32 b = 1; b <= 4; ++b) AppendYZPrism(Mesh, Stringer, BX[b] - 0.5 * Str, BX[b] + 0.5 * Str);
		}

		// 2) 柱頂石 with 古鏡, then the columns: 檐柱 to the head, 金柱 on up into the ceiling.
		{
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			for (int32 r = 0; r < 4; ++r)
			{
				const bool bGold = (r == 1 || r == 2);
				const double Half = 0.5 * (bGold ? G::GoldBase : G::EaveBase) * K;
				const double Mirror = (bGold ? G::GoldMirror : G::EaveMirror) * K;
				for (const double X : BX)
				{
					AppendBox(Mesh, FVector3d(X - Half, Y[r] - Half, Floor - 4.0), FVector3d(X + Half, Y[r] + Half, Floor + 0.5));
					AppendCylinder(Mesh, FVector3d(X, Y[r], Floor), 1.2 * (bGold ? GoldR : ColR), Mirror, 20);
				}
			}
		}
		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		for (int32 r = 0; r < 4; ++r)
		{
			const bool bGold = (r == 1 || r == 2);
			const double R = bGold ? GoldR : ColR;
			const double Foot = Floor + (bGold ? G::GoldMirror : G::EaveMirror) * K;
			const double Top = bGold ? CeilZ + 2.0 : Eave;
			const double TopR = Frame::TaperedTopRadius(R, Top - Foot, P.ColumnTaperRatio);
			for (const double X : BX) Frame::AppendColumn(Mesh, X, Y[r], R, TopR, Foot, Top, 1.0, 24);
		}
		WoodTag.Close();

		// 3) 山牆 at both ends over the 檐柱 and 金柱 there, up under the 大額枋; a 裙肩 of the base course.
		{
			const double T = G::GableWall * K;
			const double Skirt = Floor + G::GableSkirt * K;
			for (const double X : { BX[0], BX.Last() })
			{
				FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
				AppendBox(Mesh, FVector3d(X - 0.5 * T - 2.0, Y[0], Floor), FVector3d(X + 0.5 * T + 2.0, Y[3], Skirt));
				BaseTag.Close();
				AppendBox(Mesh, FVector3d(X - 0.5 * T, Y[0] + 1.0, Skirt), FVector3d(X + 0.5 * T, Y[3] - 1.0, Eave - G::BigLintelH * K));
			}
		}

		// 4) Under the 檐柱 heads round all four sides: 大額枋, and on the open fronts 由額墊板 and 小額枋, each
		// passing the corner as a 箍頭; the 平板枋 over them.
		{
			FSlotScope PaintTag(Mesh, MatSlot_Paint);
			const double Head = G::EaveColumn * K;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const double L = Along(Side) + Head, E = -Out(Side);
				// The end sides' members a hair inside the fronts' where they cross.
				const double In = (Side % 2) ? 0.5 : 0.0;
				const int32 Mark = Mesh.MaxVertexID();
				double Z = Eave;
				auto Member = [&](double H, double Wd)
				{
					AppendBox(Mesh, FVector3d(-L, E - 0.5 * Wd + In, Z - H + In), FVector3d(L, E + 0.5 * Wd - In, Z - In));
					Z -= H;
				};
				Member(G::BigLintelH * K, G::BigLintelW * K);
				if (Side % 2 == 0)
				{
					Member(G::LintelBoardH * K, G::LintelBoardW * K);
					Member(G::SmallLintelH * K, G::SmallLintelW * K);
				}
				AppendBox(Mesh, FVector3d(-L, E - 0.5 * G::PlateW * K + In, Eave + In), FVector3d(L, E + 0.5 * G::PlateW * K - In, PlateTop - In));
				PlaceSide(Side, Mark);
			}
		}

		// 5) 斗栱 round all four sides: 柱頭科 on every column, 平身科 between, the corner's two crossing with
		// a 角昂 on the diagonal; the 正心枋 and 墊栱板 behind them, 挑檐枋 and 挑檐桁 out on the second 踩, the
		// 正心桁 on the column line.
		{
			FSlotScope PaintTag(Mesh, MatSlot_Paint);
			const double BoardTop = PurlinZ - 0.5 * G::Purlin * K;
			const double OuterY = -6.0 * Dk;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const double L = Along(Side), E = -Out(Side);
				const int32 Mark = Mesh.MaxVertexID();
				{
					// 墊栱板 and 正心枋 behind the sets: red, as the boards between 斗栱 are painted.
					FSlotScope BoardTag(Mesh, MatSlot_Wood);
					AppendBox(Mesh, FVector3d(-L, E - 0.3 * Dk, PlateTop), FVector3d(L, E + 0.3 * Dk, BoardTop + 1.0));
				}
				if (P.bHasDougong)
				{
					const TArray<double> Cols = SideColumns(Side);
					for (int32 c = 0; c < Cols.Num(); ++c)
					{
						const int32 V0 = Mesh.MaxVertexID();
						AppendBracketSet(Mesh, Dk, PlateTop, true);
						TransformVerticesFrom(Mesh, V0, FTransform(FVector(Cols[c], E, 0.0)));
						if (c + 1 == Cols.Num()) break;
						const int32 Sets = (Side % 2 == 0) ? G::BracketSets[Side == 0 ? c : Cols.Num() - 2 - c] : SetsBetween(Cols[c + 1] - Cols[c], Dk);
						for (int32 k = 1; k <= Sets; ++k)
						{
							const int32 V1 = Mesh.MaxVertexID();
							AppendBracketSet(Mesh, Dk, PlateTop, false);
							TransformVerticesFrom(Mesh, V1, FTransform(FVector(FMath::Lerp(Cols[c], Cols[c + 1], double(k) / (Sets + 1)), E, 0.0)));
						}
					}
					// 角昂 out along the diagonal at this side's +X end.
					const int32 V2 = Mesh.MaxVertexID();
					HutongMeshUtils::AppendBox(Mesh, FVector3d(-Dk, -9.0 * Dk, PlateTop + 3.2 * Dk), FVector3d(Dk, 0.0, PlateTop + 7.2 * Dk));
					TransformVerticesFrom(Mesh, V2, FTransform(FRotator(0.0, 45.0, 0.0), FVector(L, E, 0.0)));
					AppendBox(Mesh, FVector3d(-L - 6.0 * Dk, E + OuterY - 0.5 * Dk, PlateTop + 5.2 * Dk), FVector3d(L + 6.0 * Dk, E + OuterY + 0.5 * Dk, PlateTop + 7.2 * Dk));
				}
				PlaceSide(Side, Mark);
			}
			PaintTag.Close();

			FSlotScope PurlinTag(Mesh, MatSlot_Wood);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const double L = Along(Side), E = -Out(Side);
				const int32 Mark = Mesh.MaxVertexID();
				auto Purlin = [&](double Yp, double Zc, double R, double Reach)
				{
					const int32 V = Mesh.MaxVertexID();
					AppendCylinder(Mesh, FVector3d::ZeroVector, R, 2.0 * (L + Reach), 16);
					TransformVerticesFrom(Mesh, V, FTransform(FQuat(FVector::YAxisVector, HALF_PI), FVector(-L - Reach, Yp, Zc)));
				};
				Purlin(E, PurlinZ, 0.5 * G::Purlin * K * (Side % 2 ? 0.999 : 1.0), 0.6 * G::Purlin * K);
				if (P.bHasDougong) Purlin(E + OuterY, PlateTop + 7.2 * Dk + 0.5 * G::OuterPurlin * K, 0.5 * G::OuterPurlin * K * (Side % 2 ? 0.999 : 1.0), 6.0 * Dk);
				PlaceSide(Side, Mark);
			}
		}

		// 6) The facades at both 金柱 lines, the front's turned half round for the back: per bay 抱框, 下檻 or a
		// 檻牆 and 風檻, 中檻, 上檻; four 隔扇 in the 明間 and 次間, four 檻窗 in the 梢間; 橫披 over 中檻; a
		// 走馬板 on up to the ceiling.
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const int32 Mark = Mesh.MaxVertexID();
			const double Yf = Y[1] - CY;
			const double Rail = G::Rail * K, Tf = G::Jamb * K;
			const double Mid = Floor + G::MiddleRail * K;
			const double TopRail = Eave - G::TopRail * K;
			const double Bar = FMath::Max(0.12 * K, 1.5);
			const double Leaf = FMath::Max(0.25 * K, 2.0);
			for (int32 b = 0; b < 5; ++b)
			{
				const double X0 = BX[b] - CX + GoldR, X1 = BX[b + 1] - CX - GoldR;
				const bool bDoors = (b >= 1 && b <= 3);
				auto Box = [&](int32 Slot, double Xa, double Ya, double Za, double Xb, double Yb, double Zb)
				{
					FSlotScope Tag(Mesh, Slot);
					AppendBox(Mesh, FVector3d(Xa, Yf + Ya, Za), FVector3d(Xb, Yf + Yb, Zb));
				};
				const double H = 0.5 * Tf;
				const double Low = bDoors ? Floor + Rail : Floor + G::SillWall * K + Rail;
				if (bDoors) Box(MatSlot_Wood, X0, -H, Floor, X1, H, Floor + Rail);
				else
				{
					Box(MatSlot_Body, X0, -1.5 * H, Floor, X1, 1.5 * H, Floor + G::SillWall * K);
					Box(MatSlot_Wood, X0, -H, Floor + G::SillWall * K, X1, H, Low);
				}
				Box(MatSlot_Wood, X0, -H, Floor, X0 + Tf, H, TopRail);
				Box(MatSlot_Wood, X1 - Tf, -H, Floor, X1, H, TopRail);
				Box(MatSlot_Wood, X0, -H, Mid, X1, H, Mid + Rail);
				Box(MatSlot_Wood, X0, -H, TopRail, X1, H, Eave);
				Box(MatSlot_DoorPaint, X0, -0.3 * H, Eave, X1, 0.3 * H, CeilZ + 1.0);

				// A panel of 格心: frame, 窗紙 behind, a grid of bars in front.
				auto Lattice = [&](double Xa, double Za, double Xb, double Zb, int32 Across, int32 Up)
				{
					Box(MatSlot_Paper, Xa, 0.1 * Leaf, Za, Xb, 0.2 * Leaf, Zb);
					for (int32 i = 1; i < Across; ++i) { const double X = FMath::Lerp(Xa, Xb, double(i) / Across); Box(MatSlot_Lattice, X - 0.5 * Bar, -0.5 * Leaf, Za, X + 0.5 * Bar, 0.0, Zb); }
					for (int32 j = 1; j < Up; ++j) { const double Z = FMath::Lerp(Za, Zb, double(j) / Up); Box(MatSlot_Lattice, Xa, -0.5 * Leaf, Z - 0.5 * Bar, Xb, 0.0, Z + 0.5 * Bar); }
				};
				// 橫披
				Lattice(X0 + Tf, Mid + Rail, X1 - Tf, TopRail, 6, 2);
				// Four leaves: 邊梃 and 抹頭 lacquered, 格心 over a 裙板 in a 隔扇, 格心 whole in a 檻窗.
				const double LW = (X1 - X0 - 2.0 * Tf) / 4.0;
				const double Stile = FMath::Max(0.3 * K, 2.0);
				for (int32 l = 0; l < 4; ++l)
				{
					const double La = X0 + Tf + l * LW, Lb = La + LW;
					const double Hgt = Mid - Low;
					Box(MatSlot_DoorPaint, La, -0.5 * Leaf, Low, La + Stile, 0.5 * Leaf, Mid);
					Box(MatSlot_DoorPaint, Lb - Stile, -0.5 * Leaf, Low, Lb, 0.5 * Leaf, Mid);
					Box(MatSlot_DoorPaint, La + Stile, -0.5 * Leaf, Mid - Stile, Lb - Stile, 0.5 * Leaf, Mid);
					Box(MatSlot_DoorPaint, La + Stile, -0.5 * Leaf, Low, Lb - Stile, 0.5 * Leaf, Low + Stile);
					const double GridLow = bDoors ? Low + 0.42 * Hgt : Low + Stile;
					if (bDoors)
					{
						// 裙板 and 絛環板 below the 格心.
						Box(MatSlot_DoorPaint, La + Stile, -0.3 * Leaf, Low + Stile, Lb - Stile, 0.3 * Leaf, GridLow - Stile);
						Box(MatSlot_DoorPaint, La + Stile, -0.5 * Leaf, GridLow - Stile, Lb - Stile, 0.5 * Leaf, GridLow);
					}
					Lattice(La + Stile, GridLow, Lb - Stile, Mid - Stile, 3, bDoors ? 6 : 8);
				}
			}
			if (Face == 1) TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, 180.0, 0.0), FVector(0.0, 0.0, 0.0)));
			TransformVerticesFrom(Mesh, Mark, FTransform(FVector(CX, CY, 0.0)));
		}

		// 7) 歇山 on the figure's section, 收山 to the 山面 金桁; 正吻 on the ridge's ends, 仙人走獸 up the 戧脊.
		const double RoofW = (BX.Last() - BX[0]) + 2.0 * O;
		const double RoofD = (Y[3] - Y[0]) + 2.0 * O;
		const HutongGen::FHutongRoofSection Section = P.GetRoofSection(D);
		const double Rise = P.GetRoofRise(D);
		const FVector3d RoofMin(BX[0] - O, Y[0] - O, RoofZ);
		TArray<FRoofPanel> Panels;
		TArray<UE::Geometry::FIndex2i> Timber;
		FXieshanRoofSpec Xie;
		{
			FSlotScope RoofTag(Mesh, MatSlot_Roof);
			Xie.Width = RoofW;
			Xie.Depth = RoofD;
			Xie.Rise = Rise;
			Xie.Section = Section;
			Xie.TileRowSpacing = G::TileRow * K;
			Xie.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile) && !P.bHasTileRuns;
			Xie.ShouInset = G::ShouInset * K;
			Xie.FlareRise = 4.0 * G::Rafter * K;
			Xie.FlareRun = 3.0 * G::Rafter * K;
			Xie.FlareLength = 0.3 * FMath::Min(RoofW, RoofD);
			Xie.FasciaDrop = 2.0 * G::Rafter * K;
			Xie.EaveOverhang = O;
			Xie.UndersideRise = P.GetUndersideRise();
			Xie.RidgeWidth = G::RidgeWidth * K;
			Xie.RidgeHeight = G::RidgeHeight * K;
			Xie.BargeThickness = G::BargeThickness * K;
			Xie.BargeDepth = G::BargeDepth * K;
			Xie.EaveSegments = P.RoofEaveSegments;
			Xie.SlopeSegments = FMath::Max(P.RoofSlopeSegments, 3);
			if (P.bHasTileRuns) Shell::DressedRoofSegments(Xie.EaveSegments, Xie.SlopeSegments, RoofW,
				FMath::Sqrt(FMath::Square(0.5 * RoofD) + FMath::Square(Rise)), Xie.TileRowSpacing);
			AppendXieshanRoof(Mesh, RoofMin, Xie, &Panels, &Timber);
		}
		for (const UE::Geometry::FIndex2i& R : Timber) SetMaterialIDForTriangleRange(Mesh, R.A, R.B, MatSlot_Wood);

		if (P.bHasRidgeOrnament)
		{
			FSlotScope OrnamentTag(Mesh, MatSlot_Finial);
			// The ridge ends where AppendXieshanRoof puts the 山花: its 收山 as the roof clamps it.
			const double Hy = 0.5 * RoofD;
			const double Shou = FMath::Clamp(FMath::Min(Xie.ShouInset, 0.2 * RoofW) / Hy, 0.05, 0.75) * Hy;
			const double RidgeZ = RoofZ + Rise * Section.CrownFactor();
			const double BeastH = G::RidgeBeast * K;
			const double Thick = 0.55 * G::RidgeWidth * K;
			// 正吻 in profile after 正面立面 (inward from the ridge end, up): the mouth over the ridge, the body
			// rising, the tail curled over at the top, the 劍靶 standing on its outer side.
			const TArray<FVector2d> Beast = {
				{ -0.12, -0.1 }, { 0.55, -0.1 }, { 0.55, 0.12 }, { 0.40, 0.22 }, { 0.42, 0.40 }, { 0.50, 0.55 }, { 0.52, 0.72 },
				{ 0.46, 0.86 }, { 0.34, 0.95 }, { 0.20, 0.97 }, { 0.10, 0.90 }, { 0.12, 0.80 }, { 0.22, 0.80 }, { 0.26, 0.72 },
				{ 0.18, 0.64 }, { 0.06, 0.66 }, { 0.0, 0.76 }, { -0.06, 0.90 }, { -0.08, 1.05 }, { -0.18, 1.05 }, { -0.20, 0.50 },
				{ -0.12, 0.30 } };
			for (const double Dir : { 1.0, -1.0 })
			{
				const double EndX = (Dir > 0.0) ? RoofMin.X + Shou : RoofMin.X + RoofW - Shou;
				TArray<FVector2d> Prof;
				for (const FVector2d& Pt : Beast) Prof.Add(FVector2d(EndX + Dir * Pt.X * BeastH, RidgeZ + Pt.Y * BeastH));
				// Profile (x, z) swept across the ridge.
				auto At = [&](double Yp) { return FTransform(FMatrix(FVector::XAxisVector, FVector::UpVector, FVector(0.0, -1.0, 0.0), FVector(0.0, Yp, 0.0))); };
				AppendSweptProfile(Mesh, Prof, { At(CY + 0.5 * Thick), At(CY - 0.5 * Thick) });
			}
			// 仙人走獸: seven up the lower end of each 戧脊, sat on its courses.
			const double Fig = 0.45 * G::RidgeHeight * K;
			for (const int32 p : { 0, 2 })
			{
				if (!Panels.IsValidIndex(p)) continue;
				const FRoofPanel& Pn = Panels[p];
				for (const double U : { 0.0, 1.0 })
				{
					double V = 0.04;
					for (int32 f = 0; f < G::HipFigures; ++f)
					{
						const FVector3d At = Pn.Sample(U, V) + FVector3d(0.0, 0.0, 0.6 * G::RidgeHeight * K);
						const double Sz = (f == 0 ? 1.3 : 1.0) * Fig;
						AppendBox(Mesh, At + FVector3d(-0.25 * Sz, -0.25 * Sz, -0.2 * Sz), At + FVector3d(0.25 * Sz, 0.25 * Sz, 0.6 * Sz));
						AppendCylinder(Mesh, At + FVector3d(0.0, 0.0, 0.6 * Sz), 0.2 * Sz, 0.4 * Sz, 8);
						// Next figure a figure and a half on up the hip.
						double Next = V;
						for (int32 i = 0; i < 40 && (Pn.Sample(U, Next) - Pn.Sample(U, V)).Length() < 1.2 * Fig; ++i) Next += 0.004;
						V = Next;
					}
				}
			}
		}

		// 壟, 椽頭 and 飛椽 on every panel.
		Shell::FRoofDressing Dress;
		Dress.Tile = P.RoofTile;
		Dress.TileRowSpacing = G::TileRow * K;
		Dress.FasciaDrop = Xie.FasciaDrop;
		Dress.bTileRuns = P.bHasTileRuns;
		Dress.RafterSection = P.RafterEndSection > 0.0 ? G::Rafter * K : 0.0;
		Dress.RafterSpacing = G::RafterSpacing * K;
		Dress.EaveOverhang = O;
		Shell::AppendRoofDressing(Mesh, Panels, Dress);
	}

	void BuildHall(FDynamicMesh3& Mesh, const FHutongHallParams& P)
	{
		if (P.IsGrand())
		{
			BuildGrandHall(Mesh, P);
			return;
		}
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = P.GetEaveHeight();
		// Roof on the frame's line; walls to the ceiling under it; columns and 額枋 at the column top.
		const double RoofZ = Eave + P.GetRoofLift();
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.35);
		const double ColH = Eave - Floor;
		const double ColR = P.GetColumnRadiusFor(W, D);

		const int32 N = (P.BayCountOverride > 0)
			? P.BayCountOverride
			: ComputeBayCount(W, P.MinBayWidth, P.MaxBayWidth);
		const int32 DoorIdx = N / 2;
		auto BayBoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, W, ColR); };

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);
		const double WallBottom = bBaseCourse ? Floor + BaseH : Floor;

		// 1) 臺基, 踏跺 under the 明間.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		const double PlatSide = bBaseCourse ? BaseP : 0.0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide,
			P.StepCount, P.StepTread, BayBoundaryX(DoorIdx), BayBoundaryX(DoorIdx + 1));

		// 2) 下鹼 and the three closed walls.
		{
			FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP);
			BaseTag.Close();
		}

		AppendBox(Mesh, FVector3d(T, D - T, WallBottom), FVector3d(W - T, D, CeilZ));
		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, CeilZ));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, CeilZ));

		// No 墀頭: it heads a gable wall, and this roof has no gables.

		const double ColTopR = Frame::TaperedTopRadius(ColR, ColH, P.ColumnTaperRatio);
		const double ArchSection = FMath::Max(2.0 * ColR * 0.85, 10.0);
		const double ArchBottom = Eave - ArchSection;

		// 3) 檐柱 row on the facade line.
		FSlotScope ColumnTag(Mesh, MatSlot_Wood);
		Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, Floor, Eave, 1.0, 32);
		ColumnTag.Close();

		// 額枋 and 雀替 tagged 彩畫.
		FSlotScope PaintTag(Mesh, MatSlot_Paint);
		Frame::AppendArchitrave(Mesh, BayBoundaryX(0), BayBoundaryX(N), 0.0,
			ArchSection, ArchBottom, Eave);

		// 雀替 in each bay's top corners.
		if (P.bHasBrackets && ArchBottom > Floor + 60.0)
		{
			const double Reach = FMath::Clamp(P.BracketReach, 5.0,
				0.35 * (BayBoundaryX(1) - BayBoundaryX(0)));
			const double Drop = FMath::Min(0.55 * Reach, 0.25 * ColH);
			const double BY = 0.62 * ColR;
			for (int32 i = 0; i < N; ++i)
			{
				const double X0 = BayBoundaryX(i);
				const double X1 = BayBoundaryX(i + 1);
				// Wedge read as two steps.
				for (int32 s = 0; s < 2; ++s)
				{
					const double F = (s + 1) / 2.0;
					const double R = Reach * (1.0 - 0.5 * s);
					AppendBox(Mesh, FVector3d(X0, -BY, ArchBottom - Drop * F),
						FVector3d(X0 + R, BY, ArchBottom - Drop * (F - 0.5)));
					AppendBox(Mesh, FVector3d(X1 - R, -BY, ArchBottom - Drop * F),
						FVector3d(X1, BY, ArchBottom - Drop * (F - 0.5)));
				}
			}
		}

		PaintTag.Close();

		// Bays: 隔扇 in the 明間, 檻牆 with lattice over it elsewhere.
		const double Sill = Floor + FMath::Clamp(P.SillHeightAboveFloor, 20.0, 0.6 * ColH);
		const double DoorHead = FMath::Clamp(
			Floor + FMath::Clamp(P.DoorHeadRatio, 0.2, 0.95) * ColH,
			Floor + 100.0, ArchBottom - 5.0);
		const double BarT = FMath::Clamp(P.LatticeBarThickness, 1.0, 0.5 * T);

		for (int32 i = 0; i < N; ++i)
		{
			const double X0 = BayBoundaryX(i);
			const double X1 = BayBoundaryX(i + 1);

			if (i == DoorIdx)
			{
				FSlotScope DoorTag(Mesh, MatSlot_Wood);
				const double BayW = X1 - X0;
				const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, T);
				const double DoorW = BayW * FMath::Clamp(P.DoorWidthFraction, 0.2, 1.0);
				const double JambL = X0 + 0.5 * (BayW - DoorW);
				const double JambR = JambL + DoorW;

				FHutongDoorAssembly Door;
				Door.OpeningX0 = JambL;
				Door.OpeningX1 = JambR;
				Door.FrontY = 0.0;
				Door.BackY = T;
				Door.BottomZ = Floor;
				Door.LeafTopZ = DoorHead;
				Door.JambTopZ = ArchBottom;
				Door.FrameThickness = FrameT;
				Door.ThresholdHeight = P.ThresholdHeight;
				Door.PegCount = P.DoorPegCount;
				// Both leaves folded flat, as on the 垂花門.
				Door.bUseLeafAngles = true;
				Door.LeftLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
				Door.RightLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
				AppendDoorAssembly(Mesh, Door);

				// 餘塞板 from each jamb to the column, board over the head.
				if (JambL - FrameT > X0)
				{
					AppendBox(Mesh, FVector3d(X0, 0.2 * T, Floor),
						FVector3d(JambL - FrameT, 0.8 * T, DoorHead));
				}
				if (X1 > JambR + FrameT)
				{
					AppendBox(Mesh, FVector3d(JambR + FrameT, 0.2 * T, Floor),
						FVector3d(X1, 0.8 * T, DoorHead));
				}
				if (ArchBottom > DoorHead)
				{
					AppendBox(Mesh, FVector3d(X0, 0.2 * T, DoorHead),
						FVector3d(X1, 0.8 * T, ArchBottom));
				}

				// Whole bay tagged joinery; the leaves lacquer themselves.
				DoorTag.Close();
				continue;
			}

			// 檻牆: one box per run, not per bay.
			if (Sill > Floor)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, Floor), FVector3d(X1, T, Sill));
			}

			// Lattice above.
			const double Top = ArchBottom;
			if (Top - Sill > 3.0 * BarT && X1 - X0 > 3.0 * BarT)
			{
				const double MullY0 = 0.18 * T;
				const double RailY0 = 0.34 * T;

				// 窗紙 first so the bars stand in front.
				if (P.bHasWindowPaper)
				{
					FSlotScope PaperTag(Mesh, MatSlot_Paper);
					const double PaperY = RailY0 + 1.4 * BarT;
					AppendBox(Mesh, FVector3d(X0, PaperY, Sill),
						FVector3d(X1, PaperY + 0.25 * BarT, Top));
					PaperTag.Close();
				}

				FSlotScope LatticeTag(Mesh, MatSlot_Lattice);
				const int32 Mullions = FMath::Clamp(P.LatticeMullions, 0, 24);
				for (int32 j = 1; j <= Mullions; ++j)
				{
					const double A = X0 + (X1 - X0) * j / double(Mullions + 1);
					AppendBox(Mesh, FVector3d(A - 0.5 * BarT, MullY0, Sill),
						FVector3d(A + 0.5 * BarT, MullY0 + BarT, Top));
				}
				const int32 Rails = FMath::Clamp(P.LatticeRails, 0, 16);
				for (int32 j = 1; j <= Rails; ++j)
				{
					const double Z = Sill + (Top - Sill) * j / double(Rails + 1);
					AppendBox(Mesh, FVector3d(X0, RailY0, Z - 0.5 * BarT),
						FVector3d(X1, RailY0 + BarT, Z + 0.5 * BarT));
				}

				LatticeTag.Close();
			}
		}

		// 6) Roof, overhanging all four sides.
		const double O = FMath::Max(P.RoofOverhang, 0.0);
		FSlotScope RoofTag(Mesh, MatSlot_Roof);

		const double RoofW = W + 2.0 * O;
		const double RoofD = D + 2.0 * O;
		// 舉架 over the depth, eave overhang as the outermost run.
		const HutongGen::FHutongRoofSection Section =
			HutongGen::Jiajia::MakeSection(P.Purlins, 0.5 * D, O, P.RoofApexRoll);
		const double Rise = P.GetRoofRise(D);
		const FVector3d RoofMin(-O, -O, RoofZ);
		if (RoofZ > Eave) Frame::AppendEaveStack(Mesh, 0.5 * T, W - 0.5 * T, 0.0, FMath::Max(P.ColumnDiameter, 2.0), Eave);
		TArray<FRoofPanel> Panels;
		TArray<UE::Geometry::FIndex2i> Timber;

		if (P.RoofType == EHutongRoofType::Xieshan)
		{
			FXieshanRoofSpec Xie;
			Xie.Width = RoofW;
			Xie.Depth = RoofD;
			Xie.Rise = Rise;
			Xie.Section = Section;
			Xie.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile) && !P.bHasTileRuns;
			Xie.TileRowSpacing = P.TileRowSpacing;
			Xie.ShouInset = FMath::Max(P.ShouInset, 1.0);
			Xie.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Xie.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			Xie.FlareLength = 0.35 * FMath::Min(RoofW, RoofD);
			Xie.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Xie.EaveOverhang = O;
			Xie.UndersideRise = P.GetUndersideRise();
			if (P.bExposedFrame) Xie.ShellCover = HutongCanon::Frame::RoofCover * FMath::Max(P.ColumnDiameter, 2.0);
			Xie.RidgeWidth = FMath::Max(P.RidgeCourseWidth, 0.0);
			Xie.RidgeHeight = FMath::Max(P.RidgeCourseHeight, 0.0);
			Xie.BargeThickness = FMath::Max(P.BargeBoardThickness, 0.0);
			Xie.BargeDepth = FMath::Max(P.BargeBoardDepth, 0.0);
			Xie.EaveSegments = P.RoofEaveSegments;
			Xie.SlopeSegments = FMath::Max(P.RoofSlopeSegments, 3);
			if (P.bHasTileRuns) Shell::DressedRoofSegments(Xie.EaveSegments, Xie.SlopeSegments, FMath::Max(Xie.Width, Xie.Depth),
				FMath::Sqrt(FMath::Square(0.5 * FMath::Min(Xie.Width, Xie.Depth)) + FMath::Square(Xie.Rise)), P.TileRowSpacing);
			AppendXieshanRoof(Mesh, RoofMin, Xie, &Panels, &Timber);
		}
		else
		{
			FHipRoofSpec Hip;
			Hip.Width = RoofW;
			Hip.Depth = RoofD;
			Hip.RidgeLength = (P.RoofType == EHutongRoofType::Wudian)
				? FMath::Max(RoofW - RoofD, 0.0) : 0.0;
			Hip.Rise = Rise;
			Hip.Section = Section;
			Hip.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile) && !P.bHasTileRuns;
			Hip.TileRowSpacing = P.TileRowSpacing;
			Hip.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Hip.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			Hip.FlareLength = 0.35 * FMath::Min(RoofW, RoofD);
			Hip.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Hip.EaveOverhang = O;
			Hip.UndersideRise = P.GetUndersideRise();
			if (P.bExposedFrame) Hip.ShellCover = HutongCanon::Frame::RoofCover * FMath::Max(P.ColumnDiameter, 2.0);
			Hip.EaveSegments = P.RoofEaveSegments;
			Hip.SlopeSegments = P.RoofSlopeSegments;
			if (P.bHasTileRuns) Shell::DressedRoofSegments(Hip.EaveSegments, Hip.SlopeSegments, FMath::Max(Hip.Width, Hip.Depth),
				FMath::Sqrt(FMath::Square(0.5 * FMath::Min(Hip.Width, Hip.Depth)) + FMath::Square(Hip.Rise)), P.TileRowSpacing);
			AppendHippedRoof(Mesh, RoofMin, Hip, &Panels);
		}

		RoofTag.Close();

		// 徹上明造: cross frames on the interior column lines clear of the hips, 檁 on every line stopping at
		// the hips (and at the 收山 plane over a 歇山), under the shell.
		if (P.bExposedFrame)
		{
			const FrameLayout::FLayout L = FrameLayout::Make(FMath::Max(P.ColumnDiameter, 2.0), Eave, Floor, D, P.Purlins, false, false);
			// The 歇山's 山花 plane, from the column line: its 收山 as the roof function clamps it.
			const double Hy = 0.5 * RoofD;
			const double ShouCol = (P.RoofType == EHutongRoofType::Xieshan)
				? FMath::Clamp(FMath::Min(FMath::Max(P.ShouInset, 1.0), 0.2 * RoofW) / Hy, 0.05, 0.75) * Hy - O
				: BIG_NUMBER;
			auto Reach = [&](int32 i) { return FMath::Max(FMath::Min(FMath::Min(L.Y[i], D - L.Y[i]), ShouCol), 0.0); };
			double Deepest = 0.0;
			for (int32 i = 0; i < L.Num(); ++i) Deepest = FMath::Max(Deepest, Reach(i));
			TArray<double> FrameX;
			for (int32 i = 1; i < N; ++i)
			{
				const double X = BayBoundaryX(i);
				if (X >= Deepest && X <= W - Deepest) FrameX.Add(X);
			}
			FRoofFrameOptions Options;
			Options.bSkipEaveLines = true;
			Options.RearLimit = D - 1.0;
			// The slope over a depth, less the shell's cover: the section, as the roof function samples it.
			const double Cover = HutongCanon::Frame::RoofCover * FMath::Max(P.ColumnDiameter, 2.0);
			Options.Underside = [&](double Y)
			{
				const double V = (O + FMath::Min(Y, D - Y)) / FMath::Max(Hy, 1.0);
				return RoofZ + Rise * Section.HeightFraction(1.0 - V) - Cover;
			};
			Options.LineExtent = [&](int32 i, double& X0, double& X1)
			{
				X0 = FMath::Max(X0, Reach(i));
				X1 = FMath::Min(X1, W - Reach(i));
			};
			AppendRoofFrame(Mesh, L, FrameX, 0.5 * T, W - 0.5 * T, Options);
		}
		for (const UE::Geometry::FIndex2i& R : Timber) SetMaterialIDForTriangleRange(Mesh, R.A, R.B, MatSlot_Wood);

		// The courtyard houses' roof work, on every panel: 壟, 椽頭, and on a 廡殿 its 垂脊 and 正脊.
		Shell::FRoofDressing Dress;
		Dress.Tile = P.RoofTile;
		Dress.TileRowSpacing = P.TileRowSpacing;
		Dress.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
		Dress.bTileRuns = P.bHasTileRuns;
		Dress.RafterSection = P.RafterEndSection;
		Dress.RafterSpacing = P.RafterEndSpacing;
		Dress.EaveOverhang = O;
		if (P.RoofType != EHutongRoofType::Xieshan)
		{
			Dress.RidgeWidth = FMath::Max(P.RidgeCourseWidth, 0.0);
			Dress.RidgeHeight = FMath::Max(P.RidgeCourseHeight, 0.0);
		}
		Shell::AppendRoofDressing(Mesh, Panels, Dress);
	}
}
