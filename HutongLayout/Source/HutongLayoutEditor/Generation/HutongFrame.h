#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongCanon.h"

namespace HutongGen
{
	namespace Frame
	{
		// 收分: 清式 reduces the diameter by TaperRatio × 柱高; the radius by half that.
		inline double TaperedTopRadius(double BaseRadius, double ColumnHeight, double TaperRatio)
		{
			const double Reduction =
				0.5 * FMath::Max(ColumnHeight, 0.0) * FMath::Clamp(TaperRatio, 0.0, 0.05);
			return FMath::Max(BaseRadius - Reduction, 0.3 * FMath::Max(BaseRadius, 0.5));
		}

		void AppendColumn(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot = 1.0,
			int32 Sides = 24);

		// 方柱 half-width with the same section area as a round column of this radius.
		inline double SquareHalfWidth(double Radius) { return 0.5 * FMath::Sqrt(PI) * Radius; }

		void AppendSquareColumn(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double HalfWidth, double TopHalfWidth,
			double BottomZ, double TopZ, double Overshoot = 1.0);

		void AppendColumnRow(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot = 1.0,
			int32 Sides = 24);

		// Centred on the column axis so the cylinders swallow its side faces.
		void AppendArchitrave(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double Section,
			double BottomZ, double TopZ);

		// 圖5-3-1's section. Tags its own materials.
		void AppendEaveStack(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double ColumnDiameter, double ColumnTop,
			double BoardHeight = HutongCanon::Frame::BoardHeight, double PurlinDiameter = HutongCanon::Frame::PurlinDiameter,
			double PurlinReach = 0.0, bool bSwallowTails = false);

		// 清式營造則例 表四. Tags its own material.
		void AppendSwallowTail(
			UE::Geometry::FDynamicMesh3& Mesh,
			double XIn, double XOut, double CenterY, double ColumnDiameter, double TopZ);

		void AppendEaveStackAlongY(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Y0, double Y1, double CenterX, double ColumnDiameter, double ColumnTop,
			double BoardHeight = HutongCanon::Frame::BoardHeight, double PurlinDiameter = HutongCanon::Frame::PurlinDiameter,
			double PurlinReach = 0.0);

		void AppendTieBeams(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double OuterY, double InnerY, double Section,
			double BottomZ, double TopZ, double Overshoot = 1.0);

		void AppendHangingFrieze(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CY, double RailTop, double Drop, double Bar, int32 Drops);

		void AppendBenchRail(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CY, double Floor, double SeatTop, double SeatDepth,
			double PostHalf, bool bSeat, bool bLattice);

		void AppendHangingPost(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double TopZ, double Drop, double Radius,
			double BudFraction);
	}
}
