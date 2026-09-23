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

	// Puts the active tool down, next tick. Escape with nothing in hand: no preview under the
	// cursor, and a click on the ground lays nothing out.
	void PutToolDown();

	// The palette command that started a tool, by the tool's registered identifier.
	TSharedPtr<FUICommandInfo> FindToolCommand(const FString& ToolIdentifier) const;

	// The mode's settings object, from wherever a tool or the toolkit needs it.
	static class UHutongLayoutModeSettings* GetActiveSettings();

	// The mode itself, and starting one of its tools by identifier — how a button on the mode
	// panel hands over to a tool that has to be dragged.
	static UHutongLayoutEdMode* GetActive();
	static bool StartTool(const TCHAR* ToolIdentifier);

	// Whether that identifier is one of the palette's own, for the toolkit's tab switching: the
	// palette a tool sits on is stated once, in GetModeCommands, and read back here.
	bool IsToolOnPalette(const FString& ToolIdentifier, FName PaletteName) const;
	// The palette that identifier sits on, or None.
	FName PaletteOfTool(const FString& ToolIdentifier) const;

	// The tool that places buildings of this one's kind, or empty for a kind no tool places.
	static FString ToolIdentifierFor(const class UHutongBuildingComponent* Building);

private:
	// A save deactivates every active tool — UModeManagerInteractiveToolsContext terminates them
	// on PreSaveWorld, since a tool may be holding preview actors the map must not keep. Nothing
	// here holds any: the previews are PDI lines and actors are spawned on a click. So the tool the
	// save closed is opened again on the other side of it, and an autosave stops taking the tool
	// out of your hand mid-street. Only a save restores it — a tool closed any other way stays closed.
	void RememberToolBeforeSave();
	void RestoreToolAfterSave();

	FString ToolBeforeSave;
	FDelegateHandle PreSaveHandle;
	FDelegateHandle PostSaveHandle;

	// Clicking a building on the map is the request to edit it: the tool for its kind comes up,
	// on its own tab, whether nothing was up or something else was. Never while a placement or
	// an edit is in hand — the click that selected it may be the press of a drag.
	void OnEditorSelectionChanged(UObject* Selection);
	void FollowSelection();
	FDelegateHandle SelectionHandle;
	bool bFollowSelectionQueued = false;

	TSharedPtr<IInputProcessor> InputProcessor;
	TMap<FString, TSharedPtr<FUICommandInfo>> ToolCommandsById;
};
