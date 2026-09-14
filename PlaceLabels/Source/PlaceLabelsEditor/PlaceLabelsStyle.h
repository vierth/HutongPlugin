#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

// Icons for the mode's tool palette.
class FPlaceLabelsStyle
{
public:
	static void Register();
	static void Unregister();
	static FName GetStyleSetName();

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
