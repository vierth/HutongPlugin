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

	// Switching tabs closes a tool not on the new tab; see the definition.
	virtual void OnToolPaletteChanged(FName PaletteName) override;

	// The active tool's panel or, on the Scene tab, the mode's actions on placed buildings.
	virtual TSharedPtr<SWidget> GetInlineContent() const override;
	// Labels get the wider column: at the default split a label was cut to leave a checkbox half the row.
	virtual void CustomizeDetailsViewArgs(FDetailsViewArgs& ArgsInOut) override { ArgsInOut.ColumnWidth = NameColumnShare; }
	virtual void CustomizeModeDetailsViewArgs(FDetailsViewArgs& ArgsInOut) override { ArgsInOut.ColumnWidth = NameColumnShare; }
	// The value column's share of the row.
	static constexpr float NameColumnShare = 0.4f;

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

	// Layout-only checkbox and its Generate button, at the top so a setting that stops building is seen.
	TSharedRef<SWidget> MakePlanRow() const;
	ECheckBoxState GetPlanOnlyState() const;
	void OnPlanOnlyChanged(ECheckBoxState State);

	// Plan visibility, beside Layout Only and on the tool panel: plans are hidden while working
	// over them, which is while a tool is in hand.
	ECheckBoxState GetShowPlansState() const;
	void OnShowPlansChanged(ECheckBoxState State);

	// Collapses the help block while no tool is active; see the definition.
	EVisibility GetHelpVisibility() const;

	// The Scene tab carries no tool, so the two halves of the panel swap on it.
	bool IsScenePaletteActive() const;
	EVisibility GetToolPanelVisibility() const;
	EVisibility GetScenePanelVisibility() const;

	// What the cursor rests on, polled like the prompt; own box, hidden during a placement.
	FText GetHoverText() const;
	FText GetHoverHeaderText() const;
	EVisibility GetHoverVisibility() const;

	// Simple / advanced: one switch over the tool's panel, read by the details views' filter.
	bool IsPropertyVisible(const struct FPropertyAndParent& PropertyAndParent) const;
	ECheckBoxState GetShowAdvancedState() const;
	void OnShowAdvancedChanged(ECheckBoxState State);

	// Selected building's component, shown here rather than via the level editor's selection:
	// selecting the component from here restarted the active tool mid-click and dropped the plan handles.
	void OnSelectionChanged(UObject* Object);
	void RefreshSelectedBuilding();
	FText GetSelectedBuildingText() const;
	EVisibility GetSelectedBuildingVisibility() const;

	TSharedPtr<IDetailsView> SelectedBuildingView;
	FDelegateHandle SelectionChangedHandle;
	FText SelectedBuildingLabel;
};
