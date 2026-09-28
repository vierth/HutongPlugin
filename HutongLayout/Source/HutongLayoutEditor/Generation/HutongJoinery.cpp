#include "Generation/HutongJoinery.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace Joinery
	{
		using namespace HutongMeshUtils;
		namespace J = HutongCanon::Joinery;

		namespace
		{
			// Lattice over paper in an opening; bars buried 0.5 in the frame round it. Mullions and rails at
			// different depths so no two front faces share a plane where they cross.
			void AppendGrille(FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
				double Y0, double Depth, const FLattice& Lattice)
			{
				if (X1 - X0 < 1.0 || Z1 - Z0 < 1.0) return;
				const double PaperY = Y0 + 0.7 * Depth;
				if (Lattice.bPaper)
				{
					FSlotScope PaperTag(Mesh, MatSlot_Paper);
					AppendBox(Mesh, FVector3d(X0 - 0.5, PaperY, Z0 - 0.5), FVector3d(X1 + 0.5, PaperY + 0.4, Z1 + 0.5));
				}
				if (!Lattice.bBars) return;
				const double BarDepth = FMath::Max(0.7 * Depth - 1.5, 0.5);
				const double HalfW = 0.5 * FMath::Clamp(Lattice.BarWidth, 0.5, 0.25 * FMath::Min(X1 - X0, Z1 - Z0));
				FSlotScope BarTag(Mesh, MatSlot_Lattice);
				const int32 Mullions = FMath::Clamp(Lattice.Mullions, 0, 12);
				for (int32 j = 1; j <= Mullions; ++j)
				{
					const double X = X0 + (X1 - X0) * j / (Mullions + 1);
					AppendBox(Mesh, FVector3d(X - HalfW, Y0 + 0.5, Z0 - 0.5), FVector3d(X + HalfW, Y0 + 0.5 + BarDepth, Z1 + 0.5));
				}
				const int32 Rails = FMath::Clamp(Lattice.Rails, 0, 12);
				for (int32 j = 1; j <= Rails; ++j)
				{
					const double Z = Z0 + (Z1 - Z0) * j / (Rails + 1);
					AppendBox(Mesh, FVector3d(X0 - 0.5, Y0 + 0.9, Z - HalfW), FVector3d(X1 + 0.5, Y0 + 0.9 + BarDepth, Z + HalfW));
				}
			}

			// A sunk board between members, its edges buried 0.5 in them.
			// Thickness, where given, sets the board's own (表十四's 1/20 of the leaf); else it sits PanelRecessCm in.
			void AppendPanel(FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1, double Y0, double Depth,
				double Thickness = 0.0)
			{
				if (X1 - X0 < 0.5 || Z1 - Z0 < 0.5) return;
				const double Recess = (Thickness > 0.0)
					? FMath::Clamp(0.5 * (Depth - Thickness), 1.0, 0.45 * Depth)
					: FMath::Min(J::PanelRecessCm, 0.35 * Depth);
				AppendBox(Mesh, FVector3d(X0 - 0.5, Y0 + Recess, Z0 - 0.5), FVector3d(X1 + 0.5, Y0 + Depth - Recess, Z1 + 0.5));
				// Raised field, standing off the board on the street face, still behind the frame's.
				const double Inset = FMath::Clamp(J::PanelFieldInsetShare * FMath::Min(X1 - X0, Z1 - Z0), 1.5, 5.0);
				const double Proud = FMath::Min(J::PanelFieldProudCm, Recess - 0.5);
				if (X1 - X0 > 3.0 * Inset && Z1 - Z0 > 3.0 * Inset && Proud > 0.2)
				{
					AppendBox(Mesh, FVector3d(X0 + Inset, Y0 + Recess - Proud, Z0 + Inset), FVector3d(X1 - Inset, Y0 + Recess + 1.0, Z1 - Inset));
				}
			}

			// Rotation about a vertical axis through Pivot: every vertex appended since FirstVertex.
			void SwingFrom(FDynamicMesh3& Mesh, int32 FirstVertex, const FVector& Pivot, double Degrees)
			{
				const FQuat Q(FVector::ZAxisVector, FMath::DegreesToRadians(Degrees));
				TransformVerticesFrom(Mesh, FirstVertex, FTransform(Q, Pivot - Q.RotateVector(Pivot)));
			}
		}

		void AppendGeshan(FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
			double SplitZ, double Y0, double Depth, const FLattice& Lattice)
		{
			const double W = X1 - X0, H = Z1 - Z0;
			if (W < 4.0 || H < 20.0) return;
			// 表十四, in the leaf's width.
			const double Stile = FMath::Clamp(J::GeshanStileOfWidth * W, 2.5, 12.0);
			const double Small = J::GeshanSmallPanelOfWidth * W;
			const double Skirt = J::GeshanSkirtOfWidth * W;
			const double Board = J::GeshanPanelThicknessOfWidth * W;
			const double IX0 = X0 + Stile, IX1 = X1 - Stile;

			// The lower run ends on the sill (p.104); on a leaf of the table's own proportion that is 1.2 W up, and
			// otherwise its parts stretch or shrink together to meet it.
			const double Split = FMath::Clamp(SplitZ, Z0 + 0.2 * H, Z0 + 0.7 * H);
			const double LowerScale = (Split - Z0) / (2.0 * Stile + Small + Skirt);
			// Above: 抹頭, 中絛環, 抹頭, then the 槅心 takes what is left under the top 抹頭; the fixed parts give way
			// on a short leaf so the 槅心 keeps at least two rails' height.
			const double UpperFixed = 3.0 * Stile + Small;
			const double UpperScale = FMath::Min(1.0, (Z1 - Split) / (UpperFixed + 2.0 * Stile));

			struct FBand { double Z0, Z1; bool bRail; };
			TArray<FBand> Bands;
			double Z = Z0;
			auto AddBand = [&](double Height, bool bRail) { Bands.Add({ Z, Z + Height, bRail }); Z += Height; };
			AddBand(Stile * LowerScale, true);
			AddBand(Small * LowerScale, false);
			AddBand(Stile * LowerScale, true);
			AddBand(Skirt * LowerScale, false);
			AddBand(Stile * UpperScale, true);
			AddBand(Small * UpperScale, false);
			AddBand(Stile * UpperScale, true);
			const int32 GrilleBand = Bands.Num();
			AddBand(FMath::Max(Z1 - Stile * UpperScale - Z, 1.0), false);
			AddBand(Z1 - Z, true);

			{
				FSlotScope LeafTag(Mesh, MatSlot_DoorPaint);
				AppendBox(Mesh, FVector3d(X0, Y0, Z0), FVector3d(IX0, Y0 + Depth, Z1));
				AppendBox(Mesh, FVector3d(IX1, Y0, Z0), FVector3d(X1, Y0 + Depth, Z1));
				for (int32 k = 0; k < Bands.Num(); ++k)
				{
					if (Bands[k].bRail) AppendBox(Mesh, FVector3d(IX0, Y0, Bands[k].Z0), FVector3d(IX1, Y0 + Depth, Bands[k].Z1));
					else if (k != GrilleBand) AppendPanel(Mesh, IX0, IX1, Bands[k].Z0, Bands[k].Z1, Y0, Depth, Board);
				}
			}

			// 槅心: a 仔邊 round the lattice, ⅔ of the 邊梃 on the face and 7/10 its depth, set in from the leaf's face;
			// the 欞條 ⅘ of the 仔邊.
			const FBand& G = Bands[GrilleBand];
			const double Rim = J::GeshanRimOfStile * Stile;
			const double RimDepth = J::GeshanRimDepthOfStile * Depth;
			const double RimY = Y0 + 0.5 * (Depth - RimDepth);
			if (IX1 - IX0 > 3.0 * Rim && G.Z1 - G.Z0 > 3.0 * Rim)
			{
				{
					FSlotScope RimTag(Mesh, MatSlot_DoorPaint);
					AppendBox(Mesh, FVector3d(IX0 - 0.5, RimY, G.Z0 - 0.5), FVector3d(IX0 + Rim, RimY + RimDepth, G.Z1 + 0.5));
					AppendBox(Mesh, FVector3d(IX1 - Rim, RimY, G.Z0 - 0.5), FVector3d(IX1 + 0.5, RimY + RimDepth, G.Z1 + 0.5));
					AppendBox(Mesh, FVector3d(IX0 + Rim, RimY, G.Z0 - 0.5), FVector3d(IX1 - Rim, RimY + RimDepth, G.Z0 + Rim));
					AppendBox(Mesh, FVector3d(IX0 + Rim, RimY, G.Z1 - Rim), FVector3d(IX1 - Rim, RimY + RimDepth, G.Z1 + 0.5));
				}
				FLattice Bars = Lattice;
				Bars.BarWidth = J::GeshanBarOfRim * Rim;
				AppendGrille(Mesh, IX0 + Rim, IX1 - Rim, G.Z0 + Rim, G.Z1 - Rim, RimY, RimDepth, Bars);
			}
			else
			{
				AppendGrille(Mesh, IX0, IX1, G.Z0, G.Z1, Y0, Depth, Lattice);
			}
		}

		void AppendWindow(FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
			double Y0, double Depth, double PostY0, double PostY1, const FLattice& Lattice)
		{
			if (X1 - X0 < 20.0 || Z1 - Z0 < 20.0) return;
			const double Mid = 0.5 * (X0 + X1);
			const double HalfPost = 0.5 * FMath::Max(J::WindowPostShare * (X1 - X0), 3.0);
			const double ZS = Z0 + J::WindowSplitShare * (Z1 - Z0);
			const double F = FMath::Clamp(0.06 * FMath::Min(Mid - HalfPost - X0, ZS - Z0), 2.5, 6.0);
			// 間框, and each half's 支窗 over 摘窗 framed as one: two 邊梃, three 抹頭 (the middle one both sashes').
			{
				FSlotScope FrameTag(Mesh, MatSlot_Wood);
				AppendBox(Mesh, FVector3d(Mid - HalfPost, PostY0, Z0), FVector3d(Mid + HalfPost, PostY1, Z1));
				for (const TPair<double, double>& Half : { TPair<double, double>(X0, Mid - HalfPost), TPair<double, double>(Mid + HalfPost, X1) })
				{
					const double A = Half.Key, B = Half.Value;
					AppendBox(Mesh, FVector3d(A, Y0, Z0), FVector3d(A + F, Y0 + Depth, Z1));
					AppendBox(Mesh, FVector3d(B - F, Y0, Z0), FVector3d(B, Y0 + Depth, Z1));
					AppendBox(Mesh, FVector3d(A + F, Y0, Z0), FVector3d(B - F, Y0 + Depth, Z0 + F));
					AppendBox(Mesh, FVector3d(A + F, Y0, ZS - 0.75 * F), FVector3d(B - F, Y0 + Depth, ZS + 0.75 * F));
					AppendBox(Mesh, FVector3d(A + F, Y0, Z1 - F), FVector3d(B - F, Y0 + Depth, Z1));
				}
			}
			// Lattice in the 支窗 only; the 摘窗 is a plain pane.
			FLattice Upper = Lattice;
			Upper.bPaper = false;
			for (const TPair<double, double>& Half : { TPair<double, double>(X0, Mid - HalfPost), TPair<double, double>(Mid + HalfPost, X1) })
			{
				AppendGrille(Mesh, Half.Key + F, Half.Value - F, ZS + 0.75 * F, Z1 - F, Y0, Depth, Upper);
			}
			// One sheet of paper behind the whole window, through the 間框.
			if (Lattice.bPaper)
			{
				FSlotScope PaperTag(Mesh, MatSlot_Paper);
				const double PaperY = Y0 + 0.7 * Depth;
				AppendBox(Mesh, FVector3d(X0 - 0.5, PaperY, Z0 + F), FVector3d(X1 + 0.5, PaperY + 0.4, Z1 - F));
			}
		}

		void AppendTransom(FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
			double Y0, double Depth, double PostY0, double PostY1, int32 Panes, const FLattice& Lattice)
		{
			const int32 N = FMath::Clamp(Panes, 1, 8);
			if (X1 - X0 < 10.0 * N || Z1 - Z0 < 4.0) return;
			// The 短間框, 中檻 and 上檻 frame each pane; one sheet of paper behind them all.
			const double Post = J::TransomPostShare * (X1 - X0);
			const double PaneW = (X1 - X0 - (N - 1) * Post) / N;
			FLattice Pane = Lattice;
			Pane.Mullions = FMath::Min(Lattice.Mullions, 1);
			Pane.Rails = FMath::Min(Lattice.Rails, 1);
			Pane.bPaper = false;
			for (int32 i = 0; i < N; ++i)
			{
				const double PX = X0 + i * (PaneW + Post);
				AppendGrille(Mesh, PX, PX + PaneW, Z0, Z1, Y0, Depth, Pane);
				if (i + 1 < N)
				{
					FSlotScope PostTag(Mesh, MatSlot_Wood);
					AppendBox(Mesh, FVector3d(PX + PaneW, PostY0, Z0), FVector3d(PX + PaneW + Post, PostY1, Z1));
				}
			}
			if (Lattice.bPaper)
			{
				FSlotScope PaperTag(Mesh, MatSlot_Paper);
				const double PaperY = Y0 + 0.7 * Depth;
				AppendBox(Mesh, FVector3d(X0 - 0.5, PaperY, Z0 - 0.5), FVector3d(X1 + 0.5, PaperY + 0.4, Z1 + 0.5));
			}
		}

		double LeafThickness(const FDoorBay& Bay)
		{
			// 表十四: 3/20 of the leaf's width deep, inside the 檻.
			const double LeafW = (Bay.X1 - Bay.X0) / J::GeshanPerBay;
			return FMath::Min(J::GeshanDepthOfWidth * LeafW, FMath::Max(Bay.RailY1 - Bay.RailY0 - 2.0, 1.0));
		}

		FCurtain LayoutCurtain(const FDoorBay& Bay)
		{
			FCurtain C;
			const double LeafW = (Bay.X1 - Bay.X0) / J::GeshanPerBay;
			const double H = Bay.RailZ - Bay.SillTopZ;
			if (LeafW <= 0.0 || H <= 0.0) return C;

			C.X0 = Bay.X0 + LeafW;
			C.X1 = Bay.X1 - LeafW;
			const double W = C.X1 - C.X0;
			C.Stile = FMath::Clamp(J::CurtainStileShare * W, 3.0, 8.0);
			C.Y1 = Bay.RailY0 - 0.5;
			C.Y0 = C.Y1 - J::CurtainThicknessCm;

			const double TopRail = FMath::Max(J::CurtainTopRail * H, 2.5);
			const double MidRail = FMath::Max(J::CurtainMidRail * H, 2.5);
			double Transom = J::CurtainTransom * H;
			double Lintel = J::CurtainLintel * H;
			double Dumb = J::CurtainDumbSill * H;
			// The 門口 is sized to the 吉門 height: short of it the bands give way, 楣子, then 簾架橫陂, then the
			// 啞吧檻; still short of a crouch's clear height, no 簾架.
			const double Want = FMath::Max(Bay.DoorClearHeight, Bay.MinClearHeight);
			double Short = Want - (H - TopRail - Transom - MidRail - Lintel - Dumb);
			for (double* Band : { &Lintel, &Transom, &Dumb })
			{
				const double Floor = (Band == &Dumb) ? 0.0 : C.Stile;
				const double Give = FMath::Clamp(Short, 0.0, FMath::Max(*Band - Floor, 0.0));
				*Band -= Give;
				Short -= Give;
			}
			if (Short > Want - Bay.MinClearHeight) return C;

			C.TransomTopZ = Bay.RailZ - TopRail;
			C.TransomBottomZ = C.TransomTopZ - Transom;
			C.LintelTopZ = C.TransomBottomZ - MidRail;
			C.DoorTopZ = C.LintelTopZ - Lintel;
			C.DoorBottomZ = Bay.SillTopZ + Dumb;

			const double Inner = W - 2.0 * C.Stile;
			const double DoorW = FMath::Min(FMath::Max(J::CurtainDoorShare * W, Bay.MinDoorWidth), Inner);
			const double Mid = 0.5 * (C.X0 + C.X1);
			C.DoorX0 = Mid - 0.5 * DoorW;
			C.DoorX1 = Mid + 0.5 * DoorW;
			C.bValid = true;
			return C;
		}

		void AppendDoorBay(FDynamicMesh3& Mesh, const FDoorBay& Bay, const FLattice& Lattice)
		{
			const double LeafW = (Bay.X1 - Bay.X0) / J::GeshanPerBay;
			if (LeafW < 10.0 || Bay.RailZ - Bay.SillTopZ < 40.0) return;
			const double LeafT = LeafThickness(Bay);
			const double LeafY = Bay.RailY0 + 0.5 * (Bay.RailY1 - Bay.RailY0 - LeafT);

			{
				FSlotScope SillTag(Mesh, MatSlot_Wood);
				AppendBox(Mesh, FVector3d(Bay.X0 - 1.0, Bay.RailY0, Bay.FloorZ), FVector3d(Bay.X1 + 1.0, Bay.RailY1, Bay.SillTopZ));
			}

			// 一樘四扇: the outer two fixed, the middle two swung into the room about their outer edges.
			for (int32 i = 0; i < J::GeshanPerBay; ++i)
			{
				const double LX0 = Bay.X0 + i * LeafW;
				const int32 V0 = Mesh.MaxVertexID();
				AppendGeshan(Mesh, LX0, LX0 + LeafW, Bay.SillTopZ, Bay.RailZ, Bay.SplitZ, LeafY, LeafT, Lattice);
				const bool bMoving = (i == J::GeshanPerBay / 2 - 1) || (i == J::GeshanPerBay / 2);
				if (Bay.bOpen && bMoving)
				{
					const bool bLeft = (i < J::GeshanPerBay / 2);
					SwingFrom(Mesh, V0, FVector(bLeft ? LX0 : LX0 + LeafW, LeafY + LeafT, 0.0),
						bLeft ? J::GeshanOpenDeg : -J::GeshanOpenDeg);
				}
			}

			if (!Bay.bCurtain) return;
			const FCurtain C = LayoutCurtain(Bay);
			if (!C.bValid) return;

			const double S = C.Stile;
			const double Y0 = C.Y0, Y1 = C.Y1, D = Y1 - Y0;
			const double DoorStile = 0.8 * S;
			FSlotScope CurtainTag(Mesh, MatSlot_Wood);
			// 大框: 邊梃 on 荷葉墩 from the 下檻's top to the 中檻, 栓斗 holding their heads to it.
			for (const double SX : { C.X0, C.X1 - S })
			{
				AppendBox(Mesh, FVector3d(SX, Y0, Bay.SillTopZ), FVector3d(SX + S, Y1, Bay.RailZ));
				AppendBox(Mesh, FVector3d(SX - 0.3 * S, Y0 - 1.0, Bay.FloorZ), FVector3d(SX + 1.3 * S, Bay.RailY0 + 1.0, Bay.SillTopZ));
				AppendBox(Mesh, FVector3d(SX - 0.4 * S, Y0 - 1.5, Bay.RailZ - 1.0),
					FVector3d(SX + 1.4 * S, Bay.RailY0 + 1.0, Bay.RailZ + 0.6 * (Bay.RailTopZ - Bay.RailZ)));
			}
			const double IX0 = C.X0 + S, IX1 = C.X1 - S;
			// 抹頭, 簾架橫陂, 抹頭, 楣子 over the 門口.
			AppendBox(Mesh, FVector3d(IX0, Y0, C.TransomTopZ), FVector3d(IX1, Y1, Bay.RailZ));
			AppendPanel(Mesh, IX0, IX1, C.TransomBottomZ, C.TransomTopZ, Y0, D);
			AppendBox(Mesh, FVector3d(IX0, Y0, C.LintelTopZ), FVector3d(IX1, Y1, C.TransomBottomZ));
			AppendPanel(Mesh, IX0, IX1, C.DoorTopZ + DoorStile, C.LintelTopZ, Y0, D);
			AppendBox(Mesh, FVector3d(IX0, Y0, C.DoorTopZ), FVector3d(IX1, Y1, C.DoorTopZ + DoorStile));
			// 餘塞 (腿子) either side of the 門口, framed at the door.
			if (C.DoorX0 - IX0 > DoorStile + 1.0)
			{
				AppendBox(Mesh, FVector3d(C.DoorX0 - DoorStile, Y0, Bay.SillTopZ), FVector3d(C.DoorX0, Y1, C.DoorTopZ));
				AppendBox(Mesh, FVector3d(C.DoorX1, Y0, Bay.SillTopZ), FVector3d(C.DoorX1 + DoorStile, Y1, C.DoorTopZ));
				AppendPanel(Mesh, IX0, C.DoorX0 - DoorStile, Bay.SillTopZ, C.DoorTopZ, Y0, D);
				AppendPanel(Mesh, C.DoorX1 + DoorStile, IX1, Bay.SillTopZ, C.DoorTopZ, Y0, D);
			}
			// 啞吧檻 before the 下檻, under the 風門.
			if (C.DoorBottomZ > Bay.SillTopZ)
			{
				AppendBox(Mesh, FVector3d(C.DoorX0, Y0, Bay.FloorZ), FVector3d(C.DoorX1, Bay.RailY0 + 1.0, C.DoorBottomZ));
			}
			CurtainTag.Close();

			// 風門: one leaf, lattice over two boards, opening outward about its left edge.
			const int32 V0 = Mesh.MaxVertexID();
			const double FY0 = Y0 + 0.5, FD = D - 1.0;
			const double FX0 = C.DoorX0, FX1 = C.DoorX1;
			const double FH = C.DoorTopZ - C.DoorBottomZ;
			const double FS = FMath::Clamp(0.1 * (FX1 - FX0), 3.0, 8.0);
			const double ZA = C.DoorBottomZ + 0.3 * FH, ZB = C.DoorBottomZ + 0.4 * FH;
			{
				FSlotScope LeafTag(Mesh, MatSlot_DoorPaint);
				AppendBox(Mesh, FVector3d(FX0, FY0, C.DoorBottomZ), FVector3d(FX0 + FS, FY0 + FD, C.DoorTopZ));
				AppendBox(Mesh, FVector3d(FX1 - FS, FY0, C.DoorBottomZ), FVector3d(FX1, FY0 + FD, C.DoorTopZ));
				for (const TPair<double, double>& Rail : { TPair<double, double>(C.DoorBottomZ, C.DoorBottomZ + FS),
					TPair<double, double>(ZA, ZA + FS), TPair<double, double>(ZB, ZB + FS), TPair<double, double>(C.DoorTopZ - FS, C.DoorTopZ) })
				{
					AppendBox(Mesh, FVector3d(FX0 + FS, FY0, Rail.Key), FVector3d(FX1 - FS, FY0 + FD, Rail.Value));
				}
				AppendPanel(Mesh, FX0 + FS, FX1 - FS, C.DoorBottomZ + FS, ZA, FY0, FD);
				AppendPanel(Mesh, FX0 + FS, FX1 - FS, ZA + FS, ZB, FY0, FD);
			}
			AppendGrille(Mesh, FX0 + FS, FX1 - FS, ZB + FS, C.DoorTopZ - FS, FY0, FD, Lattice);
			if (Bay.bOpen)
			{
				// As far as it goes: back against the 餘塞 with its far end 1 cm clear of the frame's face (荷葉墩 and
				// 栓斗 stand below and above the leaf), short of the bay's edge if it would reach it.
				const double Leaf = FX1 - FX0;
				const double Flat = 180.0 - FMath::RadiansToDegrees(FMath::Asin(FMath::Min((FY0 - Y0 + 1.0) / Leaf, 1.0)));
				const double Room = FX0 - Bay.X0 - 1.0;
				const double Edge = (Room < Leaf) ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-Room / Leaf, -1.0, 1.0))) : 180.0;
				SwingFrom(Mesh, V0, FVector(FX0, FY0, 0.0), -FMath::Max(FMath::Min(Flat, Edge), J::FengmenOpenDeg));
			}
		}
	}
}
