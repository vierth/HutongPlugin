#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"

// The 小式 proportion rules as arithmetic over plain scalars, in one place. Every params struct's
// Get* accessor forwards here rather than spelling a rule out again, so there is one answer to
// what 柱高 is and one place a rule can be checked or changed. The figures the rules are applied
// to live in HutongCanon.h.
//
// R is 柱高 in 柱徑, K is 臺明高 in 柱徑, A is 額枋高 in 柱徑, T is the 橫披窗 band as a share of
// the opening. Each function clamps its own arguments, so a caller may pass a user-edited field
// straight in.
namespace HutongGen
{
	namespace Proportions
	{
		inline double SafeR(double R) { return FMath::Max(R, 1.0); }
		inline double SafeK(double K) { return FMath::Max(K, 0.0); }
		inline double SafeA(double A, double R) { return FMath::Clamp(A, 0.1, 0.9 * SafeR(R)); }
		inline double SafeTransom(double T) { return FMath::Clamp(T, 0.0, 0.5); }

		// 臺明高 = K 柱徑 and 柱高 = R 柱徑 are measured off the same eave, so the two solve together:
		// Floor = K·Eave/(R+K). Set separately, one eats the other.
		inline double FloorFromEave(double Eave, double R, double K)
		{
			const double Rr = SafeR(R), Kk = SafeK(K);
			return FMath::Max(Eave, 1.0) * Kk / (Rr + Kk);
		}

		// The same solve read the other way: the eave a given 柱高 stands its platform under.
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


		// 檐柱高 = 8/10 明間面闊, floored: the rule cannot hold all the way down to a 耳房.
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

		// 墀頭 projection, floored so the pier covers the corner column whose centre sits on the
		// wall plane: the column's foot otherwise shows through the pier's face as a wedge that
		// the 收分 taper closes higher up.
		inline double ChitouProjection(double Requested, double ColumnRadius)
		{
			return FMath::Max(Requested,
				FMath::Max(ColumnRadius, 0.0) + HutongCanon::Wall::ChitouColumnClearanceCm);
		}

		// 額枋's underside: 柱高 less the beam's own depth, which is what 則例 states.
		inline double ArchitraveBottom(double FloorHeight, double ColumnHeight, double R, double A)
		{
			const double Rr = SafeR(R);
			return FloorHeight + ColumnHeight * (1.0 - SafeA(A, Rr) / Rr);
		}

		// 中檻: one member across the bay, so the window head and the leaf head are the same height.
		inline double MiddleRail(double ArchitraveBottomZ, double FloorHeight, double TransomFraction)
		{
			const double Opening = FMath::Max(ArchitraveBottomZ - FloorHeight, 1.0);
			return ArchitraveBottomZ - SafeTransom(TransomFraction) * Opening;
		}

		// The lowest eave that still leaves Need of clear height under the 中檻. Nothing below the
		// eave can buy headroom — the whole stack is a fraction of 柱高 and 柱高 a fraction of the
		// eave — so this solves for the eave with every other term cancelling.
		inline double MinEaveForHeadroom(double Need, double R, double K, double A, double TransomFraction)
		{
			const double Rr = SafeR(R);
			const double ToRail = (1.0 - SafeA(A, Rr) / Rr) * (1.0 - SafeTransom(TransomFraction));
			return EaveFromColumn(Need / FMath::Max(ToRail, 0.15), Rr, K);
		}

		// 進深 = (檁數 - 1) × 步架, and the 舉 sequence over it. Both live in HutongJiajia.h; they
		// are named here so a reader looking for the roof rule finds where it went.
		//   Jiajia::DepthFor, Jiajia::MakeSection, Jiajia::DefaultRatios
		//
		// Bay spacing is HutongBays.h, included above:
		//   ComputeBayCount, BayBoundary
	}
}
