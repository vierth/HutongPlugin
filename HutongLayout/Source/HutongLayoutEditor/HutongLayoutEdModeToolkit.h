#pragma once

#include "Tools/HutongSaveState.h"

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

	virtual void OnToolPaletteChanged(FName PaletteName) override;

	virtual TSharedPtr<SWidget> GetInlineContent() const override;
	// Labels get the wider column so a label and its checkbox both fit.
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

	FText GetToolTitleText() const;
	FText GetToolDescriptionText() const;

	FText GetStageLabel(int32 Index) const;
	FSlateColor GetStageColor(int32 Index) const;
	EVisibility GetStageVisibility(int32 Index) const;
	TSharedRef<SWidget> MakeStageRow() const;

	TSharedRef<SWidget> MakePlanRow() const;
	ECheckBoxState GetPlanOnlyState() const;
	void OnPlanOnlyChanged(ECheckBoxState State);

	// Plans are hidden while working over them, which is while a tool is in hand.
	ECheckBoxState GetShowPlansState() const;
	void OnShowPlansChanged(ECheckBoxState State);

	EVisibility GetHelpVisibility() const;

	bool IsScenePaletteActive() const;
	EVisibility GetToolPanelVisibility() const;
	EVisibility GetScenePanelVisibility() const;

	FText GetHoverText() const;
	FText GetHoverHeaderText() const;
	EVisibility GetHoverVisibility() const;

	bool IsPropertyVisible(const struct FPropertyAndParent& PropertyAndParent) const;
	ECheckBoxState GetShowAdvancedState() const;
	void OnShowAdvancedChanged(ECheckBoxState State);

	// Shown in the panel: selecting the component in the editor restarts the active tool mid-click and drops the plan handles.
	void OnSelectionChanged(UObject* Object);
	void RefreshSelectedBuilding();
	FText GetSelectedBuildingText() const;
	EVisibility GetSelectedBuildingVisibility() const;

	TSharedPtr<IDetailsView> SelectedBuildingView;
	TSharedPtr<HutongSaveState::FPolled> HoverSaveStatus;
	FDelegateHandle SelectionChangedHandle;
	FText SelectedBuildingLabel;
};
