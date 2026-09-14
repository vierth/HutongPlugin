#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

// Slate style set holding the toolbar icons for the mode's tools.
class FHutongLayoutStyle
{
public:
	static void Register();
	static void Unregister();
	static FName GetStyleSetName();

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
