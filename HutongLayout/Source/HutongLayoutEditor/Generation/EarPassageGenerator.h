#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongDetail.h"
#include "EarPassageGenerator.generated.h"

// 耳房 with a 過道 beside it: the low room against a hall's flank, stopping short of the boundary
// by a covered way through, as the compound lays it out on the gate's side. One footprint: the
// room takes the frontage less the strip; the strip is the clear passage plus the wall it runs
// along, roofed between that wall and the room's gable, and closed across the front by a 隔牆
// with the doorway that is the point of it.
USTRUCT(BlueprintType)
struct FHutongEarPassageParams
{
	GENERATED_BODY()

	FHutongEarPassageParams();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room", meta=(DisplayName="Room (耳房)", ToolTip="The ear room itself: the house generator's parameters, at the 耳房 preset by default."))
	FHutongSiheyuanParams Room;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Passage Width (過道)", UIMin="120", UIMax="400", ClampMin="80", Units="cm", ToolTip="Clear width of the way through, wall face to wall face, in cm. Widened to what the closing wall's doorway needs when that is more."))
	double PassageWidth = 240.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Passage At Far End", ToolTip="Puts the passage at the far end of the frontage instead of the origin end. [ and ] swap it while placing."))
	bool bPassageAtFarEnd = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Roof (過道頂)", ToolTip="The roof over the passage, bearing into the wall on one side and the room's gable on the other; its eave is where the closing wall's top is held."))
	FHutongPassageParams Passage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Has Outer Wall", ToolTip="Builds the wall the passage runs along on its outer side. Off when the building stands against a wall already there."))
	bool bHasOuterWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Outer Wall", EditCondition="bHasOuterWall", ToolTip="The wall along the passage's outer side, the full depth of the building."))
	FHutongWallParams OuterWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Close The Front (隔牆)", ToolTip="A wall across the front of the passage, its top held at the roof's eave, with the doorway it is given through it."))
	bool bHasClosingWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Closing Wall", EditCondition="bHasClosingWall", ToolTip="The wall closing the passage's front. Its doorway and where it sits along the run are its own; its height follows the roof's eave; none stops the way through."))
	FHutongWallParams ClosingWall;

	// Set by the tool from the footprint; not user-editable.
	double Width = 740.0;
	double Depth = 340.0;
	int32 BayCountOverride = 0;

	// A room narrower than this is no room: the frontage is held at the strip plus it.
	static constexpr double MinRoomWidthCm = 100.0;

	double GetOuterWallThickness() const { return bHasOuterWall ? OuterWall.GetThickness() : 0.0; }
	// The closing wall with the doorway it was given, kept to a width the clear way can carry
	// with the masonry the wall generator insists on beside it; not yet pinned or given its length.
	FHutongWallParams FittedClosingWall() const
	{
		FHutongWallParams C = ClosingWall;
		C.bHasGate = false;
		C.FootprintThickness = C.GetThickness();
		// A 院牆 carries no garden doorway, and the pin below would let one through.
		if (C.bDeriveFromRole && C.Role != EHutongWallRole::Courtyard) C.Doorway = EHutongWallDoorway::None;
		if (C.Doorway != EHutongWallDoorway::None && C.Doorway != EHutongWallDoorway::Moon)
		{
			const double Carry = FMath::Max(PassageWidth, 60.0) - 2.0 * FMath::Max(C.DoorwaySurroundWidth, 0.0) - FHutongWallParams::DoorwayMasonryCm - 1.0;
			C.DoorwayWidth = FMath::Clamp(C.DoorwayWidth, 20.0, FMath::Max(Carry, 20.0));
		}
		return C;
	}

	// The passage's roof lands on the wall that closes it, so the wall's top is the roof's
	// eave — raised to what the wall's doorway needs above its head, or the wall would stand
	// through the roof.
	double GetPassageEaveHeight() const
	{
		const double Eave = Passage.GetEaveHeight();
		return bHasClosingWall ? FMath::Max(Eave, FittedClosingWall().GetMinHeight()) : Eave;
	}

	// The closing wall as it is built: fitted, pinned to the eave the roof bears on, across the clear way.
	FHutongWallParams ClosingWallParams() const
	{
		FHutongWallParams C = FittedClosingWall();
		C.PinHeight(GetPassageEaveHeight());
		C.Length = GetClearWidth();
		return C;
	}
	// Whether the closing wall is cut through: the doorway as fitted, on the run it gets.
	bool HasClosingDoorway() const
	{
		if (!bHasClosingWall) return false;
		const FHutongWallParams C = ClosingWallParams();
		return C.HasDecorativeDoorway(C.Length);
	}

	// The clear way through, wall face to gable: what was asked, or what the closing wall's
	// doorway needs to be cut at all — a passage narrower than its own doorway is walled shut.
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
	// The strip the passage takes off the frontage: the clear way plus the wall it runs along.
	double GetStripWidth() const { return GetClearWidth() + GetOuterWallThickness(); }
	// The frontage as built: never less than the strip and the narrowest room.
	double GetWidth() const { return FMath::Max(Width, GetStripWidth() + MinRoomWidthCm); }
	double GetRoomWidth() const { return GetWidth() - GetStripWidth(); }
	double GetRoomX0() const { return bPassageAtFarEnd ? 0.0 : GetStripWidth(); }
	// The clear way, wall face to gable, in the frontage's own frame.
	double GetClearX0() const { return bPassageAtFarEnd ? GetWidth() - GetStripWidth() : GetOuterWallThickness(); }
	double GetOuterWallX0() const { return bPassageAtFarEnd ? GetWidth() - GetOuterWallThickness() : 0.0; }

	// The room's parameters with its own share of the footprint filled in.
	FHutongSiheyuanParams RoomParams() const
	{
		FHutongSiheyuanParams R = Room;
		R.Width = GetRoomWidth();
		R.Depth = FMath::Max(Depth, 1.0);
		R.BayCountOverride = BayCountOverride;
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
	// Facade on -Y, footprint [0, Width] x [0, Depth]; every level, the block included.
	void BuildEarPassage(UE::Geometry::FDynamicMesh3& Mesh, const FHutongEarPassageParams& P,
		EHutongDetail Detail = EHutongDetail::Near);
}
