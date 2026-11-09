#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

// A params-struct field needs meta=(HutongBasic); a field on a property set or component shows unless meta=(HutongAdvanced).
namespace HutongPanel
{
	inline const FName BasicKey(TEXT("HutongBasic"));
	inline const FName AdvancedKey(TEXT("HutongAdvanced"));
	// On a nested params struct: comma-separated fields hidden wherever it is used.
	inline const FName HideChildrenKey(TEXT("HutongHideChildren"));

	inline bool IsHiddenBy(const FProperty& Prop, const FProperty* Parent)
	{
		if (!Parent || !Parent->HasMetaData(HideChildrenKey)) return false;
		TArray<FString> Names;
		Parent->GetMetaData(HideChildrenKey).ParseIntoArray(Names, TEXT(","));
		return Names.ContainsByPredicate([&](const FString& N) { return Prop.GetFName() == FName(*N.TrimStartAndEnd()); });
	}

	inline bool PassesSimple(const FProperty& Prop)
	{
		if (Prop.HasMetaData(AdvancedKey)) return false;
		const UScriptStruct* Owner = Cast<UScriptStruct>(Prop.GetOwnerStruct());
		const bool bOurStruct = Owner && Owner->GetName().StartsWith(TEXT("Hutong"));
		return !bOurStruct || Prop.HasMetaData(BasicKey);
	}

	inline bool IsVisible(const FProperty& Prop, TConstArrayView<const FProperty*> Parents, bool bShowAdvanced)
	{
		for (const FProperty* Parent : Parents)
		{
			if (IsHiddenBy(Prop, Parent)) return false;
		}
		if (bShowAdvanced) return true;
		if (!PassesSimple(Prop)) return false;
		for (const FProperty* Parent : Parents)
		{
			if (Parent && !PassesSimple(*Parent)) return false;
		}
		return true;
	}
}
