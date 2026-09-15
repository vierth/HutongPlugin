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

	// For the case the file is being written by something else while this is open.
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

	// Straight back to the coordinates the file recorded, with no drag.
	UFUNCTION(CallInEditor, Category="Placement", meta=(DisplayName="Place At Recorded Coordinates", ToolTip="Places the loaded set at the world coordinates the file recorded."))
	void PlaceAtRecordedCoordinates();

	// TransientToolProperty, or RestoreProperties copies a shut-down tool's pointer back out of
	// the CDO cache and every button on this set forwards to nothing.
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

	// Opens the dialog, loads, and refreshes the preview. Called from the property set's buttons.
	void BrowseForFile();
	// bPrompt: whether an unrecognised type in the file may put a modal question up now. The file
	// remembered from the last session is loaded as the tool starts, and a dialog on the way in to
	// a tool is a dialog nobody asked for; that one is answered at the moment of placing instead.
	void LoadFile(const FString& InPath, bool bPrompt = true);
	void PlaceAtRecordedCoordinates();

	bool HasScene() const { return Loaded.Records.Num() > 0; }

protected:
	virtual void RegisterToolSettings() override;
	virtual void SpawnFinalActor() override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Imported"); }
	virtual FString GetPlacementDetail() const override;
	virtual double GetPreviewHeight() const override { return 0.0; }

	// Nothing to bake: every record carries its own parameters and its own detail level.
	virtual void BuildMeshForRect(double, double, UE::Geometry::FDynamicMesh3&, EHutongDetail) override {}

	UPROPERTY()
	TObjectPtr<UHutongImportToolProperties> Settings;

private:
	// Four per record, in the set frame, each already carrying the record's own relative yaw.
	void RebuildPreview();

	// Past this many footprints only the set's bounding rectangle is drawn.
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
