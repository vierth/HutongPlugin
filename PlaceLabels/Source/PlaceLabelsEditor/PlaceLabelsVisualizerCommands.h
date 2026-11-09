#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

class FPlaceLabelsVisualizerCommands : public TCommands<FPlaceLabelsVisualizerCommands>
{
public:
	FPlaceLabelsVisualizerCommands()
		: TCommands<FPlaceLabelsVisualizerCommands>(
			TEXT("PlaceLabelsVisualizer"),
			NSLOCTEXT("Contexts", "PlaceLabelsVisualizer", "Place Labels Vertex Editing"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> DeleteVertex;
	TSharedPtr<FUICommandInfo> DuplicateVertex;
	TSharedPtr<FUICommandInfo> SelectAllVertices;
};
