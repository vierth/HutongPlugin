#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

class UHutongCourtsTool;

namespace HutongCourtsWindow
{
	TSharedRef<SWidget> MakeContent(UHutongCourtsTool* Tool);
}
