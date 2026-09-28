#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"

// 小式 proportion rules over plain scalars. Every Get* accessor forwards here; figures live in HutongCanon.h.
//
// R = 柱高, K = 臺明高, A = 額枋高, all in 柱徑; T = 橫披窗 share of the opening.
// Each function clamps its own args, so user-edited fields pass straight in.
namespace HutongGen
{
	namespace Proportions
	{
		// How far a roof stands above its 檐柱 tops (圖5-3-1): 墊板 and 檐檁 on the column, the rafters and
		// the roof's cover on them, less what the slope rises over the overhang. The roof's base (its eave
		// line at the eave edge) is the column top plus this; its surface then clears the frame's 檁 by the
		// rafters and cover all the way up, since both follow the same 舉架.
		inline double RoofLift(double ColumnDiameter, double Overhang, double EaveJu)
		{
			namespace F = HutongCanon::Frame;
			const double OverFrame = (F::BoardHeight + F::PurlinDiameter + F::RafterDiameter + F::RoofCover) * FMath::Max(ColumnDiameter, 0.0);
			return FMath::Max(OverFrame - FMath::Max(EaveJu, 0.0) * FMath::Max(Overhang, 0.0), 0.0);
		}

		// The roof's underside at the column line, above its base: the slope's rise over the overhang less
		// the cover, i.e. the rafters' tops on the 檐檁. Zero where the roof has no lift.
		inline double UndersideRise(double ColumnDiameter, double Overhang, double EaveJu)
		{
			if (RoofLift(ColumnDiameter, Overhang, EaveJu) <= 0.0) return 0.0;
			return FMath::Max(EaveJu, 0.0) * FMath::Max(Overhang, 0.0) - HutongCanon::Frame::RoofCover * FMath::Max(ColumnDiameter, 0.0);
		}

		inline double SafeR(double R) { return FMath::Max(R, 1.0); }
		inline double SafeK(double K) { return FMath::Max(K, 0.0); }
		inline double SafeA(double A, double R) { return FMath::Clamp(A, 0.1, 0.9 * SafeR(R)); }

		// 臺明 and 柱高 share one eave, so solve together: Floor = K·Eave/(R+K).
		inline double FloorFromEave(double Eave, double R, double K)
		{
			const double Rr = SafeR(R), Kk = SafeK(K);
			return FMath::Max(Eave, 1.0) * Kk / (Rr + Kk);
		}

		// Inverse: eave for a given 柱高.
		inline double EaveFromColumn(double ColumnHeight, double R, double K)
		{
			const double Rr = SafeR(R), Kk = SafeK(K);
			return ColumnHeight * (Rr + Kk) / Rr;
		}


		// 柱徑 = 柱高 / R.
		inline double ColumnDiameter(double ColumnHeight, double R)
		{
			return FMath::Max(ColumnHeight / SafeR(R), 2.0);
		}


		// 檐柱高 = 8/10 明間面闊, floored for 耳房 scale.
		inline double ColumnFromCentralBay(double CentralBayWidth, double PerBay, double MinColumn)
		{
			return FMath::Max(
				FMath::Clamp(PerBay, 0.4, 1.5) * CentralBayWidth,
				FMath::Max(MinColumn, 1.0));
		}

		// 上檐出 as a share of 柱高.
		inline double EaveOverhang(double ColumnHeight, double Ratio)
		{
			return FMath::Max(ColumnHeight * Ratio, 0.0);
		}

		// 墀頭 projection, floored to cover the corner column centred on the wall plane; else its
		// foot shows through as a wedge.
		inline double ChitouProjection(double Requested, double ColumnRadius)
		{
			return FMath::Max(Requested,
				FMath::Max(ColumnRadius, 0.0) + HutongCanon::Wall::ChitouColumnClearanceCm);
		}

		// 額枋 underside: 柱高 less its depth (則例).
		inline double ArchitraveBottom(double FloorHeight, double ColumnHeight, double R, double A)
		{
			const double Rr = SafeR(R);
			return FloorHeight + ColumnHeight * (1.0 - SafeA(A, Rr) / Rr);
		}

		// The rail at the 檐枋's line (上檻, or 中檻 under a 前廊's 橫陂), RailInD 柱徑 deep (清式營造則例 表十三):
		// one member across the bay, so window and leaf heads match.
		inline double MiddleRail(double ArchitraveBottomZ, double ColumnDiameter, double RailInD)
		{
			return ArchitraveBottomZ - FMath::Max(RailInD, 0.0) * FMath::Max(ColumnDiameter, 0.0);
		}

		// Share of 柱高 clear between the 下檻's top and the rail's underside: 柱徑 = 柱高 / R, so the 額枋 (A), the
		// rail and the 下檻 (all in 柱徑) come off the column in proportion.
		inline double ClearShare(double R, double A, double RailInD, double SillInD)
		{
			const double Rr = SafeR(R);
			return FMath::Max(1.0 - (SafeA(A, Rr) + FMath::Max(RailInD, 0.0) + FMath::Max(SillInD, 0.0)) / Rr, 0.15);
		}

		// Lowest eave leaving Need clear between the 下檻 and the rail. The whole stack scales with the eave, so only
		// the eave buys headroom.
		inline double MinEaveForHeadroom(double Need, double R, double K, double A, double RailInD, double SillInD)
		{
			return EaveFromColumn(Need / ClearShare(R, A, RailInD, SillInD), SafeR(R), K);
		}

		// The same on a floor held at FloorHeight (a house raised to a court walk's floor): the column stands on it,
		// so the headroom is bought above it.
		inline double MinEaveForHeadroomOnFloor(double Need, double FloorHeight, double R, double A, double RailInD, double SillInD)
		{
			return FloorHeight + Need / ClearShare(R, A, RailInD, SillInD);
		}

		// Elsewhere: 進深 and 舉 sequence in HutongJiajia.h (DepthFor, MakeSection, DefaultRatios);
		// bays in HutongBays.h (ComputeBayCount, BayBoundary).
	}
}
