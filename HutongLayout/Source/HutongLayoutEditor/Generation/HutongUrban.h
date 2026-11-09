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

		double NominalWidth(EHutongStreetClass Class);

		// Lane centre to lane centre.
		inline constexpr double PitchCm = HutongCanon::Urban::LanePitchPaces * BuCm;

		EHutongStreetClass Classify(double WidthCm);

		bool IsInBand(double WidthCm, double ToleranceCm = HutongCanon::Urban::InBandToleranceCm);

		double NearestCanonicalWidth(double WidthCm, double ToleranceCm);

		FString DescribeWidth(double WidthCm);

		FString DescribeDistance(double DistanceCm);
	}
}
