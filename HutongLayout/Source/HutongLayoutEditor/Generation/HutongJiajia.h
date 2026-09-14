#pragma once

#include "CoreMinimal.h"
#include "HutongJiajia.generated.h"

// 檁數, which is the decision a Qing builder makes: the depth and the roof profile both fall out of it.
UENUM(BlueprintType)
enum class EHutongPurlins : uint8
{
	Three   UMETA(DisplayName = "Three Purlins (三檁), 1 purlin step (步架) per side", ToolTip="One purlin step (步架) per side, giving a single straight slope."),

	Five    UMETA(DisplayName = "Five Purlins (五檁), 2 purlin steps (步架) per side", ToolTip="Two purlin steps (步架) per side."),

	Seven   UMETA(DisplayName = "Seven Purlins (七檁), 3 purlin steps (步架) per side", ToolTip="Three purlin steps (步架) per side."),

	Nine    UMETA(DisplayName = "Nine Purlins (九檁), 4 purlin steps (步架) per side", ToolTip="Four purlin steps (步架) per side."),
};

namespace HutongGen
{
	// 舉架, the Qing roof section: a polyline, not a curve.
	struct FHutongRoofSection
	{
		// Eave to ridge, including the overhang as segment 0 when the caller has one.
		TArray<double> Run;

		// 舉 for each segment: rise divided by that segment's own run.
		TArray<double> Ju;

		// 捲棚/過壟脊: rounds the apex over the last stretch instead of folding it, as a fraction of the half span.
		double ApexRoll = 0.0;

		double HalfSpan() const
		{
			double S = 0.0;
			for (double R : Run) S += R;
			return S;
		}

		double Rise() const
		{
			double Z = 0.0;
			for (int32 i = 0; i < Run.Num() && i < Ju.Num(); ++i) Z += Run[i] * Ju[i];
			return Z;
		}

		// Height above the eave line, measured from the ridge.
		double HeightAtDistanceFromRidge(double DistanceFromRidge) const;
		// Where the roof actually tops out: the fold, or on a 捲棚 the rounded crown below it.
		double CrownHeight() const { return HeightAtDistanceFromRidge(0.0); }
		// The crown as a fraction of the fold's rise, 1 on a sharp section.
		double CrownFactor() const { const double R = Rise(); return R > 0.0 ? CrownHeight() / R : 1.0; }

		// The same profile as a fraction of the total rise.
		double HeightFraction(double NormalizedDistanceFromRidge) const;

		bool IsValid() const { return Run.Num() > 0 && Run.Num() == Ju.Num() && HalfSpan() > 0.0; }
	};

	namespace Jiajia
	{
		int32 PurlinCount(EHutongPurlins Purlins);

		// One less than the purlin count halved and rounded — 2 for 五檁.
		int32 StepsPerSide(EHutongPurlins Purlins);

		// The canonical 舉 sequence, eave first.
		TArray<double> DefaultRatios(EHutongPurlins Purlins);

		// (檁數 - 1) × 步架, both sides.
		double DepthFor(EHutongPurlins Purlins, double StepRun);

		// Half a gable roof: the eave overhang, then the 步架 in from it.
		FHutongRoofSection MakeSection(
			EHutongPurlins Purlins, double HalfDepth, double EaveOverhang, double ApexRoll);

		// The same with the ratios supplied, for a non-canonical pitch.
		FHutongRoofSection MakeSectionWithRatios(
			const TArray<double>& Ratios, double HalfDepth, double EaveOverhang, double ApexRoll);
	}
}
