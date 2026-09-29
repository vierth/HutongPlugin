#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

class UHutongCourtsTool;

namespace HutongCourtsWindow
{
	// The court list, the new-court row and the Outliner action, reading and writing the tool.
	TSharedRef<SWidget> MakeContent(UHutongCourtsTool* Tool);
}
