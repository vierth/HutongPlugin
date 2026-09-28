#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongCanon.h"

namespace HutongGen
{
	// The timber frame: rows of columns and the beams between them.
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

		// 方柱: a square post with chamfered arrises, half-width HalfWidth, tapering like AppendColumn.
		void AppendSquareColumn(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double HalfWidth, double TopHalfWidth,
			double BottomZ, double TopZ, double Overshoot = 1.0);

		// Columns at each of Count+1 boundaries along X.
		void AppendColumnRow(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double CenterY, double Radius, double TopRadius,
			double BottomZ, double TopZ, double Overshoot = 1.0,
			int32 Sides = 24);

		// 額枋 along a column row. Ends buried in the end columns; centred on the column axis so the
		// cylinders swallow its side faces.
		void AppendArchitrave(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double Section,
			double BottomZ, double TopZ);

		// 檐墊板 (彩畫) and 檐檁 (wood) on the column tops along a row, from ColumnTop up, 圖5-3-1's section:
		// what stands between the columns and a roof lifted to the frame's line. Tags its own materials.
		// Board and purlin in 柱徑 (the house frame's by default); the 檁 runs PurlinReach past each end
		// (搭交 出頭 at a corner, a 懸山's gable overhang); bSwallowTails hangs a 燕尾枋 under each such end.
		void AppendEaveStack(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CenterY, double ColumnDiameter, double ColumnTop,
			double BoardHeight = HutongCanon::Frame::BoardHeight, double PurlinDiameter = HutongCanon::Frame::PurlinDiameter,
			double PurlinReach = 0.0, bool bSwallowTails = false);

		// 燕尾枋 (清式營造則例 表四): a plate under a 懸山's 檁 from the frame line at XIn out to XOut, TopZ its top,
		// the underside rising toward the end. Tags its own material.
		void AppendSwallowTail(
			UE::Geometry::FDynamicMesh3& Mesh,
			double XIn, double XOut, double CenterY, double ColumnDiameter, double TopZ);

		// The same along Y, on the line x = CenterX.
		void AppendEaveStackAlongY(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Y0, double Y1, double CenterX, double ColumnDiameter, double ColumnTop,
			double BoardHeight = HutongCanon::Frame::BoardHeight, double PurlinDiameter = HutongCanon::Frame::PurlinDiameter,
			double PurlinReach = 0.0);

		// 穿插枋 tying an outer column row back to an inner one.
		void AppendTieBeams(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 Count, double OuterY, double InnerY, double Section,
			double BottomZ, double TopZ, double Overshoot = 1.0);

		// 倒掛楣子 in bay [X0, X1] on line CY, hung from RailTop: head, foot and middle rails plus
		// drops, reading as 步步錦.
		void AppendHangingFrieze(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CY, double RailTop, double Drop, double Bar, int32 Drops);

		// 坐凳楣子 in bay [X0, X1] on line CY: lattice from floor to seat underside, seat between the
		// posts (PostHalf in from each end); without a seat, a post-wide plank on the lattice.
		void AppendBenchRail(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double CY, double Floor, double SeatTop, double SeatDepth,
			double PostHalf, bool bSeat, bool bLattice);

		// 垂蓮柱: short hanging post ending in a lotus bud.
		void AppendHangingPost(
			UE::Geometry::FDynamicMesh3& Mesh,
			double CenterX, double CenterY, double TopZ, double Drop, double Radius,
			double BudFraction);
	}
}
