#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/FrameGenerator.h"
#include "FrameTool.generated.h"

UCLASS()
class UHutongFrameToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Stamp", meta=(DisplayName="Full House (房)", ToolTip="Stamps the whole house these params describe — walls, windows, roof — instead of its bare frame, at the same footprint, so the two can be placed side by side."))
	bool bFullHouse = false;

	UPROPERTY(EditAnywhere, Category="Frame", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the timber frame (構架)."))
	FHutongFrameParams Params;
};

// Stamps a house's 構架 alone at its preset's canonical footprint — columns, beams, purlins and rafters on the 臺明 — for teaching how it is put together.
UCLASS()
class UHutongFrameTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual double GetPreviewHeight() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return IsFullHouse() ? TEXT("Hutong_Siheyuan") : TEXT("Hutong_Frame"); }
	virtual FString GetPlacementDetail() const override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;
	virtual void RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround) override;

	// The preset's own frontage and depth, laid along the actor's axes for a front on Side.
	FVector2D StampSize(EHutongBaySide Side) const;
	int32 BayCount() const;
	bool IsFullHouse() const { return Settings && Settings->bFullHouse; }

	// Front edge in green with a mark at every column line, round a footprint centred on Origin.
	void DrawFront(FPrimitiveDrawInterface* PDI, const FVector& Origin, EHutongBaySide Side) const;

	UPROPERTY()
	TObjectPtr<UHutongFrameToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	EHutongBaySide BaySide = EHutongBaySide::MinusY;
};

UCLASS()
class UHutongFrameToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
