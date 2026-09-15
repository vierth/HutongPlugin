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
	// Declared first: category order is declaration order, and export is the first thing on the tab.
	// Writes every loaded building's placement and whatever it has been tuned away from its type's
	// defaults, which is the whole of what it takes to build the street again.
	UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayName = "Export Loaded Buildings", ToolTip = "Writes every loaded building to a scene file."))
	void ExportLoaded();

	UFUNCTION(CallInEditor, Category = "Export", meta = (DisplayName = "Export Selection", ToolTip = "Writes the selected buildings to a scene file."))
	void ExportSelection();

private:
	// The two export buttons differ in what they gather and which name they remember.
	void DoExport(bool bSelection, const TCHAR* FallbackName, FString& Remembered);

public:
	UPROPERTY(EditAnywhere, config, Category = "Plan", meta = (DisplayName = "Layout Only (outlines, no geometry)", ToolTip = "Places new buildings as footprint outlines with no geometry."))
	bool bPlanOnly = true;

	// A view onto HutongPlanOutline's switch rather than a setting of its own: the plans draw
	// whether or not this mode is up, and the panel that most wants them out of the way is another
	// plugin's. Seeded from the switch when the mode is entered, so a fresh settings object cannot
	// report the plans visible while they are hidden.
	UPROPERTY(EditAnywhere, Category = "Plan", meta = (DisplayName = "Show Plan Outlines", ToolTip = "Draws the footprint outline of every laid-out building."))
	bool bShowPlanOutlines = true;


	UFUNCTION(CallInEditor, Category = "Plan", meta = (DisplayName = "Generate Geometry (loaded)", ToolTip = "Builds full geometry for every laid-out building currently loaded."))
	void GenerateLoadedGeometry();

	UFUNCTION(CallInEditor, Category = "Plan", meta = (DisplayName = "Generate Geometry (selection)", ToolTip = "Builds full geometry for the selected laid-out buildings."))
	void GenerateSelectedGeometry();

	// Nothing about the placement changes: every building is built again from the parameters it is already carrying.
	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Rebuild Selection", ToolTip = "Re-bakes each selected building from the parameters it carries."))
	void RebuildSelection();

	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Promote To", ToolTip = "Detail level that Promote Selection and Set Loaded Region apply."))
	EHutongDetail TargetLevel = EHutongDetail::Near;

	UFUNCTION(CallInEditor, Category = "Selection", meta = (DisplayName = "Promote Selection", ToolTip = "Sets the selected buildings to the Promote To level and rebuilds them."))
	void PromoteSelection();

	// What a placed building is, changed after the fact: the footprint, the facing and the
	// transform are the plan's and stay, the parameters come from the type converted to. A shop
	// becomes a house, a 院牆 becomes a 隔牆, without the rectangle being drawn again.
	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Convert To", GetOptions = "GetConvertOptions", ToolTip = "Building type that Convert Selection turns the selected buildings into."))
	FString ConvertTo;

	UFUNCTION()
	TArray<FString> GetConvertOptions() const;

	// Empty builds from the target type's own defaults, which is what a type with one preset or
	// none wants; a house wants to be told which house.
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

	// Everything streamed in — which under World Partition is the region you are standing in rather than the level.
	UFUNCTION(CallInEditor, Category = "Loaded Region", meta = (DisplayName = "Set Loaded Region To Promote Level", ToolTip = "Sets every loaded building to the Promote To level and rebuilds it."))
	void SetLoadedRegionToTarget();

	UFUNCTION(CallInEditor, Category = "Loaded Region", meta = (DisplayName = "Rebuild Loaded Buildings", ToolTip = "Re-bakes every loaded building from the parameters it carries."))
	void RebuildLoaded();

	// One editable material asset per slot, which every placement and rebuild then wears in place
	// of the tinted default until a material is assigned on the palette. Existing assets are kept.
	UFUNCTION(CallInEditor, Category = "Materials", meta = (DisplayName = "Create Starter Materials", ToolTip = "Writes one editable material per surface slot to /Game/HutongLayout/Materials; existing assets are kept."))
	void CreateStarterMaterials();

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Import Folder", ToolTip = "Outliner folder imported buildings go in; empty keeps the file's."))
	FName ImportFolder = TEXT("HutongImport");

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Update Matching Placements", ToolTip = "Updates buildings whose id matches an imported record in place."))
	bool bUpdateMatchingPlacements = true;

	// A file written in one level lands in another at coordinates that mean nothing there, so the
	// set has to be positionable by hand: this hands the file to the Import tool, which carries the
	// whole set under the cursor, turns it with R and lays it down on a click.
	UFUNCTION(CallInEditor, Category = "Import", meta = (DisplayName = "Place By Hand (Import Tool)", ToolTip = "Loads a scene file into the Import tool to place by hand."))
	void PlaceSceneByHand();

	// Remembered here rather than on the import tool's property set.
	UPROPERTY(config)
	FString LastSceneFile;

	// What each export button last wrote, so the save dialog opens on that folder with that name
	// rather than on the shipped default every time: a street is exported over and over as it is
	// laid out, and retyping the name each time is how a session ends up with one file called
	// HutongScene and no idea which lane it holds. Kept apart from the selection's, since one
	// careless Save over a whole-scene file with a selection loses the rest of the street.
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
