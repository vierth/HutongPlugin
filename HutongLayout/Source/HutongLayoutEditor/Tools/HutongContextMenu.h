#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;
class UHutongBuildingComponent;

// The Hutong section of the level editor's right-click menu on selected buildings, and the type and
// preset lists it shares with the P / T menu.
namespace HutongContextMenu
{
	// A pick: index into HutongDetailOps::ConvertTargets() (INDEX_NONE keeps each building's type) and preset.
	using FOnPick = TFunction<void(int32 TypeIndex, const FString& Preset)>;

	// The presets of the buildings' one type (current one checked); a note when they mix types.
	void FillPresetMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, FOnPick OnPick);
	// Every type all of them may become (HutongDetailOps::CanExchange), under group headings; a type
	// with presets opens onto them.
	void FillTypeMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, FOnPick OnPick);

	// The Options submenu's entries for a building of this type, "Group › Field".
	TArray<FString> OptionLabels(const UHutongBuildingComponent* Building);

	// Type or preset on the selection, asking first in a dialog when customized values would be lost.
	void ChangeSelectionAsked(int32 TypeIndex, const FString& Preset);

	void Register();
	void Unregister();
}
