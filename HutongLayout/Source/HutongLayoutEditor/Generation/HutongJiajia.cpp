#include "Generation/HutongJiajia.h"
#include "Generation/HutongCanon.h"

namespace HutongGen
{
	namespace
	{
		// Where a distance from the ridge falls in the section, walking in from the eave.
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

		// Height above the eave line, and the local slope, at a distance measured from the ridge.
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
			// Measured against distance-from-the-ridge, which runs the other way.
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

		// 捲棚: inside the roll band the profile rejoins the polyline at the band's edge with its
		// value and slope, and reaches the crown horizontal. The crown sits where a parabola
		// through that join lands, half the band's climb short of the fold: a fillet that rounds
		// the corner off. Held at the fold's own height it was a bulge above both slopes — a
		// pillow on the ridge, on every rolled roof.
		double JoinH = 0.0, JoinSlope = 0.0;
		PolylineAt(*this, Roll, JoinH, JoinSlope);

		const double Crown = JoinH - 0.5 * JoinSlope * Roll;
		const double s = D / Roll;
		const double s2 = s * s, s3 = s2 * s;
		const double H00 = 2.0 * s3 - 3.0 * s2 + 1.0;
		const double H01 = -2.0 * s3 + 3.0 * s2;
		const double H11 = s3 - s2;
		// The h10 term drops out: the crown tangent is zero, which is the point.
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

		// The canon's arrays are constexpr; the section wants them as a TArray.
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

			// The overhang carries the eave step's own 舉.
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
