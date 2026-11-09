#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;

class FPlaceLabelsStyle
{
public:
	static void Register();
	static void Unregister();
	static FName GetStyleSetName();

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
