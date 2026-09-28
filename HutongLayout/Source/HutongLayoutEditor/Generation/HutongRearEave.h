#pragma once

#include "CoreMinimal.h"
#include "HutongRearEave.generated.h"

// What stands behind the building; decides its back eave.
UENUM()
enum class EHutongRearEave : uint8
{
	Lane UMETA(DisplayName = "Sealed Rear Eave (封護檐) — backs onto a lane", ToolTip="The roof stops at the back wall; the brickwork rises past the eave."),

	Courtyard UMETA(DisplayName = "Courtyard — eaves symmetrical", ToolTip="The rear eave overhangs, dressed like the facade."),
};

// The brick cornice (檐子) under a 封護檐's drip course (四合院建築及其構造 圖5-3-9).
UENUM()
enum class EHutongSealedCornice : uint8
{
	IceTray UMETA(DisplayName = "Four-Course Ice-Tray Cornice (冰盤檐)", ToolTip="Four plain brick courses, each stepping out."),
	Rounded UMETA(DisplayName = "Three-Course Rounded Cornice (雞素子檐)", ToolTip="A plain course, a rounded course, a cover course (Fig 5-3-9.8)."),
	Drawer UMETA(DisplayName = "Three-Course Drawer Cornice (抽屜檐)", ToolTip="A plain course, a course of spaced blocks like drawers, a cover course (Fig 5-3-9.9)."),
	SevenCourse UMETA(DisplayName = "Seven-Course Cornice (七層)", ToolTip="A string of beads and a half-round under a tall plain band, then brick rafter ends (Fig 5-3-9.10)."),
};
