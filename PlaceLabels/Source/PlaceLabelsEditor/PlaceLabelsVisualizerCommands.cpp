#include "PlaceLabelsVisualizerCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PlaceLabelsVisualizerCommands"

void FPlaceLabelsVisualizerCommands::RegisterCommands()
{
	UI_COMMAND(DeleteVertex, "Delete Vertex",
		"Remove the selected corners from this region.",
		EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));

	UI_COMMAND(DuplicateVertex, "Duplicate Vertex",
		"Insert a copy of the selected corner just after it.",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::D));

	UI_COMMAND(SelectAllVertices, "Select All Vertices",
		"Select every corner of this region.",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));
}

#undef LOCTEXT_NAMESPACE
