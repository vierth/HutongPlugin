#include "Generation/FrameGenerator.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace C = HutongCanon::Frame;
	using namespace HutongMeshUtils;

	double FrameLayout::FLayout::PurlinCentre(int32 i) const
	{
		return Support[i] + (C::BoardHeight + 0.5 * C::PurlinDiameter) * D;
	}

	FrameLayout::FLayout FrameLayout::Make(const FHutongSiheyuanParams& House)
	{
		return Make(House.GetColumnDiameter(), House.GetEaveHeight(), House.GetFloorHeight(), House.Depth, House.Purlins,
			House.bHasFrontVeranda, House.bHasRearVeranda);
	}

	FrameLayout::FLayout FrameLayout::Make(double ColumnDiameter, double ColumnTop, double Floor, double Depth,
		EHutongPurlins Purlins, bool bFrontVeranda, bool bRearVeranda)
	{
		FLayout L;
		L.D = ColumnDiameter;
		L.GoldD = L.D + C::GoldColumnExtraCm;
		L.Floor = Floor;

		const int32 N = FMath::Max(Jiajia::PurlinCount(Purlins), 3);
		L.Step = FMath::Max(Depth, 1.0) / (N - 1);
		const TArray<double> Ju = Jiajia::DefaultRatios(Purlins);

		// 舉架 from the 檐檁 up; the far slope mirrors the near.
		L.Y.SetNum(N);
		L.Support.SetNum(N);
		double Z = ColumnTop;
		for (int32 i = 0; i <= N / 2; ++i)
		{
			if (i > 0) Z += (Ju.IsValidIndex(i - 1) ? Ju[i - 1] : 0.5) * L.Step;
			L.Support[i] = L.Support[N - 1 - i] = Z;
		}
		for (int32 i = 0; i < N; ++i) L.Y[i] = i * L.Step;

		// A 廊 takes one 步架 and leaves the main beams at least a 三架梁.
		L.bFrontVeranda = bFrontVeranda && N >= 5;
		L.bRearVeranda = L.bFrontVeranda && bRearVeranda;
		L.Front = L.bFrontVeranda ? 1 : 0;
		// 前廊後無廊 keeps the main beams symmetric: the rear end stands on a 瓜柱 on the 插梁.
		L.Rear = N - 1 - L.Front;
		return L;
	}

namespace
{
	// Round member along X.
	void AppendLog(FDynamicMesh3& Mesh, double X0, double X1, double Y, double Z, double Radius, int32 Sides)
	{
		const int32 Mark = Mesh.MaxVertexID();
		AppendCylinder(Mesh, FVector3d::ZeroVector, Radius, X1 - X0, Sides);
		// Quarter turn about Y takes +Z onto +X.
		TransformVerticesFrom(Mesh, Mark, FTransform(FQuat(FVector::YAxisVector, HALF_PI), FVector(X0, Y, Z)));
	}

	// Member across the depth on the frame line at X.
	void AppendBeam(FDynamicMesh3& Mesh, double X, double Y0, double Y1, double Bottom, double Height, double Width)
	{
		AppendBox(Mesh,
			FVector3d(X - 0.5 * Width, FMath::Min(Y0, Y1), Bottom),
			FVector3d(X + 0.5 * Width, FMath::Max(Y0, Y1), Bottom + Height));
	}

	// 瓜柱: square, head bevelled across the depth to seat the 檁.
	void AppendStrut(FDynamicMesh3& Mesh, double X, double Y, double Bottom, double Top, double Section)
	{
		if (Top <= Bottom) return;
		const double H = 0.5 * Section;
		const double Head = 0.5 * C::StrutHeadWidth * Section;
		const double Bevel = FMath::Min(C::StrutHeadBevel * Section, 0.6 * (Top - Bottom));
		const TArray<FVector2d> Profile = {
			{ Y - H, Bottom }, { Y + H, Bottom },
			{ Y + H, Top - Bevel }, { Y + Head, Top },
			{ Y - Head, Top }, { Y - H, Top - Bevel } };
		AppendYZPrism(Mesh, Profile, X - H, X + H);
	}

	struct FBeamSection { double H = 0.0; double W = 0.0; };

	// 大柁 (and 雙步梁), 表二 算梁通例.
	FBeamSection LowestBeam(double D, double DepthOfWidth = C::MainBeamDepthOfWidth)
	{
		const double W = D + C::MainBeamExtraCm;
		return { DepthOfWidth * W, W };
	}

	FBeamSection BeamAbove(const FBeamSection& Below)
	{
		return { C::UpperBeamDepth * Below.H, C::UpperBeamWidth * Below.W };
	}
}

	void AppendRoofFrame(FDynamicMesh3& Mesh, const FrameLayout::FLayout& InLayout,
		const TArray<double>& FrameX, double X0, double X1, const FRoofFrameOptions& Options)
	{
		FrameLayout::FLayout L = InLayout;
		if (Options.Underside)
		{
			const double OverSupport = (C::BoardHeight + C::PurlinDiameter + C::RafterDiameter) * L.D;
			for (int32 i = 0; i < L.Num(); ++i)
			{
				L.Support[i] = FMath::Min(L.Support[i], Options.Underside(L.Y[i]) - OverSupport);
			}
		}
		const int32 N = L.Num();
		const double D = L.D;
		const double Eave = L.Support[0];
		const int32 Mark = Mesh.MaxVertexID();
		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		// Beams across the depth, held inside the limits.
		auto Beam = [&](double X, double Y0, double Y1, double Bottom, double Height, double Width)
		{
			const double A = FMath::Clamp(FMath::Min(Y0, Y1), Options.FrontLimit, Options.RearLimit);
			const double B = FMath::Clamp(FMath::Max(Y0, Y1), Options.FrontLimit, Options.RearLimit);
			if (B - A > 0.5) AppendBeam(Mesh, X, A, B, Bottom, Height, Width);
		};
		const double BoardH = C::BoardHeight * D;
		// A 瓜柱 stands to its purlin's underside; the line's 枋 and 墊板 tenon into its sides.
		auto PurlinFoot = [&](int32 i) { return L.Support[i] + BoardH; };
		// Post top: into the 檁's lower third, head notched to seat it.
		auto PurlinSeat = [&](int32 i)
		{
			return PurlinFoot(i) + C::StrutIntoPurlin * C::PurlinDiameter * D;
		};

		const double Tie = C::ThroughTieHeight * D;
		const double TieT = C::ThroughTieThickness * D;
		const double FollowT = FMath::Max(D - C::FollowTieLessCm, 0.5 * D);
		const double Strut = C::StrutSection * D;
		const double HeadH = C::HeadBeamHeight * D;
		const double HeadW = C::HeadBeamWidth * D;
		// 穿插枋 and 隨梁枋 hang a clear gap under the member they tie.
		const double TieDrop = 1.2 * D + Tie;
		const int32 RidgeIdx = L.Ridge();

		// Cross frame (排架) on each line asked for.
		for (const double X : FrameX)
		{

			// 廊: 抱頭梁 from 檐柱 (carrying the 檐檁) into the 金柱; 穿插枋 under it through both.
			auto Veranda = [&](double OuterY, double InnerY)
			{
				const double Out = (OuterY < InnerY) ? -1.0 : 1.0;
				Beam(X, OuterY + Out * D, InnerY, Eave, HeadH, HeadW);
				if (!Options.bVerandaTies) return;

				// 穿插枋 runs axis to axis; past each column shows its 出頭: half height, a little narrower,
				// flush with the member's underside.
				const double Bottom = Eave - TieDrop;
				Beam(X, OuterY, InnerY, Bottom, Tie, TieT);
				const double TenonH = C::TenonHeight * Tie;
				const double TenonW = C::TenonWidth * TieT;
				const double Reach = C::TenonReach * D;
				Beam(X, OuterY + Out * Reach, OuterY, Bottom, TenonH, TenonW);
				Beam(X, InnerY, InnerY - Out * Reach, Bottom, TenonH, TenonW);
			};
			if (Options.bGoldColumns)
			{
				const double GoldR = 0.5 * L.GoldD;
				if (L.bFrontVeranda) Frame::AppendColumn(Mesh, X, L.Y[L.Front], GoldR, GoldR, L.Floor, L.Support[L.Front]);
				if (L.bRearVeranda) Frame::AppendColumn(Mesh, X, L.Y[L.Rear], GoldR, GoldR, L.Floor, L.Support[L.Rear]);
			}
			if (L.bFrontVeranda) Veranda(L.Y[0], L.Y[L.Front]);
			if (L.bRearVeranda)
			{
				Veranda(L.Y[N - 1], L.Y[L.Rear]);
			}
			else if (L.bFrontVeranda)
			{
				// 圖5-3-2: the 插梁 runs from the 鑽金柱 at 抱頭梁 height over the 後檐柱; a 瓜柱 on it carries
				// the main beams' rear end.
				Veranda(L.Y[N - 1], L.Y[L.Front]);
				AppendStrut(Mesh, X, L.Y[L.Rear], Eave + HeadH, PurlinSeat(L.Rear), Strut);
			}

			// 角背 at a post's foot: flat top, ends straight up off the beam with a short steep corner cut, sunk 1 cm
			// into the beam so its foot is not the post's.
			auto Brace = [&](double Yr, double Top, double PostTop, double Foot)
			{
				if (!Options.bRidgeBraces || PostTop <= Top) return;
				const double Cut = FMath::Min(C::BraceCut * L.Step, 0.4 * Foot);
				const double Rise = C::BraceHeight * (PostTop - Top);
				// Thinner than the post, so no side is its.
				const double HalfW = FMath::Min(0.5 * C::BraceThickness * Rise, 0.5 * Strut - 1.0);
				const double Straight = Top + C::BraceStraight * Rise;
				const double Crown = Top + Rise;
				const TArray<FVector2d> Shape = {
					{ Yr - Foot, Top - 1.0 }, { Yr + Foot, Top - 1.0 },
					{ Yr + Foot, Straight }, { Yr + Foot - Cut, Crown },
					{ Yr - Foot + Cut, Crown }, { Yr - Foot, Straight } };
				AppendYZPrism(Mesh, Shape, X - HalfW, X + HalfW);
			};

			if (Options.CentreColumnDiameter > 0.0)
			{
				const double R = 0.5 * Options.CentreColumnDiameter;
				Frame::AppendColumn(Mesh, X, L.Y[RidgeIdx], R, R, L.Floor, PurlinSeat(RidgeIdx));
				for (const int32 Dir : { 1, -1 })
				{
					// 雙步梁 below, as the 大柁; each 步梁 above it a layer smaller.
					FBeamSection Section = LowestBeam(D);
					for (int32 i = (Dir > 0) ? L.Front : L.Rear; i != RidgeIdx; i += Dir)
					{
						if (i != ((Dir > 0) ? L.Front : L.Rear)) Section = BeamAbove(Section);
						const double BH = Section.H, BW = Section.W;
						Beam(X, L.Y[i] - Dir * 0.6 * D, L.Y[RidgeIdx], L.Support[i], BH, BW);
						if (i + Dir != RidgeIdx)
						{
							AppendStrut(Mesh, X, L.Y[i + Dir], L.Support[i] + BH, PurlinSeat(i + Dir), Strut);
						}
					}
				}
				continue;
			}

			// Main beams, each on the posts of the one below, down to the ridge post.
			int32 Lo = L.Front, Hi = L.Rear;
			// The lowest is the 大柁 whatever its span (a 卷棚's 四架梁 at the figure's 1.3).
			FBeamSection Section = LowestBeam(D, (Hi - Lo == 3) ? C::RollBeamDepthOfWidth : C::MainBeamDepthOfWidth);
			while (Hi - Lo >= 2)
			{
				if (Lo != L.Front) Section = BeamAbove(Section);
				const double BH = Section.H, BW = Section.W;
				const double Bottom = L.Support[Lo];
				const double Top = Bottom + BH;
				// Heads run past their posts to carry the purlins.
				Beam(X, L.Y[Lo] - 0.6 * D, L.Y[Hi] + 0.6 * D, Bottom, BH, BW);
				// 隨梁枋 under the 五架梁 between the 金柱, or a 卷棚's 四架梁 between the columns.
				if (Lo == L.Front && (L.bRearVeranda || Options.bTieUnderMainBeam))
				{
					Beam(X, L.Y[Lo], L.Y[Hi], Bottom - TieDrop, Tie, FollowT);
				}

				const int32 NextLo = Lo + 1, NextHi = Hi - 1;
				if (NextHi - NextLo >= 2)
				{
					// 金瓜柱.
					AppendStrut(Mesh, X, L.Y[NextLo], Top, PurlinSeat(NextLo), Strut);
					AppendStrut(Mesh, X, L.Y[NextHi], Top, PurlinSeat(NextHi), Strut);
				}
				else if (NextHi == NextLo)
				{
					// 脊瓜柱 to the 脊檁 underside, braced by its 角背.
					AppendStrut(Mesh, X, L.Y[RidgeIdx], Top, PurlinSeat(RidgeIdx), Strut);
					Brace(L.Y[RidgeIdx], Top, PurlinFoot(RidgeIdx), C::BraceReach * L.Step);
				}
				else if (NextHi - NextLo == 1)
				{
					// 卷棚 (圖18): a 瓜柱 with its 角背 under each 頂檁, the 月梁 across their heads carrying the two.
					const double MoonH = FMath::Max(BH - C::TopBeamLessCm, 0.5 * BH);
					const double MoonW = FMath::Max(BW - C::TopBeamLessCm, 0.5 * BW);
					const double MoonBottom = FMath::Min(L.Support[NextLo], L.Support[NextHi]);
					const double Gap = L.Y[NextHi] - L.Y[NextLo];
					if (MoonBottom - Top >= C::TuodunBelow * D)
					{
						for (const int32 i : { NextLo, NextHi })
						{
							AppendStrut(Mesh, X, L.Y[i], Top, MoonBottom + 1.0, Strut);
							Brace(L.Y[i], Top, MoonBottom, FMath::Min(C::BraceReach * L.Step, 0.45 * Gap));
						}
					}
					else if (MoonBottom > Top)
					{
						// Too low for 瓜柱 (a narrow 抄手遊廊): one 柁墩 under both 頂檁, a little short of the 月梁's ends.
						const double HalfT = 0.5 * Strut;
						AppendBox(Mesh, FVector3d(X - HalfT, L.Y[NextLo] - 0.45 * D, Top - 1.0),
							FVector3d(X + HalfT, L.Y[NextHi] + 0.45 * D, MoonBottom + 1.0));
					}
					Beam(X, L.Y[NextLo] - 0.6 * D, L.Y[NextHi] + 0.6 * D, MoonBottom, MoonH, MoonW);
				}
				Lo = NextLo;
				Hi = NextHi;
			}
		}

		const double DefX0 = X0, DefX1 = X1;
		auto Skip = [&](int32 i) { return Options.bSkipEaveLines && (i == 0 || i == N - 1); };

		auto Extent = [&](int32 i, double& A, double& B)
		{
			A = DefX0; B = DefX1;
			if (Options.LineExtent) Options.LineExtent(i, A, B);
		};

		// 檁 per line, end to end.
		for (int32 i = 0; i < N; ++i)
		{
			if (Skip(i)) continue;
			double EndX0, EndX1;
			Extent(i, EndX0, EndX1);
			if (EndX1 - EndX0 < 1.0) continue;
			const double Reach = FMath::Max(Options.GableReach, 0.0);
			AppendLog(Mesh, EndX0 - Reach, EndX1 + Reach, L.Y[i], L.PurlinCentre(i), 0.5 * C::PurlinDiameter * D, 12);
		}
		WoodTag.Close();

		// 枋 and 墊板 under each 檁 (carry 彩畫).
		FSlotScope PaintTag(Mesh, MatSlot_Paint);
		const double BoardT = C::BoardThickness * D;
		for (int32 i = 0; i < N; ++i)
		{
			if (Skip(i)) continue;
			double EndX0, EndX1;
			Extent(i, EndX0, EndX1);
			if (EndX1 - EndX0 < 1.0) continue;
			const double Y = L.Y[i], T = L.Support[i];
			// 檐枋 on an eave line, 金枋 and 脊枋 2 寸 smaller within.
			const bool bEaveLine = i == 0 || i == N - 1;
			const double LineTie = bEaveLine ? C::EaveTieHeight * D : FMath::Max(C::EaveTieHeight * D - C::InnerTieLessCm, 0.5 * D);
			const double LineTieT = bEaveLine ? C::EaveTieThickness * D
				: FMath::Max(C::EaveTieThickness * D - C::InnerTieLessCm, BoardT + 2.0);
			AppendBox(Mesh, FVector3d(EndX0, Y - 0.5 * LineTieT, T - LineTie),
				FVector3d(EndX1, Y + 0.5 * LineTieT, T));
			AppendBox(Mesh, FVector3d(EndX0, Y - 0.5 * BoardT, T),
				FVector3d(EndX1, Y + 0.5 * BoardT, T + BoardH));
			if (Options.GableReach > 0.0)
			{
				Frame::AppendSwallowTail(Mesh, EndX0, EndX0 - Options.GableReach, Y, D, T + BoardH);
				Frame::AppendSwallowTail(Mesh, EndX1, EndX1 + Options.GableReach, Y, D, T + BoardH);
			}
		}
		PaintTag.Close();
		if (Options.YOffset != 0.0) TransformVerticesFrom(Mesh, Mark, FTransform(FVector(0.0, Options.YOffset, 0.0)));
	}

	void BuildFrame(FDynamicMesh3& Mesh, const FHutongFrameParams& P)
	{
		const FHutongSiheyuanParams& H = P.House;
		const FrameLayout::FLayout L = FrameLayout::Make(H);
		const int32 N = L.Num();
		const double D = L.D;
		const double W = FMath::Max(H.Width, 1.0);
		const double Depth = FMath::Max(H.Depth, 1.0);
		const double Eave = L.Support[0];
		const double Floor = L.Floor;
		const double Over = H.GetRoofOverhang();
		const double RearOver = H.GetRearRoofOverhang();

		const int32 Bays = H.GetBayCount();
		const double ColR = H.GetColumnRadius();
		auto BayX = [&](int32 i) { return H.GetBayBoundary(i, Bays, W, ColR); };

		// Column rows front to back: position, diameter, top.
		struct FRow { double Y, Diameter, Top; };
		TArray<FRow> Rows = { { L.Y[0], D, Eave } };
		if (L.bFrontVeranda) Rows.Add({ L.Y[L.Front], L.GoldD, L.Support[L.Front] });
		if (L.bRearVeranda)  Rows.Add({ L.Y[L.Rear], L.GoldD, L.Support[L.Rear] });
		Rows.Add({ L.Y[N - 1], D, Eave });

		// 臺明: brick body, 階條 round the edge, 方磚 floor, 柱頂石 under each column, 踏跺 before the
		// door bay between 垂帶 on its column lines.
		const bool bPlatform = P.bHasPlatform && Floor > 0.0;
		const double ColumnFoot = bPlatform ? Floor + 0.12 * D : Floor;
		if (bPlatform)
		{
			const double Side = C::PlatformSide * D;
			const double Front = FMath::Max(C::PlatformReachOfEave * Over, Side);
			const double Back = (RearOver > 0.0) ? FMath::Max(C::PlatformReachOfEave * RearOver, Side) : Side;
			const double X0 = -Side, X1 = W + Side, Y0 = -Front, Y1 = Depth + Back;
			const double Edge = FMath::Min(C::EdgeStoneWidth * D, 0.25 * FMath::Min(X1 - X0, Y1 - Y0));
			const double CapZ = Floor - FMath::Min(0.4 * D, 0.3 * Floor);

			FSlotScope BrickTag(Mesh, MatSlot_Body);
			AppendBox(Mesh, FVector3d(X0, Y0, 0.0), FVector3d(X1, Y1, CapZ));
			BrickTag.Close();
			FSlotScope FloorTag(Mesh, MatSlot_Floor);
			AppendBox(Mesh, FVector3d(X0 + Edge, Y0 + Edge, CapZ), FVector3d(X1 - Edge, Y1 - Edge, Floor));
			FloorTag.Close();

			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			AppendBox(Mesh, FVector3d(X0, Y0, CapZ), FVector3d(X1, Y0 + Edge, Floor));
			AppendBox(Mesh, FVector3d(X0, Y1 - Edge, CapZ), FVector3d(X1, Y1, Floor));
			AppendBox(Mesh, FVector3d(X0, Y0 + Edge, CapZ), FVector3d(X0 + Edge, Y1 - Edge, Floor));
			AppendBox(Mesh, FVector3d(X1 - Edge, Y0 + Edge, CapZ), FVector3d(X1, Y1 - Edge, Floor));

			const double Half = 0.5 * C::BaseStoneSide * D;
			for (const FRow& Row : Rows)
			{
				for (int32 i = 0; i <= Bays; ++i)
				{
					const double X = BayX(i);
					AppendBox(Mesh, FVector3d(X - Half, Row.Y - Half, CapZ), FVector3d(X + Half, Row.Y + Half, ColumnFoot));
				}
			}

			const int32 Door = H.GetDoorBayIndex(Bays);
			const double SX0 = BayX(Door), SX1 = BayX(Door + 1);
			const double Band = C::StringerWidth * D;
			const int32 Steps = FMath::Clamp(H.StepCount, 0, 8);
			if (Steps > 0 && SX1 - SX0 > 2.0 * Band)
			{
				const double Run = Steps * FMath::Max(H.StepTread, 5.0);
				Shell::AppendSteps(Mesh, SX0 + 0.5 * Band, SX1 - 0.5 * Band, Y0, -1.0, Steps, H.StepTread, Floor);
				const double Toe = FMath::Min(Floor / (Steps + 1), 0.5 * D);
				const TArray<FVector2d> Stringer = { { Y0, 0.0 }, { Y0, Floor }, { Y0 - Run, Toe }, { Y0 - Run, 0.0 } };
				AppendYZPrism(Mesh, Stringer, SX0 - 0.5 * Band, SX0 + 0.5 * Band);
				AppendYZPrism(Mesh, Stringer, SX1 - 0.5 * Band, SX1 + 0.5 * Band);
			}
			StoneTag.Close();
		}

		FSlotScope WoodTag(Mesh, MatSlot_Wood);

		for (const FRow& Row : Rows)
		{
			const double R = 0.5 * Row.Diameter;
			const double TopR = Frame::TaperedTopRadius(R, Row.Top - ColumnFoot, H.ColumnTaperRatio);
			Frame::AppendColumnRow(Mesh, BayX, Bays, Row.Y, R, TopR, ColumnFoot, Row.Top);
		}

		WoodTag.Close();

		// Every member passes the end columns by one reach, so 檁, 墊板 and 枋 end on one plane past the
		// gable frame.
		const double RunOut = C::RunProjection * D;
		TArray<double> FrameX;
		for (int32 j = 0; j <= Bays; ++j) FrameX.Add(BayX(j));
		FRoofFrameOptions Options;
		Options.bRidgeBraces = P.bHasRidgeBraces;
		AppendRoofFrame(Mesh, L, FrameX, BayX(0) - RunOut, BayX(Bays) + RunOut, Options);

		if (!P.bHasRafters) return;

		// 椽 one 椽徑 apart: 檐椽 over each eave, 花架椽 between, 腦椽 to the ridge.
		FSlotScope RafterTag(Mesh, MatSlot_Wood);
		const double Cd = C::RafterDiameter * D;
		const double Pitch = 2.0 * Cd;
		const int32 Count = FMath::Max(1, FMath::FloorToInt32((W - Cd) / Pitch) + 1);
		const double FirstX = 0.5 * (W - (Count - 1) * Pitch);
		const double Board = P.bHasRoofBoards ? FMath::Max(0.2 * Cd, 1.5) : 0.0;

		// 上檐出 splits between 檐椽 and 飛椽.
		const double Fly = P.bHasFlyingRafters ? C::FlyingShareOfEave : 0.0;
		const double FrontFly = Fly * Over, RearFly = Fly * RearOver;
		const double FrontOut = Over - FrontFly, RearOut = RearOver - RearFly;

		// One 步架 slope in its own frame: u along from A, z up off the purlin-top line.
		auto Slope = [&](int32 i)
		{
			const FVector2d A(L.Y[i], L.PurlinTop(i));
			const FVector2d B(L.Y[i + 1], L.PurlinTop(i + 1));
			const double Len = (B - A).Length();
			const double Cos = (B.X - A.X) / Len;
			const double Theta = FMath::Atan2(B.Y - A.Y, B.X - A.X);
			const bool bFrontEave = (i == 0);
			const bool bRearEave = (i == N - 2);
			const double U0 = bFrontEave ? -FrontOut / Cos : -0.5 * Cd;
			const double U1 = bRearEave ? Len + RearOut / Cos : Len + 0.5 * Cd;

			const int32 Mark = Mesh.MaxVertexID();
			for (int32 k = 0; k < Count; ++k)
			{
				const double X = FirstX + k * Pitch;
				AppendBox(Mesh, FVector3d(X - 0.5 * Cd, U0, 0.0), FVector3d(X + 0.5 * Cd, U1, Cd));
			}
			if (Board > 0.0)
			{
				AppendBox(Mesh, FVector3d(0.0, U0, Cd), FVector3d(W, U1, Cd + Board));
			}

			// Eave: 小連檐 across 檐椽 ends, 飛椽 on top reaching the rest out, 大連檐 across theirs.
			auto EaveEnd = [&](double Tip, double Dir, double FlyOut)
			{
				if (P.bHasEaveBoards)
				{
					AppendBox(Mesh, FVector3d(0.0, FMath::Min(Tip, Tip - Dir * Cd), Cd),
						FVector3d(W, FMath::Max(Tip, Tip - Dir * Cd), Cd + 0.4 * Cd));
				}
				if (FlyOut <= 0.0) return;
				const double Head = Tip + Dir * FlyOut / Cos;
				const double Tail = Head - Dir * (1.0 + C::FlyingTailRatio) * FlyOut / Cos;
				const double Z0 = Cd + Board;
				for (int32 k = 0; k < Count; ++k)
				{
					const double X = FirstX + k * Pitch;
					AppendBox(Mesh, FVector3d(X - 0.5 * Cd, FMath::Min(Head, Tail), Z0),
						FVector3d(X + 0.5 * Cd, FMath::Max(Head, Tail), Z0 + Cd));
				}
				if (P.bHasEaveBoards)
				{
					AppendBox(Mesh, FVector3d(0.0, FMath::Min(Head, Head - Dir * Cd), Z0 + Cd),
						FVector3d(W, FMath::Max(Head, Head - Dir * Cd), Z0 + 1.5 * Cd));
				}
			};
			if (bFrontEave && Over > 0.0) EaveEnd(U0, -1.0, FrontFly);
			if (bRearEave && RearOver > 0.0) EaveEnd(U1, 1.0, RearFly);

			TransformVerticesFrom(Mesh, Mark, FTransform(FQuat(FVector::XAxisVector, Theta), FVector(0.0, A.X, A.Y)));
		};
		for (int32 i = 0; i + 1 < N; ++i) Slope(i);
		RafterTag.Close();
	}
}
