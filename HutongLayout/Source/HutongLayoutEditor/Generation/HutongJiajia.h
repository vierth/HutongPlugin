#pragma once

#include "CoreMinimal.h"
#include "HutongJiajia.generated.h"

// 檁數: sets both depth and roof profile.
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
	// 舉架 roof section: a polyline, not a curve.
	struct FHutongRoofSection
	{
		// Eave to ridge; segment 0 is the overhang if any.
		TArray<double> Run;

		// 舉 per segment: rise over its own run.
		TArray<double> Ju;

		// 捲棚/過壟脊: apex rounding band, as a fraction of the half span.
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

		// Height above the eave line at a distance from the ridge.
		double HeightAtDistanceFromRidge(double DistanceFromRidge) const;
		// Top of the roof: the fold, or a 捲棚's crown below it.
		double CrownHeight() const { return HeightAtDistanceFromRidge(0.0); }
		// Crown / fold rise; 1 when sharp.
		double CrownFactor() const { const double R = Rise(); return R > 0.0 ? CrownHeight() / R : 1.0; }

		// Profile as a fraction of total rise.
		double HeightFraction(double NormalizedDistanceFromRidge) const;

		bool IsValid() const { return Run.Num() > 0 && Run.Num() == Ju.Num() && HalfSpan() > 0.0; }
	};

	namespace Jiajia
	{
		int32 PurlinCount(EHutongPurlins Purlins);

		// 步架 per side: 2 for 五檁.
		int32 StepsPerSide(EHutongPurlins Purlins);

		// Canonical 舉 sequence, eave first.
		TArray<double> DefaultRatios(EHutongPurlins Purlins);

		// The eave step's 舉, which the overhang takes too.
		inline double EaveJu(EHutongPurlins Purlins)
		{
			const TArray<double> R = DefaultRatios(Purlins);
			return R.Num() > 0 ? R[0] : 0.5;
		}

		// The eave step's 舉 once the section is scaled to the rise actually built (a fixed RoofRise).
		inline double BuiltEaveJu(const FHutongRoofSection& S, double BuiltRise)
		{
			const double R = S.Rise();
			return (S.Ju.Num() > 0 && R > 0.0) ? S.Ju[0] * FMath::Max(BuiltRise, 0.0) / R : 0.5;
		}

		// (檁數 - 1) × 步架, both sides.
		double DepthFor(EHutongPurlins Purlins, double StepRun);

		// Half a gable roof: overhang, then the 步架.
		FHutongRoofSection MakeSection(
			EHutongPurlins Purlins, double HalfDepth, double EaveOverhang, double ApexRoll);

		// With explicit ratios, for a non-canonical pitch.
		FHutongRoofSection MakeSectionWithRatios(
			const TArray<double>& Ratios, double HalfDepth, double EaveOverhang, double ApexRoll);
	}
}
