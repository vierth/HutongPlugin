#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongDetail.h"
#include "Generation/BaySide.h"

class UHutongBuildingComponent;
class UWorld;

// Promotion and demotion of buildings already placed.
namespace HutongDetailOps
{
	// The building components on the actors the level editor has selected.
	TArray<UHutongBuildingComponent*> CollectSelected();

	// Every building component in the world.
	TArray<UHutongBuildingComponent*> CollectLoaded(UWorld* World);

	// Every building in the level, loading what World Partition has unloaded; those stay loaded until
	// the world is torn down, so an id minted on them can be saved. For Export All only: it is slow.
	TArray<UHutongBuildingComponent*> CollectAll(UWorld* World);

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
		// Kind of building, for grouping a menu (ConvertGroupName).
		int32 Group = INDEX_NONE;
		// Types one may become from the P / T menu share a family (CanExchange).
		int32 Family = INDEX_NONE;

		// The dropdown label; must name the target on its own.
		bool IsValid() const { return Class != nullptr; }
	};

	// The Unknown type's family: exchanges with every other.
	inline constexpr int32 UnknownFamily = 100;

	// Every conversion target, found by walking the component classes, not a hand-kept list.
	const TArray<FConvertTarget>& ConvertTargets();

	// A target group's heading: buildings, gates, walls, ways, temple and pavilion, furnishings.
	FText ConvertGroupName(int32 Group);

	// Whether the P / T menu offers To for a building of type From: roofed buildings, gates, temple and
	// pavilion among themselves; walls; corridor, passage and path; flower bed and water jar.
	bool CanExchange(const FConvertTarget& From, const FConvertTarget& To);
	// Whether every one of them may become To from the menu or keys.
	bool CanAllBecome(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& To);

	// The target a dropdown label names, invalid when nothing matches.
	FConvertTarget FindConvertTarget(const FString& Label);

	// Index in ConvertTargets() of the building's own type (its label, else its class); INDEX_NONE if none.
	int32 FindConvertTargetIndex(const UHutongBuildingComponent* Building);

	// The preset key a target type's presets are saved under.
	FName PresetKeyOf(const FConvertTarget& Target);

	// A throwaway building of the target type (variant, preset) as the drag tool would have left it over
	// Placed's footprint: the preset's values with the drag's own writes on top (ApplyDragDerived).
	UHutongBuildingComponent* MakeAsDrawn(const UHutongBuildingComponent& Placed, UClass* Class, FName Variant,
		const FString& Preset);

	// Parameters on which the building differs from its own preset as drawn over its footprint (type
	// defaults when it names none, or one that no longer exists), as "Group › Field" display names. What
	// a change of preset or type would throw away; values the drag itself gave are not among them.
	TArray<FString> CustomizedFields(const UHutongBuildingComponent* Building);

	// Loads a preset of the building's own type as if it had been drawn with it here; the footprint
	// stays. Rebuild is the caller's. False if the preset does not load.
	bool ApplyPresetAsDrawn(UHutongBuildingComponent* Building, const FString& Preset);

	// ---- Edits on a set of buildings, shared by the viewport keys and the right-click menu ----
	// Each opens its own transaction and rebuilds what it changed; false when none of them applies.

	// Those not already the given type (index into ConvertTargets; INDEX_NONE keeps each one's) and preset.
	TArray<UHutongBuildingComponent*> NeedingChange(const TArray<UHutongBuildingComponent*>& Buildings,
		int32 TypeIndex, const FString& Preset);
	// Every customized field across them (CustomizedFields), each once.
	TArray<FString> CustomizedAcross(const TArray<UHutongBuildingComponent*>& Buildings);
	// Type and preset as the drag tool would have drawn them here (bay count carried). Returns how many changed.
	int32 ChangeTypeOrPreset(const TArray<UHutongBuildingComponent*>& Buildings, int32 TypeIndex, const FString& Preset);
	// One bay fewer or more on each that forces a count (a derived count seeded from the bays drawn).
	bool AdjustBays(const TArray<UHutongBuildingComponent*>& Buildings, int32 Delta);
	// Each facade to NextSide of its current one.
	bool SetFacing(const TArray<UHutongBuildingComponent*>& Buildings, const FText& Title,
		TFunctionRef<EHutongBaySide(const UHutongBuildingComponent&, EHutongBaySide)> NextSide);
	bool TurnFacing(const TArray<UHutongBuildingComponent*>& Buildings, int32 Delta);
	bool FlipFacing(const TArray<UHutongBuildingComponent*>& Buildings);
	// Each wall's opening off, or a 牆垣門 on.
	bool ToggleGate(const TArray<UHutongBuildingComponent*>& Buildings);
	// Built re-bakes, laid-out redraws: the same seam as a panel edit.
	void RebuildEdited(UHutongBuildingComponent* Building);

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
