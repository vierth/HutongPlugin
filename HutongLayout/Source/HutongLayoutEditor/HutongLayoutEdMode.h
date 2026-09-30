#pragma once

#include "CoreMinimal.h"
#include "Tools/UEdMode.h"
#include "HutongLayoutEdMode.generated.h"

class IInputProcessor;

UCLASS()
class UHutongLayoutEdMode : public UEdMode
{
	GENERATED_BODY()

public:
	static const FEditorModeID EM_HutongLayoutModeId;

	UHutongLayoutEdMode();

	virtual void Enter() override;
	virtual void Exit() override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;

	// Puts the active tool down next tick: no cursor preview, and a ground click lays nothing out.
	void PutToolDown();

	// The palette command that started a tool, by the tool's registered identifier.
	TSharedPtr<FUICommandInfo> FindToolCommand(const FString& ToolIdentifier) const;

	// The mode's settings object.
	static class UHutongLayoutModeSettings* GetActiveSettings();

	// The active mode, and starting a tool by identifier (how a panel button hands over to a drag tool).
	static UHutongLayoutEdMode* GetActive();
	static bool StartTool(const TCHAR* ToolIdentifier);

	// Whether the identifier is on that palette, for tab switching; palettes are stated once, in GetModeCommands.
	bool IsToolOnPalette(const FString& ToolIdentifier, FName PaletteName) const;
	// The palette that identifier sits on, or None.
	FName PaletteOfTool(const FString& ToolIdentifier) const;

	// The tool that places buildings of this one's kind, or empty for a kind no tool places.
	static FString ToolIdentifierFor(const class UHutongBuildingComponent* Building);

private:
	// The editor's context ends every tool before a save (lest one hold preview actors). Ours hold
	// none (PDI previews, actors spawned on click), so while the mode is up a save leaves the tool
	// alone and autosave no longer drops a placement in progress. The previous answer, for Exit.
	bool bContextEndedToolsOnSave = true;

	// Clicking a building opens its kind's tool on its tab. Never mid-placement or mid-edit: the
	// selecting click may be a drag's press.
	void OnEditorSelectionChanged(UObject* Selection);
	void FollowSelection();
	FDelegateHandle SelectionHandle;
	bool bFollowSelectionQueued = false;

	TSharedPtr<IInputProcessor> InputProcessor;
	TMap<FString, TSharedPtr<FUICommandInfo>> ToolCommandsById;
};
