#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/CityWallGenerator.h"
#include "WallCornerTool.generated.h"

UCLASS()
class UHutongWallCornerToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Corner", meta=(ShowOnlyInnerProperties, ToolTip="The wall's section, which the corner carries round."))
	FHutongCityWallParams Params;

	UPROPERTY(EditAnywhere, Category="Corner", meta=(HutongBasic, DisplayName="Shape", ToolTip="A square platform under a corner tower, or the wall carried round a curve."))
	EHutongWallCornerShape Shape = EHutongWallCornerShape::Platform;

	UPROPERTY(EditAnywhere, Category="Corner", meta=(HutongBasic, DisplayName="Tower", EditCondition="Shape == EHutongWallCornerShape::Platform", ToolTip="What stands on a square corner platform."))
	EHutongCornerTower Tower = EHutongCornerTower::None;

	UPROPERTY(EditAnywhere, Category="Corner", meta=(DisplayName="Platform Size", UIMin="0", UIMax="6000", ClampMin="0", Units="cm", ToolTip="Width of the square corner platform, in cm; zero takes the city's."))
	double PlatformSize = 0.0;
};

// Places a 城角: click the wall's outside corner, move toward the inside of the city, click again.
UCLASS()
class UHutongWallCornerTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_WallCorner"); }
	virtual FString GetPlacementDetail() const override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;

	double PlatformSize() const;
	// Which footprint corner the first click is, from the quadrant the cursor is in.
	int32 OuterCornerIndex() const;

	EHutongCityWallRank LastRank = EHutongCityWallRank::Inner;

	UPROPERTY()
	TObjectPtr<UHutongWallCornerToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

UCLASS()
class UHutongWallCornerToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
