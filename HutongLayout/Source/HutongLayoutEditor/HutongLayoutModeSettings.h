#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Generation/HutongDetail.h"
#include "HutongLayoutModeSettings.generated.h"

// The step drawn sizes round to, in 營造尺 (32 cm): buildings of one kind come out the same size and share
// one mesh, which is built once (about a quarter second) and reused (about a millisecond).
UENUM()
enum class EHutongSizeStep : uint8
{
	Off      UMETA(DisplayName = "Off", ToolTip = "Sizes follow the cursor exactly."),
	Cun      UMETA(DisplayName = "1 cun (寸, 3.2 cm)", ToolTip = "Sizes round to whole cun."),
	FiveCun  UMETA(DisplayName = "5 cun (五寸, 16 cm)", ToolTip = "Sizes round to five cun."),
	Chi      UMETA(DisplayName = "1 chi (尺, 32 cm)", ToolTip = "Sizes round to whole chi."),
	Zhang    UMETA(DisplayName = "1 zhang (丈, 3.2 m)", ToolTip = "Sizes round to whole zhang."),
};

namespace HutongSizeStep
{
	inline double Cm(EHutongSizeStep Step)
	{
		switch (Step)
		{
		case EHutongSizeStep::Cun:     return 3.2;
		case EHutongSizeStep::FiveCun: return 16.0;
		case EHutongSizeStep::Chi:     return 32.0;
		case EHutongSizeStep::Zhang:   return 320.0;
		default:                       return 0.0;
		}
	}

	// Length rounded to a whole number of steps, never under one step.
	inline double Round(double Length, double StepCm)
	{
		if (StepCm <= 0.0) return Length;
		const double Sign = Length < 0.0 ? -1.0 : 1.0;
		return Sign * FMath::Max(StepCm, FMath::RoundToDouble(FMath::Abs(Length) / StepCm) * StepCm);
	}
}

// The mode's own panel, under the active tool's, collapsed as "Placed Buildings".
UCLASS(config = EditorPerProjectUserSettings)
class UHutongLayoutModeSettings : public UObject
{
	GENERATED_BODY()

public:
	// Categories: Geometry, Export, Import, Housekeeping, Selection, Information (FHutongModeSettingsCustomization
	// orders them and draws every action as a button row with its description, greyed when it has nothing to act on).

	// Tool panel's simple/advanced switch; toggled there, not on this panel.
	UPROPERTY(config)
	bool bShowAdvancedSettings = false;

	// ---- Geometry: laid-out outlines or built meshes ----

	UPROPERTY(EditAnywhere, config, Category = "Geometry", meta = (DisplayName = "Layout Only (no geometry)", ToolTip = "New buildings are placed as outlines only."))
	bool bPlanOnly = true;

	// Mirrors HutongPlanOutline's switch: plans draw regardless of mode, and another plugin's panel
	// wants them hidden. Seeded on Enter so a fresh object cannot report hidden plans as visible.
	UPROPERTY(EditAnywhere, config, Category = "Geometry", meta = (DisplayName = "Size Step", ToolTip = "Drawn and resized footprints round to this step, so buildings of one kind match and share one mesh. Snapping to a placed building still wins."))
	EHutongSizeStep SizeStep = EHutongSizeStep::FiveCun;

	UPROPERTY(EditAnywhere, Category = "Geometry", meta = (DisplayName = "Show Plan Outlines", ToolTip = "Show every building's footprint outline."))
	bool bShowPlanOutlines = true;

	// Type and preset written on each footprint once it is big enough on screen to hold them.
	UPROPERTY(EditAnywhere, config, Category = "Geometry", meta = (DisplayName = "Label Buildings Up Close", ToolTip = "Show each building's type and preset on its footprint when zoomed in."))
	bool bShowBuildingLabels = true;

	// Off: a built building hides the plans behind it. Mirrors hutong.PlansOverBuildings.
	UPROPERTY(EditAnywhere, config, Category = "Geometry", meta = (DisplayName = "Plans Show Through Buildings", ToolTip = "Draw outlines over built buildings."))
	bool bPlansOverBuildings = false;

	void GenerateLoadedGeometry();

	void GenerateSelectedGeometry();

	void RevertLoadedToLayout();

	void RevertSelectedToLayout();

	// ---- Selection: a setting, then Apply (drawn by FHutongModeSettingsCustomization) ----

	// Follows the selection: shows the selected buildings' level until changed.
	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Detail Level", ToolTip = "Detail level for the selected buildings."))
	EHutongDetail TargetLevel = EHutongDetail::Near;

	// Converts a placed building's type: footprint, facing and transform stay; parameters come from
	// the new type (shop to house, 院牆 to 隔牆) with no redraw.
	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Convert To", GetOptions = "GetConvertOptions", ToolTip = "Type for the selected buildings."))
	FString ConvertTo;

	UFUNCTION()
	TArray<FString> GetConvertOptions() const;

	// Empty uses the target type's defaults, right for types with one preset or none; a house
	// needs a preset named.
	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Convert Preset", GetOptions = "GetConvertPresetOptions", ToolTip = "Preset for the selected buildings; empty uses the type's defaults."))
	FString ConvertPreset;

	UFUNCTION()
	TArray<FString> GetConvertPresetOptions() const;

	UPROPERTY(EditAnywhere, config, Category = "Selection", meta = (DisplayName = "Divide At Bay Line", UIMin = "0", UIMax = "32", ClampMin = "0", ToolTip = "Bay line to divide at, counted from the start; 0 is the middle."))
	int32 DivideAtBayLine = 0;

	// Whether the selection holds anything the button acts on.
	bool HasSelection() const;
	bool HasSelectedLayout() const;
	bool HasSelectedBuilt() const;

	// What each Apply would do, for its button's enabled state.
	bool CanApplyDetailLevel() const;
	bool CanApplyConvert() const;
	bool CanApplyDivide() const;
	bool CanFuse() const;

	void ApplyDetailLevel();
	void ApplyConvert();
	void ApplyDivide();
	void FuseSelection();

	// Called when the editor selection changes: the level dropdown shows what is selected.
	void SyncToSelection();

	// ---- Export ----

	// Writes each loaded building's placement and its changes from type defaults: enough to rebuild the street.
	void ExportAll();

	void ExportSelection();

	// ---- Import ----

	// Off: the file's buildings go back to the coordinates it recorded. On: the Import tool carries
	// the set under the cursor to be turned and dropped by hand.
	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Customize Placement", ToolTip = "Place an imported layout by hand instead of at its saved position."))
	bool bCustomizePlacement = false;

	void ImportLayout();

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Import Folder", ToolTip = "Outliner folder for imported buildings; empty keeps the file's."))
	FName ImportFolder = TEXT("HutongImport");

	UPROPERTY(EditAnywhere, config, Category = "Import", meta = (DisplayName = "Update Matching Placements", ToolTip = "Update buildings already in the level instead of adding copies."))
	bool bUpdateMatchingPlacements = true;

	// Coordinates from another level mean nothing here, so the Import tool carries the set under
	// the cursor, turns it with R and drops it on a click.
	void PlaceSceneByHand();

	// Straight back to the world coordinates the file recorded, with no drag.
	void ImportAtRecordedCoordinates();

	// Remembered here rather than on the import tool's property set.
	UPROPERTY(config)
	FString LastSceneFile;

	// Last file each export button wrote, so the dialog reopens there instead of on the default.
	// Separate from the selection's: saving a selection over a whole-scene file loses the street.
	UPROPERTY(config)
	FString LastExportFile;

	UPROPERTY(config)
	FString LastSelectionExportFile;

	// ---- Housekeeping ----

	// Placement unchanged: rebuilt from the parameters each building carries.
	void RebuildLoaded();

	void RebuildSelection();

	// One editable material asset per slot, worn by every placement and rebuild instead of the
	// tinted default until a palette material is assigned. Existing assets are kept.
	void CreateStarterMaterials();

	// Identical buildings share one mesh asset under /Game/HutongLayout/Generated; a rebuild or a deleted
	// building can leave one no building wears.
	// Two buildings on one footprint under different ids; imports check their own arrivals.
	void FindOverlappingBuildings();

	void DeleteUnusedGeneratedMeshes();

	// ---- Information ----

	UPROPERTY(VisibleAnywhere, Category = "Information", meta = (DisplayName = "Buildings", ToolTip = "Loaded buildings at the last count."))
	int32 LoadedBuildings = 0;

	UPROPERTY(VisibleAnywhere, Category = "Information", meta = (DisplayName = "Triangles", ToolTip = "Triangles in the loaded buildings at the last count."))
	int64 LoadedTriangles = 0;

	// Counted on demand rather than every frame.
	void RefreshCounts();

private:
	// The two export buttons differ in what they gather and which name they remember.
	void DoExport(bool bSelection, const TCHAR* FallbackName, FString& Remembered);

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
};
