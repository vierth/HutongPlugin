#include "Generation/HutongShell.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace Shell
	{
		using namespace HutongMeshUtils;

		void AppendSteps(
			FDynamicMesh3& Mesh,
			double X0, double X1, double EdgeY, double Direction,
			int32 StepCount, double StepTread, double FloorHeight)
		{
			const int32 StoneFirstTri = Mesh.MaxTriangleID();
			const int32 Steps = FMath::Clamp(StepCount, 0, 8);
			if (Steps <= 0 || X1 <= X0 || FloorHeight <= 0.0)
			{
				return;
			}

			const double Tread = FMath::Max(StepTread, 5.0);
			const double Dir = (Direction < 0.0) ? -1.0 : 1.0;

			// Solid blocks rather than treads.
			for (int32 i = 1; i <= Steps; ++i)
			{
				const double TopZ = FloorHeight * double(Steps + 1 - i) / double(Steps + 1);
				const double Ya = EdgeY + Dir * (i - 1) * Tread;
				const double Yb = EdgeY + Dir * i * Tread;
				AppendBox(Mesh,
					FVector3d(X0, FMath::Min(Ya, Yb), 0.0),
					FVector3d(X1, FMath::Max(Ya, Yb), TopZ));
			}
			SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);
		}

		void AppendApron(
			FDynamicMesh3& Mesh,
			double X0, double Y0, double X1, double Y1,
			double FrontWidth, double RearWidth, double SideWidth, double ApronThickness)
		{
			const double AF = FMath::Max(FrontWidth, 0.0);
			const double AR = FMath::Max(RearWidth, 0.0);
			const double AS = FMath::Max(SideWidth, 0.0);
			const double Z = FMath::Max(ApronThickness, 0.0);
			if (Z <= 0.0 || X1 <= X0 || Y1 <= Y0) return;
			if (AF <= 0.0 && AR <= 0.0 && AS <= 0.0) return;

			const int32 FirstTri = Mesh.MaxTriangleID();

			// 散水 is scatter-water and the fall is the point.
			const double CX0 = X0 - AS;
			const double CX1 = X1 + AS;
			if (AF > 0.0)
			{
				AppendTriPrism(Mesh, FVector3d(CX0, Y0 - AF, 0.0), CX1 - CX0, AF, Z,
					EAxis2D::X, /*ApexFraction*/ 1.0);
			}
			if (AR > 0.0)
			{
				AppendTriPrism(Mesh, FVector3d(CX0, Y1, 0.0), CX1 - CX0, AR, Z,
					EAxis2D::X, /*ApexFraction*/ 0.0);
			}
			if (AS > 0.0)
			{
				AppendTriPrism(Mesh, FVector3d(X0 - AS, Y0, 0.0), Y1 - Y0, AS, Z,
					EAxis2D::Y, /*ApexFraction*/ 1.0);
				AppendTriPrism(Mesh, FVector3d(X1, Y0, 0.0), Y1 - Y0, AS, Z,
					EAxis2D::Y, /*ApexFraction*/ 0.0);
			}

			SetMaterialIDForTrianglesFrom(Mesh, FirstTri, MatSlot_BaseCourse);
		}

		void AppendPlatform(
			FDynamicMesh3& Mesh,
			double Width, double Depth, double FloorHeight,
			double FrontOverhang, double SideProjection,
			int32 StepCount, double StepTread, double StepX0, double StepX1)
		{
			// 階條石 and 踏跺 are stone, and this function owns saying so — a 臺基 left untagged comes out the colour of brick.
			const int32 StoneFirstTri = Mesh.MaxTriangleID();
			if (FloorHeight <= 0.0)
			{
				return;
			}

			AppendBox(Mesh,
				FVector3d(-SideProjection, -FrontOverhang, 0.0),
				FVector3d(Width + SideProjection, Depth + SideProjection, FloorHeight));

			AppendSteps(Mesh, StepX0, StepX1, -FrontOverhang, -1.0,
				StepCount, StepTread, FloorHeight);
			SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);
		}

		void AppendBaseCourseU(
			FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double CourseHeight, double Projection,
			bool bIncludeRear, bool bToGround)
		{
			if (CourseHeight <= 0.0 || Projection <= 0.0)
			{
				return;
			}

			const double T = WallThickness;
			const double Top = FloorHeight + CourseHeight;
			// Only the bottom drops, so a caller lining several buildings' bands up is unaffected.
			const double Bottom = bToGround ? 0.0 : FloorHeight;

			// Left, right, then the back between them — tiling, never overlapping.
			AppendBox(Mesh,
				FVector3d(-Projection, 0.0, Bottom),
				FVector3d(T, Depth + Projection, Top));
			AppendBox(Mesh,
				FVector3d(Width - T, 0.0, Bottom),
				FVector3d(Width + Projection, Depth + Projection, Top));
			if (bIncludeRear)
			{
				AppendBox(Mesh,
					FVector3d(T, Depth - T, Bottom),
					FVector3d(Width - T, Depth + Projection, Top));
			}
		}

		void AppendChitou(
			FDynamicMesh3& Mesh,
			double Width, double WallThickness,
			double FloorHeight, double EaveHeight,
			double Projection, int32 CorbelSteps)
		{
			if (Projection <= 0.0)
			{
				return;
			}

			const int32 Steps = FMath::Clamp(CorbelSteps, 0, 10);
			const double CourseH = Projection;
			const double StepOut = (Steps > 0) ? Projection / Steps : 0.0;
			const double CorbelBase = FMath::Max(EaveHeight - Steps * CourseH, FloorHeight);

			// Left pier spans the left wall's thickness, right pier the right wall's.
			const double PierX[2][2] = { { 0.0, WallThickness }, { Width - WallThickness, Width } };
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const double X0 = PierX[Side][0], X1 = PierX[Side][1];

				AppendBox(Mesh,
					FVector3d(X0, -Projection, FloorHeight),
					FVector3d(X1, 0.0,         CorbelBase));

				for (int32 i = 0; i < Steps; ++i)
				{
					const double Proj = Projection + (i + 1) * StepOut;
					AppendBox(Mesh,
						FVector3d(X0, -Proj, CorbelBase + i * CourseH),
						FVector3d(X1, 0.0,   CorbelBase + (i + 1) * CourseH));
				}
			}
		}

		void AppendRoundRafterEnds(
			FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double CentreZ, double Diameter, double Spacing, int32 Sides)
		{
			const double D = FMath::Max(Diameter, 0.0);
			if (D <= 0.0 || X1 - X0 <= D || Y1 <= Y0) return;

			// Same spacing rule as the square row.
			const double Pitch = FMath::Max(Spacing, D * 1.5);
			const double Span = X1 - X0;
			const int32 Count = FMath::Max(FMath::FloorToInt32(Span / Pitch), 1);
			const double Used = Count * Pitch;
			const double Start = X0 + 0.5 * (Span - Used) + 0.5 * Pitch;

			// AppendCylinder only builds up Z, so each is raised at the origin and tipped over.
			const FQuat Lay = FQuat::FindBetweenNormals(FVector::UpVector, FVector(0.0, -1.0, 0.0));
			for (int32 i = 0; i < Count; ++i)
			{
				const double CX = Start + i * Pitch;
				if (CX - 0.5 * D < X0 || CX + 0.5 * D > X1) continue;

				const int32 Mark = Mesh.MaxVertexID();
				AppendCylinder(Mesh, FVector3d::Zero(), 0.5 * D, Y1 - Y0, FMath::Max(Sides, 5));
				TransformVerticesFrom(Mesh, Mark,
					FTransform(Lay, FVector(CX, Y1, CentreZ)));
			}
		}

		void AppendRafterEnds(
			FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double TopZ, double Section, double Spacing)
		{
			const double S = FMath::Max(Section, 0.0);
			if (S <= 0.0 || X1 - X0 <= S || Y1 <= Y0)
			{
				return;
			}

			// Never let the spacing fall below the section, or the rafters merge into a solid band.
			const double Pitch = FMath::Max(Spacing, S * 1.5);
			const double Span = X1 - X0;
			const int32 Count = FMath::Max(FMath::FloorToInt32(Span / Pitch), 1);

			// Centred on the span, so the row is symmetrical about the middle of the eave.
			const double Used = Count * Pitch;
			const double Start = X0 + 0.5 * (Span - Used) + 0.5 * Pitch;

			for (int32 i = 0; i < Count; ++i)
			{
				const double CX = Start + i * Pitch;
				if (CX - 0.5 * S < X0 || CX + 0.5 * S > X1) continue;
				AppendBox(Mesh,
					FVector3d(CX - 0.5 * S, Y0, TopZ - S),
					FVector3d(CX + 0.5 * S, Y1, TopZ));
			}
		}

		// Shared by AppendGableRoof and RearEaveLift so the two cannot disagree about where the back of the roof is.
		static void RoofSpan(
			const FRoofParams& Roof, double Depth,
			double& OutFrontO, double& OutRearO, double& OutBuilt, double& OutNominal)
		{
			OutFrontO = FMath::Max(Roof.FrontOverhang, 0.0);
			OutRearO = FMath::Max(Roof.RearOverhang, 0.0);
			OutBuilt = Depth + OutFrontO + OutRearO;
			OutNominal = OutBuilt + FMath::Clamp(Roof.RearSlopeTrim, 0.0, OutBuilt);
		}

		double RoofRise(const FRoofParams& Roof)
		{
			return (Roof.Rise > 0.0) ? Roof.Rise : FMath::Max(Roof.Section.Rise(), 1.0);
		}

		double RearEaveLift(const FRoofParams& Roof, double Depth)
		{
			double O, RearO, Built, Nominal;
			RoofSpan(Roof, Depth, O, RearO, Built, Nominal);
			if (Nominal <= Built)
			{
				return 0.0;
			}

			const double DistanceFromRidge = FMath::Abs(2.0 * Built / Nominal - 1.0);
			return RoofRise(Roof) * Roof.Section.HeightFraction(DistanceFromRidge);
		}

		namespace
		{
			// 勾頭: the round cap closing each 壟, laid along one eave at the tile pitch.
			void AppendEaveCaps(FDynamicMesh3& Mesh, const FRoofParams& Roof,
				double GX0, double GX1, double YFace, double ZLine, double FasciaDrop, double Direction)
			{
				if (!RoofTile::HasEaveCaps(Roof.Tile)) return;

				const double Pitch = FMath::Max(Roof.TileRowSpacing, 4.0);
				const double CapR = FMath::Clamp(0.34 * Pitch, 2.0, 0.9 * FasciaDrop);
				const int32 Count = FMath::Clamp(FMath::FloorToInt32((GX1 - GX0) / Pitch), 1, 400);
				// Centred on the run, so the row is symmetrical about the building.
				const double Span = Count * Pitch;
				const double Start = 0.5 * (GX0 + GX1) - 0.5 * Span + 0.5 * Pitch;
				// Sunk back into the fascia by part of its radius.
				const double CapLen = FMath::Max(0.8 * CapR, 1.0);
				const double CapZ = ZLine - 0.5 * FasciaDrop;

				const FQuat Lay = FQuat::FindBetweenNormals(
					FVector::UpVector, FVector(0.0, Direction, 0.0));

				for (int32 i = 0; i < Count; ++i)
				{
					const int32 Mark = Mesh.MaxVertexID();
					AppendCylinder(Mesh, FVector3d::Zero(), CapR, CapLen, HutongGen::RoofTile::EaveCapSides);
					TransformVerticesFrom(Mesh, Mark,
						FTransform(Lay, FVector(Start + i * Pitch, YFace - Direction * 0.4 * CapR, CapZ)));
				}
			}

			// 蠍子尾: the ridge's end curling upward past the gable.
			void AppendRidgeTail(FDynamicMesh3& Mesh, double GableX, double Dir,
				double RidgeY, double ZStart, double Kick, double Out, double Inset,
				double TailW, double TailD)
			{
				const int32 Steps = 7;
				TArray<FTransform> Stations;
				Stations.Reserve(Steps);
				for (int32 i = 0; i < Steps; ++i)
				{
					const double T = (double)i / (double)(Steps - 1);
					const double A = T * HALF_PI;
					const double X = GableX + Dir * (-Inset + (Inset + Out) * FMath::Sin(A));
					const double Z = ZStart + Kick * (1.0 - FMath::Cos(A));

					// Tangent turns from along the ridge to straight up over the run, taken analytically so the first station leaves the ridge exactly horizontal.
					const FVector Tangent(Dir * FMath::Cos(A), 0.0, FMath::Sin(A));
					const FQuat Rot = FRotationMatrix::MakeFromZX(
						Tangent, FVector(0.0, 1.0, 0.0)).ToQuat();

					// Full section at the root, closing toward the tip but never to zero.
					const double S = FMath::Lerp(1.0, 0.32, T * T);
					Stations.Add(FTransform(Rot, FVector(X, RidgeY, Z), FVector(S, S, 1.0)));
				}

				const TArray<FVector2d> Blade = {
					FVector2d(-0.5 * TailW, -0.5 * TailD), FVector2d(0.5 * TailW, -0.5 * TailD),
					FVector2d(0.5 * TailW,   0.5 * TailD), FVector2d(-0.5 * TailW, 0.5 * TailD) };
				AppendSweptProfile(Mesh, Blade, Stations);
			}
		}

		void AppendGableRoof(
			FDynamicMesh3& Mesh,
			double Width, double Depth, double EaveHeight,
			const FRoofParams& Roof)
		{
			const double W = Width;
			const double D = Depth;
			const double Eave = EaveHeight;
			const double Rise = RoofRise(Roof);

			double O, RearO, Built, Nominal;
			RoofSpan(Roof, D, O, RearO, Built, Nominal);

			// 懸山: the roof runs past both gables instead of stopping on them, and everything hung off the eaves follows it out.
			const double G = FMath::Max(Roof.GableOverhang, 0.0);
			const double GX0 = -G;
			const double GX1 = W + G;

			// Where the rear slope terminates. With no trim this is the eave line.
			const double RearEaveZ = Eave + RearEaveLift(Roof, D);

			// Marked here so everything below lands in the roof slot, ridge included.
			const int32 RoofFirstTri = Mesh.MaxTriangleID();

			UE::Geometry::FIndex2i GableFaces;
			AppendCurvedGableRoof(Mesh,
				FVector3d(GX0, -O, Eave),
				GX1 - GX0,
				Nominal,
				Rise,
				Roof.Section,
				Roof.SlopeSegments,
				EAxis2D::X,
				Nominal - Built,
				HutongGen::RoofTile::DefaultRowSpacing,
				&GableFaces);

			// The eave course: 花邊瓦 over 滴水 on 合瓦, the same band plus a row of 勾頭 on 筒瓦.
			const double FasciaDrop = FMath::Max(Roof.FasciaDepth, 0.0);
			const double FasciaW = FMath::Max(Roof.FasciaWidth, 0.0);
			if (FasciaDrop > 0.0 && FasciaW > 0.0)
			{
				AppendBox(Mesh,
					FVector3d(GX0, -O,           Eave - FasciaDrop),
					FVector3d(GX1, -O + FasciaW, Eave + 0.5 * FasciaDrop));
				AppendEaveCaps(Mesh, Roof, GX0, GX1, -O, Eave, FasciaDrop, -1.0);

				// A sealed rear eave has no drip course.
				if (RearO >= FasciaW)
				{
					AppendBox(Mesh,
						FVector3d(GX0, D + RearO - FasciaW, RearEaveZ - FasciaDrop),
						FVector3d(GX1, D + RearO,           RearEaveZ + 0.5 * FasciaDrop));
					AppendEaveCaps(Mesh, Roof, GX0, GX1, D + RearO, RearEaveZ, FasciaDrop, 1.0);
				}
			}

			const double RcH = FMath::Max(Roof.RidgeCourseHeight, 0.0);
			const double RcW = FMath::Max(Roof.RidgeCourseWidth, 0.0);
			if (Roof.bHasRidgeCourse && RcH > 0.0 && RcW > 0.0)
			{
				const double ApexZ = Eave + Rise;
				// The apex is the middle of the span the profile is developed over.
				const double RidgeY = -O + 0.5 * Nominal;

				// Sunk by its own width so the underside stays buried in the slopes however steep the roof is; the buried faces are interior and get culled.
				AppendBox(Mesh,
					FVector3d(GX0, RidgeY - 0.5 * RcW, ApexZ - RcW),
					FVector3d(GX1, RidgeY + 0.5 * RcW, ApexZ + RcH));

				const double Kick = FMath::Max(Roof.RidgeEndKick, 0.0);
				if (Kick > 0.0)
				{
					const double Out = 0.55 * Kick;
					const double TailW = 0.8 * RcW;          // thinner than the ridge it grows from
					const double TailD = FMath::Max(0.9 * RcH, 8.0);
					// Started inside the ridge box so the blade's base is buried in it.
					const double Inset = 0.4 * RcW;
					const double ZStart = ApexZ + 0.45 * RcH;

					AppendRidgeTail(Mesh, GX0, -1.0, RidgeY, ZStart, Kick, Out, Inset, TailW, TailD);
					AppendRidgeTail(Mesh, GX1,  1.0, RidgeY, ZStart, Kick, Out, Inset, TailW, TailD);
				}
			}

			SetMaterialIDForTrianglesFrom(Mesh, RoofFirstTri, MatSlot_Roof);

			// 硬山: the roof stops on the gable, and the face closing it is the 山牆 pediment, brick
			// up to the ridge. 懸山 runs past the gable and that face is the open end of the roof.
			if (G <= 0.0 && GableFaces.B > GableFaces.A)
			{
				SetMaterialIDForTriangleRange(Mesh, GableFaces.A, GableFaces.B, MatSlot_Body);
			}

			// 椽頭 last and tagged separately.
			const double RSec = FMath::Max(Roof.RafterSection, 0.0);
			if (RSec > 0.0)
			{
				const int32 RafterFirstTri = Mesh.MaxTriangleID();
				// Tops pushed up inside the drip course above them, so no face is left coplanar with its underside.
				const double TopZ = Eave - 0.5 * FasciaDrop;
				const double Reach = 3.0 * RSec;

				// And set back behind the drip course's face.
				const double Setback = (FasciaDrop > 0.0 && FasciaW > 0.0)
					? FMath::Min(0.5 * FasciaW, 0.4 * Reach)
					: 0.0;

				// Two courses, which is what an eave shows.
				if (Roof.bFlyingRafters)
				{
					AppendRafterEnds(Mesh, GX0, GX1, -O + Setback, -O + Reach,
						TopZ, RSec, Roof.RafterSpacing);
				}

				// The round course sits back and down from the flying one so the two read as a pair, its top buried in the 飛椽 above.
				const double EaveRafterOut = Roof.bFlyingRafters ? (Reach + 0.55 * RSec) : Reach;
				const double EaveRafterIn = Roof.bFlyingRafters ? (Setback + 0.5 * Reach) : Setback;
				const double RoundDrop = Roof.bFlyingRafters ? (1.15 * RSec) : (0.5 * RSec);
				AppendRoundRafterEnds(Mesh, GX0, GX1,
					-O + EaveRafterIn, -O + EaveRafterOut,
					TopZ - RoundDrop, 1.05 * RSec, Roof.RafterSpacing);

				// A sealed rear eave shows brickwork, not rafters — the same test the drip course uses, for the same reason.
				if (RearO >= FasciaW && RearO > Reach)
				{
					const double RearTopZ = RearEaveZ - 0.5 * FasciaDrop;
					if (Roof.bFlyingRafters)
					{
						AppendRafterEnds(Mesh, GX0, GX1, D + RearO - Reach, D + RearO - Setback,
							RearTopZ, RSec, Roof.RafterSpacing);
					}
					AppendRoundRafterEnds(Mesh, GX0, GX1,
						D + RearO - EaveRafterOut, D + RearO - EaveRafterIn,
						RearTopZ - RoundDrop, 1.05 * RSec, Roof.RafterSpacing);
				}

				SetMaterialIDForTrianglesFrom(Mesh, RafterFirstTri, MatSlot_Wood);
			}
		}
	}
}
