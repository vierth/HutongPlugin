#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongExchange.h"
#include "HutongImportTool.generated.h"

class UHutongImportTool;

UCLASS()
class UHutongImportToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category="File", meta=(DisplayName="Scene File", ToolTip="Path of the scene file currently loaded."))
	FString FileName;

	UFUNCTION(CallInEditor, Category="File", meta=(DisplayName="Browse…", ToolTip="Opens a file dialog to pick a scene file and loads it."))
	void Browse();

	// For a file being rewritten externally while open.
	UFUNCTION(CallInEditor, Category="File", meta=(DisplayName="Reload", ToolTip="Loads the current scene file again from disk."))
	void Reload();

	UPROPERTY(VisibleAnywhere, Category="File", meta=(DisplayName="Buildings", ToolTip="Number of buildings in the loaded file."))
	int32 RecordCount = 0;

	UPROPERTY(VisibleAnywhere, Category="File", meta=(DisplayName="Extent", ToolTip="Size of the loaded set's bounding rectangle."))
	FString Extent;

	UPROPERTY(VisibleAnywhere, Category="File", meta=(DisplayName="From Level", ToolTip="Level the file was exported from."))
	FString SourceLevel;

	UPROPERTY(VisibleAnywhere, Category="File", meta=(DisplayName="Contents", ToolTip="Whether the file carries parameters or only the arrangement."))
	FString Contents;

	UPROPERTY(EditAnywhere, Category="Placement", meta=(DisplayName="Outliner Folder", ToolTip="Outliner folder imported buildings go in; empty keeps the file's."))
	FName OutlinerFolder = TEXT("HutongImport");

	UPROPERTY(EditAnywhere, Category="Placement", meta=(DisplayName="Update Matching Placements", ToolTip="Updates buildings whose id matches an imported record in place."))
	bool bUpdateMatchingPlacements = true;

	// Back to the file's recorded coordinates, no drag.
	UFUNCTION(CallInEditor, Category="Placement", meta=(DisplayName="Place At Recorded Coordinates", ToolTip="Places the loaded set at the world coordinates the file recorded."))
	void PlaceAtRecordedCoordinates();

	// TransientToolProperty, else RestoreProperties restores a dead tool's pointer from the CDO cache.
	UPROPERTY(meta=(TransientToolProperty))
	TWeakObjectPtr<UHutongImportTool> Owner;
};

// Loads a scene file and carries the whole set under the cursor.
UCLASS()
class UHutongImportTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void GetEffectiveRectBounds(
		double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;

	// Opens the dialog, loads, refreshes the preview. Called from the property set's buttons.
	void BrowseForFile();
	// bPrompt: may an unknown type raise a modal now. The last session's file loads on tool start, where a
	// dialog is unasked for; that case is asked at placement instead.
	void LoadFile(const FString& InPath, bool bPrompt = true);
	void PlaceAtRecordedCoordinates();

	bool HasScene() const { return Loaded.Records.Num() > 0; }

protected:
	virtual void RegisterToolSettings() override;
	virtual void SpawnFinalActor() override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Imported"); }
	virtual FString GetPlacementDetail() const override;
	virtual double GetPreviewHeight() const override { return 0.0; }

	// Nothing to bake: each record carries its own params and detail level.
	virtual void BuildMeshForRect(double, double, UE::Geometry::FDynamicMesh3&, EHutongDetail) override {}

	UPROPERTY()
	TObjectPtr<UHutongImportToolProperties> Settings;

private:
	// Four corners per record, set frame, record's relative yaw applied.
	void RebuildPreview();

	// Beyond this many footprints only the set's bounds are drawn.
	static constexpr int32 MaxPreviewFootprints = 400;

	FString FilePath;
	HutongExchange::FSceneFile Loaded;
	TArray<FVector2D> PreviewCorners;
};

UCLASS()
class UHutongImportToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
