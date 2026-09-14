#include "PlaceLabelsCommands.h"

#define LOCTEXT_NAMESPACE "PlaceLabelsCommands"

void FPlaceLabelsCommands::RegisterCommands()
{
	// Shift+digit rather than a plain digit.
	UI_COMMAND(BeginSelectTool, "Select",
		"Click regions to select them, as anywhere else in the editor.\n"
		"This tool takes no clicks of its own: that is what puts the pen down, so a click picks a "
		"region rather than drawing a corner. The panel's form follows the selection and the "
		"ordinary gizmo moves it.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::One));

	UI_COMMAND(BeginPenTool, "Pen",
		"Draw a place region by clicking its corners.\n"
		"Click to add a point, Enter or click the first point to close, Backspace to remove the "
		"last point, Esc to cancel.\n"
		"Hold Alt to ignore snapping, Shift to constrain the segment to 15 degrees.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Two));

	UI_COMMAND(BeginEditTool, "Edit",
		"Reshape a region that is already placed.\n"
		"Click a region to pick it up, drag a corner to move it, click an edge to add a corner, "
		"Alt+click a corner to remove it.\n"
		"Hold Alt while dragging to ignore snapping. Delete removes every selected corner.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Three));
}

#undef LOCTEXT_NAMESPACE
