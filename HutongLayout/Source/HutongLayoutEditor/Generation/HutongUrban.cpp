#include "Generation/HutongUrban.h"

namespace HutongGen
{
	namespace Urban
	{
		double NominalWidth(EHutongStreetClass Class)
		{
			switch (Class)
			{
			case EHutongStreetClass::Hutong:      return HutongCanon::Urban::HutongPaces * BuCm;
			case EHutongStreetClass::MinorStreet: return HutongCanon::Urban::MinorStreetPaces * BuCm;
			case EHutongStreetClass::MajorStreet: return HutongCanon::Urban::MajorStreetPaces * BuCm;
			default:                              return 0.0;           // open-ended either end
			}
		}

		EHutongStreetClass Classify(double WidthCm)
		{
			// Boundaries sit between the nominals.
			const double Hutong = NominalWidth(EHutongStreetClass::Hutong);
			const double Minor = NominalWidth(EHutongStreetClass::MinorStreet);
			const double Major = NominalWidth(EHutongStreetClass::MajorStreet);

			if (WidthCm < 0.5 * Hutong)              return EHutongStreetClass::Alley;
			if (WidthCm < 0.5 * (Hutong + Minor))    return EHutongStreetClass::Hutong;
			if (WidthCm < 0.5 * (Minor + Major))     return EHutongStreetClass::MinorStreet;
			if (WidthCm < 1.5 * Major)               return EHutongStreetClass::MajorStreet;
			return EHutongStreetClass::Open;
		}

		bool IsInBand(double WidthCm, double ToleranceCm)
		{
			const double Nominal = NominalWidth(Classify(WidthCm));
			// The open-ended classes have no nominal to be in band with.
			return Nominal > 0.0 && FMath::Abs(WidthCm - Nominal) <= ToleranceCm;
		}

		double NearestCanonicalWidth(double WidthCm, double ToleranceCm)
		{
			double Best = 0.0;
			double BestDelta = ToleranceCm;
			for (const EHutongStreetClass C : { EHutongStreetClass::Hutong,
												EHutongStreetClass::MinorStreet,
												EHutongStreetClass::MajorStreet })
			{
				const double Nominal = NominalWidth(C);
				const double Delta = FMath::Abs(WidthCm - Nominal);
				if (Delta <= BestDelta)
				{
					Best = Nominal;
					BestDelta = Delta;
				}
			}
			return Best;
		}

		namespace
		{
			const TCHAR* ClassName(EHutongStreetClass Class)
			{
				switch (Class)
				{
				case EHutongStreetClass::Alley:       return TEXT("alley (夾道)");
				case EHutongStreetClass::Hutong:      return TEXT("lane (胡同), 6 paces (步)");
				case EHutongStreetClass::MinorStreet: return TEXT("minor street (小街), 12 paces (步)");
				case EHutongStreetClass::MajorStreet: return TEXT("avenue (大街), 24 paces (步)");
				default:                              return TEXT("wider than an avenue (大街)");
				}
			}
		}

		FString DescribeWidth(double WidthCm)
		{
			const EHutongStreetClass C = Classify(WidthCm);
			const double Nominal = NominalWidth(C);

			// The band note is the useful half.
			FString Note;
			if (Nominal <= 0.0)
			{
				Note = TEXT("off the module");
			}
			else if (IsInBand(WidthCm))
			{
				Note = TEXT("in band");
			}
			else
			{
				Note = FString::Printf(TEXT("%+.1f m off nominal"), (WidthCm - Nominal) * 0.01);
			}

			return FString::Printf(TEXT("%.1f m · %s · %s"), WidthCm * 0.01, ClassName(C), *Note);
		}

		FString DescribeDistance(double DistanceCm)
		{
			return FString::Printf(TEXT("%.1f m · %.1f paces (步)"), DistanceCm * 0.01, DistanceCm / BuCm);
		}
	}
}
