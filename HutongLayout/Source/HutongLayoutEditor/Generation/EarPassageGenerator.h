#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongDetail.h"
#include "EarPassageGenerator.generated.h"

UENUM(BlueprintType)
enum class EHutongEarPassage : uint8
{
	None UMETA(DisplayName="None", ToolTip="The room alone, across the whole frontage."),
	AtEnd UMETA(DisplayName="At One End", ToolTip="A covered passage takes one end of the frontage, the room the rest."),
	Whole UMETA(DisplayName="Whole Frontage (一間過道)", ToolTip="The whole frontage is a one-bay passage, walled on both sides."),
};

// 耳房, alone or with a 過道: a low room on a hall's flank, beside (or as) a covered way to the boundary.
USTRUCT(BlueprintType)
struct FHutongEarPassageParams
{
	GENERATED_BODY()

	FHutongEarPassageParams();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room", meta=(HutongBasic, DisplayName="Room (耳房)", HutongHideChildren="bHasFrontVeranda, bHasRearVeranda, VerandaDepth, bHasVerandaEndDoorways, bHasRearHighWindows, RearWindowWidth, RearWindowHeight, RearWindowHeadDrop", ToolTip="The ear room's house parameters."))
	FHutongSiheyuanParams Room;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Passage (過道)", ToolTip="Where a covered passage runs: none, at one end, or the whole frontage."))
	EHutongEarPassage Passageway = EHutongEarPassage::AtEnd;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Passage Width (過道)", EditCondition="Passageway != EHutongEarPassage::None", UIMin="120", UIMax="400", ClampMin="80", Units="cm", ToolTip="Clear width of the passage between wall faces, in cm."))
	double PassageWidth = 240.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Passage At Far End", EditCondition="Passageway != EHutongEarPassage::None", ToolTip="Puts the passage, or its outer wall, at the far end of the frontage."))
	bool bPassageAtFarEnd = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Room's Roof Runs Over It", ToolTip="Runs the ear room's roof over the passage to a gable outer wall."))
	bool bRoofOverPassage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Roof (過道頂)", EditCondition="!bRoofOverPassage", ToolTip="The passage's own low roof, when the room's does not run over it."))
	FHutongPassageParams Passage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Has Outer Wall", ToolTip="Builds the wall along the passage's outer side."))
	bool bHasOuterWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Outer Wall", EditCondition="bHasOuterWall", ToolTip="The wall along the passage's outer side, the full depth of the building."))
	FHutongWallParams OuterWall;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Has Inner Wall", EditCondition="Passageway == EHutongEarPassage::Whole", ToolTip="Adds a wall on the passage's inner side when it spans the whole frontage."))
	bool bHasInnerWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(HutongBasic, DisplayName="Close The Front (隔牆)", ToolTip="The wall across the front of the passage and its doorway."))
	bool bHasClosingWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Closing Wall", EditCondition="bHasClosingWall", ToolTip="The wall closing the passage's front."))
	FHutongWallParams ClosingWall;

	double Width = 740.0;
	double Depth = 340.0;
	int32 BayCountOverride = 0;

	static constexpr double MinRoomWidthCm = 100.0;
	// A drag derives one or two bays and holds at two; three only when asked (some large compounds).
	static constexpr int32 MaxDerivedRoomBays = 2;
	static constexpr int32 MaxRoomBays = 3;

	bool HasPassage() const { return Passageway != EHutongEarPassage::None; }
	bool HasRoom() const { return Passageway != EHutongEarPassage::Whole; }
	double GetOuterWallThickness() const { return HasPassage() && bHasOuterWall ? OuterWall.GetThickness() : 0.0; }
	double GetInnerWallThickness() const { return Passageway == EHutongEarPassage::Whole && bHasInnerWall ? OuterWall.GetThickness() : 0.0; }
	double GetSideWallsThickness() const { return GetOuterWallThickness() + GetInnerWallThickness(); }
	FHutongWallParams FittedClosingWall() const
	{
		FHutongWallParams C = ClosingWall;
		C.bHasGate = false;
		C.FootprintThickness = C.GetThickness();
		// Only a 隔牆 keeps a garden doorway through the pin below.
		if (C.bDeriveFromRole && C.Role != EHutongWallRole::Courtyard) C.Doorway = EHutongWallDoorway::None;
		if (C.Doorway != EHutongWallDoorway::None && C.Doorway != EHutongWallDoorway::Moon)
		{
			const double Carry = FMath::Max(PassageWidth, 60.0) - 2.0 * FMath::Max(C.DoorwaySurroundWidth, 0.0) - FHutongWallParams::DoorwayMasonryCm - 1.0;
			C.DoorwayWidth = FMath::Clamp(C.DoorwayWidth, 20.0, FMath::Max(Carry, 20.0));
		}
		return C;
	}

	// The passage roof bears on the closing wall, so the wall top is the eave.
	double GetPassageEaveHeight() const
	{
		if (bRoofOverPassage) return RoomParams().GetRoofBaseHeight() + RoomParams().GetUndersideRise();
		const double Eave = Passage.GetEaveHeight();
		return bHasClosingWall ? FMath::Max(Eave, FittedClosingWall().GetMinHeight()) : Eave;
	}

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
	bool HasClosingDoorway() const
	{
		if (!HasPassage() || !bHasClosingWall) return false;
		const FHutongWallParams C = ClosingWallParams();
		return C.HasDecorativeDoorway(C.Length);
	}

	double GetMinClearWidth() const
	{
		double Clear = 60.0;
		if (bHasClosingWall)
		{
			const FHutongWallParams C = FittedClosingWall();
			if (C.Doorway != EHutongWallDoorway::None) Clear = FMath::Max(Clear, C.GetDoorwayRunNeed());
		}
		return Clear;
	}
	double GetClearWidth() const
	{
		switch (Passageway)
		{
		case EHutongEarPassage::None:  return 0.0;
		case EHutongEarPassage::Whole: return FMath::Max(Width - GetSideWallsThickness(), GetMinClearWidth());
		default:                       return FMath::Max(PassageWidth, GetMinClearWidth());
		}
	}
	double GetStripWidth() const { return HasPassage() ? GetClearWidth() + GetSideWallsThickness() : 0.0; }
	double GetWidth() const
	{
		switch (Passageway)
		{
		case EHutongEarPassage::None:  return FMath::Max(Width, 1.0);
		case EHutongEarPassage::Whole: return GetStripWidth();
		default:                       return FMath::Max(Width, GetStripWidth() + MinRoomWidthCm);
		}
	}
	double GetSuggestedWidth() const
	{
		if (!HasRoom()) return FMath::Max(PassageWidth, GetMinClearWidth()) + GetSideWallsThickness();
		return Room.SuggestedFrontage > 0.0 ? Room.SuggestedFrontage + GetStripWidth() : 0.0;
	}
	double GetRoomWidth() const { return HasRoom() ? GetWidth() - GetStripWidth() : 0.0; }
	double GetRoomX0() const { return bPassageAtFarEnd || !HasPassage() ? 0.0 : GetStripWidth(); }
	double GetStripX0() const { return bPassageAtFarEnd ? GetWidth() - GetStripWidth() : 0.0; }
	double GetClearX0() const { return bPassageAtFarEnd ? GetStripX0() + GetInnerWallThickness() : GetOuterWallThickness(); }
	double GetOuterWallX0() const { return bPassageAtFarEnd ? GetWidth() - GetOuterWallThickness() : 0.0; }
	double GetInnerWallX0() const { return bPassageAtFarEnd ? GetStripX0() : GetClearX0() + GetClearWidth(); }

	// Over the whole frontage, the one bay the passage takes, so roof and eave are a one-bay ear room's.
	FHutongSiheyuanParams RoomParams() const
	{
		FHutongSiheyuanParams R = Room;
		R.Width = HasRoom() ? GetRoomWidth() : GetWidth();
		R.Depth = FMath::Max(Depth, 1.0);
		R.BayCountOverride = HasRoom() ? FMath::Min(BayCountOverride, MaxRoomBays) : 1;
		if (R.BayCountOverride <= 0 && R.GetBayCount() > MaxDerivedRoomBays) R.BayCountOverride = MaxDerivedRoomBays;
		const bool bRunsOn = bRoofOverPassage && Passageway == EHutongEarPassage::AtEnd;
		R.bRoofRunsOnLow = bRunsOn && !bPassageAtFarEnd;
		R.bRoofRunsOnHigh = bRunsOn && bPassageAtFarEnd;
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
