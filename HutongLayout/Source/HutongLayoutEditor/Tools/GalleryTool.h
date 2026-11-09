#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "GalleryTool.generated.h"

// One band of the gallery, and the tool that places it alone. All = the whole gallery.
UENUM()
enum class EHutongGalleryCategory : uint8
{
	All,
	Walls UMETA(DisplayName="Walls (牆)"),
	Houses UMETA(DisplayName="Houses (房)"),
	Gates UMETA(DisplayName="Gates (門)"),
	Courtyard UMETA(DisplayName="Courtyard Pieces (院)"),
	Street UMETA(DisplayName="Street Buildings (街)"),
	Temples UMETA(DisplayName="Temples and Pavilions (殿亭)"),
};

// What the gallery lays out.
UCLASS()
class UHutongGalleryToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Include Variants", ToolTip="Lays out every variant of each type."))
	bool bIncludeVariants = true;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="All Four Facings", ToolTip="Places each piece four times, as laid out and turned a quarter, a half and three quarters, each rebuilt from its own settings, to check that a building and its turned twin share one mesh. A type that cannot face a way leaves that square empty."))
	bool bAllFacings = false;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Spacing", UIMin="200", UIMax="1500", ClampMin="50", Units="cm", ToolTip="Clear ground between one footprint and the next in a cluster, in cm. Clusters stand twice this apart, categories three times."))
	double Spacing = 700.0;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Pieces Per Row", UIMin="2", UIMax="10", ClampMin="1", ClampMax="16", ToolTip="Most pieces on one row; a larger cluster wraps onto its own rows."))
	int32 Columns = 6;

	// Set by the tool's builder: one category, or All for the whole gallery. Never saved with the settings.
	UPROPERTY(Transient)
	EHutongGalleryCategory GalleryCategory = EHutongGalleryCategory::All;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Outliner Folder", ToolTip="Outliner folder every piece the gallery spawns is placed in."))
	FName OutlinerFolder = TEXT("HutongGallery");

	UPROPERTY(EditAnywhere, Category="Contents", meta=(EditCondition="GalleryCategory == EHutongGalleryCategory::All", EditConditionHides, DisplayName="Walls (牆)", ToolTip="Includes the walls, garden doorways and decorative-window walls."))
	bool bWalls = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(EditCondition="GalleryCategory == EHutongGalleryCategory::All", EditConditionHides, DisplayName="Houses: Main Hall (正房), Side House (廂房), …", ToolTip="Includes the house presets."))
	bool bHouses = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(EditCondition="GalleryCategory == EHutongGalleryCategory::All", EditConditionHides, DisplayName="Gates: Gate Houses (屋宇式門), Wall Gates (牆垣式門), Inner Gates (垂花門)", ToolTip="Includes the gate houses, the gates in walls and the inner gates."))
	bool bGates = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(EditCondition="GalleryCategory == EHutongGalleryCategory::All", EditConditionHides, DisplayName="Courtyard: Corridor (遊廊), Screen Wall (影壁), Path (甬路), Flower Bed (花池), Water Jar (魚缸)", ToolTip="Includes the garden types."))
	bool bCourtyard = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(EditCondition="GalleryCategory == EHutongGalleryCategory::All", EditConditionHides, DisplayName="Street: Shopfront (鋪面房), Storey (樓), Memorial Arch (牌坊)", ToolTip="Includes the shopfronts, the storeyed buildings and the paifang."))
	bool bStreet = true;

	UPROPERTY(EditAnywhere, Category="Contents", meta=(EditCondition="GalleryCategory == EHutongGalleryCategory::All", EditConditionHides, DisplayName="Roofed Types: Pavilion (亭), Temple Hall (殿)", ToolTip="Includes the pavilion and the temple hall."))
	bool bRoofed = true;

	UPROPERTY(EditAnywhere, Category="Gallery", meta=(DisplayName="Variation Seed", ToolTip="Seed for the gates' ajar angles."))
	int32 RandomSeed = 20250809;

	bool Includes(EHutongGalleryCategory C) const;
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

	// Set by the builder before Setup.
	EHutongGalleryCategory Category = EHutongGalleryCategory::All;

protected:
	virtual void RegisterToolSettings() override;
	virtual void SpawnFinalActor() override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Gallery"); }
	virtual FString GetPlacementDetail() const override;
	virtual double GetPreviewHeight() const override { return 0.0; }

	// SpawnFinalActor is fully overridden, as in the compound.
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

	EHutongGalleryCategory Category = EHutongGalleryCategory::All;
};

namespace HutongGallery
{
	struct FPlacedPiece
	{
		FString Label;
		EHutongGalleryCategory Category = EHutongGalleryCategory::All;
		FString Cluster;
		FBox2D Footprint = FBox2D(ForceInit);
	};
	TArray<FPlacedPiece> Plan(const UHutongGalleryToolProperties* Settings);

	bool TurnPiece(class UHutongBuildingComponent* Building, int32 Quarter);

	void ForEachAttached(const UHutongGalleryToolProperties* Settings, UWorld* World,
		TFunctionRef<void(const FString& Label, class AStaticMeshActor* Actor)> Visit);

	FString CategoryName(EHutongGalleryCategory Category);

	struct FHutongCostRow
	{
		static constexpr int32 NumLevels = 4;   // Massing, Far, Near, Hero — EHutongDetail's order

		FString Label;
		int32   Triangles[NumLevels] = {};
		int32   Vertices[NumLevels] = {};
		int32   MaterialSlots[NumLevels] = {};
		double  BuildMs[NumLevels] = {};

		int32 NearTriangles() const { return Triangles[(int32)EHutongDetail::Near]; }
	};

	TArray<UClass*> AttachedClasses(const UHutongGalleryToolProperties* Settings, UWorld* World);

	void ForEachBuilt(const UHutongGalleryToolProperties* Settings, EHutongDetail Level,
		TFunctionRef<void(const FString& Label, const FVector2D& Footprint, const UE::Geometry::FDynamicMesh3& Mesh)> Visit);

	void WriteObj(const UHutongGalleryToolProperties* Settings, const FString& Path);

	TArray<FHutongCostRow> BuildCostReport(const UHutongGalleryToolProperties* Settings);
}
