#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongCanon.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace Frame
	{
		using namespace HutongMeshUtils;

		void AppendColumn(
			FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot, int32 Sides)
		{
			const double R = FMath::Max(Radius, 0.5);
			const double RT = FMath::Clamp(TopRadius, 0.2 * R, R);
			const double Z0 = BottomZ - Overshoot;
			const double Z1 = TopZ + Overshoot;
			if (Z1 <= Z0)
			{
				return;
			}

			// Unit-radius profile, scaled per station.
			const TArray<FVector2d> Shaft = MakeCircleProfile(1.0, FMath::Max(Sides, 6));
			const TArray<FTransform> Stations = {
				FTransform(FQuat::Identity, FVector(CenterX, CenterY, Z0), FVector(R,  R,  1.0)),
				FTransform(FQuat::Identity, FVector(CenterX, CenterY, Z1), FVector(RT, RT, 1.0)),
			};
			AppendSweptProfile(Mesh, Shaft, Stations);
		}

		void AppendSquareColumn(
			FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double HalfWidth, double TopHalfWidth,
			double BottomZ, double TopZ, double Overshoot)
		{
			const double H = FMath::Max(HalfWidth, 0.5);
			const double HT = FMath::Clamp(TopHalfWidth, 0.2 * H, H);
			const double Z0 = BottomZ - Overshoot;
			const double Z1 = TopZ + Overshoot;
			if (Z1 <= Z0)
			{
				return;
			}

			// Unit square, CCW, each arris cut back by a chamfer.
			constexpr double C = 0.85;
			const TArray<FVector2d> Shaft = {
				{ 1.0, -C}, { 1.0,  C}, { C,  1.0}, {-C,  1.0},
				{-1.0,  C}, {-1.0, -C}, {-C, -1.0}, { C, -1.0},
			};
			const TArray<FTransform> Stations = {
				FTransform(FQuat::Identity, FVector(CenterX, CenterY, Z0), FVector(H,  H,  1.0)),
				FTransform(FQuat::Identity, FVector(CenterX, CenterY, Z1), FVector(HT, HT, 1.0)),
			};
			AppendSweptProfile(Mesh, Shaft, Stations);
		}

		void AppendColumnRow(
			FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot, int32 Sides)
		{
			for (int32 i = 0; i <= Count; ++i)
			{
				AppendColumn(Mesh, BoundaryX(i), CenterY, Radius, TopRadius,
					BottomZ, TopZ, Overshoot, Sides);
			}
		}

		void AppendHangingFrieze(
			FDynamicMesh3& Mesh,
			double X0, double X1, double CY, double RailTop, double Drop, double Bar, int32 Drops)
		{
			const double B = FMath::Max(Bar, 1.0);
			if (X1 - X0 < 4.0 * B || Drop <= B) return;
			AppendBox(Mesh, FVector3d(X0, CY - 0.5 * B, RailTop - B), FVector3d(X1, CY + 0.5 * B, RailTop));
			// Drops wider than the rails in Y: proud of both faces, no coplanar overlap.
			const int32 Count = FMath::Clamp(Drops, 1, 24);
			for (int32 j = 1; j <= Count; ++j)
			{
				const double CX = X0 + (X1 - X0) * j / double(Count + 1);
				AppendBox(Mesh,
					FVector3d(CX - 0.5 * B, CY - 0.9 * B, RailTop - Drop),
					FVector3d(CX + 0.5 * B, CY + 0.9 * B, RailTop - 0.4 * B));
			}
			AppendBox(Mesh, FVector3d(X0, CY - 0.7 * B, RailTop - Drop - B), FVector3d(X1, CY + 0.7 * B, RailTop - Drop));
			AppendBox(Mesh,
				FVector3d(X0, CY - 0.7 * B, RailTop - 0.55 * Drop - 0.5 * B),
				FVector3d(X1, CY + 0.7 * B, RailTop - 0.55 * Drop + 0.5 * B));
		}

		void AppendBenchRail(
			FDynamicMesh3& Mesh,
			double X0, double X1, double CY, double Floor, double SeatTop, double SeatDepth,
			double PostHalf, bool bSeat, bool bLattice)
		{
			const double D = FMath::Max(SeatDepth, 5.0);
			const double Seat = FMath::Max(0.22 * D, 4.0);
			if (bSeat)
			{
				AppendBox(Mesh,
					FVector3d(X0 + PostHalf, CY - 0.5 * D, SeatTop - Seat),
					FVector3d(X1 - PostHalf, CY + 0.5 * D, SeatTop));
			}
			if (!bLattice) return;
			constexpr double Bar = 3.0;
			const double Z0 = Floor + 4.0, Z1 = SeatTop - Seat;
			if (Z1 - Z0 <= 4.0 * Bar) return;
			const double Y0 = CY - 0.5 * Bar, Y1 = CY + 0.5 * Bar;
			AppendBox(Mesh, FVector3d(X0, Y0, Z0), FVector3d(X1, Y1, Z0 + Bar));
			AppendBox(Mesh, FVector3d(X0, Y0, 0.5 * (Z0 + Z1) - 0.5 * Bar), FVector3d(X1, Y1, 0.5 * (Z0 + Z1) + 0.5 * Bar));
			if (!bSeat)
			{
				const double Half = FMath::Max(PostHalf, 0.5 * Bar + 1.0);
				AppendBox(Mesh, FVector3d(X0, CY - Half, Z1 - 4.0), FVector3d(X1, CY + Half, Z1));
			}
			const int32 N = FMath::Max(FMath::RoundToInt32((X1 - X0) / 16.0), 2);
			for (int32 k = 1; k < N; ++k)
			{
				const double X = X0 + (X1 - X0) * k / N;
				AppendBox(Mesh, FVector3d(X - 0.5 * Bar, Y0, Z0), FVector3d(X + 0.5 * Bar, Y1, Z1));
			}
		}

		void AppendArchitrave(
			FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double Section,
			double BottomZ, double TopZ)
		{
			if (X1 <= X0 || TopZ <= BottomZ)
			{
				return;
			}
			const double H = 0.5 * FMath::Max(Section, 1.0);
			AppendBox(Mesh,
				FVector3d(X0, CenterY - H, BottomZ),
				FVector3d(X1, CenterY + H, TopZ));
		}

		void AppendEaveStack(
			FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double ColumnDiameter, double ColumnTop,
			double BoardHeight, double PurlinDiameter, double PurlinReach, bool bSwallowTails)
		{
			namespace C = HutongCanon::Frame;
			const double D = FMath::Max(ColumnDiameter, 2.0);
			if (X1 <= X0) return;
			const double BoardTop = ColumnTop + BoardHeight * D;
			{
				FSlotScope BoardTag(Mesh, MatSlot_Paint);
				AppendBox(Mesh,
					FVector3d(X0, CenterY - 0.5 * C::BoardThickness * D, ColumnTop),
					FVector3d(X1, CenterY + 0.5 * C::BoardThickness * D, BoardTop));
			}
			FSlotScope PurlinTag(Mesh, MatSlot_Wood);
			const double R = 0.5 * PurlinDiameter * D;
			const double Reach = FMath::Max(PurlinReach, 0.0);
			const int32 Mark = Mesh.MaxVertexID();
			AppendCylinder(Mesh, FVector3d::ZeroVector, R, X1 - X0 + 2.0 * Reach, 12);
			// Quarter turn about Y takes +Z onto +X.
			TransformVerticesFrom(Mesh, Mark, FTransform(FQuat(FVector::YAxisVector, HALF_PI), FVector(X0 - Reach, CenterY, BoardTop + R)));
			if (bSwallowTails && Reach > 0.0)
			{
				AppendSwallowTail(Mesh, X0, X0 - Reach, CenterY, D, BoardTop);
				AppendSwallowTail(Mesh, X1, X1 + Reach, CenterY, D, BoardTop);
			}
		}

		void AppendSwallowTail(
			FDynamicMesh3& Mesh,
			double XIn, double XOut, double CenterY, double ColumnDiameter, double TopZ)
		{
			namespace C = HutongCanon::Frame;
			const double D = FMath::Max(ColumnDiameter, 2.0);
			// From inside the column, so the root is buried; the top 1 cm into the 檁.
			const double Dir = (XOut >= XIn) ? 1.0 : -1.0;
			const double Run = FMath::Abs(XOut - XIn) + 0.25 * D;
			if (Run < 0.5 * D) return;
			const double H = C::SwallowTailHeight * D;
			const double Top = TopZ + 1.0;
			const double Taper = C::SwallowTailRun * Run;
			// Along the run (profile X) and up.
			const TArray<FVector2d> Profile = {
				{ 0.0, TopZ - H }, { Run - Taper, TopZ - H }, { Run, TopZ - C::SwallowTailEnd * H },
				{ Run, Top }, { 0.0, Top } };
			const double HalfT = 0.5 * C::SwallowTailThickness * D;
			FSlotScope Tag(Mesh, MatSlot_Paint);
			const int32 Mark = Mesh.MaxVertexID();
			AppendYZPrism(Mesh, Profile, -HalfT, HalfT);
			// The run lies along +Y; a quarter turn about Z lays it along ±X, a rotation either way.
			TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, -90.0 * Dir, 0.0),
				FVector(XIn - Dir * 0.25 * D, CenterY, 0.0)));
		}

		void AppendEaveStackAlongY(
			FDynamicMesh3& Mesh,
			double Y0, double Y1, double CenterX, double ColumnDiameter, double ColumnTop,
			double BoardHeight, double PurlinDiameter, double PurlinReach)
		{
			// Built along X, then a quarter turn about Z: (x, y) → (−y, x), a rotation, never a mirror.
			const int32 Mark = Mesh.MaxVertexID();
			AppendEaveStack(Mesh, Y0, Y1, -CenterX, ColumnDiameter, ColumnTop, BoardHeight, PurlinDiameter, PurlinReach);
			TransformVerticesFrom(Mesh, Mark, FTransform(FRotator(0.0, 90.0, 0.0)));
		}

		void AppendTieBeams(
			FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double OuterY, double InnerY, double Section,
			double BottomZ, double TopZ, double Overshoot)
		{
			if (TopZ <= BottomZ)
			{
				return;
			}

			const double H = 0.5 * FMath::Max(Section, 1.0);
			const double Y0 = FMath::Min(OuterY, InnerY);
			const double Y1 = FMath::Max(OuterY, InnerY);

			for (int32 i = 0; i <= Count; ++i)
			{
				const double CX = BoundaryX(i);
				AppendBox(Mesh,
					FVector3d(CX - H, Y0 - H, BottomZ),
					// Overshoots bury the ends in the columns and the top past the 額枋: no coplanar faces.
					FVector3d(CX + H, Y1 + H, TopZ + Overshoot));
			}
		}

		void AppendHangingPost(
			FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double TopZ, double Drop, double Radius,
			double BudFraction)
		{
			const double R = FMath::Max(Radius, 1.0);
			const double D = FMath::Max(Drop, 4.0 * R);
			const double Bud = FMath::Clamp(BudFraction, 0.1, 0.85) * D;
			const double ShaftBottom = TopZ - (D - Bud);

			// Bud radius down its length, as a fraction of the shaft's.
			static const double BudR[] = { 1.0, 1.28, 1.34, 1.18, 0.82, 0.40, 0.16 };
			const int32 Steps = UE_ARRAY_COUNT(BudR);

			const TArray<FVector2d> Circle = MakeCircleProfile(1.0, 12);
			TArray<FTransform> Stations;
			Stations.Reserve(Steps + 2);

			Stations.Add(FTransform(FQuat::Identity,
				FVector(CenterX, CenterY, TopZ + 2.0), FVector(R, R, 1.0)));
			Stations.Add(FTransform(FQuat::Identity,
				FVector(CenterX, CenterY, ShaftBottom), FVector(R, R, 1.0)));
			for (int32 i = 1; i < Steps; ++i)
			{
				const double T = double(i) / double(Steps - 1);
				Stations.Add(FTransform(FQuat::Identity,
					FVector(CenterX, CenterY, ShaftBottom - T * Bud),
					FVector(R * BudR[i], R * BudR[i], 1.0)));
			}

			AppendSweptProfile(Mesh, Circle, Stations);
		}
	}
}
