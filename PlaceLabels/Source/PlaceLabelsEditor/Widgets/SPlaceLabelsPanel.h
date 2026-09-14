#pragma once

#include "CoreMinimal.h"
#include "PlaceLabelTypes.h"
#include "PlaceLabelsValidation.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class ITableRow;
class SSearchBox;
class UPlaceLabelTypeAsset;
class UPlaceRegionComponent;
template <typename ItemType> class SListView;
template <typename OptionType> class SComboBox;

// One entry in the inline editor's confidence dropdown.
struct FPlaceConfidenceOption
{
	EPlaceConfidence Value = EPlaceConfidence::Attested;
	FText Label;
};

// One entry in the inline editor's type dropdown. Null Type is the "no type" entry.
struct FPlaceTypeOption
{
	TWeakObjectPtr<UPlaceLabelTypeAsset> Type;
	FString Label;
};

// One line in the region browser. A snapshot, refreshed on a timer rather than bound live.
struct FPlaceRegionRow
{
	TWeakObjectPtr<UPlaceRegionComponent> Region;

	FText DisplayName;
	FText TypeLabel;
	FName TypeId;
	FLinearColor Colour = FLinearColor::White;

	int32 Corners = 0;
	double AreaSquareMetres = 0.0;
	int32 IssueCount = 0;
	bool bHasSevereIssue = false;

	// Lower-cased name and type, so filtering does not re-case every row on every keystroke.
	FString SearchKey;
};

// One line in the problems list. Seams carry two regions and a repair; everything else carries one.
struct FPlaceLabelsProblemRow
{
	FText Text;
	TWeakObjectPtr<UPlaceRegionComponent> Region;
	TWeakObjectPtr<UPlaceRegionComponent> SecondRegion;
	bool bIsSeam = false;
	bool bSevere = false;
};

// Which half of the panel is showing. Drawing regions and naming them is the session's work;
// hierarchy, welding, import, export and the starter types are settled once and then in the way.
enum class EPlaceLabelsPanelTab : uint8
{
	Regions,
	Tools,
};

// Everything in the Place Labels mode panel above the details views.
class SPlaceLabelsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlaceLabelsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Rebuilds the region list, and the problems list too if that section is open.
	void RefreshNow();

private:
	static UWorld* GetEditorWorld();

	// The active tool's prompt, resolved here rather than passed in.
	FText GetPromptText() const;
	FSlateColor GetPromptColour() const;

	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);

	// Adopts whatever region is selected in the level editor.
	void SyncSelectionFromEditor();

	void RebuildRegionRows();
	void RebuildProblemRows();

	EVisibility GetRegionsTabVisibility() const
	{
		return ActiveTab == EPlaceLabelsPanelTab::Regions ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetToolsTabVisibility() const
	{
		return ActiveTab == EPlaceLabelsPanelTab::Tools ? EVisibility::Visible : EVisibility::Collapsed;
	}

	// Recomputes the visible set and refreshes the list only if it actually changed.
	void ApplyFilter();

	TSharedRef<ITableRow> MakeRegionRow(TSharedPtr<FPlaceRegionRow> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> MakeProblemRow(TSharedPtr<FPlaceLabelsProblemRow> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	void OnRegionClicked(TSharedPtr<FPlaceRegionRow> Item);
	void OnRegionDoubleClicked(TSharedPtr<FPlaceRegionRow> Item);
	void OnProblemClicked(TSharedPtr<FPlaceLabelsProblemRow> Item);

	// Selects the owning actor, and optionally flies the viewport to it.
	static void SelectRegion(UPlaceRegionComponent* Region, bool bFocusViewport);

	void OnSearchTextChanged(const FText& NewText);

	TSharedRef<SWidget> MakeTypeFilterEntry(TSharedPtr<FString> Item);
	void OnTypeFilterChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetTypeFilterLabel() const;

	FText GetSummaryText() const;
	void UpdateSummary();
	FText GetProblemsHeaderText() const;

	// The inline editor: name, type, note and metadata for whichever region the form is on.
	TSharedRef<SWidget> BuildRegionEditor();

	// **The form follows the pen while the pen has an outline in hand.** Two forms carrying the
	// same three name fields — this one on the selected region, the tool's own on the region about
	// to be placed — is how a name gets typed into the last region instead of the next one, and
	// the last region is selected precisely because it was just placed. So there is one form, and
	// while something is being drawn it is the drawing's.
	class UPlaceRegionPenTool* GetAuthoringPen() const;
	class UPlaceRegionPenToolProperties* GetAuthoringSettings() const;
	bool IsAuthoringNewRegion() const { return GetAuthoringPen() != nullptr; }

	FReply OnCreatePendingRegion();
	FReply OnDiscardPendingOutline();
	EVisibility GetPlacedButtonsVisibility() const;
	EVisibility GetPendingButtonsVisibility() const;

	void SetSelectedRegion(UPlaceRegionComponent* Region);
	UPlaceRegionComponent* GetSelectedRegion() const { return SelectedRegion.Get(); }
	EVisibility GetEditorVisibility() const;
	FText GetEditorHeaderText() const;

	// Rebuilt when the selection changes.
	void RebuildTypeOptions();
	TSharedRef<SWidget> MakeTypeOptionEntry(TSharedPtr<FPlaceTypeOption> Item);
	void OnEditorTypeChanged(TSharedPtr<FPlaceTypeOption> NewValue, ESelectInfo::Type SelectInfo);
	FText GetEditorTypeLabel() const;

	// Which of the three name fields, so one pair of handlers covers all of them.
	enum class ENameField : uint8 { Chinese, Pinyin, English };
	static FText& NameFieldOf(struct FPlaceName& Name, ENameField Field);
	FText GetEditorName(ENameField Field) const;
	void CommitEditorName(const FText& NewText, ETextCommit::Type CommitType, ENameField Field);

	FText GetEditorNote() const;
	void CommitEditorNote(const FText& NewText, ETextCommit::Type CommitType);

	// Metadata: where the name was read, and how far the source actually says it.
	FText GetEditorSource() const;
	void CommitEditorSource(const FText& NewText, ETextCommit::Type CommitType);

	void BuildConfidenceOptions();
	TSharedRef<SWidget> MakeConfidenceEntry(TSharedPtr<FPlaceConfidenceOption> Item);
	void OnEditorConfidenceChanged(TSharedPtr<FPlaceConfidenceOption> NewValue,
		ESelectInfo::Type SelectInfo);
	FText GetEditorConfidenceLabel() const;
	void SyncConfidenceSelection();

	FReply OnFrameSelected();
	FReply OnRecomputeSelectedParent();
	FReply OnDeleteSelected();

	FReply OnRecomputeHierarchy();
	FReply OnCreateStarterTypes();
	FReply OnWeldAll();
	FReply OnExportGeoJson();
	FReply OnExportCsv();
	FReply OnImport();
	FReply OnWeldPair(TWeakObjectPtr<UPlaceRegionComponent> A, TWeakObjectPtr<UPlaceRegionComponent> B);

	// Shared by the export buttons: a save dialog seeded next to the project.
	static bool PickSaveFile(const FString& Title, const FString& FileTypes,
		const FString& DefaultName, FString& OutPath);
	static bool PickOpenFile(const FString& Title, const FString& FileTypes, FString& OutPath);

	static void Notify(const FText& Message, bool bSuccess);

	TArray<TSharedPtr<FPlaceRegionRow>> AllRegionRows;
	TArray<TSharedPtr<FPlaceRegionRow>> VisibleRegionRows;
	TArray<TSharedPtr<FPlaceLabelsProblemRow>> ProblemRows;

	// One stable row object per region, kept across refreshes so the list view sees the same pointers and leaves its widgets.
	TMap<TWeakObjectPtr<UPlaceRegionComponent>, TSharedPtr<FPlaceRegionRow>> RowByRegion;

	// Recomputed on refresh rather than per frame.
	FText CachedSummary;

	TArray<TSharedPtr<FString>> TypeFilterOptions;
	TSharedPtr<FString> SelectedTypeFilter;

	TSharedPtr<SListView<TSharedPtr<FPlaceRegionRow>>> RegionListView;
	TSharedPtr<SListView<TSharedPtr<FPlaceLabelsProblemRow>>> ProblemListView;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeFilterCombo;

	// The inline editor.
	TWeakObjectPtr<UPlaceRegionComponent> SelectedRegion;
	TArray<TSharedPtr<FPlaceTypeOption>> TypeOptions;
	TSharedPtr<FPlaceTypeOption> SelectedTypeOption;
	TSharedPtr<SComboBox<TSharedPtr<FPlaceTypeOption>>> EditorTypeCombo;

	TArray<TSharedPtr<FPlaceConfidenceOption>> ConfidenceOptions;
	TSharedPtr<FPlaceConfidenceOption> SelectedConfidenceOption;
	TSharedPtr<SComboBox<TSharedPtr<FPlaceConfidenceOption>>> EditorConfidenceCombo;

	// So the dropdowns are reloaded when the form changes what it is pointing at.
	bool bWasAuthoring = false;

	// Not remembered between sessions: the panel opens on the work, not on the last button pressed.
	EPlaceLabelsPanelTab ActiveTab = EPlaceLabelsPanelTab::Regions;

	FString SearchFilter;
	bool bProblemsOnly = false;
	bool bProblemsExpanded = false;

	// Shared by seam reporting and by welding.
	double WeldTolerance = 25.0;

	PlaceLabelsValidation::FReport Report;
};
