#include "Generation/HutongShell.h"
#include "Generation/HutongCanon.h"
#include "Algo/Reverse.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
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
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
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
			StoneTag.Close();
		}

		void AppendPlatform(
			FDynamicMesh3& Mesh,
			double Width, double Depth, double FloorHeight,
			double FrontOverhang, double SideProjection,
			int32 StepCount, double StepTread, double StepX0, double StepX1, bool bPaved)
		{
			// Tags 階條石 and 踏跺 as stone here; untagged they read as brick.
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			if (FloorHeight <= 0.0)
			{
				return;
			}

			const FVector3d Lo(-SideProjection, -FrontOverhang, 0.0);
			const FVector3d Hi(Width + SideProjection, Depth + SideProjection, FloorHeight);
			// Paved: 階條石 round the edge and 方磚 inside, flush, over a stone body a course lower.
			const double Band = FMath::Min(PlatformEdgeCm, 0.2 * FMath::Min(Hi.X - Lo.X, Hi.Y - Lo.Y));
			const double Course = FMath::Min(4.0, 0.5 * FloorHeight);
			if (!bPaved || Band <= 1.0)
			{
				AppendBox(Mesh, Lo, Hi);
			}
			else
			{
				const double Z = FloorHeight - Course;
				AppendBox(Mesh, Lo, FVector3d(Hi.X, Hi.Y, Z));
				AppendBox(Mesh, FVector3d(Lo.X, Lo.Y, Z), FVector3d(Hi.X, Lo.Y + Band, Hi.Z));
				AppendBox(Mesh, FVector3d(Lo.X, Hi.Y - Band, Z), FVector3d(Hi.X, Hi.Y, Hi.Z));
				AppendBox(Mesh, FVector3d(Lo.X, Lo.Y + Band, Z), FVector3d(Lo.X + Band, Hi.Y - Band, Hi.Z));
				AppendBox(Mesh, FVector3d(Hi.X - Band, Lo.Y + Band, Z), FVector3d(Hi.X, Hi.Y - Band, Hi.Z));
				FSlotScope FloorTag(Mesh, MatSlot_Floor);
				AppendBox(Mesh, FVector3d(Lo.X + Band, Lo.Y + Band, Z), FVector3d(Hi.X - Band, Hi.Y - Band, Hi.Z));
			}

			AppendSteps(Mesh, StepX0, StepX1, -FrontOverhang, -1.0,
				StepCount, StepTread, FloorHeight);
			StoneTag.Close();
		}

		void AppendBaseCourseU(
			FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double CourseHeight, double Projection,
			bool bIncludeRear, bool bToGround, double SideGapY0, double SideGapY1, double RearWallThickness)
		{
			if (CourseHeight <= 0.0 || Projection <= 0.0)
			{
				return;
			}

			const double T = WallThickness;
			const double Top = FloorHeight + CourseHeight;
			// Only the bottom drops, so bands aligned across buildings stay aligned.
			const double Bottom = bToGround ? 0.0 : FloorHeight;

			// Left, right, then back between them: tiling, never overlapping.
			const bool bGap = SideGapY1 > SideGapY0 && SideGapY0 > 0.0 && SideGapY1 < Depth;
			auto Side = [&](double X0, double X1)
			{
				if (bGap)
				{
					AppendBox(Mesh, FVector3d(X0, 0.0, Bottom), FVector3d(X1, SideGapY0, Top));
					AppendBox(Mesh, FVector3d(X0, SideGapY1, Bottom), FVector3d(X1, Depth + Projection, Top));
				}
				else
				{
					AppendBox(Mesh, FVector3d(X0, 0.0, Bottom), FVector3d(X1, Depth + Projection, Top));
				}
			};
			Side(-Projection, T);
			Side(Width - T, Width + Projection);
			if (bIncludeRear)
			{
				const double TR = (RearWallThickness > 0.0) ? RearWallThickness : T;
				AppendBox(Mesh,
					FVector3d(T, Depth - TR, Bottom),
					FVector3d(Width - T, Depth + Projection, Top));
			}
		}

		static void RoofSpan(
			const FRoofParams& Roof, double Depth,
			double& OutFrontO, double& OutRearO, double& OutBuilt, double& OutNominal);

		double ChitouBodyProjection(double Projection, double Overhang)
		{
			return FMath::Max(Projection, 0.5 * FMath::Max(Overhang, 0.0));
		}

		void AppendChitou(
			FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double EaveHeight,
			double Projection, int32 CorbelSteps, const FRoofParams& Roof,
			double BaseCourseHeight, double BaseCourseProjection)
		{
			if (Projection <= 0.0)
			{
				return;
			}

			// 上身 from the ground (past the platform's edge it stood on nothing), its foot in the 下鹼,
			// proud on the outer side and the front as the wall's is.
			const double BaseTop = (BaseCourseHeight > 0.0) ? FloorHeight + BaseCourseHeight : 0.0;
			const double BaseP = FMath::Max(BaseCourseProjection, 0.0);
			auto Pier = [&](double X0, double X1, double Front, bool bOuterLow, double Top)
			{
				if (BaseTop > 0.0)
				{
					FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
					AppendBox(Mesh, FVector3d(bOuterLow ? X0 - BaseP : X0, Front - BaseP, 0.0),
						FVector3d(bOuterLow ? X1 : X1 + BaseP, 0.0, BaseTop));
				}
				AppendBox(Mesh, FVector3d(X0, Front, BaseTop), FVector3d(X1, 0.0, Top));
			};

			double O, RearO, Built, Nominal;
			RoofSpan(Roof, Depth, O, RearO, Built, Nominal);
			const double FD = FMath::Max(Roof.FasciaDepth, 0.0);
			const double FW = FMath::Max(Roof.FasciaWidth, 0.0);
			const bool bFascia = FD > 0.0 && FW > 0.0;
			const double T = WallThickness;
			const int32 Steps = FMath::Clamp(FMath::Max(CorbelSteps, 4), 1, 10);

			// Pair at the Y = 0 eave with overhang Over; the rear pair is this turned round, so its low end is built
			// where the roof's high end will be (bTurned).
			auto AppendPair = [&](double Over, bool bTurned)
			{
				// Open end: no corner to close.
				auto IsOpen = [&](bool bRight) { return (bRight != bTurned) ? Roof.bOpenHighEnd : Roof.bOpenLowEnd; };
				const double Reach = Over - 1.0;
				const double Body = ChitouBodyProjection(Projection, Over);
				const double TopZ = bFascia ? EaveHeight - FD : EaveHeight;
				// 圖5-3-6.2: 盤頭 corbels out most of the rest, rise = reach per course; 戧檐 a short steep lean under the 博縫 foot.
				const double Lean = 0.12 * Over;
				const double Pan = Reach - Body - Lean;
				if (Pan < 2.0 * Steps)
				{
					if (!IsOpen(false)) Pier(0.0, T, -Body, true, EaveHeight);
					if (!IsOpen(true)) Pier(Width - T, Width, -Body, false, EaveHeight);
					return;
				}
				const double CourseH = FMath::Max(Pan / Steps, 6.0);
				const double QiangZ = FMath::Max(TopZ - 2.5 * Lean, FloorHeight + 40.0);
				const double PanZ = FMath::Max(QiangZ - Steps * CourseH, FloorHeight + 20.0);
				const double StepH = (QiangZ - PanZ) / Steps;

				for (const bool bRight : { false, true })
				{
					if (IsOpen(bRight)) continue;
					// Flush with the gable face, so nothing pokes past a boundary wall.
					const double X0 = bRight ? Width - T : 0.0;
					const double X1 = bRight ? Width : T;

					Pier(X0, X1, -Body, !bRight, PanZ);

					for (int32 k = 1; k <= Steps; ++k)
					{
						AppendBox(Mesh,
							FVector3d(X0, -(Body + Pan * k / Steps), PanZ + (k - 1) * StepH),
							FVector3d(X1, 0.0, PanZ + k * StepH));
					}

					// 戧檐: leans from the top 盤頭 course to just behind the 連檐 face, then plumb to the soffit.
					// Buries the 連檐's end; stepped under it, the soffit showed.
					const TArray<FVector2d> Section = {
						FVector2d(0.0, QiangZ), FVector2d(-(Body + Pan), QiangZ),
						FVector2d(-Reach, TopZ), FVector2d(-Reach, EaveHeight),
						// Up with the soffit to the column line, where the ceiling stands above the eave line.
						FVector2d(0.0, EaveHeight + FMath::Max(Roof.UndersideRise, 0.0)) };
					// Swept along X: local X→Y, Y→Z, Z→X. Just inside the gable plane, where the roof's end
					// closes its soffit.
					const FQuat AlongX(FMatrix(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector));
					constexpr double GableClear = 0.5;
					AppendSweptProfile(Mesh, Section, {
						FTransform(AlongX, FVector(X0 + (bRight ? 0.0 : GableClear), 0.0, 0.0)),
						FTransform(AlongX, FVector(X1 - (bRight ? GableClear : 0.0), 0.0, 0.0)) });
				}
			};

			AppendPair(O, false);
			// Rear 墀頭 only where the rear eave shows rafters; a trimmed one ends on the wall.
			if (RearO >= FW && RearO > 3.0 * FMath::Max(Roof.RafterSection, 0.0) && RearEaveLift(Roof, Depth) <= 0.0)
			{
				const int32 V0 = Mesh.MaxVertexID();
				AppendPair(RearO, true);
				YawVerticesFrom(Mesh, V0, FVector2d(0.5 * Width, 0.5 * Depth), 180.0);
			}
		}

		void AppendRoundRafterEnds(
			FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double CentreZ, double Diameter, double Spacing, int32 Sides, double Rise)
		{
			const double D = FMath::Max(Diameter, 0.0);
			if (D <= 0.0 || X1 - X0 <= D || Y1 <= Y0) return;

			// Same spacing rule as the square row.
			const double Pitch = FMath::Max(Spacing, D * 1.5);
			const double Span = X1 - X0;
			const int32 Count = FMath::Max(FMath::FloorToInt32(Span / Pitch), 1);
			const double Used = Count * Pitch;
			const double Start = X0 + 0.5 * (Span - Used) + 0.5 * Pitch;

			// AppendCylinder builds along Z: build at origin, then lay it from the Y1 end down to Y0.
			const FVector Axis(0.0, -(Y1 - Y0), -Rise);
			const FQuat Lay = FQuat::FindBetweenNormals(FVector::UpVector, Axis.GetSafeNormal());
			for (int32 i = 0; i < Count; ++i)
			{
				const double CX = Start + i * Pitch;
				if (CX - 0.5 * D < X0 || CX + 0.5 * D > X1) continue;

				const int32 Mark = Mesh.MaxVertexID();
				AppendCylinder(Mesh, FVector3d::Zero(), 0.5 * D, Axis.Length(), FMath::Max(Sides, 5));
				TransformVerticesFrom(Mesh, Mark,
					FTransform(Lay, FVector(CX, Y1, CentreZ + Rise)));
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

			// Spacing ≥ section, or rafters merge into a band.
			const double Pitch = FMath::Max(Spacing, S * 1.5);
			const double Span = X1 - X0;
			const int32 Count = FMath::Max(FMath::FloorToInt32(Span / Pitch), 1);

			// Centred on the span.
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

		// Shared by AppendGableRoof and RearEaveLift so they agree on the rear slope's end.
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
			// 壟 positions along the ridge; 勾頭, swept courses and slope texture all read this so they align.
			struct FTileRows
			{
				double Pitch = 20.0;
				// First 壟 centre; rows symmetrical about the building.
				double Start = 0.0;
				int32 Count = 0;

				double X(int32 i) const { return Start + i * Pitch; }
				// Pattern draws a 壟 at each half-unit: u = 0.5 is the first.
				float U(double WorldX) const { return (float)((WorldX - Start) / Pitch + 0.5); }
			};

			FTileRows TileRows(const FRoofParams& Roof, double GX0, double GX1)
			{
				FTileRows R;
				R.Pitch = FMath::Max(Roof.TileRowSpacing, 4.0);
				const bool bClosedBoth = !Roof.bOpenLowEnd && !Roof.bOpenHighEnd;
				if (bClosedBoth && !Roof.bHasTileRowPhase)
				{
					R.Count = FMath::Clamp(FMath::FloorToInt32((GX1 - GX0) / R.Pitch), 1, 400);
					R.Start = 0.5 * (GX0 + GX1) - 0.5 * R.Count * R.Pitch + 0.5 * R.Pitch;
					return R;
				}
				// Closed end keeps half a row clear; open end lets rows run on.
				const double Lo = GX0 + (Roof.bOpenLowEnd ? 0.0 : 0.5 * R.Pitch);
				const double Hi = GX1 - (Roof.bOpenHighEnd ? 0.01 : 0.5 * R.Pitch);
				if (Roof.bHasTileRowPhase)
				{
					R.Start = Roof.TileRowPhase + FMath::CeilToDouble((Lo - Roof.TileRowPhase) / R.Pitch - 1e-6) * R.Pitch;
				}
				else if (!Roof.bOpenLowEnd)
				{
					R.Start = Lo;
				}
				else
				{
					R.Start = Hi - FMath::FloorToDouble((Hi - Lo) / R.Pitch) * R.Pitch;
				}
				R.Count = (Hi >= R.Start) ? FMath::Clamp(FMath::FloorToInt32((Hi - R.Start) / R.Pitch + 1e-6) + 1, 0, 400) : 0;
				return R;
			}

			// 筒瓦 radius = ¼ row: a 三號筒瓦 ~10 cm on a 20 cm 壟, 板瓦 channel as wide between. 勾頭 = its end, inside the eave course.
			double TubeRadius(double Pitch, double FasciaDrop)
			{
				const double R = FMath::Max(0.25 * Pitch, 2.0);
				return FasciaDrop > 0.0 ? FMath::Min(R, FMath::Max(0.9 * FasciaDrop, 2.0)) : R;
			}

			// 勾頭: round cap closing each 壟, along one eave at the tile pitch.
			void AppendEaveCaps(FDynamicMesh3& Mesh, const FRoofParams& Roof,
				double GX0, double GX1, double YFace, double ZLine, double FasciaDrop, double Direction)
			{
				if (!RoofTile::HasEaveCaps(Roof.Tile)) return;

				const FTileRows Rows = TileRows(Roof, GX0, GX1);
				const double CapR = TubeRadius(Rows.Pitch, FasciaDrop);
				// Sunk into the fascia by part of its radius.
				const double CapLen = FMath::Max(0.8 * CapR, 1.0);
				const double CapZ = ZLine - 0.5 * FasciaDrop;

				const FQuat Lay = FQuat::FindBetweenNormals(
					FVector::UpVector, FVector(0.0, Direction, 0.0));

				for (int32 i = 0; i < Rows.Count; ++i)
				{
					const int32 Mark = Mesh.MaxVertexID();
					AppendCylinder(Mesh, FVector3d::Zero(), CapR, CapLen, HutongGen::RoofTile::EaveCapSides);
					TransformVerticesFrom(Mesh, Mark,
						FTransform(Lay, FVector(Rows.X(i), YFace - Direction * 0.4 * CapR, CapZ)));
				}
			}

			// Stations along a gable profile at one X: local Z = tangent, Y = upward slope normal, X = -X
			// front-to-back, +X back-to-front.
			TArray<FTransform> ProfileStations(const TArray<FVector2d>& Profile,
				double X, double Y0, double Z0, bool bReverse)
			{
				TArray<FVector> Points;
				for (const FVector2d& S : Profile) Points.Add(FVector(X, Y0 + S.X, Z0 + S.Y));
				if (bReverse) Algo::Reverse(Points);
				TArray<FTransform> Out;
				for (int32 i = 0; i < Points.Num(); ++i)
				{
					const FVector Prev = Points[FMath::Max(i - 1, 0)];
					const FVector Next = Points[FMath::Min(i + 1, Points.Num() - 1)];
					const FVector T = (Next - Prev).GetSafeNormal();
					// Upward slope normal in the section plane.
					FVector Up(0.0, -T.Z, T.Y);
					if (Up.Z < 0.0) Up = -Up;
					Out.Add(FTransform(FRotationMatrix::MakeFromZY(T, Up).ToQuat(), Points[i]));
				}
				return Out;
			}

			// Point against a profile from (Y0, Z0): arc length from the front eave to its foot, and depth below.
			struct FProfileSpot { double S = 0.0; double Below = 0.0; FVector2d Tangent = FVector2d(1.0, 0.0); };

			// Face orientation vs the profile: toward a gable, off the slope, or down the run (end cap).
			enum class EProfileFace { Gable, Slope, End };

			FProfileSpot SpotOnProfile(const TArray<FVector2d>& Profile, double Y0, double Z0, double Y, double Z)
			{
				FProfileSpot Best;
				double BestDist = TNumericLimits<double>::Max();
				double Run = 0.0;
				const FVector2d P(Y - Y0, Z - Z0);
				for (int32 i = 0; i + 1 < Profile.Num(); ++i)
				{
					const FVector2d AB = Profile[i + 1] - Profile[i];
					const double L = AB.Length();
					if (L <= UE_SMALL_NUMBER) continue;
					const FVector2d Dir = AB / L;
					const FVector2d AP = P - Profile[i];
					const double T = FMath::Clamp(AP.Dot(Dir), 0.0, L);
					const double Dist = (AP - Dir * T).Length();
					if (Dist < BestDist)
					{
						BestDist = Dist;
						// Profile runs front to back: below is to its right.
						const double Side = Dir.X * AP.Y - Dir.Y * AP.X;
						Best.S = Run + T;
						Best.Below = Side < 0.0 ? Dist : -Dist;
						Best.Tangent = Dir;
					}
					Run += L;
				}
				return Best;
			}

			// Per-triangle UVs since FirstTri, from each corner's profile position and the face orientation.
			void MapAlongProfile(FDynamicMesh3& Mesh, int32 FirstTri,
				const TArray<FVector2d>& Profile, double Y0, double Z0,
				TFunctionRef<FVector2f(double X, const FProfileSpot& Spot, EProfileFace Face)> UVOf)
			{
				EnsureUVLayer(Mesh);
				UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
				if (!UV) return;
				for (int32 tid = FirstTri; tid < Mesh.MaxTriangleID(); ++tid)
				{
					if (!Mesh.IsTriangle(tid)) continue;
					const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
					const FVector3d P[3] = { Mesh.GetVertex(T.A), Mesh.GetVertex(T.B), Mesh.GetVertex(T.C) };
					const FVector3d N = (P[1] - P[0]).Cross(P[2] - P[0]).GetSafeNormal();
					const FVector3d C = (P[0] + P[1] + P[2]) / 3.0;
					const FVector2d T2 = SpotOnProfile(Profile, Y0, Z0, C.Y, C.Z).Tangent;
					const double Along = FMath::Abs(N.Y * T2.X + N.Z * T2.Y);
					const double Across = FMath::Abs(N.X);
					const double Off = FMath::Abs(N.Z * T2.X - N.Y * T2.Y);
					const EProfileFace Face = (Along >= Across && Along >= Off) ? EProfileFace::End
						: (Across >= Off ? EProfileFace::Gable : EProfileFace::Slope);
					int32 Elems[3];
					for (int32 k = 0; k < 3; ++k)
					{
						const FProfileSpot Spot = SpotOnProfile(Profile, Y0, Z0, P[k].Y, P[k].Z);
						Elems[k] = UV->AppendElement(UVOf(P[k].X, Spot, Face));
					}
					UV->SetTriangle(tid, UE::Geometry::FIndex3i(Elems[0], Elems[1], Elems[2]));
				}
			}

			// 壟 courses, one per row, front eave over the ridge to rear: half-round 筒瓦, or a low 蓋瓦 arch
			// on 合瓦; channels left to the slope. Feet sunk below the slope (no coplanar faces).
			// One 壟's cross-section, feet sunk below the slope: a half-round 筒瓦, or a low 蓋瓦 arch on 合瓦.
			TArray<FVector2d> TileSection(EHutongRoofTile Tile, double Pitch, double FasciaDrop)
			{
				TArray<FVector2d> Section;
				if (RoofTile::HasEaveCaps(Tile))
				{
					constexpr int32 ArcSteps = 6;
					const double R = TubeRadius(Pitch, FasciaDrop);
					const double Sink = 0.2 * R;
					for (int32 k = 0; k <= ArcSteps; ++k)
					{
						const double A = UE_DOUBLE_PI * k / ArcSteps;
						Section.Add(FVector2d(R * FMath::Cos(A), R * FMath::Sin(A) - Sink));
					}
				}
				else
				{
					// Covers the texture's 壟, as high as a 板瓦's curve; four smooth facets suffice.
					constexpr int32 ArcSteps = 4;
					const double Half = 0.3 * Pitch;
					const double Crown = 0.15 * Pitch;
					for (int32 k = 0; k <= ArcSteps; ++k)
					{
						const double T = 1.0 - 2.0 * k / ArcSteps;
						Section.Add(FVector2d(Half * T, Crown * (1.0 - T * T) - 1.0));
					}
				}
				return Section;
			}

			void AppendTileRuns(FDynamicMesh3& Mesh, const FRoofParams& Roof, const FTileRows& Rows,
				const TArray<FVector2d>& Profile, double Y0, double Z0, double FasciaDrop)
			{
				const TArray<FVector2d> Section = TileSection(Roof.Tile, Rows.Pitch, FasciaDrop);

				const int32 First = Mesh.MaxTriangleID();
				for (int32 i = 0; i < Rows.Count; ++i)
				{
					TArray<FTransform> Stations = ProfileStations(Profile, Rows.X(i), Y0, Z0, false);
					// Plumb cut at the eave like a 滴水; square to the slope it faced down and got retagged as soffit.
					// Slightly proud of the drip course face to avoid coplanarity.
					const FQuat Plumb = FRotationMatrix::MakeFromZY(FVector(0.0, 1.0, 0.0), FVector::UpVector).ToQuat();
					constexpr double NoseProud = 1.0;
					Stations[0].SetRotation(Plumb);
					Stations[0].AddToTranslation(FVector(0.0, -NoseProud, 0.0));
					Stations.Last().SetRotation(Plumb);
					Stations.Last().AddToTranslation(FVector(0.0, NoseProud, 0.0));
					AppendSweptProfile(Mesh, Section, Stations);
				}
				// Samples a narrow band of its own 壟 in the texture (joints and laps, no painted relief). v folded
				// at the apex, as on the slope.
				int32 Apex = 0;
				for (int32 k = 1; k < Profile.Num(); ++k)
				{
					if (Profile[k].Y > Profile[Apex].Y) Apex = k;
				}
				double ApexS = 0.0;
				for (int32 k = 1; k <= Apex; ++k) ApexS += (Profile[k] - Profile[k - 1]).Length();

				const double RowV = 1.0 / HutongGen::RoofTile::DefaultRowSpacing;
				const double Band = RoofTile::HasEaveCaps(Roof.Tile) ? 0.0 : 0.5;
				MapAlongProfile(Mesh, First, Profile, Y0, Z0,
					[&Rows, RowV, Band, ApexS](double X, const FProfileSpot& Spot, EProfileFace Face)
					{
						const int32 Row = FMath::RoundToInt32((X - Rows.Start) / Rows.Pitch);
						const double Across = (X - Rows.X(Row)) / Rows.Pitch;
						const float U = (float)(Row + 0.5 + Band + 0.1 * Across);
						const double Along = ApexS - FMath::Abs(Spot.S - ApexS);
						return FVector2f(U, (float)((Face == EProfileFace::End ? Spot.Below : Along) * RowV));
					});
			}

			// 蠍子尾 (HutongCanon::Roof::Tail*): a straight blade from its root inside the ridge up and out at the lean,
			// its last stretch hooked outward, tapering; the tip TailTipPastEndCm past the ridge end at EndX.
			void AppendRidgeTail(FDynamicMesh3& Mesh, double EndX, double Dir,
				double RidgeY, double RootZ, double Rise, double TailW, double TailD)
			{
				namespace R = HutongCanon::Roof;
				const double Lean = FMath::DegreesToRadians(R::TailLeanDeg);
				const double Hook = FMath::DegreesToRadians(R::TailHookDeg);
				// The blade's direction at a share T of its length: the lean, turning to the hook over its last stretch.
				auto AngleAt = [&](double T)
				{
					return (T <= 1.0 - R::TailHookShare) ? Lean : FMath::Lerp(Lean, Hook, (T - (1.0 - R::TailHookShare)) / R::TailHookShare);
				};
				// Stations along a unit-length path, then scaled so the tip rises Rise and lands past the end.
				const int32 Steps = 6;
				TArray<FVector2d> Path = { FVector2d::ZeroVector };
				for (int32 i = 1; i < Steps; ++i)
				{
					const double A = 0.5 * (AngleAt(double(i - 1) / (Steps - 1)) + AngleAt(double(i) / (Steps - 1)));
					Path.Add(Path.Last() + FVector2d(FMath::Cos(A), FMath::Sin(A)) / (Steps - 1));
				}
				const double Length = Rise / FMath::Max(Path.Last().Y, 1e-3);
				const FVector Root(EndX + Dir * (R::TailTipPastEndCm - Length * Path.Last().X), RidgeY, RootZ);

				TArray<FTransform> Stations;
				for (int32 i = 0; i < Steps; ++i)
				{
					const double T = double(i) / (Steps - 1);
					const double A = AngleAt(T);
					const FVector Tangent(Dir * FMath::Cos(A), 0.0, FMath::Sin(A));
					const FVector P = Root + FVector(Dir * Path[i].X, 0.0, Path[i].Y) * Length;
					const double S = FMath::Lerp(1.0, R::TailTipTaper, T);
					Stations.Add(FTransform(FRotationMatrix::MakeFromZX(Tangent, FVector(0.0, 1.0, 0.0)).ToQuat(), P, FVector(S, S, 1.0)));
				}
				const TArray<FVector2d> Blade = {
					FVector2d(-0.5 * TailW, -0.5 * TailD), FVector2d(0.5 * TailW, -0.5 * TailD),
					FVector2d(0.5 * TailW,   0.5 * TailD), FVector2d(-0.5 * TailW, 0.5 * TailD) };
				AppendSweptProfile(Mesh, Blade, Stations);
			}
		}

		double UndersideAt(const FRoofParams& Roof, double Depth, double RoofBase, double Y)
		{
			const bool bVeranda = Roof.ShellCover <= 0.0 && Roof.VerandaEnd > 0.0 && Y <= Roof.VerandaEnd + 1e-6;
			if (Roof.ShellCover <= 0.0 && !bVeranda) return RoofBase + Roof.UndersideRise;
			double O, RearO, Built, Nominal;
			RoofSpan(Roof, Depth, O, RearO, Built, Nominal);
			const TArray<FVector2d> Profile = GableRoofProfile(Nominal, RoofRise(Roof), Roof.Section, Roof.SlopeSegments, Nominal - Built);
			auto SlopeAt = [&](double C)
			{
				for (int32 i = 0; i + 1 < Profile.Num(); ++i)
				{
					if (C <= Profile[i + 1].X || i + 2 == Profile.Num())
					{
						const double T = FMath::Clamp((C - Profile[i].X) / FMath::Max(Profile[i + 1].X - Profile[i].X, 1e-6), 0.0, 1.0);
						return FMath::Lerp(Profile[i].Y, Profile[i + 1].Y, T);
					}
				}
				return Roof.UndersideRise;
			};
			const double C = O + FMath::Clamp(Y, 0.0, Depth);
			if (bVeranda)
			{
				// The level ceiling's depth under the slope at the column line, carried up the slope.
				const double Level = FMath::Min(Roof.UndersideRise, SlopeAt(O) - 1.0);
				return RoofBase + SlopeAt(C) - (SlopeAt(O) - Level);
			}
			return RoofBase + SlopeAt(C) - Roof.ShellCover;
		}

		void AppendWallToUnderside(FDynamicMesh3& Mesh, const FRoofParams& Roof, double Depth,
			double RoofBase, double X0, double X1, double Y0, double Y1, double BottomZ)
		{
			if ((Roof.ShellCover <= 0.0 && Roof.VerandaEnd <= 0.0) || X1 <= X0 || Y1 <= Y0) return;
			// The underside's creases between the faces, plus the faces; top 1 cm into the shell.
			double O, RearO, Built, Nominal;
			RoofSpan(Roof, Depth, O, RearO, Built, Nominal);
			const TArray<FVector2d> Profile = GableRoofProfile(Nominal, RoofRise(Roof), Roof.Section, Roof.SlopeSegments, Nominal - Built);
			TArray<double> Ys = { Y0 };
			for (const FVector2d& P : Profile)
			{
				const double Y = P.X - O;
				if (Y > Y0 + 0.5 && Y < Y1 - 0.5) Ys.Add(Y);
			}
			Ys.Add(Y1);
			TArray<FVector2d> Section = { FVector2d(Y0, BottomZ), FVector2d(Y1, BottomZ) };
			double Highest = -BIG_NUMBER;
			for (int32 i = Ys.Num() - 1; i >= 0; --i)
			{
				Section.Add(FVector2d(Ys[i], UndersideAt(Roof, Depth, RoofBase, Ys[i]) + 1.0));
				Highest = FMath::Max(Highest, Section.Last().Y);
			}
			if (Highest <= BottomZ + 1.5) return;
			// (Y, Z) → world, swept along X: a rotation, never a mirror.
			const FMatrix Basis(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector);
			FTransform A(Basis), B(Basis);
			A.SetTranslation(FVector(X0, 0, 0));
			B.SetTranslation(FVector(X1, 0, 0));
			AppendSweptProfile(Mesh, Section, { A, B });
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

			// 懸山: roof and eave details run past both gables.
			const double G = FMath::Max(Roof.GableOverhang, 0.0);
			const double GX0 = -G;
			const double GX1 = W + G;

			// Rear slope end; the eave line when untrimmed.
			const double RearEaveZ = Eave + RearEaveLift(Roof, D);

			// Roof slot from here, the 正脊 and soffit stated as they are built.
			const int32 RoofFirstTri = Mesh.MaxTriangleID();
			FSlotScope RoofTag(Mesh, MatSlot_Roof);
			const FTileRows Rows = TileRows(Roof, GX0, GX1);

			// Over rafters, the soffit rises from behind each eave course to the column line, the rooms' ceiling
			// flat between; a coping without rafters sits flat on its corbels.
			const double FasciaDrop = FMath::Max(Roof.FasciaDepth, 0.0);
			const double FasciaW = FMath::Max(Roof.FasciaWidth, 0.0);
			FGableUnderside Under;
			if (Roof.RafterSection > 0.0)
			{
				Under.InnerStart = O;
				Under.InnerEnd = O + D;
				Under.FrontLap = FasciaW;
				// A rear soffit only where the overhang reaches past the eave course, as at the front.
				Under.RearLap = (RearO > FasciaW + 1.0 && RearEaveZ <= Eave + 1.0) ? FasciaW : 0.0;
				Under.Drop = FasciaDrop;
				Under.InnerRise = Roof.UndersideRise;
				Under.ShellCover = Roof.ShellCover;
				if (Roof.VerandaEnd > 0.0) Under.VerandaEnd = O + Roof.VerandaEnd;
			}
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
				&GableFaces,
				Under);
			const int32 SlabEndTri = Mesh.MaxTriangleID();
			// 封護檐: rear slope trimmed to end over the wall, above the eave.
			const bool bSealedRear = RearEaveZ > Eave + 1.0;
			const double SealFace = D + RearO;
			TArrayView<const HutongCanon::Cornice::FCourse> Courses;
			switch (Roof.SealedCornice)
			{
			case EHutongSealedCornice::Rounded:     Courses = HutongCanon::Cornice::Rounded; break;
			case EHutongSealedCornice::Drawer:      Courses = HutongCanon::Cornice::Drawer; break;
			case EHutongSealedCornice::SevenCourse: Courses = HutongCanon::Cornice::SevenCourse; break;
			default:                                Courses = HutongCanon::Cornice::IceTray; break;
			}
			const double SealCornice = FMath::Max(Courses.Last().ProudCm, Courses.Last().OuterCm);
			constexpr double SealLap = 3.0;        // drip course lap past the cornice

			// Slope u in 壟 from the first row, so texture 壟 align with 勾頭 and courses.
			if (UE::Geometry::FDynamicMeshUVOverlay* SlopeUV =
					Mesh.HasAttributes() ? Mesh.Attributes()->PrimaryUV() : nullptr)
			{
				for (int32 tid = RoofFirstTri; tid < GableFaces.A; ++tid)
				{
					if (!Mesh.IsTriangle(tid) || !SlopeUV->IsSetTriangle(tid)) continue;
					const UE::Geometry::FIndex3i E = SlopeUV->GetTriangle(tid);
					for (const int32 Elem : { E.A, E.B, E.C })
					{
						const FVector2f Old = SlopeUV->GetElement(Elem);
						const double X = Mesh.GetVertex(SlopeUV->GetParentVertex(Elem)).X;
						SlopeUV->SetElement(Elem, FVector2f(Rows.U(X), Old.Y));
					}
				}
			}

			// Eave course: 花邊瓦 over 滴水 on 合瓦; plus 勾頭 on 筒瓦.
			// Eave course and ridge stop just inside the gable; on its plane they z-fought the roof's end face.
			constexpr double EndInset = 0.5;
			const double CX0 = GX0 + (Roof.bOpenLowEnd ? 0.0 : EndInset);
			const double CX1 = GX1 - (Roof.bOpenHighEnd ? 0.0 : EndInset);
			TArray<UE::Geometry::FIndex2i> FasciaBoxes;
			if (FasciaDrop > 0.0 && FasciaW > 0.0)
			{
				const int32 BoxFirst = Mesh.MaxTriangleID();
				AppendBox(Mesh,
					FVector3d(CX0, -O,           Eave - FasciaDrop),
					FVector3d(CX1, -O + FasciaW, Eave + 0.5 * FasciaDrop));
				FasciaBoxes.Emplace(BoxFirst, Mesh.MaxTriangleID());
				AppendEaveCaps(Mesh, Roof, GX0, GX1, -O, Eave, FasciaDrop, -1.0);

				// Sealed rear eave: no drip course.
				if (RearO >= FasciaW)
				{
					const int32 RearBoxFirst = Mesh.MaxTriangleID();
					AppendBox(Mesh,
						FVector3d(CX0, D + RearO - FasciaW, RearEaveZ - FasciaDrop),
						FVector3d(CX1, D + RearO,           RearEaveZ + 0.5 * FasciaDrop));
					FasciaBoxes.Emplace(RearBoxFirst, Mesh.MaxTriangleID());
					AppendEaveCaps(Mesh, Roof, GX0, GX1, D + RearO, RearEaveZ, FasciaDrop, 1.0);
				}
				// 封護檐 drip course sits on the cornice and laps past it.
				else if (bSealedRear)
				{
					const double DripFace = SealFace + SealCornice + SealLap;
					AppendBox(Mesh,
						FVector3d(CX0, SealFace,  RearEaveZ - FasciaDrop),
						FVector3d(CX1, DripFace,  RearEaveZ + 0.5 * FasciaDrop));
					AppendEaveCaps(Mesh, Roof, GX0, GX1, DripFace, RearEaveZ, FasciaDrop, 1.0);
				}
			}

			const double RoofY0 = -O;
			const TArray<FVector2d> Profile = GableRoofProfile(Nominal, Rise, Roof.Section, Roof.SlopeSegments, Nominal - Built);
			// The roof's surface at a cross position from the eave edge.
			auto TopAt = [&](double C)
			{
				for (int32 i = 0; i + 1 < Profile.Num(); ++i)
				{
					if (C <= Profile[i + 1].X || i + 2 == Profile.Num())
					{
						const double T = FMath::Clamp((C - Profile[i].X) / FMath::Max(Profile[i + 1].X - Profile[i].X, 1e-6), 0.0, 1.0);
						return Eave + FMath::Lerp(Profile[i].Y, Profile[i + 1].Y, T);
					}
				}
				return Eave;
			};
			if (Roof.bTileRuns && Profile.Num() >= 2)
			{
				AppendTileRuns(Mesh, Roof, Rows, Profile, RoofY0, Eave, FasciaDrop);
			}

			FSlotScope RidgeTag(Mesh, MatSlot_Ridge);
			const double RcH = FMath::Max(Roof.RidgeCourseHeight, 0.0);
			const double RcW = FMath::Max(Roof.RidgeCourseWidth, 0.0);
			if (Roof.bHasRidgeCourse && RcH > 0.0 && RcW > 0.0)
			{
				const double ApexZ = Eave + Rise;
				// Apex = middle of the profile's span.
				const double RidgeY = -O + 0.5 * Nominal;

				// Sunk by its width so the underside stays buried at any pitch; buried faces cull.
				AppendBox(Mesh,
					FVector3d(CX0, RidgeY - 0.5 * RcW, ApexZ - RcW),
					FVector3d(CX1, RidgeY + 0.5 * RcW, ApexZ + RcH));

				const double Kick = FMath::Max(Roof.RidgeEndKick, 0.0);
				if (Kick > 0.0)
				{
					namespace R = HutongCanon::Roof;
					const double TailW = 0.8 * RcW;          // thinner than the ridge
					const double TailD = FMath::Max(0.9 * RcH, 8.0);
					const double BlockTop = ApexZ + RcH * (1.0 + R::EndBlockAboveCourses);
					const double HalfBlock = 0.5 * R::EndBlockWidthShare * RcW;
					const double BlockRun = FMath::Max(Kick / FMath::Tan(FMath::DegreesToRadians(R::TailLeanDeg)), RcW);
					// Root in the 盤子, its section clear of the block's top.
					const double RootZ = BlockTop - 0.5 * TailD;
					for (const bool bHigh : { false, true })
					{
						if (bHigh ? Roof.bOpenHighEnd : Roof.bOpenLowEnd) continue;
						const double EndX = bHigh ? CX1 : CX0;
						const double Dir = bHigh ? 1.0 : -1.0;
						// 盤子 at the ridge end, sunk into the roof like the ridge.
						// 1 cm past the ridge's end, or their end faces share a plane.
						const double BX0 = bHigh ? EndX - BlockRun : EndX - 1.0;
						const double BX1 = bHigh ? EndX + 1.0 : EndX + BlockRun;
						AppendBox(Mesh, FVector3d(BX0, RidgeY - HalfBlock, ApexZ - RcW), FVector3d(BX1, RidgeY + HalfBlock, BlockTop));
						// The tip's top, not its centre line, Kick over the ridge.
						AppendRidgeTail(Mesh, EndX, Dir, RidgeY, RootZ, Kick + (ApexZ + RcH - RootZ) - 0.5 * R::TailTipTaper * TailD, TailW, TailD);
					}
				}
			}

			// 正脊 and 蠍子尾 last: coursed, not tiled.
			RidgeTag.Close();
			RoofTag.Close();

			// 封護檐: back wall carried up to the roof, topped by a four-course 冰盤檐 under the drip course.
			// The trimmed roof end is brick wall face; cornice flush with the gables.
			if (bSealedRear)
			{
				for (int32 tid = RoofFirstTri; tid < SlabEndTri; ++tid)
				{
					if (!Mesh.IsTriangle(tid)) continue;
					// Pre-bake normals point inward: a rear-facing face reads -Y.
					if (Mesh.GetTriNormal(tid).Y < -0.9 && Mesh.GetTriCentroid(tid).Y > SealFace - 0.5)
					{
						SetMaterialIDForTriangleRange(Mesh, tid, tid + 1, MatSlot_Body);
					}
				}
				FSlotScope CorniceTag(Mesh, MatSlot_Body);
				using HutongCanon::Cornice::ECourse;
				const double Top = RearEaveZ - FasciaDrop;
				double Stack = 0.0;
				for (const HutongCanon::Cornice::FCourse& C : Courses) Stack += C.HeightCm;
				// Down the wall face where the roof stops on it; above the eave line where it overhangs.
				const double FloorZ = (RearO > 1.0 || Roof.SealedCorniceFloorZ < -1.0e8) ? Eave : FMath::Min(Roof.SealedCorniceFloorZ, Top);
				const double Scale = FMath::Clamp((Top - FloorZ) / Stack, 3.0 / 6.5, 1.0);
				double CZ = Top - Scale * Stack;
				for (const HutongCanon::Cornice::FCourse& C : Courses)
				{
					const double CZ1 = CZ + Scale * C.HeightCm;
					if (C.Kind == ECourse::Round)
					{
						// Half round across the wall, (Y, Z) swept along X: a rotation, never a mirror.
						TArray<FVector2d> CourseSection = { FVector2d(SealFace - 2.0, CZ) };
						constexpr int32 Arc = 6;
						for (int32 k = 0; k <= Arc; ++k)
						{
							const double Ang = PI * k / Arc;
							CourseSection.Add(FVector2d(SealFace + FMath::Lerp(C.ProudCm, C.OuterCm, FMath::Sin(Ang)), CZ + 0.5 * (CZ1 - CZ) * (1.0 - FMath::Cos(Ang))));
						}
						CourseSection.Add(FVector2d(SealFace - 2.0, CZ1));
						const FMatrix CourseBasis(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector);
						FTransform SweepFrom(CourseBasis), SweepTo(CourseBasis);
						SweepFrom.SetTranslation(FVector(GX0, 0, 0));
						SweepTo.SetTranslation(FVector(GX1, 0, 0));
						AppendSweptProfile(Mesh, CourseSection, { SweepFrom, SweepTo });
					}
					else
					{
						AppendBox(Mesh, FVector3d(GX0, SealFace - 2.0, CZ), FVector3d(GX1, SealFace + C.ProudCm, CZ1));
					}
					if (C.Kind == ECourse::Blocks && C.PitchCm > 0.0)
					{
						const int32 BlockCount = FMath::Max(FMath::FloorToInt32((GX1 - GX0 - C.WidthCm) / C.PitchCm), 0) + 1;
						const double Start = 0.5 * (GX0 + GX1) - 0.5 * ((BlockCount - 1) * C.PitchCm + C.WidthCm);
						for (int32 k = 0; k < BlockCount; ++k)
						{
							const double BX = Start + k * C.PitchCm;
							AppendBox(Mesh, FVector3d(BX, SealFace + C.ProudCm - 1.0, CZ), FVector3d(BX + C.WidthCm, SealFace + C.OuterCm, CZ1));
						}
					}
					CZ = CZ1;
				}
				CorniceTag.Close();
			}
			// The eave course's underside is the 連檐 and 瓦口: timber.
			for (const UE::Geometry::FIndex2i& Box : FasciaBoxes) TagUndersides(Mesh, Box.A, Box.B, MatSlot_Wood);

			// 硬山: the end face is the brick 山牆 pediment. 懸山: it is the roof's open end.
			if (G <= 0.0 && GableFaces.B > GableFaces.A)
			{
				// Closed ends only; an open end is buried in the next roof.
				const double MidX = 0.5 * (GX0 + GX1);
				for (int32 tid = GableFaces.A; tid < GableFaces.B; ++tid)
				{
					if (!Mesh.IsTriangle(tid)) continue;
					const bool bLow = Mesh.GetTriCentroid(tid).X < MidX;
					if (bLow ? Roof.bOpenLowEnd : Roof.bOpenHighEnd) continue;
					SetMaterialIDForTriangleRange(Mesh, tid, tid + 1, MatSlot_Body);
				}
			}

			// 徹上明造: the attic triangle over the ceiling line, closed by the 山牆 carried up (硬山) or the 山花
			// board (懸山), its top 1 cm into the shell.
			if (Roof.ShellCover > 0.0 && Profile.Num() >= 2)
			{
				const double Ceil = Eave + Roof.UndersideRise;
				TArray<FVector2d> Attic = { FVector2d(0.0, Ceil), FVector2d(D, Ceil) };
				for (int32 i = Profile.Num() - 1; i >= 0; --i)
				{
					const double Y = Profile[i].X - O;
					if (Y > 0.0 && Y < D) Attic.Add(FVector2d(Y, Eave + Profile[i].Y - Roof.ShellCover + 1.0));
				}
				Attic.Insert(FVector2d(D, TopAt(O + D) - Roof.ShellCover + 1.0), 2);
				Attic.Add(FVector2d(0.0, TopAt(O) - Roof.ShellCover + 1.0));
				const bool bBrick = G <= 0.0;
				const double Thick = bBrick ? FMath::Max(Roof.GableWallThickness, 0.0) : 2.0;
				if (Thick > 0.0)
				{
					// (Y, Z) → world: profile X → Y, profile Y → Z, swept along X; a rotation, never a mirror.
					const FMatrix Basis(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector);
					auto Prism = [&](double X0, double X1)
					{
						FTransform A(Basis), B(Basis);
						A.SetTranslation(FVector(X0, 0, 0));
						B.SetTranslation(FVector(X1, 0, 0));
						AppendSweptProfile(Mesh, Attic, { A, B });
					};
					FSlotScope AtticTag(Mesh, bBrick ? MatSlot_Body : MatSlot_Wood);
					if (!Roof.bOpenLowEnd)  Prism(0.0, Thick);
					if (!Roof.bOpenHighEnd) Prism(W - Thick, W);
				}
			}

			// 博縫 (brick band) and 排山勾滴 (tile course) swept down each gable edge along the roof creases.
			// Buried a few cm into the 山牆 so no face lies on the gable plane or slope.
			const double RakeD = FMath::Max(Roof.RakeDepth, 0.0);
			const double RakeP = FMath::Max(Roof.RakeProjection, 0.0);
			if (G <= 0.0 && RakeD > 0.0 && RakeP > 0.0)
			{
				if (Profile.Num() >= 2)
				{
					constexpr double Bury = 4.0;
					// Band top just under the slope; tiles straddle it.
					const TArray<FVector2d> Brick = {
						FVector2d(-Bury, -RakeD), FVector2d(RakeP, -RakeD),
						FVector2d(RakeP, -1.0), FVector2d(-Bury, -1.0) };
					const double TileTop = FMath::Min(7.0, 0.3 * FMath::Max(Roof.RidgeCourseHeight, 7.0));
					// Outer face leans out rather than overhanging: a ledge underside would be retagged as soffit.
					const TArray<FVector2d> Tile = {
						FVector2d(-Bury, -2.0), FVector2d(RakeP, -2.0),
						FVector2d(RakeP + 1.5, TileTop), FVector2d(-Bury, TileTop) };

					// Local X points outward: sweep front-to-back on -X gable, back-to-front on +X.
					const TArray<FTransform> Left = ProfileStations(Profile, GX0, RoofY0, Eave, false);
					const TArray<FTransform> Right = ProfileStations(Profile, GX1, RoofY0, Eave, true);

					// Full depth down the rake; near the eave the underside levels off just below the eave line and
					// runs to the eave end: the shoulder onto the 墀頭 (Fig 2-10). Not tapered (ran to a point),
					// not cut square (read as cut off), not full depth (foot hung out of a boundary wall).
					const double Ledge = 0.6 * RakeD;
					auto Shouldered = [&](TArray<FTransform> Stations, bool bReverse)
					{
						for (int32 i = 0; i < Stations.Num() && i < Profile.Num(); ++i)
						{
							const FVector2d& Pt = Profile[bReverse ? Profile.Num() - 1 - i : i];
							const double Line = (Pt.X > 0.5 * Nominal) ? RearEaveZ - Eave : 0.0;
							const double S = FMath::Clamp((Pt.Y - Line + Ledge) / RakeD, 0.1, 1.0);
							Stations[i].SetScale3D(FVector(1.0, S, 1.0));
						}
						return Stations;
					};
					const int32 BrickFirstTri = Mesh.MaxTriangleID();
					FSlotScope BrickTag(Mesh, MatSlot_Body);
					if (!Roof.bOpenLowEnd)  AppendSweptProfile(Mesh, Brick, Shouldered(Left, false));
					if (!Roof.bOpenHighEnd) AppendSweptProfile(Mesh, Brick, Shouldered(Right, true));
					BrickTag.Close();

					// 方磚博縫: one course of 尺四方磚 on edge, joints square to the rake (not box-projected level courses).
					constexpr double FangzhuanLength = 44.8;
					const double PerU = HutongGen::BrickPattern::FaceLength / 100.0 / FangzhuanLength;
					const double PerV = HutongGen::BrickPattern::CourseHeight / 100.0 / RakeD;
					MapAlongProfile(Mesh, BrickFirstTri, Profile, RoofY0, Eave,
						[PerU, PerV](double X, const FProfileSpot& Spot, EProfileFace Face)
						{
							switch (Face)
							{
							case EProfileFace::Gable: return FVector2f((float)(Spot.S * PerU), (float)(Spot.Below * PerV));
							case EProfileFace::Slope: return FVector2f((float)(Spot.S * PerU), (float)(X * PerV));
							default:                  return FVector2f((float)(X * PerU), (float)(Spot.Below * PerV));
							}
						});

					FSlotScope TileTag(Mesh, MatSlot_Roof);
					if (!Roof.bOpenLowEnd)  AppendSweptProfile(Mesh, Tile, Left);
					if (!Roof.bOpenHighEnd) AppendSweptProfile(Mesh, Tile, Right);
					TileTag.Close();
				}
			}

			// 博縫板 and 梅花釘 down each 懸山 rake; 山花 boarded behind.
			if (G > 0.0 && Roof.bWoodenGable && Profile.Num() >= 3)
			{
				if (GableFaces.B > GableFaces.A) SetMaterialIDForTriangleRange(Mesh, GableFaces.A, GableFaces.B, MatSlot_Wood);

				const double BD = FMath::Max(Roof.BargeboardDepth, 8.0);
				const double BT = FMath::Max(Roof.BargeboardThickness, 2.0);
				// Local X outward, Y slope normal; board hangs under the tile edge, buried 1 cm into the roof end.
				const TArray<FVector2d> Board = {
					FVector2d(-1.0, -BD), FVector2d(BT, -BD), FVector2d(BT, -1.0), FVector2d(-1.0, -1.0) };
				const TArray<FTransform> Left = ProfileStations(Profile, GX0, RoofY0, Eave, false);
				const TArray<FTransform> Right = ProfileStations(Profile, GX1, RoofY0, Eave, true);
				FSlotScope BoardTag(Mesh, MatSlot_Wood);
				AppendSweptProfile(Mesh, Board, Left);
				AppendSweptProfile(Mesh, Board, Right);
				BoardTag.Close();

				// 梅花釘: quincunx of five studs, three sets per slope.
				FSlotScope StudTag(Mesh, MatSlot_Paint);
				const double Stud = 2.4, Ring = 0.2 * BD;
				auto Cluster = [&](const FTransform& At)
				{
					const FVector2d Offsets[5] = { {0, 0}, {Ring, 0}, {-Ring, 0}, {0, Ring}, {0, -Ring} };
					for (const FVector2d& O : Offsets)
					{
						// Across the board (local Y), along the rake (local Z), proud of its face.
						const int32 V0 = Mesh.MaxVertexID();
						AppendBox(Mesh, FVector3d(BT - 0.5, -0.5 * BD + O.X - 0.5 * Stud, O.Y - 0.5 * Stud),
							FVector3d(BT + 1.5, -0.5 * BD + O.X + 0.5 * Stud, O.Y + 0.5 * Stud));
						TransformVerticesFrom(Mesh, V0, At);
					}
				};
				for (const TArray<FTransform>* Side : { &Left, &Right })
				{
					const int32 N = Side->Num();
					for (const double F : { 0.18, 0.33, 0.67, 0.82 })
					{
						const int32 Idx = FMath::Clamp(FMath::RoundToInt32(F * (N - 1)), 1, N - 2);
						Cluster((*Side)[Idx]);
					}
				}
				StudTag.Close();
			}

			// 椽頭 last and tagged separately.
			const double RSec = FMath::Max(Roof.RafterSection, 0.0);
			if (RSec > 0.0)
			{
				FSlotScope RafterTag(Mesh, MatSlot_Paint);
				// Tops pushed into the drip course: no face coplanar with its underside.
				const double TopZ = Eave - 0.5 * FasciaDrop;
				const double Reach = 3.0 * RSec;

				// Set back behind the drip course face.
				const double Setback = (FasciaDrop > 0.0 && FasciaW > 0.0)
					? FMath::Min(0.5 * FasciaW, 0.4 * Reach)
					: 0.0;

				// Clear of the 墀頭 at each closed end.
				const double Inset = FMath::Max(Roof.RafterEndInset, 0.0);
				const double RX0 = GX0 + (Roof.bOpenLowEnd ? 0.0 : Inset);
				const double RX1 = GX1 - (Roof.bOpenHighEnd ? 0.0 : Inset);

				// Two courses, as an eave shows.
				if (Roof.bFlyingRafters)
				{
					AppendRafterEnds(Mesh, RX0, RX1, -O + Setback, -O + Reach,
						TopZ, RSec, Roof.RafterSpacing);
				}

				// 檐椽 behind the 飛椽, from their ends up the soffit to the column line, tops buried 1 cm in it.
				const double EaveRafterIn = Roof.bFlyingRafters ? (Setback + 0.5 * Reach) : Setback;
				const double RoundD = 1.05 * RSec;
				const double Climb = (Under.Drop > 0.0 && O > FasciaW + 1.0) ? (FasciaDrop + Under.InnerRise) / (O - FasciaW) : 0.0;
				auto SoffitDrop = [&](double FromEdge) { return Climb > 0.0 ? FasciaDrop - Climb * (FromEdge - FasciaW) : 0.0; };
				const double FrontZ = Eave - SoffitDrop(EaveRafterIn) - 0.5 * RoundD + 1.0;
				AppendRoundRafterEnds(Mesh, RX0, RX1, -O + EaveRafterIn, 0.0,
					FrontZ, RoundD, Roof.RafterSpacing, 6, Climb * (O - EaveRafterIn));

				// Over a 前廊 the 檐椽 run on from the column line up to the 金檁, under the open underside, their
				// tops buried in the wall on the 金柱 line.
				if (Roof.ShellCover <= 0.0 && Under.VerandaEnd > O + 1.0)
				{
					const double Level = FMath::Min(Eave + Under.InnerRise, TopAt(O) - 1.0);
					const double Cover = TopAt(O) - Level;
					const double Za = TopAt(O) - Cover - 0.5 * RoundD + 1.0;
					const double Zb = TopAt(Under.VerandaEnd) - Cover - 0.5 * RoundD + 1.0;
					AppendRoundRafterEnds(Mesh, RX0, RX1, 0.0, Under.VerandaEnd - O, Za, RoundD, Roof.RafterSpacing, 6, Zb - Za);
				}

				// Sealed rear eave shows brick, not rafters (same test as the drip course).
				if (RearO >= FasciaW && RearO > Reach)
				{
					const double RearTopZ = RearEaveZ - 0.5 * FasciaDrop;
					if (Roof.bFlyingRafters)
					{
						AppendRafterEnds(Mesh, RX0, RX1, D + RearO - Reach, D + RearO - Setback,
							RearTopZ, RSec, Roof.RafterSpacing);
					}
					if (Under.RearLap > 0.0)
					{
						// Built as a front row, then turned half round: no mirroring.
						const double RearClimb = (RearO > FasciaW + 1.0) ? (FasciaDrop + Under.InnerRise) / (RearO - FasciaW) : 0.0;
						const double RearFrontZ = Eave - (FasciaDrop - RearClimb * (EaveRafterIn - FasciaW)) - 0.5 * RoundD + 1.0;
						const int32 V0 = Mesh.MaxVertexID();
						AppendRoundRafterEnds(Mesh, RX0, RX1, -RearO + EaveRafterIn, 0.0,
							RearFrontZ, RoundD, Roof.RafterSpacing, 6, RearClimb * (RearO - EaveRafterIn));
						TransformVerticesFrom(Mesh, V0, FTransform(FRotator(0.0, 180.0, 0.0), FVector(RX0 + RX1, D, 0.0)));
					}
					else
					{
						// A lifted rear eave keeps a flat underside: stubs only.
						const double RoundOut = Roof.bFlyingRafters ? (Reach + 0.55 * RSec) : Reach;
						const double RoundDrop = Roof.bFlyingRafters ? (1.15 * RSec) : (0.5 * RSec);
						AppendRoundRafterEnds(Mesh, RX0, RX1, D + RearO - RoundOut, D + RearO - EaveRafterIn,
							RearTopZ - RoundDrop, RoundD, Roof.RafterSpacing);
					}
				}

				// 徹上明造: the 檐椽 run on as 花架椽 and 腦椽, purlin line to purlin line, under the shell.
				if (Roof.ShellCover > 0.0 && Under.Drop > 0.0 && Profile.Num() >= 2)
				{
					const double Cover = Roof.ShellCover;
					// Purlin lines: the section's creases, both slopes, between the column lines.
					const double Scale = 0.5 * Nominal / FMath::Max(Roof.Section.HalfSpan(), 1e-6);
					TArray<double> Lines = { O, O + D };
					double Acc = 0.0;
					for (const double R : Roof.Section.Run)
					{
						Acc += R * Scale;
						for (const double C : { Acc, Nominal - Acc })
						{
							if (C > O + 1.0 && C < O + D - 1.0) Lines.AddUnique(C);
						}
					}
					Lines.Sort();
					// 羅鍋椽: where the underside bends between two lines (a 捲棚's crown) the rafters follow it.
					{
						TArray<double> Bent;
						for (int32 i = 0; i + 1 < Lines.Num(); ++i)
						{
							const double Ca = Lines[i], Cb = Lines[i + 1];
							TArray<double> Inside;
							double Worst = 0.0;
							for (const FVector2d& Pt : Profile)
							{
								if (Pt.X <= Ca + 1.0 || Pt.X >= Cb - 1.0) continue;
								Inside.Add(Pt.X);
								const double Chord = FMath::Lerp(TopAt(Ca), TopAt(Cb), (Pt.X - Ca) / (Cb - Ca));
								Worst = FMath::Max(Worst, FMath::Abs(TopAt(Pt.X) - Chord));
							}
							if (Worst > 1.0) Bent.Append(Inside);
						}
						Lines.Append(Bent);
						Lines.Sort();
					}
					for (int32 i = 0; i + 1 < Lines.Num(); ++i)
					{
						const double Ca = Lines[i], Cb = Lines[i + 1];
						const double Za = TopAt(Ca) - Cover - 0.5 * RoundD + 1.0;
						const double Zb = TopAt(Cb) - Cover - 0.5 * RoundD + 1.0;
						AppendRoundRafterEnds(Mesh, RX0, RX1, -O + Ca, -O + Cb, Za, RoundD, Roof.RafterSpacing, 6, Zb - Za);
					}
				}

				// 椽頭 take the 彩畫 slot, never the frame's red.
				RafterTag.Close();
			}
		}
		void AppendRoofDressing(
			FDynamicMesh3& Mesh,
			const TArray<HutongMeshUtils::FRoofPanel>& Panels,
			const FRoofDressing& D)
		{
			using HutongMeshUtils::FRoofPanel;

			// 壟: one course per row along each eave, square to it, up the slope until it meets a hip.
			const double Pitch = FMath::Max(D.TileRowSpacing, 4.0);
			if (D.bTileRuns)
			{
				const TArray<FVector2d> Section = TileSection(D.Tile, Pitch, D.FasciaDrop);
				double HalfWidth = 1.0;
				for (const FVector2d& Pt : Section) HalfWidth = FMath::Max(HalfWidth, FMath::Abs(Pt.X));
				FSlotScope RoofTag(Mesh, MatSlot_Roof);
				int32 Steps = CourseStations;
				// 勾頭 at each 筒瓦 course's foot, square to its eave, sunk into the eave course as on a hipped roof's own row.
				const bool bCaps = HutongGen::RoofTile::HasEaveCaps(D.Tile) && D.FasciaDrop > 0.0;
				const double CapR = FMath::Clamp(0.34 * Pitch, 1.5, 0.9 * D.FasciaDrop);
				const double CapLen = FMath::Max(0.8 * CapR, 1.0);
				for (const FRoofPanel& Pn : Panels)
				{
					const double EL = (Pn.EaveB - Pn.EaveA).Length();
					const FVector2d EDir = (Pn.EaveB - Pn.EaveA) / FMath::Max(EL, 1e-6);
					Steps = CourseStationsFor((Pn.Sample(0.5, Pn.VMax) - Pn.Sample(0.5, 0.0)).Length());
					const FVector3d Outward(EDir.Y, -EDir.X, 0.0);
					const int32 Count = FMath::FloorToInt32(EL / Pitch);
					if (Count < 1) continue;
					const double Start = 0.5 * (EL - Count * Pitch) + 0.5 * Pitch;
					for (int32 k = 0; k < Count; ++k)
					{
						const double A = Start + k * Pitch;
						auto Inside = [&](double V) { const double U = Pn.SolveU(A, V); return U >= 0.0 && U <= 1.0; };
						if (!Inside(0.0)) continue;

						TArray<double> Vs = { 0.0 };
						for (int32 s = 1; s <= Steps; ++s)
						{
							const double V = Pn.VMax * s / Steps;
							if (Inside(V)) { Vs.Add(V); continue; }
							// Where the course meets the hip.
							double Lo = Vs.Last(), Hi = V;
							for (int32 i = 0; i < 10; ++i) { const double M = 0.5 * (Lo + Hi); (Inside(M) ? Lo : Hi) = M; }
							if (Lo > Vs.Last() + 1e-4) Vs.Add(Lo);
							break;
						}
						if (Vs.Num() < 2) continue;

						TArray<FVector3d> Pts;
						for (const double V : Vs) Pts.Add(Pn.Sample(Pn.SolveU(A, V), V));
						if ((Pts.Last() - Pts[0]).Length() < 0.5 * Pitch) continue;

						TArray<FTransform> Stations;
						for (int32 i = 0; i < Pts.Num(); ++i)
						{
							const FVector3d T = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal();
							const double U = Pn.SolveU(A, Vs[i]), V = Vs[i];
							constexpr double E = 1e-3;
							FVector3d N = (Pn.Sample(U + E, V) - Pn.Sample(U - E, V)).Cross(
								Pn.Sample(U, FMath::Min(V + E, 1.0)) - Pn.Sample(U, FMath::Max(V - E, 0.0)));
							if (N.Z < 0.0) N = -N;
							const FQuat Rot = FRotationMatrix::MakeFromZY(FVector(T), FVector(N.GetSafeNormal())).ToQuat();
							Stations.Add(FTransform(Rot, FVector(Pts[i])));
						}
						// The course's own 壟 in the tile pattern: its crown band across, rows up its length (and the profile's height, so an end cap is no line). Left to the
						// box projection the pattern lay across the tubes at odd angles and read as blobs.
						const int32 FirstV = Mesh.MaxVertexID(), FirstT = Mesh.MaxTriangleID();
						AppendSweptProfile(Mesh, Section, Stations);
						TArray<double> Arc = { 0.0 };
						for (int32 i = 1; i < Pts.Num(); ++i) Arc.Add(Arc.Last() + (Pts[i] - Pts[i - 1]).Length());
						HutongMeshUtils::SetSweptUVs(Mesh, FirstV, FirstT, Stations, [&](int32 S, const FVector2d& L)
						{
							return FVector2f(float(A / Pitch + 0.1 * L.X / HalfWidth), float(Arc[S] / Pitch + 0.1 * L.Y / HalfWidth));
						});
						if (bCaps)
						{
							const FVector3d At = Pts[0] - Outward * (0.4 * CapR) - FVector3d(0.0, 0.0, 0.5 * D.FasciaDrop);
							const int32 Mark = Mesh.MaxVertexID();
							AppendCylinder(Mesh, FVector3d::Zero(), CapR, CapLen, HutongGen::RoofTile::EaveCapSides);
							TransformVerticesFrom(Mesh, Mark, FTransform(FQuat::FindBetweenNormals(FVector::UpVector, FVector(Outward)), FVector(At)));
						}
					}
				}
				RoofTag.Close();
			}

			// 椽頭 under the eave course, each square to its eave, fanning toward the corner diagonal
			// under a 翼角; clear of the corners, where the 角梁 is.
			const double RSec = FMath::Max(D.RafterSection, 0.0);
			if (RSec > 0.0)
			{
				FSlotScope PaintTag(Mesh, MatSlot_Paint);
				const double Reach = 3.0 * RSec;
				const double Setback = 0.25 * RSec + 1.0;
				const double RoundOut = D.bFlyingRafters ? Reach + 0.55 * RSec : Reach;
				const double RoundIn = D.bFlyingRafters ? Setback + 0.5 * Reach : Setback;
				const double RoundDrop = D.bFlyingRafters ? 1.15 * RSec : 0.5 * RSec;
				const double RoundD = 1.05 * RSec;
				const double Spacing = FMath::Max(D.RafterSpacing, 1.5 * RSec);
				for (const FRoofPanel& Pn : Panels)
				{
					const FVector2d Run = Pn.EaveB - Pn.EaveA;
					const double EL = Run.Length();
					const FVector2d Dir = Run / FMath::Max(EL, 1e-6);
					const FVector2d Out(Dir.Y, -Dir.X);
					const double Margin = Reach + RSec;
					// The unflared eave line, where the soffit meets the column line.
					const double EaveZ = Pn.Sample(Pn.SolveU(0.5 * EL, 0.0), 0.0).Z;
					const int32 Count = D.bNoCorners ? FMath::Max(FMath::RoundToInt32(EL / Spacing), 1)
						: FMath::FloorToInt32((EL - 2.0 * Margin) / Spacing) + 1;
					if (!D.bNoCorners && (EL - 2.0 * Margin <= 0.0 || Count < 1)) continue;
					const double Step = D.bNoCorners ? EL / Count : Spacing;
					const double Start = D.bNoCorners ? 0.5 * Step : 0.5 * (EL - (Count - 1) * Spacing);
					for (int32 k = 0; k < Count; ++k)
					{
						const double A = Start + k * Step;
						const FVector3d B = Pn.Sample(Pn.SolveU(A, 0.0), 0.0);
						double Wt = (Pn.FlareLength > 0.0) ? FMath::Clamp(1.0 - FMath::Min(A, EL - A) / Pn.FlareLength, 0.0, 1.0) : 0.0;
						Wt = Wt * Wt * (3.0 - 2.0 * Wt);
						const FVector2d Diag = (A < EL - A) ? Pn.DiagA : Pn.DiagB;
						const FVector2d OutF = (Out * (1.0 - 0.6 * Wt) + Diag * (0.6 * Wt)).GetSafeNormal();
						// Built as a front eave (face at y = 0, outward -Y), then turned onto OutF.
						const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(OutF.X, -OutF.Y));
						// Tops inside the eave course, which rises with the 翼角.
						const double TopZ = B.Z - 0.5 * D.FasciaDrop;
						const int32 V0 = Mesh.MaxVertexID();
						if (D.bFlyingRafters) AppendRafterEnds(Mesh, -0.75 * RSec, 0.75 * RSec, Setback, Reach, TopZ, RSec, 1e6);
						// Up the soffit from the eave course's base toward the column line, tops buried 1 cm in it;
						// shortening into a 翼角, where rafters converging on the corner would cross.
						const double Len = FMath::Max(D.EaveOverhang * FMath::Square(1.0 - Wt), RoundOut);
						if (D.EaveOverhang > RoundIn + 1.0)
						{
							const double Base = B.Z - D.FasciaDrop;
							const double Climb = (EaveZ - Base) / D.EaveOverhang;
							AppendRoundRafterEnds(Mesh, -0.8 * RoundD, 0.8 * RoundD, RoundIn, Len,
								Base + Climb * RoundIn - 0.5 * RoundD + 1.0, RoundD, 1e6, 8, Climb * (Len - RoundIn));
						}
						else
						{
							AppendRoundRafterEnds(Mesh, -0.8 * RoundD, 0.8 * RoundD, RoundIn, RoundOut, TopZ - RoundDrop, RoundD, 1e6, 8);
						}
						HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FRotator(0.0, Yaw, 0.0), FVector(B.X, B.Y, 0.0)));
					}
				}
				PaintTag.Close();
			}

			// 垂脊 up each hip, 正脊 along the top where the long panels meet on a ridge.
			if (D.RidgeWidth > 0.0 && D.RidgeHeight > 0.0 && Panels.Num() == 4)
			{
				FSlotScope RidgeTag(Mesh, MatSlot_Ridge);
				const double HalfW = 0.5 * D.RidgeWidth, Below = 0.35 * D.RidgeHeight, Above = 0.65 * D.RidgeHeight;
				for (const int32 p : { 0, 2 })
				{
					const FRoofPanel& Pn = Panels[p];
					for (const int32 e : { 0, 1 })
					{
						// From a ridge height in from the eave corner.
						const double V0 = HutongMeshUtils::HipStartV([&](double V) { return Pn.Sample((double)e, V); }, 1.5 * D.RidgeHeight, Pn.VMax);
						TArray<FVector3d> Path;
						for (int32 s = 0; s <= 16; ++s) Path.Add(Pn.Sample((double)e, FMath::Lerp(V0, Pn.VMax, s / 16.0)));
						const FVector2d Diag = e ? Pn.DiagB : Pn.DiagA;
						HutongMeshUtils::AppendRidgeStrip(Mesh, Path, FVector3d(-Diag.Y, Diag.X, 0.0), HalfW, Below, Above);
					}
				}
				const FVector3d R0 = Panels[0].Sample(0.0, 1.0), R1 = Panels[0].Sample(1.0, 1.0);
				if ((R1 - R0).Length() > 1.0)
				{
					HutongMeshUtils::AppendRidgeStrip(Mesh, { R0, R1 }, FVector3d(0.0, 1.0, 0.0), HalfW, Below, Above);
				}
				RidgeTag.Close();
			}
		}

		void AppendRoundRoofCourses(
			FDynamicMesh3& Mesh,
			const FVector3d& Centre,
			const HutongMeshUtils::FRoundRoofSpec& Spec,
			EHutongRoofTile Tile, double FasciaDrop, double StopRadius)
		{
			const int32 Courses = FMath::Max(Spec.Panels * Spec.CoursesPerPanel, 3);
			const double Re = FMath::Max(Spec.Radius, 1.0);
			const double Pitch = 2.0 * PI * Re / Courses;
			const double Stop = FMath::Clamp(StopRadius, 0.02 * Re, 0.9 * Re);
			const TArray<FVector2d> Section = TileSection(Tile, Pitch, FasciaDrop);
			double HalfWidth = 1.0;
			for (const FVector2d& Pt : Section) HalfWidth = FMath::Max(HalfWidth, FMath::Abs(Pt.X));
			auto Height = [&](double R) { return Centre.Z + HutongMeshUtils::RoundRoofHeight(Spec, R); };

			// Stations down each meridian, finer where the slope turns.
			const int32 Steps = FMath::Max(CourseStationsFor(Re - Stop) + 4, 8);
			TArray<double> Rs, Arc;
			for (int32 s = 0; s <= Steps; ++s) Rs.Add(FMath::Lerp(Re, Stop, double(s) / Steps));
			Arc.Add(0.0);
			for (int32 s = 1; s <= Steps; ++s) Arc.Add(Arc.Last() + FVector2d(Rs[s] - Rs[s - 1], Height(Rs[s]) - Height(Rs[s - 1])).Length());

			FSlotScope RoofTag(Mesh, MatSlot_Roof);
			const bool bCaps = RoofTile::HasEaveCaps(Tile) && FasciaDrop > 0.0;
			const double CapR = FMath::Clamp(0.34 * Pitch, 1.5, 0.9 * FasciaDrop);
			for (int32 k = 0; k < Courses; ++k)
			{
				// On the slope's own lines: course k at u = k + 0.5.
				const double Th = 2.0 * PI * (k + 0.5) / Courses;
				const FVector3d Out(FMath::Cos(Th), FMath::Sin(Th), 0.0);
				const FVector3d Across(-Out.Y, Out.X, 0.0);
				TArray<FTransform> Stations;
				for (int32 s = 0; s <= Steps; ++s)
				{
					const double R = Rs[s];
					const double Rn = Rs[FMath::Min(s + 1, Steps)], Rp = Rs[FMath::Max(s - 1, 0)];
					const FVector3d T = (Out * (Rn - Rp) + FVector3d(0.0, 0.0, Height(Rn) - Height(Rp))).GetSafeNormal();
					FVector3d N = T.Cross(Across);
					if (N.Z < 0.0) N = -N;
					// Tapered 筒瓦: the course narrows with the circle, so all reach the top.
					const double Scale = R / Re;
					Stations.Add(FTransform(FRotationMatrix::MakeFromZY(FVector(T), FVector(N)).ToQuat(),
						FVector(Centre.X + R * Out.X, Centre.Y + R * Out.Y, Height(R)), FVector(Scale, Scale, 1.0)));
				}
				const int32 FirstV = Mesh.MaxVertexID(), FirstT = Mesh.MaxTriangleID();
				AppendSweptProfile(Mesh, Section, Stations);
				HutongMeshUtils::SetSweptUVs(Mesh, FirstV, FirstT, Stations, [&](int32 S, const FVector2d& L)
				{
					return FVector2f(float(k + 0.5 + 0.1 * L.X / HalfWidth), float(Arc[S] / Pitch + 0.1 * L.Y / HalfWidth));
				});
				if (bCaps)
				{
					const FVector3d Foot(Centre.X + Re * Out.X, Centre.Y + Re * Out.Y, Centre.Z);
					const FVector3d At = Foot - Out * (0.4 * CapR) - FVector3d(0.0, 0.0, 0.5 * FasciaDrop);
					const int32 Mark = Mesh.MaxVertexID();
					AppendCylinder(Mesh, FVector3d::Zero(), CapR, FMath::Max(0.8 * CapR, 1.0), RoofTile::EaveCapSides);
					TransformVerticesFrom(Mesh, Mark, FTransform(FQuat::FindBetweenNormals(FVector::UpVector, FVector(Out)), FVector(At)));
				}
			}
		}
	}
}
