#pragma once

#include "CoreMinimal.h"

namespace HutongGen
{
	// How many bays a facade of this length wants.
	inline int32 ComputeBayCount(double Width, double MinBay, double MaxBay)
	{
		const double W = FMath::Max(Width, 1.0);
		const double MinB = FMath::Max(MinBay, 1.0);
		const double MaxB = FMath::Max(MaxBay, MinB);

		int32 N = FMath::Max(1, FMath::CeilToInt32(W / MaxB));
		if ((W / N) < MinB)
		{
			N = FMath::Max(1, FMath::FloorToInt32(W / MinB));
		}
		return FMath::Max(1, N);
	}

	// 明間面闊: the wide bay plus (N-1) 次間 fill the frontage, so the wide width falls out. What
	// BayBoundary lays the posts on, so nothing else may spell it out again.
	inline double CentralBayWidth(double Span, int32 BayCount, double SideRatio)
	{
		const int32 N = FMath::Max(BayCount, 1);
		return FMath::Max(Span, 1.0) / (1.0 + (N - 1) * FMath::Clamp(SideRatio, 0.3, 1.0));
	}

	// X of bay boundary Index, and the single source of truth for 明間/次間 spacing.
	inline double BayBoundary(int32 Index, int32 BayCount, double Span,
		double EndInset, double SideRatio, int32 WideBayIndex)
	{
		if (BayCount <= 0)
		{
			return 0.0;
		}
		Index = FMath::Clamp(Index, 0, BayCount);

		const double Ratio = FMath::Clamp(SideRatio, 0.3, 1.0);
		const double WideWidth = CentralBayWidth(Span, BayCount, Ratio);

		double X = 0.0;
		for (int32 i = 0; i < Index; ++i)
		{
			X += (i == WideBayIndex) ? WideWidth : WideWidth * Ratio;
		}

		if (Index == 0)              X += EndInset;
		else if (Index == BayCount)  X -= EndInset;
		return X;
	}
}
