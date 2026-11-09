#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;
class UHutongBuildingComponent;

namespace HutongContextMenu
{
	// INDEX_NONE keeps each building's type.
	using FOnPick = TFunction<void(int32 TypeIndex, const FString& Preset)>;

	void FillPresetMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, FOnPick OnPick);
	void FillTypeMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, FOnPick OnPick);

	TArray<FString> OptionLabels(const UHutongBuildingComponent* Building);

	void ChangeSelectionAsked(int32 TypeIndex, const FString& Preset);

	void Register();
	void Unregister();
}
