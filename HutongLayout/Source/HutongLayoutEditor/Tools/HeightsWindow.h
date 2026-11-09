#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

class UHutongHeightsTool;

namespace HutongHeightsWindow
{
	TSharedRef<SWidget> MakeContent(UHutongHeightsTool* Tool);
}
