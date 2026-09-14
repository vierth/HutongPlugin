#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class URectDragToolBase;

class IDetailsView;

class FHutongLayoutEdModeToolkit : public FModeToolkit
{
public:
	virtual ~FHutongLayoutEdModeToolkit();

	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost,
		TWeakObjectPtr<UEdMode> InOwningMode) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	virtual void GetToolPaletteNames(TArray<FName>& OutPaletteNames) const override;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override;
	virtual void BuildToolPalette(FName PaletteName, FToolBarBuilder& ToolbarBuilder) override;

	// Switching tabs closes a tool that is not on the tab switched to — see the note on the definition.
	virtual void OnToolPaletteChanged(FName PaletteName) override;

	// The active tool's panel, or — on the Scene tab — the mode's own actions on what is placed.
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
	URectDragToolBase* GetActiveRectTool() const;
	FText GetPlacementPromptText() const;
	FSlateColor GetPlacementPromptColor() const;
	FText GetHelpText() const;
	FText GetKeyHintText() const;

	// The palette command behind the active tool.
	FText GetToolTitleText() const;
	FText GetToolDescriptionText() const;

	// One slot per placement stage, bound by index; slots past the tool's count collapse.
	FText GetStageLabel(int32 Index) const;
	FSlateColor GetStageColor(int32 Index) const;
	EVisibility GetStageVisibility(int32 Index) const;
	TSharedRef<SWidget> MakeStageRow() const;

	// The lay-out-only checkbox and its Generate button, at the top of the panel where a setting that stops buildings being built can be seen.
	TSharedRef<SWidget> MakePlanRow() const;
	ECheckBoxState GetPlanOnlyState() const;
	void OnPlanOnlyChanged(ECheckBoxState State);

	// Whether laid-out buildings draw at all. Beside Lay Out Only because it is the same subject —
	// the plans — and it belongs on the tool panel rather than only on the Scene tab: the plans are
	// hidden while working over them, which is while a tool is in hand.
	ECheckBoxState GetShowPlansState() const;
	void OnShowPlansChanged(ECheckBoxState State);
	FReply OnGenerateClicked();

	// Collapses the whole help block while no tool is active — see the note on the definition.
	EVisibility GetHelpVisibility() const;

	// The Scene tab carries no tool, so the two halves of the panel swap on it.
	bool IsScenePaletteActive() const;
	EVisibility GetToolPanelVisibility() const;
	EVisibility GetScenePanelVisibility() const;

	// What the cursor is resting on, polled like the prompt.
	FText GetHoverText() const;
	EVisibility GetHoverVisibility() const;

	// The building component of whatever is selected, shown in the panel rather than by reaching
	// into the level editor's own selection: the Details panel puts a placed piece's parameters a
	// click down the component tree, and selecting that component from here restarted the active
	// tool mid-click and took the plan handles with it.
	void OnSelectionChanged(UObject* Object);
	void RefreshSelectedBuilding();
	FText GetSelectedBuildingText() const;
	EVisibility GetSelectedBuildingVisibility() const;

	TSharedPtr<IDetailsView> SelectedBuildingView;
	FDelegateHandle SelectionChangedHandle;
	FText SelectedBuildingLabel;
};
