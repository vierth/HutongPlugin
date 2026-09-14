#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/BaySide.h"
#include "Generation/HallGenerator.h"
#include "HallTool.generated.h"

UCLASS()
class UHutongHallToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Hall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the temple hall (殿)."))
	FHutongHallParams Params;
};

// Places a 殿, the hall of a small temple and the one type here carrying a 歇山 roof.
UCLASS()
class UHutongHallTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetKeyHintText() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Hall"); }
	virtual FString GetPlacementDetail() const override;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual void CancelPlacement() override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	int32 ComputeBayCountForSide() const;

	UPROPERTY()
	TObjectPtr<UHutongHallToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	HutongGen::EBaySide BaySide = HutongGen::EBaySide::MinusY;

	// Zero derives the count from the frontage, as everywhere else.
	int32 BayCountOverride = 0;
};

UCLASS()
class UHutongHallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
