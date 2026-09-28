#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

// Panel's simple view. A params-struct field shows only with meta=(HutongBasic); a field declared on
// a tool property set or building component shows unless meta=(HutongAdvanced). Every nesting level
// must pass. Engine structs (vector, colour) follow their owner.
namespace HutongPanel
{
	inline const FName BasicKey(TEXT("HutongBasic"));
	inline const FName AdvancedKey(TEXT("HutongAdvanced"));

	inline bool PassesSimple(const FProperty& Prop)
	{
		if (Prop.HasMetaData(AdvancedKey)) return false;
		const UScriptStruct* Owner = Cast<UScriptStruct>(Prop.GetOwnerStruct());
		const bool bOurStruct = Owner && Owner->GetName().StartsWith(TEXT("Hutong"));
		return !bOurStruct || Prop.HasMetaData(BasicKey);
	}

	inline bool IsVisible(const FProperty& Prop, TConstArrayView<const FProperty*> Parents, bool bShowAdvanced)
	{
		if (bShowAdvanced) return true;
		if (!PassesSimple(Prop)) return false;
		for (const FProperty* Parent : Parents)
		{
			if (Parent && !PassesSimple(*Parent)) return false;
		}
		return true;
	}
}
