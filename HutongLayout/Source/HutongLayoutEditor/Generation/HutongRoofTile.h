#pragma once

#include "CoreMinimal.h"
#include "HutongRoofTile.generated.h"

// What the roof is tiled with.
UENUM(BlueprintType)
enum class EHutongRoofTile : uint8
{
	He      UMETA(DisplayName = "Flat Tiles (合瓦) — commoner", ToolTip="Flat tiles laid alternately hollow-up and hollow-down, with a flat eave course."),

	Tong    UMETA(DisplayName = "Tube Tiles (筒瓦) — rank", ToolTip="Half-round tiles over the joints, ending at the eave in round eave caps (勾頭)."),
};

namespace HutongGen
{
	namespace RoofTile
	{
		inline bool HasEaveCaps(EHutongRoofTile Tile) { return Tile == EHutongRoofTile::Tong; }

		// One 壟, which sets the 勾頭 pitch along an eave.
		inline constexpr double DefaultRowSpacing = 20.0;

		// Sides on one 勾頭.
		inline constexpr int32 EaveCapSides = 6;
	}
}
