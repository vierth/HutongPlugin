#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace HutongGen
{
	// The timber frame: rows of columns and the beams between them.
	namespace Frame
	{
		// 收分: 清式 puts the reduction at a hundredth of 柱高 on the diameter, so the radius loses half of it.
		inline double TaperedTopRadius(double BaseRadius, double ColumnHeight, double TaperRatio)
		{
			const double Reduction =
				0.5 * FMath::Max(ColumnHeight, 0.0) * FMath::Clamp(TaperRatio, 0.0, 0.05);
			return FMath::Max(BaseRadius - Reduction, 0.3 * FMath::Max(BaseRadius, 0.5));
		}

		// One column.
		void AppendColumn(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot = 1.0,
			int32 Sides = 16);

		// Columns at each of Count+1 boundaries along X.
		void AppendColumnRow(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot = 1.0,
			int32 Sides = 16);

		// 額枋 along a column row, ends buried in the end columns and straddling the column axis in Y so the cylinders swallow its side faces.
		void AppendArchitrave(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double Section,
			double BottomZ, double TopZ);

		// 穿插枋 tying an outer column row back to an inner one.
		void AppendTieBeams(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double OuterY, double InnerY, double Section,
			double BottomZ, double TopZ, double Overshoot = 1.0);

		// 垂蓮柱: a short post hanging under a beam, ending in a carved lotus bud.
		void AppendHangingPost(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double TopZ, double Drop, double Radius,
			double BudFraction);
	}
}
