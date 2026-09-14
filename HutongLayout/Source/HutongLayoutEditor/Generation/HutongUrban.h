#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "HutongUrban.generated.h"

// The street classes of the 元大都 grid the Inner City still ran on in 1750, in 步 so the numbers are checkable.
UENUM(BlueprintType)
enum class EHutongStreetClass : uint8
{
	Alley       UMETA(DisplayName = "Alley (夾道)", ToolTip="Narrower than a lane (胡同)."),

	Hutong      UMETA(DisplayName = "Lane (胡同), 6 Paces (步)", ToolTip="A lane (胡同) about 6 paces (步) wide."),

	MinorStreet UMETA(DisplayName = "Minor Street (小街), 12 Paces (步)", ToolTip="A minor street (小街) about 12 paces (步) wide."),

	MajorStreet UMETA(DisplayName = "Avenue (大街), 24 Paces (步)", ToolTip="An avenue (大街) about 24 paces (步) wide."),

	Open        UMETA(DisplayName = "Wider Than an Avenue (大街)", ToolTip="Wider than an avenue (大街)."),
};

namespace HutongGen
{
	namespace Urban
	{
		// 1 步: 5 尺 at a Yuan 尺 of ~31.6 cm.
		inline constexpr double BuCm = HutongCanon::Urban::PaceCm;

		// Zero for the two open-ended classes.
		double NominalWidth(EHutongStreetClass Class);

		// Lane centre to lane centre.
		inline constexpr double PitchCm = HutongCanon::Urban::LanePitchPaces * BuCm;

		// Which class a measured width falls in, with generous bands.
		EHutongStreetClass Classify(double WidthCm);

		// Whether it is close enough to its class's nominal to be a good example of it.
		bool IsInBand(double WidthCm, double ToleranceCm = HutongCanon::Urban::InBandToleranceCm);

		// The nearest canonical width, or zero when none is within Tolerance.
		double NearestCanonicalWidth(double WidthCm, double ToleranceCm);

		// "9.1 m · 胡同 (6 步) · in band".
		FString DescribeWidth(double WidthCm);

		// "12.4 m · 7.8 步", for anything that is not a street width.
		FString DescribeDistance(double DistanceCm);
	}
}
