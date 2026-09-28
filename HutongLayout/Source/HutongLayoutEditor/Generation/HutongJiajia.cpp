#include "Generation/HutongJiajia.h"
#include "Generation/HutongCanon.h"

namespace HutongGen
{
	namespace
	{
		// Segment containing a distance from the ridge.
		void LocateFromRidge(
			const FHutongRoofSection& S, double DistanceFromRidge,
			int32& OutSegment, double& OutRunIntoSegment)
		{
			const double Span = S.HalfSpan();
			const double FromEave = FMath::Clamp(Span - DistanceFromRidge, 0.0, Span);

			double Walked = 0.0;
			for (int32 i = 0; i < S.Run.Num(); ++i)
			{
				if (FromEave <= Walked + S.Run[i] || i == S.Run.Num() - 1)
				{
					OutSegment = i;
					OutRunIntoSegment = FMath::Clamp(FromEave - Walked, 0.0, S.Run[i]);
					return;
				}
				Walked += S.Run[i];
			}
			OutSegment = 0;
			OutRunIntoSegment = 0.0;
		}

		// Height above eave and local slope at a distance from the ridge.
		void PolylineAt(
			const FHutongRoofSection& S, double DistanceFromRidge,
			double& OutHeight, double& OutSlope)
		{
			int32 Seg = 0;
			double Into = 0.0;
			LocateFromRidge(S, DistanceFromRidge, Seg, Into);

			double Z = 0.0;
			for (int32 i = 0; i < Seg; ++i) Z += S.Run[i] * S.Ju[i];
			Z += Into * S.Ju[Seg];

			OutHeight = Z;
			// Negated: distance-from-ridge runs opposite to the run.
			OutSlope = -S.Ju[Seg];
		}
	}

	double FHutongRoofSection::HeightAtDistanceFromRidge(double DistanceFromRidge) const
	{
		if (!IsValid()) return 0.0;

		const double Span = HalfSpan();
		const double D = FMath::Clamp(DistanceFromRidge, 0.0, Span);
		const double Roll = FMath::Clamp(ApexRoll, 0.0, 1.0) * Span;

		double H = 0.0, Slope = 0.0;
		if (Roll <= 0.0 || D >= Roll)
		{
			PolylineAt(*this, D, H, Slope);
			return H;
		}

		// 捲棚: Hermite from the band edge (matching value and slope) to a level crown. Crown sits half
		// the band's climb below the fold; at fold height it bulged above both slopes.
		double JoinH = 0.0, JoinSlope = 0.0;
		PolylineAt(*this, Roll, JoinH, JoinSlope);

		const double Crown = JoinH - 0.5 * JoinSlope * Roll;
		const double s = D / Roll;
		const double s2 = s * s, s3 = s2 * s;
		const double H00 = 2.0 * s3 - 3.0 * s2 + 1.0;
		const double H01 = -2.0 * s3 + 3.0 * s2;
		const double H11 = s3 - s2;
		// h10 drops out: crown tangent is zero.
		return H00 * Crown + H01 * JoinH + H11 * JoinSlope * Roll;
	}

	double FHutongRoofSection::HeightFraction(double NormalizedDistanceFromRidge) const
	{
		const double R = Rise();
		if (R <= 0.0) return 0.0;
		return HeightAtDistanceFromRidge(
			FMath::Clamp(NormalizedDistanceFromRidge, 0.0, 1.0) * HalfSpan()) / R;
	}

	namespace Jiajia
	{
		int32 PurlinCount(EHutongPurlins Purlins)
		{
			switch (Purlins)
			{
			case EHutongPurlins::Three: return 3;
			case EHutongPurlins::Seven: return 7;
			case EHutongPurlins::Nine:  return 9;
			default:                    return 5;
			}
		}

		int32 StepsPerSide(EHutongPurlins Purlins)
		{
			return (PurlinCount(Purlins) - 1) / 2;
		}

		// constexpr canon array → TArray.
		template <SIZE_T N>
		TArray<double> Ratios(const double (&Canon)[N])
		{
			return TArray<double>(Canon, (int32)N);
		}

		TArray<double> DefaultRatios(EHutongPurlins Purlins)
		{
			switch (Purlins)
			{
			case EHutongPurlins::Three: return Ratios(HutongCanon::Roof::JuThree);
			case EHutongPurlins::Seven: return Ratios(HutongCanon::Roof::JuSeven);
			case EHutongPurlins::Nine:  return Ratios(HutongCanon::Roof::JuNine);
			default:                    return Ratios(HutongCanon::Roof::JuFive);
			}
		}

		double DepthFor(EHutongPurlins Purlins, double StepRun)
		{
			return (PurlinCount(Purlins) - 1) * FMath::Max(StepRun, 1.0);
		}

		FHutongRoofSection MakeSectionWithRatios(
			const TArray<double>& Ratios, double HalfDepth, double EaveOverhang, double ApexRoll)
		{
			FHutongRoofSection S;
			S.ApexRoll = FMath::Clamp(ApexRoll, 0.0, 1.0);

			const int32 N = FMath::Max(Ratios.Num(), 1);
			const double Steps = FMath::Max(HalfDepth, 1.0) / N;
			const double Over = FMath::Max(EaveOverhang, 0.0);

			// Overhang takes the eave step's 舉.
			if (Over > 0.0)
			{
				S.Run.Add(Over);
				S.Ju.Add(Ratios.IsValidIndex(0) ? Ratios[0] : 0.5);
			}
			for (int32 i = 0; i < N; ++i)
			{
				S.Run.Add(Steps);
				S.Ju.Add(Ratios.IsValidIndex(i) ? FMath::Clamp(Ratios[i], 0.05, 3.0) : 0.5);
			}
			return S;
		}

		FHutongRoofSection MakeSection(
			EHutongPurlins Purlins, double HalfDepth, double EaveOverhang, double ApexRoll)
		{
			return MakeSectionWithRatios(DefaultRatios(Purlins), HalfDepth, EaveOverhang, ApexRoll);
		}
	}
}
