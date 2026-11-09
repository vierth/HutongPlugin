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

	void PutToolDown();

	TSharedPtr<FUICommandInfo> FindToolCommand(const FString& ToolIdentifier) const;

	static class UHutongLayoutModeSettings* GetActiveSettings();

	static UHutongLayoutEdMode* GetActive();
	static bool StartTool(const TCHAR* ToolIdentifier);

	bool IsToolOnPalette(const FString& ToolIdentifier, FName PaletteName) const;
	FName PaletteOfTool(const FString& ToolIdentifier) const;

	static FString ToolIdentifierFor(const class UHutongBuildingComponent* Building);

private:
	// The editor's context ends every tool before a save.
	bool bContextEndedToolsOnSave = true;

	void OnEditorSelectionChanged(UObject* Selection);
	void FollowSelection();
	FDelegateHandle SelectionHandle;
	bool bFollowSelectionQueued = false;

	TSharedPtr<IInputProcessor> InputProcessor;
	TMap<FString, TSharedPtr<FUICommandInfo>> ToolCommandsById;
};
