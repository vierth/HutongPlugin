#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "GalleryTool.generated.h"

// What the gallery lays out.
UCLASS()
class UHutongGalleryToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Include Variants", ToolTip="Lays out every variant of each type rather than one representative of each."))
	bool bIncludeVariants = true;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Spacing", UIMin="200", UIMax="1500", ClampMin="50", Units="cm", ToolTip="Clear ground between one footprint and the next, in cm."))
	double Spacing = 500.0;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Columns", UIMin="2", UIMax="10", ClampMin="1", ClampMax="16", ToolTip="Number of pieces per row of the grid."))
	int32 Columns = 6;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Outliner Folder", ToolTip="Outliner folder every piece the gallery spawns is placed in."))
	FName OutlinerFolder = TEXT("HutongGallery");

	UPROPERTY(EditAnywhere, Category="Contents", meta=(DisplayName="Walls (牆)", ToolTip="Includes the wall variants."))
	bool bWalls = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(DisplayName="Houses: Main Hall (正房), Side House (廂房), …", ToolTip="Includes the house presets."))
	bool bHouses = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(DisplayName="Gates: Main Gate (大門), Inner Gate (垂花門)", ToolTip="Includes the gate house and the inner gate."))
	bool bGates = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(DisplayName="Courtyard Pieces (遊廊, 影壁, 甬路, 花池, 魚缸)", ToolTip="Includes the covered corridor (遊廊), screen wall (影壁), paved path (甬路), flower bed (花池) and water jar (魚缸)."))
	bool bCourtyard = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(DisplayName="Street: Shopfront (鋪面房), Memorial Arch (牌坊)", ToolTip="Includes the shopfront and the paifang."))
	bool bStreet = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(DisplayName="Roofed Types: Pavilion (亭), Temple Hall (殿)", ToolTip="Includes the pavilion and the temple hall."))
	bool bRoofed = true;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Variation Seed", ToolTip="Seed for the gates' ajar angles."))
	int32 RandomSeed = 20250809;
};

// Places one of everything, laid out on a grid with room to walk between them.
UCLASS()
class UHutongGalleryTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void GetEffectiveRectBounds(
		double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void SpawnFinalActor() override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Gallery"); }
	virtual FString GetPlacementDetail() const override;
	virtual double GetPreviewHeight() const override { return 0.0; }

	// Nothing to bake: SpawnFinalActor is taken over entirely, exactly as the compound does.
	virtual void BuildMeshForRect(double, double, UE::Geometry::FDynamicMesh3&, EHutongDetail) override {}

	UPROPERTY()
	TObjectPtr<UHutongGalleryToolProperties> Settings;
};

UCLASS()
class UHutongGalleryToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

namespace HutongGallery
{
	// Builds every piece the gallery would place and reports each one's label and triangle count, spawning nothing.
	TArray<TPair<FString, int32>> BuildAll(const UHutongGalleryToolProperties* Settings);

	// What one piece costs.
	struct FHutongCostRow
	{
		static constexpr int32 NumLevels = 4;   // Massing, Far, Near, Hero — EHutongDetail's order

		FString Label;
		int32   Triangles[NumLevels] = {};
		int32   Vertices[NumLevels] = {};
		int32   MaterialSlots[NumLevels] = {};
		double  BuildMs[NumLevels] = {};

		// Near is what the plugin has always built and what every earlier figure was measured at.
		int32 NearTriangles() const { return Triangles[(int32)EHutongDetail::Near]; }
	};

	// Same walk as BuildAll, priced at every level.
	TArray<FHutongCostRow> BuildCostReport(const UHutongGalleryToolProperties* Settings);
}
