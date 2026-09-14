#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "PlaceLabelsStyle.h"

class FPlaceLabelsCommands : public TCommands<FPlaceLabelsCommands>
{
public:
	FPlaceLabelsCommands()
		: TCommands<FPlaceLabelsCommands>(
			// This context string is also the icon style-name prefix.
			TEXT("PlaceLabels"),
			NSLOCTEXT("Contexts", "PlaceLabels", "Place Labels"),
			NAME_None,
			FPlaceLabelsStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> BeginSelectTool;
	TSharedPtr<FUICommandInfo> BeginPenTool;
	TSharedPtr<FUICommandInfo> BeginEditTool;
};
