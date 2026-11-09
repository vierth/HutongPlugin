#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"

// R = 柱高, K = 臺明高, A = 額枋高, all in 柱徑; T = 橫披窗 share of the opening.
// Each function clamps its own args, so user-edited fields pass straight in.
namespace HutongGen
{
	namespace Proportions
	{
		// Roof base over the 檐柱 tops (圖5-3-1): 墊板, 檐檁, rafters and cover, less the slope's rise over the overhang.
		inline double RoofLift(double ColumnDiameter, double Overhang, double EaveJu)
		{
			namespace F = HutongCanon::Frame;
			const double OverFrame = (F::BoardHeight + F::PurlinDiameter + F::RafterDiameter + F::RoofCover) * FMath::Max(ColumnDiameter, 0.0);
			return FMath::Max(OverFrame - FMath::Max(EaveJu, 0.0) * FMath::Max(Overhang, 0.0), 0.0);
		}

		// Rafter tops on the 檐檁, above the roof's base; zero where the roof has no lift.
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

		inline double EaveFromColumn(double ColumnHeight, double R, double K)
		{
			const double Rr = SafeR(R), Kk = SafeK(K);
			return ColumnHeight * (Rr + Kk) / Rr;
		}


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

		inline double EaveOverhang(double ColumnHeight, double Ratio)
		{
			return FMath::Max(ColumnHeight * Ratio, 0.0);
		}

		// 墀頭 projection, floored to cover the corner column centred on the wall plane, foot included.
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

		// The rail at the 檐枋's line, RailInD 柱徑 deep (清式營造則例 表十三).
		inline double MiddleRail(double ArchitraveBottomZ, double ColumnDiameter, double RailInD)
		{
			return ArchitraveBottomZ - FMath::Max(RailInD, 0.0) * FMath::Max(ColumnDiameter, 0.0);
		}

		inline double ClearShare(double R, double A, double RailInD, double SillInD)
		{
			const double Rr = SafeR(R);
			return FMath::Max(1.0 - (SafeA(A, Rr) + FMath::Max(RailInD, 0.0) + FMath::Max(SillInD, 0.0)) / Rr, 0.15);
		}

		// The whole stack scales with the eave, so only the eave buys headroom.
		inline double MinEaveForHeadroom(double Need, double R, double K, double A, double RailInD, double SillInD)
		{
			return EaveFromColumn(Need / ClearShare(R, A, RailInD, SillInD), SafeR(R), K);
		}

		inline double MinEaveForHeadroomOnFloor(double Need, double FloorHeight, double R, double A, double RailInD, double SillInD)
		{
			return FloorHeight + Need / ClearShare(R, A, RailInD, SillInD);
		}
	}
}
