#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

class UHutongHeightsTool;

namespace HutongHeightsWindow
{
	// The heights table and its court and apply controls, reading and writing the tool's rows.
	TSharedRef<SWidget> MakeContent(UHutongHeightsTool* Tool);
}
