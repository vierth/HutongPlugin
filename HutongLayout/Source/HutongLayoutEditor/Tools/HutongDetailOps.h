#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongDetail.h"

class UHutongBuildingComponent;
class UWorld;

// Promotion and demotion of buildings already placed.
namespace HutongDetailOps
{
	// The building components on the actors the level editor has selected.
	TArray<UHutongBuildingComponent*> CollectSelected();

	// Every building component in the world.
	TArray<UHutongBuildingComponent*> CollectLoaded(UWorld* World);

	// Sets the level on each and re-bakes it.
	int32 SetLevel(const TArray<UHutongBuildingComponent*>& Buildings, EHutongDetail Level);

	// Re-bakes each from its own parameters; placement unchanged.
	int32 Rebuild(const TArray<UHutongBuildingComponent*>& Buildings);

	// Builds every plan-only building in the list.
	int32 GeneratePlanned(const TArray<UHutongBuildingComponent*>& Buildings);

	// Strips geometry from each built building, leaving the outline; parameters stay, so Generate
	// rebuilds the same building.
	int32 RevertToPlan(const TArray<UHutongBuildingComponent*>& Buildings);

	// One conversion target. A class with variants gives one per variant: 院牆 and 隔牆 are two
	// answers to "what is this run", not one plus an edit.
	struct FConvertTarget
	{
		FString Label;
		UClass* Class = nullptr;
		FName Variant = NAME_None;

		// The dropdown label; must name the target on its own.
		bool IsValid() const { return Class != nullptr; }
	};

	// Every conversion target, found by walking the component classes, not a hand-kept list.
	const TArray<FConvertTarget>& ConvertTargets();

	// The target a dropdown label names, invalid when nothing matches.
	FConvertTarget FindConvertTarget(const FString& Label);

	// Replaces each component with the target type's, keeping what the placement decided (position,
	// footprint, run axis, facade side, detail level, palette, id) and taking the rest from the
	// target preset, or the type's defaults if none is named. Rebuilds in place.
	int32 Convert(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& Target,
		const FString& Preset);

	// One building, without the button's transaction and slow task; callable from headless tests.
	// Returns the replacement component.
	UHutongBuildingComponent* ConvertBuilding(UHutongBuildingComponent* Building,
		const FConvertTarget& Target, const FString& Preset);

	// ---- Dividing at a bay line, and fusing two end to end ----
	//
	// A footprint drawn across two compounds becomes two buildings (five bays to two + three), each
	// with its own gable and door at the cut; fusing reverses it and drops the interior wall. Generic
	// over any type forcing a bay count (house, 鋪面房, 樓, 殿): the count is found by reflection, the
	// rest is base-class footprint arithmetic.

	// Whether this building has bay lines to divide at and a bay count to carry.
	bool CanDivide(const UHutongBuildingComponent* Building);

	// The bay count a building is forcing, or zero when it derives one; INDEX_NONE when it has no such field.
	int32 GetBayCountOverride(const UHutongBuildingComponent* Building);
	bool SetBayCountOverride(UHutongBuildingComponent* Building, int32 Count);

	// Divides at bay line Index in GetPlanBays order (1 .. bays - 1). The building keeps the bays
	// before the line; a new actor (same type, params, palette, level, folder; fresh id) takes the
	// rest. Both rebuild. Caller opens the transaction. Returns the new component, or null with the reason.
	UHutongBuildingComponent* DivideBuilding(UHutongBuildingComponent* Building, int32 BayLine,
		FText& OutWhyNot);

	// Where the two would join: Other on Building's line, same type, facing and depth, end to end
	// within a few cm. bAtEnd says which end of Building.
	bool CanFuse(const UHutongBuildingComponent* Building, const UHutongBuildingComponent* Other,
		bool& bOutAtEnd, FText* OutWhyNot = nullptr);

	// Fuses Other into Building: one footprint, bay counts summed, outer corner offsets kept,
	// Other's actor destroyed; Building keeps its params and id. Caller opens the transaction.
	bool FuseBuildings(UHutongBuildingComponent* Building, UHutongBuildingComponent* Other, FText& OutWhyNot);

	// The panel's button over the level-editor selection of exactly two buildings.
	bool FuseSelected(FText& OutMessage);
}
