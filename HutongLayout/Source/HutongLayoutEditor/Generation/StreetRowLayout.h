#pragma once

#include "CoreMinimal.h"
#include "Generation/SiheyuanGenerator.h"

// A street row: one drag divided into equal bays, some of them gates. What the Street Row tool
// spawns is one house or shop per unbroken run of ordinary bays and one gate house per gate
// bay, so the arithmetic is a partition of [0, Length] and nothing else — plain scalars, so
// HutongLayout.Row.Layout runs it without a world.
//
// The bays are equal because the pieces are separate buildings: each house lays its own 明間
// and 次間 out inside its run, and the row's ticks are only where one building ends and the
// next begins.
namespace HutongGen::StreetRow
{
	enum class EPiece : uint8 { Building, Gate };

	struct FPiece
	{
		EPiece Piece = EPiece::Building;
		int32 FirstBay = 0;
		int32 BayCount = 1;
		// Along the run, from the row's start, in cm.
		double From = 0.0;
		double To = 0.0;
	};

	inline double BayWidth(double Length, int32 BayCount)
	{
		return Length / FMath::Max(BayCount, 1);
	}

	// The bay under a point along the run, or INDEX_NONE outside the row.
	inline int32 BayAt(double Along, double Length, int32 BayCount)
	{
		if (Along < 0.0 || Along > Length || Length <= 0.0) return INDEX_NONE;
		return FMath::Clamp((int32)(Along / BayWidth(Length, BayCount)), 0, FMath::Max(BayCount, 1) - 1);
	}

	// The house every ordinary piece is built from. A house derives its eave from its own 明間,
	// so a one-bay piece beside a three-bay piece came out a storey lower: the row is one
	// building cut into pieces and takes one eave, derived once over the whole run and written
	// onto the params the way the height keys write it. Left alone when nothing is derived.
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

	// Gate bays outside the row are ignored. Every bay lands in exactly one piece.
	inline TArray<FPiece> LayOut(double Length, int32 BayCount, const TSet<int32>& GateBays)
	{
		TArray<FPiece> Out;
		const int32 N = FMath::Max(BayCount, 1);
		const double W = BayWidth(Length, N);

		auto Close = [&](FPiece& P, int32 EndBay)
		{
			P.BayCount = EndBay - P.FirstBay;
			P.From = P.FirstBay * W;
			// The last piece lands exactly on the row's end.
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
