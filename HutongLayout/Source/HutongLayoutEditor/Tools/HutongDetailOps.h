#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongDetail.h"
#include "Generation/BaySide.h"

class UHutongBuildingComponent;
class UWorld;

namespace HutongDetailOps
{
	TArray<UHutongBuildingComponent*> Unlocked(const TArray<UHutongBuildingComponent*>& Buildings);

	TArray<UHutongBuildingComponent*> CollectSelected();

	UWorld* EditorWorld();

	TArray<UHutongBuildingComponent*> CollectLoaded(UWorld* World);

	// Loads what World Partition unloaded, kept loaded until teardown so minted ids save; slow, for Export All.
	TArray<UHutongBuildingComponent*> CollectAll(UWorld* World);

	int32 SetLevel(const TArray<UHutongBuildingComponent*>& Buildings, EHutongDetail Level);

	int32 Rebuild(const TArray<UHutongBuildingComponent*>& Buildings);

	int32 GeneratePlanned(const TArray<UHutongBuildingComponent*>& Buildings, bool bAsk = true);

	int32 RevertToPlan(const TArray<UHutongBuildingComponent*>& Buildings);

	struct FConvertTarget
	{
		FString Label;
		UClass* Class = nullptr;
		FName Variant = NAME_None;
		int32 Group = INDEX_NONE;
		int32 Family = INDEX_NONE;

		bool IsValid() const { return Class != nullptr; }
	};

	// The Unknown type's family: exchanges with every other.
	inline constexpr int32 UnknownFamily = 100;

	const TArray<FConvertTarget>& ConvertTargets();

	FText ConvertGroupName(int32 Group);

	bool CanExchange(const FConvertTarget& From, const FConvertTarget& To);
	bool CanAllBecome(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& To);

	FConvertTarget FindConvertTarget(const FString& Label);

	int32 FindConvertTargetIndex(const UHutongBuildingComponent* Building);

	FName PresetKeyOf(const FConvertTarget& Target);

	UHutongBuildingComponent* MakeAsDrawn(const UHutongBuildingComponent& Placed, UClass* Class, FName Variant,
		const FString& Preset);

	TArray<FString> CustomizedFields(const UHutongBuildingComponent* Building);

	// Rebuild is the caller's.
	bool ApplyPresetAsDrawn(UHutongBuildingComponent* Building, const FString& Preset);

	// Each opens its own transaction and rebuilds what it changed.

	TArray<UHutongBuildingComponent*> NeedingChange(const TArray<UHutongBuildingComponent*>& Buildings,
		int32 TypeIndex, const FString& Preset);
	TArray<FString> CustomizedAcross(const TArray<UHutongBuildingComponent*>& Buildings);
	int32 ChangeTypeOrPreset(const TArray<UHutongBuildingComponent*>& Buildings, int32 TypeIndex, const FString& Preset);
	bool AdjustBays(const TArray<UHutongBuildingComponent*>& Buildings, int32 Delta);
	bool MoveDoorBay(const TArray<UHutongBuildingComponent*>& Buildings, int32 StepsRight);
	bool SetFacing(const TArray<UHutongBuildingComponent*>& Buildings, const FText& Title,
		TFunctionRef<EHutongBaySide(const UHutongBuildingComponent&, EHutongBaySide)> NextSide);
	bool TurnFacing(const TArray<UHutongBuildingComponent*>& Buildings, int32 Delta);
	bool FlipFacing(const TArray<UHutongBuildingComponent*>& Buildings);
	bool ToggleGate(const TArray<UHutongBuildingComponent*>& Buildings);
	bool AddGate(const TArray<UHutongBuildingComponent*>& Buildings);
	void RebuildEdited(UHutongBuildingComponent* Building);

	int32 Convert(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& Target,
		const FString& Preset);

	// Skips the button's transaction and slow task, for headless tests.
	UHutongBuildingComponent* ConvertBuilding(UHutongBuildingComponent* Building,
		const FConvertTarget& Target, const FString& Preset);

	bool CanDivide(const UHutongBuildingComponent* Building);

	// Zero when it derives one; INDEX_NONE when it has no such field.
	int32 GetBayCountOverride(const UHutongBuildingComponent* Building);
	bool SetBayCountOverride(UHutongBuildingComponent* Building, int32 Count);

	// BayLine 1 .. bays - 1 in GetPlanBays order. Caller opens the transaction.
	UHutongBuildingComponent* DivideBuilding(UHutongBuildingComponent* Building, int32 BayLine,
		FText& OutWhyNot);

	bool CanFuse(const UHutongBuildingComponent* Building, const UHutongBuildingComponent* Other,
		bool& bOutAtEnd, FText* OutWhyNot = nullptr);

	// Caller opens the transaction.
	bool FuseBuildings(UHutongBuildingComponent* Building, UHutongBuildingComponent* Other, FText& OutWhyNot);

	bool FuseSelected(FText& OutMessage);
}
