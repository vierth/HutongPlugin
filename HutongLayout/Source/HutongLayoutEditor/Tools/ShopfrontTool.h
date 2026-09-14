#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/BaySide.h"
#include "Generation/ShopfrontGenerator.h"
#include "ShopfrontTool.generated.h"

UCLASS()
class UHutongShopfrontToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Shopfront", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the shopfront (鋪面房)."))
	FHutongShopfrontParams Params;
};

// Places a 鋪面房, the shop that fronts a commercial street.
UCLASS()
class UHutongShopfrontTool : public URectDragToolBase
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
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Shopfront"); }
	virtual FString GetPlacementDetail() const override;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual void CancelPlacement() override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	// Bay count, stepped with [ and ] during placement like the siheyuan's.
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	int32 ComputeBayCountForSide() const;

	UPROPERTY()
	TObjectPtr<UHutongShopfrontToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	HutongGen::EBaySide BaySide = HutongGen::EBaySide::MinusY;
};

UCLASS()
class UHutongShopfrontToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
