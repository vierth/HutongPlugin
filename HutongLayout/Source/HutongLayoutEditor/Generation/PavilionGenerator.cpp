#include "Generation/PavilionGenerator.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace
	{
		void AppendMemberUnder(FDynamicMesh3& Mesh, const FVector3d& From, const FVector3d& To, double Width, double Height)
		{
			const FVector3d Delta = To - From;
			const double Plan = FVector2d(Delta.X, Delta.Y).Length();
			const double Length = Delta.Length();
			if (Length < 1.0) return;
			const int32 Mark = Mesh.MaxVertexID();
			HutongMeshUtils::AppendBox(Mesh, FVector3d(0.0, -0.5 * Width, -Height), FVector3d(Length, 0.5 * Width, 0.0));
			const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Delta.Z, Plan));
			HutongMeshUtils::TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(Pitch, Yaw, 0.0), FVector(From)));
		}

		void AppendYawedBox(FDynamicMesh3& Mesh, double X, double Y, double Yaw, double Length, double Width, double Z0, double Z1)
		{
			const int32 Mark = Mesh.MaxVertexID();
			HutongMeshUtils::AppendBox(Mesh, FVector3d(-0.5 * Length, -0.5 * Width, Z0), FVector3d(0.5 * Length, 0.5 * Width, Z1));
			HutongMeshUtils::TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, Yaw, 0.0), FVector(X, Y, 0.0)));
		}

		void AppendPurlinX(FDynamicMesh3& Mesh, double X0, double X1, double Y, double Z, double Radius)
		{
			const int32 Mark = Mesh.MaxVertexID();
			HutongMeshUtils::AppendCylinder(Mesh, FVector3d::ZeroVector, Radius, X1 - X0, 12);
			HutongMeshUtils::TransformVerticesFrom(Mesh, Mark, FTransform(FQuat(FVector::YAxisVector, HALF_PI), FVector(X0, Y, Z)));
		}
	}

	// 陸柱圓亭, 則例 卷二十三.
	void BuildRoundPavilion(FDynamicMesh3& Mesh, const FHutongPavilionParams& P)
	{
		using namespace HutongMeshUtils;
		namespace C = HutongCanon::Pavilion;

		const double Side = FMath::Max(FMath::Min(P.Width, P.Depth), 1.0);
		const double CX = 0.5 * FMath::Max(P.Width, 1.0), CY = 0.5 * FMath::Max(P.Depth, 1.0);
		const FVector2d Ctr(CX, CY);
		const double Col = P.GetColumnDiameter();
		const double ColR = P.GetPlanColumnRadius();
		const double Bay = P.GetBay();
		const double R = FMath::Max(0.5 * Side - ColR, 10.0);
		const double Floor = P.GetFloorHeight();
		const double Foot = P.GetColumnFoot();
		const double Eave = P.GetEaveHeight();
		const double RoofZ = P.GetRoofBaseHeight();
		const double Rp = R + ColR + P.GetPlatformOverhang();
		const int32 NCol = C::RoundColumns;
		const int32 Around = 96;
		// Column i at i × 60° from +X; the entry bay (4 to 5) faces -Y.
		auto ColumnAt = [&](int32 i) { const double A = 2.0 * PI * i / NCol; return Ctr + R * FVector2d(FMath::Cos(A), FMath::Sin(A)); };
		const int32 EntryBay = 4;

		if (Floor > 0.0)
		{
			const double Edge = FMath::Min(C::EdgeStoneWidth * Col, 0.4 * Rp);
			const double CapZ = Floor - FMath::Min(C::EdgeStoneThickness * Col, 0.5 * Floor);
			{
				// 尺二方磚車輞砍墁: rings one brick wide, each ring's bricks cut to close it.
				const double Rin = Rp - Edge;
				const double CentreR = FMath::Min(C::RoundCentreStonePerBay * Bay, 0.3 * Rin);
				const int32 Rings = FMath::Max(FMath::RoundToInt32((Rin - CentreR) / (C::RoundPaverPerBay * Bay)), 1);
				const double Course = (Rin - CentreR) / Rings;
				TArray<FVector2d> Chain = { { 0.0, Floor } };
				for (int32 k = 0; k <= Rings; ++k) Chain.Add(FVector2d(CentreR + k * Course, Floor));
				Chain.Append({ { Rin, CapZ }, { Rp, CapZ }, { Rp, 0.0 }, { 0.0, 0.0 } });

				FSlotScope BrickTag(Mesh, MatSlot_Body);
				TArray<int32> Edges;
				AppendRevolvedProfile(Mesh, Ctr, Chain, FMath::Max(Around, 8 * Rings * 6), false, &Edges);
				BrickTag.Close();
				if (Edges.Num() > Rings + 1)
				{
					SetMaterialIDForTriangleRange(Mesh, Edges[0], Edges[1], MatSlot_Stone);
					SetMaterialIDForTriangleRange(Mesh, Edges[1], Edges[Rings + 1], MatSlot_Floor);
					// UVs in metres, scaled per ring so a whole number of bricks closes it.
					EnsureUVLayer(Mesh);
					UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
					const double Brick = HutongGen::FloorPaverCm / 100.0;
					for (int32 k = 0; UV && k < Rings; ++k)
					{
						const double R0 = CentreR + k * Course;
						const int32 Bricks = FMath::Max(FMath::RoundToInt32(2.0 * PI * (R0 + 0.5 * Course) / Course), 6);
						for (int32 tid = Edges[k + 1]; tid < Edges[k + 2]; ++tid)
						{
							if (!Mesh.IsTriangle(tid)) continue;
							const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
							const int32 Vid[3] = { T.A, T.B, T.C };
							double Th[3], Rad[3];
							for (int32 c = 0; c < 3; ++c)
							{
								const FVector3d P3 = Mesh.GetVertex(Vid[c]);
								Rad[c] = FVector2d(P3.X - CX, P3.Y - CY).Length();
								Th[c] = FMath::Atan2(P3.Y - CY, P3.X - CX);
								if (Th[c] < 0.0) Th[c] += 2.0 * PI;
							}
							if (FMath::Max3(Th[0], Th[1], Th[2]) - FMath::Min3(Th[0], Th[1], Th[2]) > PI)
							{
								for (double& A : Th) if (A < PI) A += 2.0 * PI;
							}
							int32 E[3];
							for (int32 c = 0; c < 3; ++c)
							{
								// Alternate rings half a brick round, so no joint runs through them all.
								E[c] = UV->AppendElement(FVector2f(float((Th[c] / (2.0 * PI) * Bricks + 0.5 * (k % 2)) * Brick), float((k + (Rad[c] - R0) / Course) * Brick)));
							}
							UV->SetTriangle(tid, UE::Geometry::FIndex3i(E[0], E[1], E[2]));
						}
					}
				}
			}
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			AppendRevolvedProfile(Mesh, Ctr, { { Rp - Edge, CapZ }, { Rp, CapZ }, { Rp, Floor }, { Rp - Edge, Floor } }, Around, true);

			for (int32 i = 0; i < NCol; ++i)
			{
				const FVector2d At = ColumnAt(i);
				AppendYawedBox(Mesh, At.X, At.Y, 360.0 * i / NCol, C::BaseStoneSide * Col, C::BaseStoneSide * Col, CapZ, Floor + 0.5);
				if (Foot > Floor + 0.5) AppendCylinder(Mesh, FVector3d(At.X, At.Y, Floor), C::MirrorRadius * Col, Foot - Floor, 16);
			}

			if (P.bHasSteps)
			{
				const double Half = 0.5 * C::RoundStepWidthPerBay * Bay;
				const double Str = C::StringerWidth * Col;
				const double Tread = C::RoundStepTreadPerBay * Bay;
				const double Y0 = CY - Rp;
				const double Back = CY - FMath::Sqrt(FMath::Max(FMath::Square(Rp - Edge) - FMath::Square(Half + Str), 0.0));
				AppendBox(Mesh, FVector3d(CX - Half, Y0 - Tread, 0.0), FVector3d(CX + Half, Back, 2.0 * Floor / 3.0));
				AppendBox(Mesh, FVector3d(CX - Half, Y0 - 2.0 * Tread, 0.0), FVector3d(CX + Half, Back, Floor / 3.0));
				AppendBox(Mesh, FVector3d(CX - Half - Str, Y0 - 3.0 * Tread, 0.0), FVector3d(CX + Half + Str, Y0 - 2.0 * Tread + 1.0, 2.0));
				// 垂帶: profile (z, out from the edge) swept along +X; up × -Y = +X, a rotation.
				const double Toe = Floor / 3.0 + 2.0;
				const TArray<FVector2d> Stringer = { { 0.0, Y0 - Back }, { Floor, Y0 - Back }, { Floor, 0.0 }, { Toe, 2.0 * Tread }, { 0.0, 2.0 * Tread } };
				for (const double X : { CX - Half - Str, CX + Half })
				{
					auto At = [&](double AtX) { return FTransform(FMatrix(FVector::UpVector, -FVector::YAxisVector, FVector::XAxisVector, FVector(AtX, Y0, 0.0))); };
					AppendSweptProfile(Mesh, Stringer, { At(X), At(X + Str) });
				}
			}
			StoneTag.Close();
		}

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);
		const double PostHalf = P.bSquarePosts ? Frame::SquareHalfWidth(ColR) : ColR;

		for (int32 i = 0; i < NCol; ++i)
		{
			const FVector2d At = ColumnAt(i);
			if (P.bSquarePosts) Frame::AppendSquareColumn(Mesh, At.X, At.Y, PostHalf, Frame::SquareHalfWidth(ColTopR), Foot, Eave);
			else Frame::AppendColumn(Mesh, At.X, At.Y, ColR, ColTopR, Foot, Eave, 1.0, 24);
		}

		auto Rect = [](double Z0, double Z1, double Y0, double Y1) { return TArray<FVector2d>{ { Z0, Y0 }, { Z0, Y1 }, { Z1, Y1 }, { Z1, Y0 } }; };
		const double LintelT = C::LintelThickness * Col;
		const double LintelBottom = Eave - C::LintelHeight * Col;
		AppendArcSweep(Mesh, Ctr, R, LintelBottom, 0.0, 360.0, Rect(0.0, C::LintelHeight * Col, -0.5 * LintelT, 0.5 * LintelT), Around);
		for (int32 i = 0; i < NCol; ++i)
		{
			const FVector2d At = ColumnAt(i);
			AppendYawedBox(Mesh, At.X, At.Y, 360.0 * i / NCol, C::BeamHeadLength * Col * 0.7, C::BeamHeadSection * Col, Eave - 0.5, Eave + C::BeamHeadSection * Col);
		}
		const double BoardH = C::BoardHeight * Col;
		const double PurlinR = 0.5 * C::PurlinDiameter * Col;
		if (RoofZ > Eave)
		{
			{
				FSlotScope BoardTag(Mesh, MatSlot_Paint);
				const double BT = 0.5 * HutongCanon::Frame::BoardThickness * Col;
				AppendArcSweep(Mesh, Ctr, R, Eave, 0.0, 360.0, Rect(0.0, BoardH, -BT, BT), Around);
			}
			AppendArcSweep(Mesh, Ctr, R, Eave + BoardH + PurlinR, 0.0, 360.0, MakeCircleProfile(PurlinR, 12), Around);
		}

		const double BarT = FMath::Max(P.FriezeBarSection, 1.0);
		const double Drop = FMath::Min(P.bDeriveProportions ? C::RoundFriezeDropPerBay * Bay : P.FriezeDrop, LintelBottom - Floor - 30.0);
		for (int32 i = 0; i < NCol; ++i)
		{
			const FVector2d A = ColumnAt(i), B = ColumnAt((i + 1) % NCol);
			const double L = 0.5 * (B - A).Length();
			const int32 Mark = Mesh.MaxVertexID();
			if (P.bHasFrieze && P.FriezeBars > 0 && Drop > BarT)
			{
				Frame::AppendHangingFrieze(Mesh, -L, L, 0.0, LintelBottom, Drop, BarT, P.FriezeBars);
				const double Reach = C::RoundBracketReachPerBay * Bay, BDrop = C::RoundBracketDropPerBay * Bay;
				const double Z = LintelBottom - Drop;
				for (const double Dir : { -1.0, 1.0 })
				{
					const double X = Dir * (L - PostHalf + 0.5);
					const TArray<FVector2d> Tri = { { X, Z + 0.5 }, { X - Dir * Reach, Z + 0.5 }, { X, Z - BDrop } };
					const FMatrix M0(FVector::XAxisVector, FVector::UpVector, FVector(0.0, -1.0, 0.0), FVector(0.0, 0.5 * BarT, 0.0));
					const FMatrix M1(FVector::XAxisVector, FVector::UpVector, FVector(0.0, -1.0, 0.0), FVector(0.0, -0.5 * BarT, 0.0));
					AppendSweptProfile(Mesh, Tri, { FTransform(M0), FTransform(M1) });
				}
			}
			const FVector2d Mid = 0.5 * (A + B);
			const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
			TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, Yaw, 0.0), FVector(Mid.X, Mid.Y, 0.0)));
		}

		if (P.bHasBench)
		{
			const double SeatTop = Floor + P.GetBenchHeight();
			const double SeatD = P.GetBenchDepth();
			const double Seat = FMath::Max(0.22 * SeatD, 4.0);
			constexpr double Bar = 3.0;
			const double Z0 = Floor + 4.0, Z1 = SeatTop - Seat, ZM = 0.5 * (Z0 + Z1);
			const double PostDeg = FMath::RadiansToDegrees(PostHalf / R);
			const double BayDeg = 360.0 / NCol;
			auto Bench = [&](double B0, double B1, double SeatFrom, double SeatTo)
			{
				const int32 Seg = FMath::Max(FMath::CeilToInt32((B1 - B0) / 3.0), 2);
				AppendArcSweep(Mesh, Ctr, R, 0.0, SeatFrom, SeatTo, Rect(SeatTop - Seat, SeatTop, -0.5 * SeatD, 0.5 * SeatD), Seg);
				if (Z1 - Z0 <= 4.0 * Bar) return;
				AppendArcSweep(Mesh, Ctr, R, 0.0, B0, B1, Rect(Z0, Z0 + Bar, -0.5 * Bar, 0.5 * Bar), Seg);
				AppendArcSweep(Mesh, Ctr, R, 0.0, B0, B1, Rect(ZM - 0.5 * Bar, ZM + 0.5 * Bar, -0.5 * Bar, 0.5 * Bar), Seg);
				const double ArcLen = FMath::DegreesToRadians(B1 - B0) * R;
				const int32 N = FMath::Max(FMath::RoundToInt32(ArcLen / 16.0), 2);
				for (int32 k = 1; k < N; ++k)
				{
					const double A = FMath::DegreesToRadians(FMath::Lerp(B0, B1, double(k) / N));
					AppendYawedBox(Mesh, CX + R * FMath::Cos(A), CY + R * FMath::Sin(A), FMath::RadiansToDegrees(A) + 90.0, Bar, Bar, Z0, Z1);
				}
			};
			for (int32 i = 0; i < NCol; ++i)
			{
				const double A0 = BayDeg * i, A1 = BayDeg * (i + 1);
				if (i != EntryBay)
				{
					Bench(A0, A1, A0 + PostDeg, A1 - PostDeg);
					continue;
				}
				const double Run = C::RoundEntryBenchShare * BayDeg;
				for (const double Dir : { 1.0, -1.0 })
				{
					const double Col0 = Dir > 0.0 ? A0 : A1;
					const double Free = Col0 + Dir * Run;
					Bench(FMath::Min(Col0, Free), FMath::Max(Col0, Free), FMath::Min(Col0 + Dir * PostDeg, Free), FMath::Max(Col0 + Dir * PostDeg, Free));
					const double A = FMath::DegreesToRadians(Free - Dir * 0.5 * PostDeg);
					AppendYawedBox(Mesh, CX + R * FMath::Cos(A), CY + R * FMath::Sin(A), FMath::RadiansToDegrees(A) + 90.0, PostHalf, 0.5 * SeatD, Floor, SeatTop);
				}
			}
		}
		WoodTag.Close();

		const double O = P.GetRoofOverhang();
		const double Cover = HutongCanon::Frame::RoofCover * Col;
		const bool bFrame = P.bExposedFrame;
		FRoundRoofSpec Spec;
		Spec.Radius = R + O;
		Spec.Rise = P.GetRoofRise();
		Spec.Section = P.GetRoofSection();
		Spec.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
		Spec.EaveOverhang = O;
		Spec.UndersideRise = P.GetUndersideRise();
		Spec.ShellCover = bFrame ? Cover : 0.0;
		Spec.CoursesPerPanel = 1;
		Spec.Panels = FMath::Max(FMath::RoundToInt32(2.0 * PI * Spec.Radius / FMath::Max(P.TileRowSpacing, 6.0)), 24);
		Spec.Segments = FMath::Max(Around, 2 * Spec.Panels);
		Spec.SlopeSegments = FMath::Max(P.RoofSlopeSegments, 12);
		const FVector3d RoofCentre(CX, CY, RoofZ);
		{
			FSlotScope RoofTag(Mesh, MatSlot_Roof);
			UE::Geometry::FIndex2i Under(-1, -1);
			AppendRoundRoof(Mesh, RoofCentre, Spec, &Under);
			RoofTag.Close();
			if (Under.A >= 0) SetMaterialIDForTriangleRange(Mesh, Under.A, Under.B, MatSlot_Wood);
		}
		const double Re = Spec.Radius;
		auto Surface = [&](double Rad) { return RoofZ + Spec.Rise * Spec.Section.HeightFraction(FMath::Clamp(Rad / Re, 0.0, 1.0)); };
		const bool bFinial = P.bHasFinial;
		const double FinialUnitH = P.GetFinialHeight() / C::FinialHeightPerBay;
		const double FinialUnitW = P.GetFinialWidth() / C::FinialWidthPerBay;
		const double CapR = C::RoundFinialCourses[0].FootRadius * FinialUnitW;
		const double CapZ = Surface(CapR) - 1.0;

		if (bFrame)
		{
			FSlotScope FrameTag(Mesh, MatSlot_Wood);
			const double Rafter = C::RafterDiameter * Col;
			auto Underside = [&](double Rad) { return Surface(Rad) - Cover; };
			const double Rg = 0.5 * R;
			const double GoldZ = Underside(Rg) - Rafter - PurlinR;
			AppendArcSweep(Mesh, Ctr, Rg, GoldZ, 0.0, 360.0, MakeCircleProfile(PurlinR, 12), Around);
			const double TieTop = GoldZ - PurlinR + 0.1 * Col;
			const double TieBottom = TieTop - C::GoldTieHeight * Col;
			const double TieT = C::GoldTieThickness * Col;
			AppendArcSweep(Mesh, Ctr, Rg, TieBottom, 0.0, 360.0, Rect(0.0, TieTop - TieBottom, -0.5 * TieT, 0.5 * TieT), Around);

			const double BlockTop = TieBottom + 0.5;
			const double BlockBottom = BlockTop - C::BlockHeight * Col;
			const double BeamTop = BlockBottom + 0.5;
			const double BeamBottom = BeamTop - C::RoundBeamHeight * Col;
			const double BeamW = C::RoundBeamWidth * Col;
			const double Sin60 = FMath::Sin(PI / 3.0);
			for (const double Dir : { -1.0, 1.0 })
			{
				AppendBox(Mesh, FVector3d(CX + Dir * 0.5 * R - 0.5 * BeamW, CY - R * Sin60 - Col, BeamBottom),
					FVector3d(CX + Dir * 0.5 * R + 0.5 * BeamW, CY + R * Sin60 + Col, BeamTop));
				AppendBox(Mesh, FVector3d(CX - 0.5 * R, CY + Dir * Rg * Sin60 - 0.5 * BeamW, BeamBottom + 0.5),
					FVector3d(CX + 0.5 * R, CY + Dir * Rg * Sin60 + 0.5 * BeamW, BeamTop - 0.5));
			}
			for (int32 i = 0; i < NCol; ++i)
			{
				const double A = 2.0 * PI * i / NCol;
				const FVector2d At = Ctr + Rg * FVector2d(FMath::Cos(A), FMath::Sin(A));
				AppendYawedBox(Mesh, At.X, At.Y, FMath::RadiansToDegrees(A) + 90.0, C::BlockLength * Col, C::BlockWidth * Col, BlockBottom, BlockTop);
				AppendMemberUnder(Mesh, FVector3d(At.X, At.Y, Underside(Rg)), FVector3d(CX, CY, Underside(0.0)), C::HipBraceWidth * Col, C::HipBraceHeight * Col);
			}

			const double PostR = 0.5 * C::KingPostDiameter * Col;
			const double PostTop = bFinial ? CapZ + 0.5 * C::RoundFinialCourses[0].Height * FinialUnitH : Surface(1.5 * PostR) - 0.5 * Cover - 2.0;
			Frame::AppendHangingPost(Mesh, CX, CY, PostTop, PostTop - GoldZ, PostR, 0.25);

			const TArray<FVector2d> Unit = { { -0.5, 0.0 }, { 0.5, 0.0 }, { 0.5, 1.0 }, { -0.5, 1.0 } };
			auto AppendRafter = [&](double Th, double R0, double R1, int32 Count)
			{
				const FVector2d Dir(FMath::Cos(Th), FMath::Sin(Th));
				const FVector3d From(CX + R0 * Dir.X, CY + R0 * Dir.Y, Underside(R0) - 0.2);
				const FVector3d To(CX + R1 * Dir.X, CY + R1 * Dir.Y, Underside(R1) - 0.2);
				const FVector3d Along = (To - From).GetSafeNormal();
				const FQuat Rot = FRotationMatrix::MakeFromZX(FVector(Along), FVector(-Dir.Y, Dir.X, 0.0)).ToQuat();
				auto Width = [&](double Rad) { return FMath::Max(FMath::Min(Rafter, 0.8 * 2.0 * PI * Rad / Count), 0.5); };
				AppendSweptProfile(Mesh, Unit, {
					FTransform(Rot, FVector(From), FVector(Width(R0), Rafter, 1.0)),
					FTransform(Rot, FVector(To), FVector(Width(R1), Rafter, 1.0)) });
			};
			const int32 Eaves = NCol * C::RoundRaftersPerBay;
			for (int32 k = 0; k < Eaves; ++k) AppendRafter(2.0 * PI * (k + 0.5) / Eaves, R, Rg, Eaves);
			const int32 Heads = Eaves / 2;
			for (int32 k = 0; k < Heads; ++k) AppendRafter(2.0 * PI * (k + 0.25) / Heads, Rg, PostR + 1.0, Heads);
		}

		if (bFinial)
		{
			FSlotScope FinialTag(Mesh, MatSlot_Finial);
			TArray<FVector2d> Up;   // (radius, height) of each course's foot and top, bottom to top
			double Z = CapZ;
			for (const C::FRoundCourse& Course : C::RoundFinialCourses)
			{
				Up.Add(FVector2d(Course.FootRadius * FinialUnitW, Z));
				Z += Course.Height * FinialUnitH;
				Up.Add(FVector2d(Course.TopRadius * FinialUnitW, Z));
			}
			TArray<FVector2d> Chain;
			// A repeated point would leave two unjoined rings.
			auto Add = [&](const FVector2d& Pt) { if (Chain.Num() == 0 || (Chain.Last() - Pt).Length() > 0.05) Chain.Add(Pt); };
			for (int32 i = UE_ARRAY_COUNT(C::RoundJewel) - 1; i >= 0; --i)
			{
				Add(FVector2d(C::RoundJewel[i][0] * FinialUnitW, Z + C::RoundJewel[i][1] * FinialUnitH));
			}
			for (int32 i = Up.Num() - 1; i >= 0; --i) Add(Up[i]);
			Add(FVector2d(0.0, CapZ));
			AppendRevolvedProfile(Mesh, Ctr, Chain, 32);
		}

		// 每面椽數 18.
		if (P.bHasTileRuns)
		{
			Shell::AppendRoundRoofCourses(Mesh, RoofCentre, Spec, P.RoofTile, FMath::Max(P.EaveFasciaDepth, 0.0), bFinial ? 0.9 * CapR : 0.05 * Re);
		}
		FRoundRoofSpec RafterPanels = Spec;
		RafterPanels.Panels = 36;
		RafterPanels.CoursesPerPanel = 1;
		Shell::FRoofDressing Dress;
		Dress.Tile = P.RoofTile;
		Dress.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
		Dress.bTileRuns = false;
		Dress.RafterSection = P.GetRafterSection();
		Dress.RafterSpacing = 2.0 * PI * Re / (NCol * C::RoundRaftersPerBay);
		Dress.EaveOverhang = O;
		Dress.bNoCorners = true;
		Shell::AppendRoofDressing(Mesh, MakeRoundRoofPanels(RoofCentre, RafterPanels), Dress);
	}

	void BuildPavilion(FDynamicMesh3& Mesh, const FHutongPavilionParams& P)
	{
		if (P.IsRound())
		{
			BuildRoundPavilion(Mesh, P);
			return;
		}

		using namespace HutongMeshUtils;
		namespace C = HutongCanon::Pavilion;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Col = P.GetColumnDiameter();
		const double ColR = P.GetPlanColumnRadius();
		const double Bay = P.GetBay();
		const double Floor = P.GetFloorHeight();
		const double Foot = P.GetColumnFoot();
		const double Eave = P.GetEaveHeight();
		const double RoofZ = P.GetRoofBaseHeight();
		const double PlatO = P.GetPlatformOverhang();

		const double X0 = ColR, X1 = W - ColR;
		const double Y0 = ColR, Y1 = D - ColR;
		const double CX = 0.5 * W, CY = 0.5 * D;
		const double HalfX = CX - X0, HalfY = CY - Y0;
		auto SideAlong = [&](int32 Side) { return (Side % 2 == 0) ? HalfX : HalfY; };
		auto SideOut = [&](int32 Side) { return (Side % 2 == 0) ? HalfY : HalfX; };
		// Built centred with the side on -Y, then turned: 0 front, 1 right, 2 back, 3 left.
		auto PlaceSide = [&](int32 Side, int32 Mark)
		{
			TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, 90.0 * Side, 0.0), FVector(CX, CY, 0.0)));
		};

		if (Floor > 0.0)
		{
			const double PX0 = -PlatO, PX1 = W + PlatO, PY0 = -PlatO, PY1 = D + PlatO;
			const double Edge = FMath::Min(C::EdgeStoneWidth * Col, 0.25 * FMath::Min(PX1 - PX0, PY1 - PY0));
			const double CapZ = Floor - FMath::Min(C::EdgeStoneThickness * Col, 0.5 * Floor);

			FSlotScope BrickTag(Mesh, MatSlot_Body);
			AppendBox(Mesh, FVector3d(PX0, PY0, 0.0), FVector3d(PX1, PY1, CapZ));
			BrickTag.Close();
			FSlotScope FloorTag(Mesh, MatSlot_Floor);
			AppendBox(Mesh, FVector3d(PX0 + Edge, PY0 + Edge, CapZ), FVector3d(PX1 - Edge, PY1 - Edge, Floor));
			FloorTag.Close();

			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			AppendBox(Mesh, FVector3d(PX0, PY0, CapZ), FVector3d(PX1, PY0 + Edge, Floor));
			AppendBox(Mesh, FVector3d(PX0, PY1 - Edge, CapZ), FVector3d(PX1, PY1, Floor));
			AppendBox(Mesh, FVector3d(PX0, PY0 + Edge, CapZ), FVector3d(PX0 + Edge, PY1 - Edge, Floor));
			AppendBox(Mesh, FVector3d(PX1 - Edge, PY0 + Edge, CapZ), FVector3d(PX1, PY1 - Edge, Floor));

			const double Half = 0.5 * C::BaseStoneSide * Col;
			for (int32 i = 0; i < 4; ++i)
			{
				const double X = (i == 0 || i == 3) ? X0 : X1;
				const double Y = (i < 2) ? Y0 : Y1;
				AppendBox(Mesh, FVector3d(X - Half, Y - Half, CapZ), FVector3d(X + Half, Y + Half, Floor + 0.5));
				if (Foot > Floor + 0.5) AppendCylinder(Mesh, FVector3d(X, Y, Floor), C::MirrorRadius * Col, Foot - Floor, 16);
			}

			if (P.bHasSteps)
			{
				for (int32 Side = 0; Side < 4; ++Side)
				{
					const double EdgeY = -(SideOut(Side) + ColR + PlatO);
					const double Ruyi = 0.5 * FMath::Min(C::RuyiWidthPerBay * Bay, 2.0 * (SideAlong(Side) + ColR + PlatO));
					const double Step = FMath::Min(0.5 * C::StepWidthPerBay * Bay, Ruyi);
					const int32 Mark = Mesh.MaxVertexID();
					AppendBox(Mesh, FVector3d(-Ruyi, EdgeY - C::RuyiReachPerBay * Bay, 0.0), FVector3d(Ruyi, EdgeY, Floor / 3.0));
					AppendBox(Mesh, FVector3d(-Step, EdgeY - C::StepReachPerBay * Bay, Floor / 3.0), FVector3d(Step, EdgeY, 2.0 * Floor / 3.0));
					PlaceSide(Side, Mark);
				}
			}
			StoneTag.Close();
		}

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);
		const double PostHalf = P.bSquarePosts ? Frame::SquareHalfWidth(ColR) : ColR;

		for (int32 i = 0; i < 4; ++i)
		{
			const double X = (i == 0 || i == 3) ? X0 : X1;
			const double Y = (i < 2) ? Y0 : Y1;
			if (P.bSquarePosts) Frame::AppendSquareColumn(Mesh, X, Y, PostHalf, Frame::SquareHalfWidth(ColTopR), Foot, Eave);
			else Frame::AppendColumn(Mesh, X, Y, ColR, ColTopR, Foot, Eave, 1.0, 24);
		}

		// The Y pair a hair inside the X pair's top and bottom, so the crossings bury their faces.
		const double LintelT = C::LintelThickness * Col;
		const double LintelBottom = Eave - C::LintelHeight * Col;
		const double Head = C::LintelHeadReach * Col;
		Frame::AppendArchitrave(Mesh, X0 - Head, X1 + Head, Y0, LintelT, LintelBottom, Eave);
		Frame::AppendArchitrave(Mesh, X0 - Head, X1 + Head, Y1, LintelT, LintelBottom, Eave);
		AppendBox(Mesh, FVector3d(X0 - 0.5 * LintelT, Y0 - Head, LintelBottom + 0.5), FVector3d(X0 + 0.5 * LintelT, Y1 + Head, Eave - 0.5));
		AppendBox(Mesh, FVector3d(X1 - 0.5 * LintelT, Y0 - Head, LintelBottom + 0.5), FVector3d(X1 + 0.5 * LintelT, Y1 + Head, Eave - 0.5));

		for (int32 i = 0; i < 4; ++i)
		{
			const double X = (i == 0 || i == 3) ? X0 : X1;
			const double Y = (i < 2) ? Y0 : Y1;
			const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Y - CY, X - CX));
			AppendYawedBox(Mesh, X, Y, Yaw, C::BeamHeadLength * Col, C::BeamHeadSection * Col, Eave - 0.5, Eave + C::BeamHeadSection * Col);
		}

		const double BarT = FMath::Max(P.FriezeBarSection, 1.0);
		const double Drop = FMath::Min(P.FriezeDrop, LintelBottom - Floor - 30.0);
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const double L = SideAlong(Side);
			const double Line = -SideOut(Side);
			const int32 Mark = Mesh.MaxVertexID();
			if (P.bHasFrieze && P.FriezeBars > 0 && Drop > BarT)
			{
				Frame::AppendHangingFrieze(Mesh, -L, L, Line, LintelBottom, Drop, BarT, P.FriezeBars);
			}
			if (P.bHasBench)
			{
				const double Run = FMath::Min(C::BenchRunPerBay * Bay, L - PostHalf);
				const double SeatTop = Floor + P.GetBenchHeight();
				const double SeatD = P.GetBenchDepth();
				if (Run > 3.0 * PostHalf)
				{
					for (const double Dir : { -1.0, 1.0 })
					{
						const double A = Dir * L, B = Dir * (L - Run);
						Frame::AppendBenchRail(Mesh, FMath::Min(A, B), FMath::Max(A, B), Line, Floor, SeatTop, SeatD, PostHalf, true, true);
						const double S0 = FMath::Min(B, B + Dir * PostHalf), S1 = FMath::Max(B, B + Dir * PostHalf);
						AppendBox(Mesh, FVector3d(S0, Line - 0.25 * SeatD, Floor), FVector3d(S1, Line + 0.25 * SeatD, SeatTop));
					}
				}
			}
			PlaceSide(Side, Mark);
		}

		WoodTag.Close();

		const double O = P.GetRoofOverhang();
		const double RoofW = (X1 - X0) + 2.0 * O;
		const double RoofD = (Y1 - Y0) + 2.0 * O;
		const double RoofRise = P.GetRoofRise();
		const FVector3d RoofMin(X0 - O, Y0 - O, RoofZ);
		const HutongGen::FHutongRoofSection Section = P.GetRoofSection();
		const bool bCuanjian = P.RoofType == EHutongRoofType::Cuanjian;
		const bool bFrame = P.bExposedFrame && bCuanjian;
		const double Cover = HutongCanon::Frame::RoofCover * Col;

		auto Surface = [&](double U) { return RoofZ + RoofRise * Section.HeightFraction(U); };
		auto UAt = [&](double Half, double FromCentre) { return FMath::Clamp(FromCentre / FMath::Max(Half + O, 1.0), 0.0, 1.0); };

		if (RoofZ > Eave)
		{
			FSlotScope StackTag(Mesh, MatSlot_Wood);
			const double Reach = Head;
			Frame::AppendEaveStack(Mesh, X0, X1, Y0, Col, Eave, C::BoardHeight, C::PurlinDiameter, Reach);
			Frame::AppendEaveStack(Mesh, X0, X1, Y1, Col, Eave, C::BoardHeight, C::PurlinDiameter, Reach);
			Frame::AppendEaveStackAlongY(Mesh, Y0, Y1, X0, Col, Eave, C::BoardHeight, C::PurlinDiameter, Reach);
			Frame::AppendEaveStackAlongY(Mesh, Y0, Y1, X1, Col, Eave, C::BoardHeight, C::PurlinDiameter, Reach);
		}

		FSlotScope RoofTag(Mesh, MatSlot_Roof);
		TArray<FRoofPanel> Panels;
		TArray<UE::Geometry::FIndex2i> Timber;
		if (P.RoofType == EHutongRoofType::Xieshan)
		{
			FXieshanRoofSpec Xie;
			Xie.Width = RoofW;
			Xie.Depth = RoofD;
			Xie.Rise = RoofRise;
			Xie.Section = Section;
			Xie.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile) && !P.bHasTileRuns;
			Xie.TileRowSpacing = P.TileRowSpacing;
			Xie.ShouInset = FMath::Max(P.ShouInset, 1.0);
			Xie.FlareRise = P.GetFlareRise();
			Xie.FlareRun = P.GetFlareRun();
			Xie.FlareLength = 0.4 * FMath::Min(Xie.Width, Xie.Depth);
			Xie.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Xie.EaveOverhang = O;
			Xie.UndersideRise = P.GetUndersideRise();
			Xie.RidgeWidth = FMath::Max(P.RidgeCourseWidth, 0.0);
			Xie.RidgeHeight = FMath::Max(P.RidgeCourseHeight, 0.0);
			Xie.BargeThickness = FMath::Max(P.BargeBoardThickness, 0.0);
			Xie.BargeDepth = FMath::Max(P.BargeBoardDepth, 0.0);
			Xie.EaveSegments = P.RoofEaveSegments;
			// The gable needs rows either side of the 收山 line.
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
			// Zero collapses the apex to a point.
			Hip.RidgeLength = (P.RoofType == EHutongRoofType::Wudian) ? FMath::Max(RoofW - RoofD, 0.0) : 0.0;
			Hip.Rise = RoofRise;
			Hip.Section = Section;
			Hip.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile) && !P.bHasTileRuns;
			Hip.TileRowSpacing = P.TileRowSpacing;
			Hip.FlareRise = P.GetFlareRise();
			Hip.FlareRun = P.GetFlareRun();
			Hip.FlareLength = 0.4 * FMath::Min(Hip.Width, Hip.Depth);
			Hip.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Hip.EaveOverhang = O;
			Hip.UndersideRise = P.GetUndersideRise();
			if (bFrame) Hip.ShellCover = Cover;
			Hip.EaveSegments = P.RoofEaveSegments;
			Hip.SlopeSegments = P.RoofSlopeSegments;

			if (P.bHasTileRuns) Shell::DressedRoofSegments(Hip.EaveSegments, Hip.SlopeSegments, FMath::Max(Hip.Width, Hip.Depth),
				FMath::Sqrt(FMath::Square(0.5 * FMath::Min(Hip.Width, Hip.Depth)) + FMath::Square(Hip.Rise)), P.TileRowSpacing);
			AppendHippedRoof(Mesh, RoofMin, Hip, &Panels);
		}
		RoofTag.Close();
		for (const UE::Geometry::FIndex2i& R : Timber) SetMaterialIDForTriangleRange(Mesh, R.A, R.B, MatSlot_Wood);

		const double ApexZ = Surface(0.0);
		const bool bFinial = P.bHasFinial && bCuanjian;
		const double FinialUnitH = P.GetFinialHeight() / C::FinialHeightPerBay;
		const double FinialUnitW = P.GetFinialWidth() / C::FinialWidthPerBay;
		const double FinialBaseTop = ApexZ + C::FinialCourses[0].Height * FinialUnitH;

		// Members' tops a rafter under the underside, so all stay inside the shell.
		if (bFrame)
		{
			FSlotScope FrameTag(Mesh, MatSlot_Wood);
			const double Rafter = C::RafterDiameter * Col;
			auto Underside = [&](double U) { return Surface(U) - Cover; };
			const double GX = 0.5 * HalfX, GY = 0.5 * HalfY;
			const double UGold = UAt(HalfY, GY);
			const double PurlinR = 0.5 * C::PurlinDiameter * Col;
			const double GoldZ = Underside(UGold) - Rafter - PurlinR;

			const double GoldReach = C::GoldPurlinHeadReach * Col;
			const double TieTop = GoldZ - PurlinR + 0.1 * Col;
			const double TieBottom = TieTop - C::GoldTieHeight * Col;
			const double TieT = C::GoldTieThickness * Col;
			for (const double Dir : { -1.0, 1.0 })
			{
				AppendPurlinX(Mesh, CX - GX - GoldReach, CX + GX + GoldReach, CY + Dir * GY, GoldZ, PurlinR);
				const int32 Mark = Mesh.MaxVertexID();
				AppendPurlinX(Mesh, -GY - GoldReach, GY + GoldReach, 0.0, GoldZ, PurlinR * 0.999);
				TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, 90.0, 0.0), FVector(CX + Dir * GX, CY, 0.0)));
				AppendBox(Mesh, FVector3d(CX - GX, CY + Dir * GY - 0.5 * TieT, TieBottom), FVector3d(CX + GX, CY + Dir * GY + 0.5 * TieT, TieTop));
				AppendBox(Mesh, FVector3d(CX + Dir * GX - 0.5 * TieT, CY - GY, TieBottom + 0.5), FVector3d(CX + Dir * GX + 0.5 * TieT, CY + GY, TieTop - 0.5));
			}

			const double BlockTop = TieBottom + 0.5;
			const double BlockBottom = BlockTop - C::BlockHeight * Col;
			const double BeamTop = BlockBottom + 0.5;
			const double BeamBottom = BeamTop - C::CornerBeamHeight * Col;
			for (int32 i = 0; i < 4; ++i)
			{
				const double SX = (i == 0 || i == 3) ? -1.0 : 1.0;
				const double SY = (i < 2) ? -1.0 : 1.0;
				const FVector2d A(CX + SX * HalfX, CY), B(CX, CY + SY * HalfY);
				const FVector2d Mid = 0.5 * (A + B);
				const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
				AppendYawedBox(Mesh, Mid.X, Mid.Y, Yaw, (B - A).Length() + Col, C::CornerBeamWidth * Col, BeamBottom, BeamTop);
				AppendYawedBox(Mesh, CX + SX * GX, CY + SY * GY, Yaw, C::BlockLength * Col, C::BlockWidth * Col, BlockBottom, BlockTop);

				const FVector3d From(CX + SX * GX, CY + SY * GY, Underside(UGold));
				const FVector3d To(CX, CY, Underside(0.0));
				AppendMemberUnder(Mesh, From, To, C::HipBraceWidth * Col, C::HipBraceHeight * Col);
			}

			const double PostR = 0.5 * C::KingPostDiameter * Col;
			// The profile starts 2 cm over its top; bare, the rim stays under the slope it meets.
			const double PostTop = bFinial ? FinialBaseTop - 3.0 : Surface(UAt(HalfY, 1.5 * PostR)) - 0.5 * Cover - 2.0;
			Frame::AppendHangingPost(Mesh, CX, CY, PostTop, PostTop - GoldZ, PostR, 0.25);

			const double Spacing = FMath::Max(P.GetRafterSpacing(), 2.0 * Rafter);
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const double L = SideAlong(Side), E = SideOut(Side);
				const int32 N = FMath::Max(FMath::FloorToInt32(2.0 * L / Spacing), 1);
				const int32 Mark = Mesh.MaxVertexID();
				for (int32 k = 0; k <= N; ++k)
				{
					const double X = -L + 2.0 * L * k / N;
					const double Stop = FMath::Abs(X) * E / FMath::Max(L, 1.0);
					if (E - Stop < Rafter) continue;
					TArray<double> Knots = { E };
					const double Gold = 0.5 * E;
					if (Gold > Stop && Gold < E) Knots.Add(Gold);
					Knots.Add(Stop);
					for (int32 j = 0; j + 1 < Knots.Num(); ++j)
					{
						const FVector3d From(X, -Knots[j], Underside(UAt(E, Knots[j])) - 0.2);
						const FVector3d To(X, -Knots[j + 1], Underside(UAt(E, Knots[j + 1])) - 0.2);
						AppendMemberUnder(Mesh, From, To, Rafter, Rafter);
					}
				}
				PlaceSide(Side, Mark);
			}
		}

		if (bFinial)
		{
			FSlotScope FinialTag(Mesh, MatSlot_Finial);
			const double BaseHalf = 0.5 * C::FinialCourses[0].Side * FinialUnitW;
			const double Sink = ApexZ - Surface(UAt(HalfY, BaseHalf)) + 1.0;
			AppendBox(Mesh, FVector3d(CX - BaseHalf, CY - BaseHalf, ApexZ - Sink), FVector3d(CX + BaseHalf, CY + BaseHalf, FinialBaseTop));
			double Z = FinialBaseTop;
			for (int32 i = 1; i < int32(UE_ARRAY_COUNT(C::FinialCourses)); ++i)
			{
				const C::FFinialCourse& Course = C::FinialCourses[i];
				const double H = Course.Height * FinialUnitH;
				const double Half = 0.5 * Course.Side * FinialUnitW;
				if (Course.bRound) AppendCylinder(Mesh, FVector3d(CX, CY, Z), Half, H, 16);
				else AppendBox(Mesh, FVector3d(CX - Half, CY - Half, Z), FVector3d(CX + Half, CY + Half, Z + H));
				Z += H;
			}
		}

		Shell::FRoofDressing Dress;
		Dress.Tile = P.RoofTile;
		Dress.TileRowSpacing = P.TileRowSpacing;
		Dress.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
		Dress.bTileRuns = P.bHasTileRuns;
		Dress.RafterSection = P.GetRafterSection();
		Dress.RafterSpacing = P.GetRafterSpacing();
		Dress.EaveOverhang = O;
		if (P.RoofType != EHutongRoofType::Xieshan)
		{
			Dress.RidgeWidth = FMath::Max(P.RidgeCourseWidth, 0.0);
			Dress.RidgeHeight = FMath::Max(P.RidgeCourseHeight, 0.0);
		}
		Shell::AppendRoofDressing(Mesh, Panels, Dress);
	}
}
