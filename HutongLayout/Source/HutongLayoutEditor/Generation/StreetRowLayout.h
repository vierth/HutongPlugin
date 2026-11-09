#pragma once

#include "CoreMinimal.h"
#include "Generation/SiheyuanGenerator.h"

// Bays are equal: each piece is its own building and lays out its own 明間/次間.
namespace HutongGen::StreetRow
{
	enum class EPiece : uint8 { Building, Gate };

	struct FPiece
	{
		EPiece Piece = EPiece::Building;
		int32 FirstBay = 0;
		int32 BayCount = 1;
		// Along the run from the row start, cm.
		double From = 0.0;
		double To = 0.0;
	};

	inline double BayWidth(double Length, int32 BayCount)
	{
		return Length / FMath::Max(BayCount, 1);
	}

	inline int32 BayAt(double Along, double Length, int32 BayCount)
	{
		if (Along < 0.0 || Along > Length || Length <= 0.0) return INDEX_NONE;
		return FMath::Clamp((int32)(Along / BayWidth(Length, BayCount)), 0, FMath::Max(BayCount, 1) - 1);
	}

	// One eave derived over the whole run, so every piece stands at one height.
	inline FHutongSiheyuanParams RowHouse(const FHutongSiheyuanParams& House, double Length,
		double Depth, int32 BayCount)
	{
		FHutongSiheyuanParams P = House;
		if (!(P.bDeriveProportions && P.bDeriveEaveFromBays)) return P;
		FHutongSiheyuanParams Whole = House;
		Whole.Width = FMath::Max(Length, 1.0);
		Whole.Depth = FMath::Max(Depth, 1.0);
		Whole.BayCountOverride = FMath::Max(BayCount, 1);
		P.EaveHeight = Whole.GetEaveHeight();
		P.bDeriveEaveFromBays = false;
		return P;
	}

	inline TArray<FPiece> LayOut(double Length, int32 BayCount, const TSet<int32>& GateBays)
	{
		TArray<FPiece> Out;
		const int32 N = FMath::Max(BayCount, 1);
		const double W = BayWidth(Length, N);

		auto Close = [&](FPiece& P, int32 EndBay)
		{
			P.BayCount = EndBay - P.FirstBay;
			P.From = P.FirstBay * W;
			P.To = (EndBay == N) ? Length : EndBay * W;
			Out.Add(P);
		};

		int32 RunStart = INDEX_NONE;
		for (int32 i = 0; i < N; ++i)
		{
			if (GateBays.Contains(i))
			{
				if (RunStart != INDEX_NONE)
				{
					FPiece Run; Run.Piece = EPiece::Building; Run.FirstBay = RunStart;
					Close(Run, i);
					RunStart = INDEX_NONE;
				}
				FPiece Gate; Gate.Piece = EPiece::Gate; Gate.FirstBay = i;
				Close(Gate, i + 1);
			}
			else if (RunStart == INDEX_NONE)
			{
				RunStart = i;
			}
		}
		if (RunStart != INDEX_NONE)
		{
			FPiece Run; Run.Piece = EPiece::Building; Run.FirstBay = RunStart;
			Close(Run, N);
		}
		return Out;
	}
}
