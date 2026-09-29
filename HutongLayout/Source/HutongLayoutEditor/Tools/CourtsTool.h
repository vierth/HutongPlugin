#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongCourts.h"
#include "CourtsTool.generated.h"

class SWindow;

UCLASS()
class UHutongCourtsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UFUNCTION(CallInEditor, Category="Courts", meta=(DisplayName="Open Courts Window", ToolTip="Opens the window listing every courtyard unit."))
	void OpenWindow();

	UPROPERTY(meta=(TransientToolProperty))
	TWeakObjectPtr<class UHutongCourtsTool> Tool;
};

// Groups placed buildings into courtyard units (院落) and files them in the Outliner:
// Courts/<tile>/<court>, a wall between courts under Courts/<tile>/Shared walls. Places nothing: clicks
// select, a floating window names, lists and edits the courts.
UCLASS()
class UHutongCourtsTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnTick(float DeltaTime) override;
	virtual bool HasPlacement() const override { return false; }
	virtual bool HasRotateKey() const override { return false; }
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;
	virtual FText GetKeyHintText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual int32 GetStageIndex() const override;

	void OpenWindow();

	// The window's model.
	const TArray<HutongCourts::FSummary>& GetCourts() const { return Courts; }
	int32 GetCourtsRevision() const { return CourtsRevision; }
	FString GetSelectionText() const { return SelectionText; }
	int32 GetSelectedCount() const { return SeenSelection.Num(); }
	// Whether a selected building is in that court.
	bool IsSelectionIn(const FString& Court) const { return SelectedCourts.Contains(Court); }
	FString GetCourtName() const { return CourtName; }
	void SetCourtName(const FString& Name) { CourtName = Name; bNameSuggested = false; }

	void SuggestName();
	// The selection into the court named in the box: a new court, or an existing one by name.
	void AssignNew();
	void SelectCourt(const FString& Court);
	void AddSelectedTo(const FString& Court);
	void RemoveSelectedFrom(const FString& Court);
	void EditHeights(const FString& Court);
	void RebuildFolders();

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double, double, UE::Geometry::FDynamicMesh3&, EHutongDetail) override {}
	virtual void SpawnFinalActor() override {}

	UWorld* EditingWorld() const;
	// The readout and, unless typed, the suggested name, when the selection changed.
	void RefreshFromSelection(bool bForce);
	void RefreshCourts();

	UPROPERTY()
	TObjectPtr<UHutongCourtsToolProperties> Settings;

	TArray<HutongCourts::FSummary> Courts;
	int32 CourtsRevision = 0;
	TArray<TWeakObjectPtr<class UHutongBuildingComponent>> SeenSelection;
	TSet<FString> SelectedCourts;
	FString SelectionText;
	FString CourtName;
	bool bNameSuggested = true;
	// Set by an undo or a building's property edit; the list is rebuilt next tick.
	bool bCourtsDirty = false;
	FDelegateHandle UndoHandle;
	FDelegateHandle PropertyHandle;

	TSharedPtr<SWindow> Window;
};

UCLASS()
class UHutongCourtsToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
