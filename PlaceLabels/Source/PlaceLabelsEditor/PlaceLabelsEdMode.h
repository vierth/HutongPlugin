#pragma once

#include "CoreMinimal.h"
#include "Tools/UEdMode.h"
#include "PlaceLabelsEdMode.generated.h"

class IInputProcessor;

UCLASS()
class UPlaceLabelsEdMode : public UEdMode
{
	GENERATED_BODY()

public:
	static const FEditorModeID EM_PlaceLabelsModeId;

	UPlaceLabelsEdMode();

	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;

private:
	// Routes Enter, Backspace and Escape to the tools.
	TSharedPtr<IInputProcessor> InputProcessor;
	// The context's own answer, put back on Exit.
	bool bContextEndedToolsOnSave = true;
};
