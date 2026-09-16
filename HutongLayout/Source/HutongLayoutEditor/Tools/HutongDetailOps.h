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

	// Re-bakes each building from the parameters it is already carrying, changing nothing about the placement.
	int32 Rebuild(const TArray<UHutongBuildingComponent*>& Buildings);

	// Builds every plan-only building in the list.
	int32 GeneratePlanned(const TArray<UHutongBuildingComponent*>& Buildings);

	// Takes the geometry off every built building in the list, leaving its footprint outline; the
	// parameters stay on the component, so Generate builds the same building again.
	int32 RevertToPlan(const TArray<UHutongBuildingComponent*>& Buildings);

	// One thing a placed building can be turned into. A class with variants in it contributes one
	// target per variant, since 院牆 and 隔牆 are two answers to "what is this run" and not one
	// answer plus an edit.
	struct FConvertTarget
	{
		FString Label;
		UClass* Class = nullptr;
		FName Variant = NAME_None;

		// What the mode panel's dropdown carries: the label is the option, so it has to name the
		// target on its own.
		bool IsValid() const { return Class != nullptr; }
	};

	// Every type a building can be converted into, built by walking the component classes rather
	// than from a list a fifteenth type would have to be added to.
	const TArray<FConvertTarget>& ConvertTargets();

	// The target a dropdown label names, invalid when nothing matches.
	FConvertTarget FindConvertTarget(const FString& Label);

	// Replaces each building's component with the target type's, keeping what the *placement*
	// decided — where it stands, its footprint and run axis, which side is the facade, its detail
	// level, its palette and its id — and taking everything else from the target preset, or from
	// the target type's defaults where no preset is named. Rebuilds each in place.
	int32 Convert(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& Target,
		const FString& Preset);

	// One building, without the transaction or the slow task the button wraps the run in — which is
	// also the half a headless test can call. Returns the component that replaced the old one.
	UHutongBuildingComponent* ConvertBuilding(UHutongBuildingComponent* Building,
		const FConvertTarget& Target, const FString& Preset);

	// ---- Dividing a building at a bay line, and fusing two that stand end to end ----
	//
	// A building drawn as one footprint across two compounds is two buildings: a five-bay row
	// becomes a two and a three, each with its own gable at the cut and its own door. The reverse
	// fuses two that stand end to end on one line into one building, whose interior wall goes.
	// Both are generic over any type that forces a bay count (house, 鋪面房, 樓, 殿): the count
	// is found by reflection, the rest is footprint arithmetic on the base class.

	// Whether this building has bay lines to divide at and a bay count to carry.
	bool CanDivide(const UHutongBuildingComponent* Building);

	// The bay count a building is forcing, or zero when it derives one; INDEX_NONE when it has no such field.
	int32 GetBayCountOverride(const UHutongBuildingComponent* Building);
	bool SetBayCountOverride(UHutongBuildingComponent* Building, int32 Count);

	// Divides at the bay line with this index in GetPlanBays order (1 .. bays - 1). The building
	// keeps the bays before the line and its own end; a new actor of the same type, parameters,
	// palette, level and folder takes the rest, with a fresh id. Both rebuild. Caller opens the
	// transaction. Returns the new component, or null with the reason.
	UHutongBuildingComponent* DivideBuilding(UHutongBuildingComponent* Building, int32 BayLine,
		FText& OutWhyNot);

	// Where the two would join if fused: Other stands on Building's line, same type, same facing,
	// same depth, end to end within a few centimetres. bAtEnd says which end of Building it is on.
	bool CanFuse(const UHutongBuildingComponent* Building, const UHutongBuildingComponent* Other,
		bool& bOutAtEnd, FText* OutWhyNot = nullptr);

	// Fuses Other into Building: one footprint spanning both, the bay counts summed, the outer
	// ends' corner offsets kept, Other's actor destroyed. Building keeps its parameters and id.
	// Caller opens the transaction.
	bool FuseBuildings(UHutongBuildingComponent* Building, UHutongBuildingComponent* Other, FText& OutWhyNot);

	// The panel's button over the level-editor selection of exactly two buildings.
	bool FuseSelected(FText& OutMessage);
}
