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

struct FPlaceConfidenceOption
{
	EPlaceConfidence Value = EPlaceConfidence::Attested;
	FText Label;
};

// Null Type is the "no type" entry.
struct FPlaceTypeOption
{
	TWeakObjectPtr<UPlaceLabelTypeAsset> Type;
	FString Label;
};

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

	FString SearchKey;
};

struct FPlaceLabelsProblemRow
{
	FText Text;
	TWeakObjectPtr<UPlaceRegionComponent> Region;
	TWeakObjectPtr<UPlaceRegionComponent> SecondRegion;
	bool bIsSeam = false;
	bool bSevere = false;
};

enum class EPlaceLabelsPanelTab : uint8
{
	Regions,
	Tools,
};

class SPlaceLabelsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPlaceLabelsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void RefreshNow();

private:
	static UWorld* GetEditorWorld();

	FText GetPromptText() const;
	FSlateColor GetPromptColour() const;

	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);

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

	void ApplyFilter();

	TSharedRef<ITableRow> MakeRegionRow(TSharedPtr<FPlaceRegionRow> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> MakeProblemRow(TSharedPtr<FPlaceLabelsProblemRow> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	void OnRegionClicked(TSharedPtr<FPlaceRegionRow> Item);
	void OnRegionDoubleClicked(TSharedPtr<FPlaceRegionRow> Item);
	void OnProblemClicked(TSharedPtr<FPlaceLabelsProblemRow> Item);

	static void SelectRegion(UPlaceRegionComponent* Region, bool bFocusViewport);

	void OnSearchTextChanged(const FText& NewText);

	TSharedRef<SWidget> MakeTypeFilterEntry(TSharedPtr<FString> Item);
	void OnTypeFilterChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetTypeFilterLabel() const;

	FText GetSummaryText() const;
	void UpdateSummary();
	FText GetProblemsHeaderText() const;

	TSharedRef<SWidget> BuildRegionEditor();

	// While the pen holds an outline the form edits that outline, leaving the just-placed region alone.
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

	void RebuildTypeOptions();
	TSharedRef<SWidget> MakeTypeOptionEntry(TSharedPtr<FPlaceTypeOption> Item);
	void OnEditorTypeChanged(TSharedPtr<FPlaceTypeOption> NewValue, ESelectInfo::Type SelectInfo);
	FText GetEditorTypeLabel() const;

	enum class ENameField : uint8 { Chinese, Pinyin, English };
	static FText& NameFieldOf(struct FPlaceName& Name, ENameField Field);
	FText GetEditorName(ENameField Field) const;
	void CommitEditorName(const FText& NewText, ETextCommit::Type CommitType, ENameField Field);

	FText GetEditorNote() const;
	void CommitEditorNote(const FText& NewText, ETextCommit::Type CommitType);

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

	static bool PickSaveFile(const FString& Title, const FString& FileTypes,
		const FString& DefaultName, FString& OutPath);
	static bool PickOpenFile(const FString& Title, const FString& FileTypes, FString& OutPath);

	static void Notify(const FText& Message, bool bSuccess);

	TArray<TSharedPtr<FPlaceRegionRow>> AllRegionRows;
	TArray<TSharedPtr<FPlaceRegionRow>> VisibleRegionRows;
	TArray<TSharedPtr<FPlaceLabelsProblemRow>> ProblemRows;

	// Stable row per region across refreshes, so the list view keeps its widgets.
	TMap<TWeakObjectPtr<UPlaceRegionComponent>, TSharedPtr<FPlaceRegionRow>> RowByRegion;

	FText CachedSummary;

	TArray<TSharedPtr<FString>> TypeFilterOptions;
	TSharedPtr<FString> SelectedTypeFilter;

	TSharedPtr<SListView<TSharedPtr<FPlaceRegionRow>>> RegionListView;
	TSharedPtr<SListView<TSharedPtr<FPlaceLabelsProblemRow>>> ProblemListView;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeFilterCombo;

	TWeakObjectPtr<UPlaceRegionComponent> SelectedRegion;
	TArray<TSharedPtr<FPlaceTypeOption>> TypeOptions;
	TSharedPtr<FPlaceTypeOption> SelectedTypeOption;
	TSharedPtr<SComboBox<TSharedPtr<FPlaceTypeOption>>> EditorTypeCombo;

	TArray<TSharedPtr<FPlaceConfidenceOption>> ConfidenceOptions;
	TSharedPtr<FPlaceConfidenceOption> SelectedConfidenceOption;
	TSharedPtr<SComboBox<TSharedPtr<FPlaceConfidenceOption>>> EditorConfidenceCombo;

	bool bWasAuthoring = false;

	EPlaceLabelsPanelTab ActiveTab = EPlaceLabelsPanelTab::Regions;

	FString SearchFilter;
	bool bProblemsOnly = false;
	bool bProblemsExpanded = false;

	double WeldTolerance = 25.0;

	PlaceLabelsValidation::FReport Report;
};
