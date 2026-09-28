#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongDetail.h"
#include "EarPassageGenerator.generated.h"

// 耳房 with a 過道: a low room on a hall's flank, beside a covered way to the boundary.
USTRUCT(BlueprintType)
struct FHutongEarPassageParams
{
	GENERATED_BODY()

	FHutongEarPassageParams();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room", meta=(DisplayName="Room (耳房)", ToolTip="The ear room's house parameters."))
	FHutongSiheyuanParams Room;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Passage Width (過道)", UIMin="120", UIMax="400", ClampMin="80", Units="cm", ToolTip="Clear width of the passage, wall face to wall face, in cm."))
	double PassageWidth = 240.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Passage At Far End", ToolTip="Puts the passage at the far end of the frontage."))
	bool bPassageAtFarEnd = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Room's Roof Runs Over It", ToolTip="Carries the ear room's own roof on over the passage to the outer wall, which rises to it as a gable; off gives the passage its own low roof on the walls."))
	bool bRoofOverPassage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Roof (過道頂)", EditCondition="!bRoofOverPassage", ToolTip="The passage's own low roof, when the room's does not run over it."))
	FHutongPassageParams Passage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Has Outer Wall", ToolTip="Builds the wall along the passage's outer side."))
	bool bHasOuterWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Outer Wall", EditCondition="bHasOuterWall", ToolTip="The wall along the passage's outer side, the full depth of the building."))
	FHutongWallParams OuterWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Close The Front (隔牆)", ToolTip="The wall across the front of the passage and its doorway."))
	bool bHasClosingWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Closing Wall", EditCondition="bHasClosingWall", ToolTip="The wall closing the passage's front."))
	FHutongWallParams ClosingWall;

	// Set by the tool from the footprint.
	double Width = 740.0;
	double Depth = 340.0;
	int32 BayCountOverride = 0;

	// Narrowest room; the frontage is held at the strip plus this.
	static constexpr double MinRoomWidthCm = 100.0;

	double GetOuterWallThickness() const { return bHasOuterWall ? OuterWall.GetThickness() : 0.0; }
	// Closing wall with its doorway, narrowed to what the clear way can carry with the wall generator's
	// flanking masonry; not yet pinned or sized.
	FHutongWallParams FittedClosingWall() const
	{
		FHutongWallParams C = ClosingWall;
		C.bHasGate = false;
		C.FootprintThickness = C.GetThickness();
		// A 院牆 carries no garden doorway; the pin below would let one through.
		if (C.bDeriveFromRole && C.Role != EHutongWallRole::Courtyard) C.Doorway = EHutongWallDoorway::None;
		if (C.Doorway != EHutongWallDoorway::None && C.Doorway != EHutongWallDoorway::Moon)
		{
			const double Carry = FMath::Max(PassageWidth, 60.0) - 2.0 * FMath::Max(C.DoorwaySurroundWidth, 0.0) - FHutongWallParams::DoorwayMasonryCm - 1.0;
			C.DoorwayWidth = FMath::Clamp(C.DoorwayWidth, 20.0, FMath::Max(Carry, 20.0));
		}
		return C;
	}

	// The passage roof bears on the closing wall, so the wall top is the eave, raised to clear the
	// wall's doorway head (else the wall pierces the roof).
	double GetPassageEaveHeight() const
	{
		// Under the room's roof carried on, walls stand to the ceiling under it.
		if (bRoofOverPassage) return RoomParams().GetRoofBaseHeight() + RoomParams().GetUndersideRise();
		const double Eave = Passage.GetEaveHeight();
		return bHasClosingWall ? FMath::Max(Eave, FittedClosingWall().GetMinHeight()) : Eave;
	}

	// Closing wall as built: fitted, pinned to the eave, across the clear way.
	FHutongWallParams ClosingWallParams() const
	{
		FHutongWallParams C = FittedClosingWall();
		C.PinHeight(GetPassageEaveHeight());
		C.Length = GetClearWidth();
		// Under the room's roof carried on, the cap stacks above the eave and pierces the tiles.
		if (bRoofOverPassage)
		{
			C.CapSlabHeight = 0.0;
			C.CapRidgeHeight = 0.0;
		}
		return C;
	}
	// Whether the fitted closing wall has its doorway on its actual run.
	bool HasClosingDoorway() const
	{
		if (!bHasClosingWall) return false;
		const FHutongWallParams C = ClosingWallParams();
		return C.HasDecorativeDoorway(C.Length);
	}

	// Clear way, wall face to gable: as asked, or wide enough for the closing wall's doorway (a
	// passage narrower than its doorway is walled shut).
	double GetClearWidth() const
	{
		double Clear = FMath::Max(PassageWidth, 60.0);
		if (bHasClosingWall)
		{
			const FHutongWallParams C = FittedClosingWall();
			if (C.Doorway != EHutongWallDoorway::None) Clear = FMath::Max(Clear, C.GetDoorwayRunNeed());
		}
		return Clear;
	}
	// Strip taken off the frontage: clear way plus the wall it runs along.
	double GetStripWidth() const { return GetClearWidth() + GetOuterWallThickness(); }
	// Frontage as built: at least the strip plus the narrowest room.
	double GetWidth() const { return FMath::Max(Width, GetStripWidth() + MinRoomWidthCm); }
	double GetRoomWidth() const { return GetWidth() - GetStripWidth(); }
	double GetRoomX0() const { return bPassageAtFarEnd ? 0.0 : GetStripWidth(); }
	// Clear way, wall face to gable, in the frontage frame.
	double GetClearX0() const { return bPassageAtFarEnd ? GetWidth() - GetStripWidth() : GetOuterWallThickness(); }
	double GetOuterWallX0() const { return bPassageAtFarEnd ? GetWidth() - GetOuterWallThickness() : 0.0; }
	double GetStripX0() const { return bPassageAtFarEnd ? GetWidth() - GetStripWidth() : 0.0; }

	// Room params with its share of the footprint.
	FHutongSiheyuanParams RoomParams() const
	{
		FHutongSiheyuanParams R = Room;
		R.Width = GetRoomWidth();
		R.Depth = FMath::Max(Depth, 1.0);
		R.BayCountOverride = BayCountOverride;
		R.bRoofRunsOnLow = bRoofOverPassage && !bPassageAtFarEnd;
		R.bRoofRunsOnHigh = bRoofOverPassage && bPassageAtFarEnd;
		return R;
	}
	FHutongPassageParams PassageParams() const
	{
		FHutongPassageParams P = Passage;
		P.EaveHeight = GetPassageEaveHeight();
		P.Length = FMath::Max(Depth, 1.0);
		P.Width = GetClearWidth();
		return P;
	}

	double GetEaveHeight() const { return RoomParams().GetEaveHeight(); }
	double GetRoofRise() const { return RoomParams().GetRoofRise(); }
};

namespace HutongGen
{
	// Facade on -Y, footprint [0, Width] x [0, Depth]; every level including the block.
	void BuildEarPassage(UE::Geometry::FDynamicMesh3& Mesh, const FHutongEarPassageParams& P,
		EHutongDetail Detail = EHutongDetail::Near);
}
