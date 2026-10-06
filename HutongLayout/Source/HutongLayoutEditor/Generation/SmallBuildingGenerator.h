#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongBays.h"
#include "SmallBuildingGenerator.generated.h"

// The minor buildings dotting the map: guard posts at lane mouths and bridgeheads, sheds, a lone room, a
// wayside shrine. PROVISIONAL — shaped from general knowledge, not yet on the user's figures (open-todos).

UENUM(BlueprintType)
enum class EHutongSmallRoof : uint8
{
	Gable  UMETA(DisplayName = "Two Slopes (硬山)", ToolTip = "A plain two-slope roof with brick gables."),
	LeanTo UMETA(DisplayName = "One Slope (一面坡)", ToolTip = "A single slope falling to the front from a higher back wall."),
};

UENUM(BlueprintType)
enum class EHutongSmallFront : uint8
{
	Door    UMETA(DisplayName = "Door and Windows", ToolTip = "A brick front with a door in the door bay and a window in each other bay."),
	Open    UMETA(DisplayName = "Open", ToolTip = "Posts at the bay lines and an open front, as a shed or stall."),
	Boarded UMETA(DisplayName = "Boarded", ToolTip = "A front of vertical boards."),
};

USTRUCT(BlueprintType)
struct FHutongSmallBuildingParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(HutongBasic, DisplayName="Roof", ToolTip="Shape of the roof."))
	EHutongSmallRoof Roof = EHutongSmallRoof::Gable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(HutongBasic, DisplayName="Front", ToolTip="What stands across the front."))
	EHutongSmallFront Front = EHutongSmallFront::Door;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(UIMin="180", UIMax="380", ClampMin="120", Units="cm", ToolTip="Height of the eave (the front wall's top) above the ground, in cm."))
	double EaveHeight = 260.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(DisplayName="Floor Height", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="Height of the floor above the ground, in cm."))
	double FloorHeight = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(UIMin="15", UIMax="45", ClampMin="8", Units="cm", ToolTip="Thickness of the walls, in cm."))
	double WallThickness = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="150", UIMax="330", ClampMin="80", Units="cm", ToolTip="Narrowest bay allowed when dividing the frontage, in cm."))
	double MinBayWidth = 220.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="180", UIMax="380", ClampMin="80", Units="cm", ToolTip="Widest bay allowed when dividing the frontage, in cm."))
	double MaxBayWidth = 330.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front", meta=(DisplayName="Door Width", UIMin="60", UIMax="160", ClampMin="40", Units="cm", ToolTip="Clear width of the door, in cm."))
	double DoorWidth = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front", meta=(DisplayName="Windows", ToolTip="A lattice window in each bay without the door."))
	bool bWindows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Rise", UIMin="40", UIMax="200", ClampMin="10", Units="cm", ToolTip="Rise of the roof from eave to its highest point, in cm."))
	double RoofRise = 110.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Overhang", UIMin="10", UIMax="120", ClampMin="0", Units="cm", ToolTip="How far the roof overhangs the front, in cm."))
	double RoofOverhang = 45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Roof Tile (瓦作)", ToolTip="Type of tile laid on the roof."))
	EHutongRoofTile RoofTile = EHutongRoofTile::He;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Models each tile course on the roof as geometry."))
	bool bHasTileRuns = true;

	// A lean-to's slab, and the overhang at its back.
	static constexpr double LeanToSlab = 14.0;
	static constexpr double LeanToRearOverhang = 15.0;

	double GetEaveHeight() const { return FMath::Max(EaveHeight, 120.0); }
	double GetFloorHeight() const { return FMath::Clamp(FloorHeight, 0.0, 0.3 * GetEaveHeight()); }
	// No exposed frame: the roof stands on the wall top.
	double GetRoofLift() const { return 0.0; }
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 10.0); }

	// Set by the component from its footprint, along and across the front.
	double Width = 400.0;
	double Depth = 360.0;
	int32 BayCountOverride = 0;

	int32 GetBayCount() const
	{
		return BayCountOverride > 0 ? BayCountOverride : HutongGen::ComputeBayCount(FMath::Max(Width, 1.0), MinBayWidth, FMath::Max(MaxBayWidth, MinBayWidth));
	}
	// Even bays: a humble building has no 明間 wider than the rest.
	double GetBayBoundary(int32 i, int32 BayCount) const { return FMath::Max(Width, 1.0) * i / FMath::Max(BayCount, 1); }
	int32 GetDoorBay(int32 BayCount) const { return BayCount / 2; }
};

namespace HutongGen
{
	// Front on -Y, width along X. bDetail off: walls, roof and platform only (塊).
	void BuildSmallBuilding(UE::Geometry::FDynamicMesh3& Mesh, const FHutongSmallBuildingParams& P, bool bDetail);
}
