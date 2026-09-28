#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Generation/HutongDetail.h"
#include "HutongLayoutModeSettings.generated.h"

// The mode's own panel, under the active tool's, collapsed as "Placed Buildings".
UCLASS(config = EditorPerProjectUserSettings)
class UHutongLayoutModeSettings : public UObject
{
	GENERATED_BODY()

public:
	// Declared first: category order is declaration order. Writes each loaded building's placement
	// and its changes from type defaults: enough to rebuild the street.
	UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayName = "Export Loaded Buildings", ToolTip = "Writes every loaded building to a scene file."))
	void ExportLoaded();

	UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayName = "Export Selection", ToolTip = "Writes the selected buildings to a scene file."))
	void ExportSelection();

private:
	// The two export buttons differ in what they gather and which name they remember.
	void DoExport(bool bSelection, const TCHAR* FallbackName, FString& Remembered);

public:
	// Tool panel's simple/advanced switch; toggled there, not on this panel.
	UPROPERTY(config)
	bool bShowAdvancedSettings = false;

	UPROPERTY(EditAnywhere, config, Category = "Plan", meta = (DisplayName = "Layout Only (outlines, no geometry)", ToolTip = "Places new buildings as footprint outlines with no geometry."))
	bool bPlanOnly = true;

	// Mirrors HutongPlanOutline's switch: plans draw regardless of mode, and another plugin's panel
	// wants them hidden. Seeded on Enter so a fresh object cannot report hidden plans as visible.
	UPROPERTY(EditAnywhere, Category = "Plan", meta = (DisplayName = "Show Plan Outlines", ToolTip = "Draws the footprint outline of every laid-out building."))
	bool bShowPlanOutlines = true;


	UFUNCTION(CallInEditor, Category = "Plan", meta = (DisplayName = "Generate Geometry (loaded)", ToolTip = "Builds full geometry for every laid-out building currently loaded."))
	void GenerateLoadedGeometry();

	UFUNCTION(CallInEditor, Category = "Plan", meta = (DisplayName = "Generate Geometry (selection)", ToolTip = "Builds full geometry for the selected laid-out buildings."))
	void GenerateSelectedGeometry();

	UFUNCTION(CallInEditor, Category = "Plan", meta = (DisplayName = "Revert To Layout (loaded)", ToolTip = "Removes the geometry of every loaded built building, leaving its outline."))
	void RevertLoadedToLayout();

	UFUNCTION(CallInEditor, Category = "Plan", meta = (DisplayName = "Revert To Layout (selection)", ToolTip = "Removes the geometry of the selected built buildings, leaving their outlines."))
	void RevertSelectedToLayout();

	// Placement unchanged: rebuilt from the parameters each building carries.
	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Rebuild Selection", ToolTip = "Re-bakes each selected building from the parameters it carries."))
	void RebuildSelection();

	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Promote To", ToolTip = "Detail level that Promote Selection and Set Loaded Region apply."))
	EHutongDetail TargetLevel = EHutongDetail::Near;

	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Promote Selection", ToolTip = "Sets the selected buildings to the Promote To level and rebuilds them."))
	void PromoteSelection();

	// Identical buildings share one mesh asset under /Game/HutongLayout/Generated; a rebuild or a deleted
	// building can leave one no building wears.
	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Delete Unused Generated Meshes", ToolTip = "Finds the shared building meshes no building uses any more and offers them for deletion, with the editor's own reference check."))
	void DeleteUnusedGeneratedMeshes();

	// Converts a placed building's type: footprint, facing and transform stay; parameters come from
	// the new type (shop to house, 院牆 to 隔牆) with no redraw.
	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Convert To", GetOptions = "GetConvertOptions", ToolTip = "Building type that Convert Selection turns the selected buildings into."))
	FString ConvertTo;

	UFUNCTION()
	TArray<FString> GetConvertOptions() const;

	// Empty uses the target type's defaults, right for types with one preset or none; a house
	// needs a preset named.
	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Convert Preset", GetOptions = "GetConvertPresetOptions", ToolTip = "Preset the converted buildings use; empty uses the type's defaults."))
	FString ConvertPreset;

	UFUNCTION()
	TArray<FString> GetConvertPresetOptions() const;

	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Convert Selection", ToolTip = "Turns the selected buildings into the Convert To type in place."))
	void ConvertSelection();

	// Straight to Massing rather than to the target.
	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Demote Selection To Massing", ToolTip = "Sets the selected buildings to the Massing level and rebuilds them."))
	void DemoteSelectionToMassing();

	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Divide At Bay Line", UIMin = "0", UIMax = "32", ClampMin = "0", ToolTip = "Bay line Divide Selection cuts at, from the origin end; zero is the middle."))
	int32 DivideAtBayLine = 0;

	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Divide Selection", ToolTip = "Divides each selected building in two at the bay line above."))
	void DivideSelection();

	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Fuse Selection", ToolTip = "Fuses two selected buildings standing end to end on one line into one."))
	void FuseSelection();

	// Everything streamed in: under World Partition, the region you stand in, not the level.
	UFUNCTION(CallInEditor, Category = "Loaded Region", meta = (DisplayName = "Set Loaded Region To Promote Level", ToolTip = "Sets every loaded building to the Promote To level and rebuilds it."))
	void SetLoadedRegionToTarget();

	UFUNCTION(CallInEditor, Category = "Loaded Region", meta = (DisplayName = "Rebuild Loaded Buildings", ToolTip = "Re-bakes every loaded building from the parameters it carries."))
	void RebuildLoaded();

	// One editable material asset per slot, worn by every placement and rebuild instead of the
	// tinted default until a palette material is assigned. Existing assets are kept.
	UFUNCTION(CallInEditor, Category = "Materials", meta = (DisplayName = "Create Starter Materials", ToolTip = "Writes one editable material per surface slot to /Game/HutongLayout/Materials; existing assets are kept."))
	void CreateStarterMaterials();

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Import Folder", ToolTip = "Outliner folder imported buildings go in; empty keeps the file's."))
	FName ImportFolder = TEXT("HutongImport");

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Update Matching Placements", ToolTip = "Updates buildings whose id matches an imported record in place."))
	bool bUpdateMatchingPlacements = true;

	// Coordinates from another level mean nothing here, so the Import tool carries the set under
	// the cursor, turns it with R and drops it on a click.
	UFUNCTION(CallInEditor, Category = "Import", meta = (DisplayName = "Place By Hand (Import Tool)", ToolTip = "Loads a scene file into the Import tool to place by hand."))
	void PlaceSceneByHand();

	// Remembered here rather than on the import tool's property set.
	UPROPERTY(config)
	FString LastSceneFile;

	// Last file each export button wrote, so the dialog reopens there instead of on the default.
	// Separate from the selection's: saving a selection over a whole-scene file loses the street.
	UPROPERTY(config)
	FString LastExportFile;

	UPROPERTY(config)
	FString LastSelectionExportFile;

	// Straight back to the world coordinates the file recorded, with no drag.
	UFUNCTION(CallInEditor, Category = "Import", meta = (DisplayName = "Import At Recorded Coordinates", ToolTip = "Places a scene file's buildings at the world coordinates it recorded."))
	void ImportAtRecordedCoordinates();

	UPROPERTY(VisibleAnywhere, Category = "Loaded Region", meta = (DisplayName = "Buildings", ToolTip = "Number of loaded buildings at the last count."))
	int32 LoadedBuildings = 0;

	UPROPERTY(VisibleAnywhere, Category = "Loaded Region", meta = (DisplayName = "Triangles", ToolTip = "Total triangles across the loaded buildings at the last count."))
	int64 LoadedTriangles = 0;

	// Counted on demand rather than every frame.
	UFUNCTION(CallInEditor, Category = "Loaded Region", meta = (DisplayName = "Count Loaded Buildings", ToolTip = "Counts the loaded buildings and their triangles."))
	void RefreshCounts();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
};
