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
		FLayout L;
		L.D = House.GetColumnDiameter();
		L.GoldD = L.D + C::GoldColumnExtraCm;
		L.Floor = House.GetFloorHeight();

		const int32 N = FMath::Max(Jiajia::PurlinCount(House.Purlins), 3);
		L.Step = FMath::Max(House.Depth, 1.0) / (N - 1);
		const TArray<double> Ju = Jiajia::DefaultRatios(House.Purlins);

		// 舉架 from the 檐檁 up, the far slope the mirror of the near one.
		L.Y.SetNum(N);
		L.Support.SetNum(N);
		double Z = House.GetEaveHeight();
		for (int32 i = 0; i <= N / 2; ++i)
		{
			if (i > 0) Z += (Ju.IsValidIndex(i - 1) ? Ju[i - 1] : 0.5) * L.Step;
			L.Support[i] = L.Support[N - 1 - i] = Z;
		}
		for (int32 i = 0; i < N; ++i) L.Y[i] = i * L.Step;

		// A 廊 takes one 步架 and leaves the main beams at least a 三架梁 to span.
		L.bFrontVeranda = House.bHasFrontVeranda && N >= 5;
		L.bRearVeranda = L.bFrontVeranda && House.bHasRearVeranda;
		L.Front = L.bFrontVeranda ? 1 : 0;
		// 前廊後無廊 keeps the main beams symmetrical too: their rear end stands on a 瓜柱 on the 插梁.
		L.Rear = N - 1 - L.Front;
		return L;
	}

namespace
{
	// A solid of one (Y, Z) section running from X0 to X1.
	void AppendYZPrism(FDynamicMesh3& Mesh, const TArray<FVector2d>& ProfileYZ, double X0, double X1)
	{
		// Profile X onto world Y, profile Y onto world Z, the sweep onto world X: a rotation, never a mirror.
		const FMatrix Basis(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector);
		FTransform A(Basis), B(Basis);
		A.SetTranslation(FVector(X0, 0, 0));
		B.SetTranslation(FVector(X1, 0, 0));
		AppendSweptProfile(Mesh, ProfileYZ, { A, B });
	}

	// A round member lying along X.
	void AppendLog(FDynamicMesh3& Mesh, double X0, double X1, double Y, double Z, double Radius, int32 Sides)
	{
		const int32 Mark = Mesh.MaxVertexID();
		AppendCylinder(Mesh, FVector3d::ZeroVector, Radius, X1 - X0, Sides);
		// A quarter turn about Y takes +Z onto +X.
		TransformVerticesFrom(Mesh, Mark, FTransform(FQuat(FVector::YAxisVector, HALF_PI), FVector(X0, Y, Z)));
	}

	// A member across the depth, on the frame line at X.
	void AppendBeam(FDynamicMesh3& Mesh, double X, double Y0, double Y1, double Bottom, double Height, double Width)
	{
		AppendBox(Mesh,
			FVector3d(X - 0.5 * Width, FMath::Min(Y0, Y1), Bottom),
			FVector3d(X + 0.5 * Width, FMath::Max(Y0, Y1), Bottom + Height));
	}

	// 瓜柱: square in section, its head bevelled in across the depth to seat the 檁 above it.
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

		// Every column row, front to back: where it stands, its diameter, its top.
		struct FRow { double Y, Diameter, Top; };
		TArray<FRow> Rows = { { L.Y[0], D, Eave } };
		if (L.bFrontVeranda) Rows.Add({ L.Y[L.Front], L.GoldD, L.Support[L.Front] });
		if (L.bRearVeranda)  Rows.Add({ L.Y[L.Rear], L.GoldD, L.Support[L.Rear] });
		Rows.Add({ L.Y[N - 1], D, Eave });

		// 臺明: a brick body under 階條 round its edge and a 方磚 floor inside them, 柱頂石 under
		// every column, and 踏跺 before the door bay between 垂帶 on its column lines.
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

			const int32 BrickTri = Mesh.MaxTriangleID();
			AppendBox(Mesh, FVector3d(X0, Y0, 0.0), FVector3d(X1, Y1, CapZ));
			AppendBox(Mesh, FVector3d(X0 + Edge, Y0 + Edge, CapZ), FVector3d(X1 - Edge, Y1 - Edge, Floor));
			SetMaterialIDForTrianglesFrom(Mesh, BrickTri, MatSlot_Body);

			const int32 StoneTri = Mesh.MaxTriangleID();
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
			SetMaterialIDForTrianglesFrom(Mesh, StoneTri, MatSlot_Stone);
		}

		const int32 WoodTri = Mesh.MaxTriangleID();

		for (const FRow& Row : Rows)
		{
			const double R = 0.5 * Row.Diameter;
			const double TopR = Frame::TaperedTopRadius(R, Row.Top - ColumnFoot, H.ColumnTaperRatio);
			Frame::AppendColumnRow(Mesh, BayX, Bays, Row.Y, R, TopR, ColumnFoot, Row.Top);
		}

		const double BoardH = C::BoardHeight * D;
		// A 瓜柱 carries a purlin, so it stands to that purlin's underside — the 枋 and 墊板 of the
		// line are tenoned into its sides, not stacked on its head.
		auto PurlinFoot = [&](int32 i) { return L.Support[i] + BoardH; };
		// The post's own top: up into the 檁's lower third, where its head is notched to seat it.
		auto PurlinSeat = [&](int32 i)
		{
			return PurlinFoot(i) + C::StrutIntoPurlin * C::PurlinDiameter * D;
		};

		const double Tie = C::TieHeight * D;
		const double TieT = C::TieThickness * D;
		const double Strut = C::StrutSection * D;
		const double HeadH = C::HeadBeamHeight * D;
		const double HeadW = C::HeadBeamWidth * D;
		// 穿插枋 and 隨梁枋 hang a clear gap under the member they tie, off the 枋 along the purlin line.
		const double TieDrop = 1.2 * D + Tie;
		const int32 RidgeIdx = L.Ridge();

		// The cross frame (排架) on every column line, gables included.
		for (int32 j = 0; j <= Bays; ++j)
		{
			const double X = BayX(j);

			// A 廊: 抱頭梁 from the 檐柱, its head carrying the 檐檁, into the 金柱; the 穿插枋 under it through both.
			auto Veranda = [&](double OuterY, double InnerY)
			{
				const double Out = (OuterY < InnerY) ? -1.0 : 1.0;
				AppendBeam(Mesh, X, OuterY + Out * D, InnerY, Eave, HeadH, HeadW);

				// The 穿插枋 runs column axis to column axis; what shows past each of them is its
				// 出頭 — half the height, a little narrower, sitting on the member's own underside.
				const double Bottom = Eave - TieDrop;
				AppendBeam(Mesh, X, OuterY, InnerY, Bottom, Tie, TieT);
				const double TenonH = C::TenonHeight * Tie;
				const double TenonW = C::TenonWidth * TieT;
				const double Reach = C::TenonReach * D;
				AppendBeam(Mesh, X, OuterY + Out * Reach, OuterY, Bottom, TenonH, TenonW);
				AppendBeam(Mesh, X, InnerY, InnerY - Out * Reach, Bottom, TenonH, TenonW);
			};
			if (L.bFrontVeranda) Veranda(L.Y[0], L.Y[L.Front]);
			if (L.bRearVeranda)
			{
				Veranda(L.Y[N - 1], L.Y[L.Rear]);
			}
			else if (L.bFrontVeranda)
			{
				// 圖5-3-2: the 插梁 runs from the 鑽金柱 at the 抱頭梁's height out over the 後檐柱, and a
				// 瓜柱 on it carries the rear end of the main beams.
				Veranda(L.Y[N - 1], L.Y[L.Front]);
				AppendStrut(Mesh, X, L.Y[L.Rear], Eave + HeadH, PurlinSeat(L.Rear), Strut);
			}

			// The main beams, each on the posts the one under it carries, down to the ridge post.
			int32 Lo = L.Front, Hi = L.Rear;
			while (Hi - Lo >= 2)
			{
				const bool bLong = Hi - Lo >= 4;
				const double BH = (bLong ? C::LongBeamHeight : C::ShortBeamHeight) * D;
				const double BW = (bLong ? C::LongBeamWidth : C::ShortBeamWidth) * D;
				const double Bottom = L.Support[Lo];
				const double Top = Bottom + BH;
				// The heads run past the posts under them to carry their purlins.
				AppendBeam(Mesh, X, L.Y[Lo] - 0.6 * D, L.Y[Hi] + 0.6 * D, Bottom, BH, BW);
				// 隨梁枋 under the 五架梁, between the two 金柱.
				if (Lo == L.Front && L.bRearVeranda)
				{
					AppendBeam(Mesh, X, L.Y[Lo], L.Y[Hi], Bottom - TieDrop, Tie, TieT);
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
					// 脊瓜柱, standing to the 脊檁's underside and braced by the 角背 it stands in.
					AppendStrut(Mesh, X, L.Y[RidgeIdx], Top, PurlinSeat(RidgeIdx), Strut);
					if (P.bHasRidgeBraces && PurlinFoot(RidgeIdx) > Top)
					{
						const double Yr = L.Y[RidgeIdx];
						const double Foot = C::BraceReach * L.Step;
						const double Cut = C::BraceCut * L.Step;
						const double Rise = C::BraceHeight * (PurlinFoot(RidgeIdx) - Top);
						const double HalfW = 0.5 * C::BraceWidth * C::ShortBeamWidth * D;
						// Flat on top, each end standing straight off the beam with a short steep cut
						// taking the corner off. Sunk a centimetre into the beam, so its foot is not
						// the post's.
						const double Straight = Top + C::BraceStraight * Rise;
						const double Crown = Top + Rise;
						const TArray<FVector2d> Brace = {
							{ Yr - Foot, Top - 1.0 }, { Yr + Foot, Top - 1.0 },
							{ Yr + Foot, Straight }, { Yr + Foot - Cut, Crown },
							{ Yr - Foot + Cut, Crown }, { Yr - Foot, Straight } };
						AppendYZPrism(Mesh, Brace, X - HalfW, X + HalfW);
					}
				}
				Lo = NextLo;
				Hi = NextHi;
			}
		}

		// Along the frontage: every member passes the end columns by the one reach, so the 檁, 墊板
		// and 枋 of all three lines end on a single plane past the gable frame.
		const double RunOut = C::RunProjection * D;
		const double EndX0 = BayX(0) - RunOut, EndX1 = BayX(Bays) + RunOut;

		// 檁 on every line, gable to gable.
		for (int32 i = 0; i < N; ++i)
		{
			AppendLog(Mesh, EndX0, EndX1, L.Y[i], L.PurlinCentre(i), 0.5 * C::PurlinDiameter * D, 12);
		}
		SetMaterialIDForTrianglesFrom(Mesh, WoodTri, MatSlot_Wood);

		// 枋 and 墊板 under each 檁, the members that carry 彩畫.
		const int32 PaintTri = Mesh.MaxTriangleID();
		const double BoardT = C::BoardThickness * D;
		for (int32 i = 0; i < N; ++i)
		{
			const double Y = L.Y[i], T = L.Support[i];
			AppendBox(Mesh, FVector3d(EndX0, Y - 0.5 * TieT, T - Tie),
				FVector3d(EndX1, Y + 0.5 * TieT, T));
			AppendBox(Mesh, FVector3d(EndX0, Y - 0.5 * BoardT, T),
				FVector3d(EndX1, Y + 0.5 * BoardT, T + BoardH));
		}
		SetMaterialIDForTrianglesFrom(Mesh, PaintTri, MatSlot_Paint);

		if (!P.bHasRafters) return;

		// 椽 one 椽徑 apart: 檐椽 out over each eave, 花架椽 between, 腦椽 to the ridge.
		const int32 RafterTri = Mesh.MaxTriangleID();
		const double Cd = C::RafterDiameter * D;
		const double Pitch = 2.0 * Cd;
		const int32 Count = FMath::Max(1, FMath::FloorToInt32((W - Cd) / Pitch) + 1);
		const double FirstX = 0.5 * (W - (Count - 1) * Pitch);
		const double Board = P.bHasRoofBoards ? FMath::Max(0.2 * Cd, 1.5) : 0.0;

		// 上檐出 splits between the 檐椽 and the 飛椽 over it.
		const double Fly = P.bHasFlyingRafters ? C::FlyingShareOfEave : 0.0;
		const double FrontFly = Fly * Over, RearFly = Fly * RearOver;
		const double FrontOut = Over - FrontFly, RearOut = RearOver - RearFly;

		// One 步架 of slope in its own frame: u along it from A, z up off the line of purlin tops.
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

			// At an eave: 小連檐 across the 檐椽 ends, the 飛椽 on top reaching the rest of the way out, 大連檐 across theirs.
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
		SetMaterialIDForTrianglesFrom(Mesh, RafterTri, MatSlot_Wood);
	}
}
