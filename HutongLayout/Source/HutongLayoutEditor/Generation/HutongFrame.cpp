#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"

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

			// A unit-radius profile scaled per station.
			const TArray<FVector2d> Shaft = MakeCircleProfile(1.0, FMath::Max(Sides, 6));
			const TArray<FTransform> Stations = {
				FTransform(FQuat::Identity, FVector(CenterX, CenterY, Z0), FVector(R,  R,  1.0)),
				FTransform(FQuat::Identity, FVector(CenterX, CenterY, Z1), FVector(RT, RT, 1.0)),
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
					// Both overshoots are load-bearing — see the header.
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

			// Radius down the bud as a fraction of the shaft's: swell, then close.
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
