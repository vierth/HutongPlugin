#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Generation/HutongMetadata.h"
#include "HeightsTool.generated.h"

class SWindow;

// One selected building in the heights list.
USTRUCT()
struct FHutongEaveRow
{
	GENERATED_BODY()

	// Actor label, and the type (with its court) beneath it.
	FString Label;
	FString Detail;
	EHutongCourtRole Role = EHutongCourtRole::Other;
	// The eave of a roofed type, the body top of a wall (UHutongBuildingComponent::GetEditHeight).
	double CurrentEave = 0.0;
	double NewEave = 0.0;
	// False where the type's height is not one number (亭, the 大式 殿, 牌坊).
	bool bEditable = true;
	// Preset as placed, and as Apply will set it.
	FString Preset;
	FString NewPreset;
	// Ridge over the eave as built, so a row's ridge follows its New Eave; zero for walls.
	double RidgeAboveEave = 0.0;
};

UCLASS()
class UHutongHeightsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category="Heights", meta=(DisplayName="Open Heights Window", ToolTip="Opens the window listing the selected buildings' eaves."))
	void OpenWindow();

	UPROPERTY(EditAnywhere, Category="Heights", meta=(DisplayName="Keep Proportions", ToolTip="Editing one New Eave scales every other row by the same factor."))
	bool bKeepProportions = false;

	UPROPERTY(EditAnywhere, Category="Heights", meta=(DisplayName="Court Name", ToolTip="Name Assign Court gives the selection; suggested from the map tile under it."))
	FString CourtName;

	UPROPERTY(meta=(TransientToolProperty))
	TWeakObjectPtr<class UHutongHeightsTool> Tool;
};

// Sets the eaves of the selected buildings together, with suggestions ranked on the court's 正房.
// Places nothing: clicks select (Shift adds, drag boxes); a floating window lists and edits.
UCLASS()
class UHutongHeightsTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual bool HasPlacement() const override { return false; }
	virtual bool HasRotateKey() const override { return false; }
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;
	virtual FText GetKeyHintText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual int32 GetStageIndex() const override;

	void OpenWindow();

	// The window's model: rows, and a revision that changes when the set of rows does.
	const TArray<FHutongEaveRow>& GetRows() const { return Rows; }
	int32 GetRowsRevision() const { return RowsRevision; }
	int32 GetReferenceRow() const { return ReferenceRow(Rows); }
	double GetSuggestion(int32 Index) const { return Suggested.IsValidIndex(Index) ? Suggested[Index] : -1.0; }
	// Why the suggestion is what it is ("0.84 × Main Hall", "ridge 35 cm over …").
	FString GetSuggestionNote(int32 Index) const { return SuggestionNotes.IsValidIndex(Index) ? SuggestionNotes[Index] : FString(); }
	TArray<FString> GetPresetOptions(int32 Index) const;
	void SetNewEave(int32 Index, double Cm);
	void SetRole(int32 Index, EHutongCourtRole Role);
	void SetPreset(int32 Index, const FString& Preset);
	void UseSuggestion(int32 Index);

	bool GetKeepProportions() const { return Settings && Settings->bKeepProportions; }
	void SetKeepProportions(bool b) { if (Settings) Settings->bKeepProportions = b; }
	FString GetCourtName() const { return Settings ? Settings->CourtName : FString(); }
	void SetCourtName(const FString& Name);

	void Apply();
	void UseSuggested();
	void Reset();
	void AssignCourt();
	void SelectCourt();
	// Fills Court Name with the next free name on the map tile under the selection.
	void SuggestCourtName();
	// Every court in the level, and selecting one whole (the selection then fills the table).
	TArray<FString> GetAllCourts() const;
	void OpenCourt(const FString& Court);

	// The row whose building ranks highest (正房 first, then the tallest), or INDEX_NONE.
	static int32 ReferenceRow(const TArray<FHutongEaveRow>& Rows);
	// Each row's suggestion from the reference row's role and New Eave; negative where none.
	static TArray<double> Suggestions(const TArray<FHutongEaveRow>& Rows);

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double, double, UE::Geometry::FDynamicMesh3&, EHutongDetail) override {}
	virtual void SpawnFinalActor() override {}

	// Rows from the selection when it changed; bForce re-reads the eaves as built.
	void RefreshRows(bool bForce);
	// Ratio suggestions, then each gate's: its ridge the canon step over the building beside it.
	void RefreshSuggestions();
	void SyncPanel();

	UPROPERTY()
	TObjectPtr<UHutongHeightsToolProperties> Settings;

	TArray<FHutongEaveRow> Rows;
	TArray<double> Suggested;
	TArray<FString> SuggestionNotes;
	// Parallel to Rows.
	TArray<TWeakObjectPtr<class UHutongBuildingComponent>> RowBuildings;
	int32 RowsRevision = 0;
	// The court name was filled by SuggestCourtName, not typed; a new selection may replace it.
	bool bCourtNameSuggested = false;
	bool bPanelDirty = false;

	TSharedPtr<SWindow> Window;
};

UCLASS()
class UHutongHeightsToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
