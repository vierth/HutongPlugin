#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/CompoundLayout.h"
#include "Generation/CorridorGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/InnerGateGenerator.h"
#include "Generation/PathGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/FlowerBedGenerator.h"
#include "Generation/WaterJarGenerator.h"
#include "Generation/ScreenWallGenerator.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/WallGenerator.h"
#include "CompoundTool.generated.h"

// Every building type a compound is made of, in one panel.
UCLASS()
class UHutongCompoundToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="North Direction (yaw)", UIMin="-180", UIMax="180", ClampMin="-360", ClampMax="360", Units="deg", ToolTip="World yaw that is north, in degrees; zero is +X."))
	double NorthYawDeg = 0.0;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(ToolTip="Number of courtyards the compound is laid out with."))
	EHutongCompoundPlan Plan = EHutongCompoundPlan::ThreeCourtyards;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Gate At East End", ToolTip="Puts the main gate (大門) at the east end of the street row."))
	bool bGateAtEastEnd = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Include Ear Rooms (耳房)", ToolTip="Adds an ear room (耳房) against each flank of the main hall (正房)."))
	bool bHasEarRooms = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Include Wing Ear Rooms (廂耳房)", ToolTip="Adds a wing ear room (廂耳房) at the south end of each side house (廂房)."))
	bool bHasWingEarRooms = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Court Walk", ToolTip="What shelters the inner court."))
	EHutongCourtWalk CourtWalk = EHutongCourtWalk::WingVerandas;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Covered Corridor (遊廊) Walk Width", EditCondition="CourtWalk == EHutongCourtWalk::Corridor", UIMin="95", UIMax="300", ClampMin="60", Units="cm", ToolTip="Clear walk width the covered corridor (遊廊) ring is built at, in cm."))
	double CorridorWalkWidth = HutongCanon::Compound::CorridorWalkWidthCm;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Include Paved Path (甬路)", ToolTip="Lays a paved path (甬路) from the gate to the main hall (正房)."))
	bool bHasPath = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Include Water Jar (魚缸) and Flower Beds (花池)", ToolTip="Places a water jar (魚缸) and flower beds (花池) in the court."))
	bool bHasCourtyardFurnishing = true;

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Main Gate (大門) Ridge Above Row", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Height of the main gate's ridge above the street row's, in cm; zero leaves it."))
	double GateRidgeClearance = HutongCanon::Gate::RidgeAboveRowCm;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Include Screen Wall (影壁) and the Gate Court", ToolTip="Adds a screen wall (影壁) and gate court inside the gate."))
	bool bHasScreenWall = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Partition the Far End of the Outer Court (外院)", ToolTip="Walls off a service yard at the far end of the outer court (外院)."))
	bool bHasOuterYard = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Service Yard Width", EditCondition="bHasOuterYard", UIMin="250", UIMax="800", ClampMin="250", Units="cm", ToolTip="Width of the service yard off the outer court (外院), in cm."))
	double OuterYardWidth = HutongCanon::Compound::OuterYardWidthCm;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Include Gate Lodge (門房)", ToolTip="Adds a gate lodge (門房) from the gate to the corner."))
	bool bHasGateLodge = true;

	UPROPERTY(EditAnywhere, Category="Plan", meta=(DisplayName="Gate Lodge (門房) Frontage", EditCondition="bHasGateLodge", UIMin="150", UIMax="600", ClampMin="120", Units="cm", ToolTip="Frontage of the gate lodge (門房), in cm."))
	double GateLodgeFrontage = HutongCanon::Compound::GateLodgeFrontageCm;

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Gap Between Buildings", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Clear ground left between neighbouring buildings inside the plot, in cm."))
	double Gap = 24.0;

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Minimum Courtyard (w x d)", Units="cm", ToolTip="Smallest clear inner court (內院), width by depth in cm."))
	FVector2D MinCourtyard = FVector2D(HutongCanon::Compound::MinCourtyardWidthCm,
		HutongCanon::Compound::MinCourtyardDepthCm);

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Ordinary Courtyard (w x d)", Units="cm", ToolTip="Size of the inner court (內院), width by depth in cm."))
	FVector2D Courtyard = FVector2D(HutongCanon::Compound::CourtyardWidthCm,
		HutongCanon::Compound::CourtyardDepthCm);

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Ordinary Outer Court (外院) Depth", UIMin="300", UIMax="1400", ClampMin="0", Units="cm", ToolTip="Depth of the outer court (外院) on an ordinary plot, in cm."))
	double TypicalOuterCourtDepth = HutongCanon::Compound::TypicalOuterCourtDepthCm;

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Ordinary Rear Court (後院) Depth", UIMin="300", UIMax="1200", ClampMin="0", Units="cm", ToolTip="Depth of the rear court (後院) on an ordinary plot, in cm."))
	double TypicalRearCourtDepth = HutongCanon::Compound::TypicalRearCourtDepthCm;

	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Base Course Top (下鹼)", UIMin="60", UIMax="180", ClampMin="30", Units="cm", ToolTip="Height the base course (下鹼) tops out at round the perimeter, in cm."))
	double BaseCourseTop = 110.0;

	// Its own setting for the reason CorridorWalkWidth and PassageWidth are: the screen's Length
	// is the drag field the panel never shows, and read from there the 影壁 was 400 for ever.
	UPROPERTY(EditAnywhere, Category="Dimensions", meta=(DisplayName="Screen Wall (影壁) Length", UIMin="150", UIMax="1200", ClampMin="150", Units="cm", ToolTip="Length of the screen wall (影壁) inside the gate, in cm."))
	double ScreenWallLength = 400.0;

	UPROPERTY(EditAnywhere, Category="Courtyard|Flower Bed (花池)", meta=(DisplayName="Bed Width", UIMin="60", ClampMin="40", Units="cm", ToolTip="Width of each flower bed (花池), in cm."))
	double FlowerBedSizeX = 200.0;

	UPROPERTY(EditAnywhere, Category="Courtyard|Flower Bed (花池)", meta=(DisplayName="Bed Depth", UIMin="60", ClampMin="40", Units="cm", ToolTip="Depth of each flower bed (花池), in cm."))
	double FlowerBedSizeY = 150.0;

	UPROPERTY(EditAnywhere, Category="Courtyard|Flower Bed (花池)", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the flower beds (花池)."))
	FHutongFlowerBedParams FlowerBed;

	UPROPERTY(EditAnywhere, Category="Courtyard|Water Jar (魚缸)", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the water jar (魚缸)."))
	FHutongWaterJarParams WaterJar;

	UPROPERTY(EditAnywhere, Category="Buildings|Main Hall (正房)", meta=(ToolTip="Parameters of the main hall (正房)."))
	FHutongSiheyuanParams MainHall;

	UPROPERTY(EditAnywhere, Category="Buildings|Ear Rooms (耳房)", meta=(ToolTip="Parameters of the ear rooms (耳房) and wing ear rooms (廂耳房)."))
	FHutongSiheyuanParams EarRoom;

	UPROPERTY(EditAnywhere, Category="Buildings|Side House (廂房)", meta=(ToolTip="Parameters of the side houses (廂房)."))
	FHutongSiheyuanParams SideHouse;

	UPROPERTY(EditAnywhere, Category="Buildings|Front Row (倒座房)", meta=(ToolTip="Parameters of the front row (倒座房) and the gate lodge (門房)."))
	FHutongSiheyuanParams FrontRow;

	UPROPERTY(EditAnywhere, Category="Buildings|Rear Row (後罩房)", meta=(ToolTip="Parameters of the rear row (後罩房)."))
	FHutongSiheyuanParams RearRow;

	UPROPERTY(EditAnywhere, Category="Buildings|Rear Row (後罩房)", meta=(DisplayName="Rear Court (後院) Depth", UIMin="200", UIMax="900", ClampMin="150", Units="cm", ToolTip="Smallest depth of the rear court (後院), in cm."))
	double RearCourtDepth = HutongCanon::Compound::RearCourtDepthCm;

	UPROPERTY(EditAnywhere, Category="Buildings|Rear Row (後罩房)", meta=(DisplayName="Covered Passage (過道) Width", UIMin="150", UIMax="400", ClampMin="120", Units="cm", ToolTip="Width of the covered passage (過道) strip past the ear room, in cm."))
	double PassageWidth = HutongCanon::Compound::PassageWidthCm;

	UPROPERTY(EditAnywhere, Category="Buildings|Rear Row (後罩房)", meta=(DisplayName="Covered Passage (過道) Roof", ToolTip="Parameters of the roof over the covered passage (過道)."))
	FHutongPassageParams Passage;

	UPROPERTY(EditAnywhere, Category="Buildings|Main Gate (大門)", meta=(ToolTip="Parameters of the main gate (大門)."))
	FHutongGateHouseParams GateHouse;

	UPROPERTY(EditAnywhere, Category="Buildings|Inner Gate (垂花門)", meta=(ToolTip="Parameters of the inner gate (垂花門)."))
	FHutongInnerGateParams InnerGate;

	UPROPERTY(EditAnywhere, Category="Buildings|Screen Wall (影壁)", meta=(ToolTip="Parameters of the screen wall (影壁)."))
	FHutongScreenWallParams ScreenWall;

	UPROPERTY(EditAnywhere, Category="Buildings|Covered Corridor (遊廊)", meta=(ToolTip="Parameters of the covered corridor (遊廊) ring."))
	FHutongCorridorParams Corridor;

	UPROPERTY(EditAnywhere, Category="Buildings|Paved Path (甬路)", meta=(ToolTip="Parameters of the paved path (甬路)."))
	FHutongPathParams Path;

	UPROPERTY(EditAnywhere, Category="Buildings|Perimeter Wall (院牆)", meta=(ToolTip="Parameters of the compound's walls."))
	FHutongWallParams Wall;

	UHutongCompoundToolProperties();
};

// Lays out a whole 四合院 from one drag, spawning a dozen ordinary actors.
UCLASS()
class UHutongCompoundTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

protected:
	virtual void RegisterToolSettings() override;

	// Forces the plot's frame so its +Y is north, which is what puts the 正房 facing south.
	void ApplyNorthOrientation();
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;
	virtual void SpawnFinalActor() override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Compound"); }
	virtual FString GetPlacementDetail() const override;
	virtual double GetPreviewHeight() const override;
	virtual void AdjustHeight(double DeltaCm) override;

	// Holds the drag up to the smallest plot the current settings can be laid out on.
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual void RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround) override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;

	// The plot's size for a cursor at LocalCursor in the plot frame.
	void PlotSizeForCursor(const FVector2D& LocalCursor, double& OutW, double& OutD) const;

	// The plan drawn from a given origin.
	void DrawPlan(FPrimitiveDrawInterface* PDI, const FVector& Origin, double SizeX, double SizeY) const;

	// Drops a building's 下鹼 so its band tops out level with the wall's.
	void AlignBaseCourse(FHutongSiheyuanParams& P,
		EHutongBaySide Facing, double SizeX, double SizeY) const;

	// Builds the layout input from the current settings and the dragged plot.
	HutongGen::FCompoundInput MakeInput(double SizeX, double SizeY) const;

	// The frontage and depth the layout gives a row's slot on the plot being placed, for seeding a
	// derived figure from the building that will actually be built rather than from the preset's
	// suggested frontage. False when the plan has no such slot.
	bool RowSlotSize(EHutongCompoundPiece Piece, double& OutFrontage, double& OutDepth) const;

public:
	// The same input off a settings object, which is the half a test can reach.
	static HutongGen::FCompoundInput MakeInputFrom(const UHutongCompoundToolProperties* S, double SizeX, double SizeY);

	// Spawns one slot as its own actor. Returns it, or null if nothing was built.
	AStaticMeshActor* SpawnSlot(UWorld* World, const FHutongCompoundSlot& Slot,
		double MinX, double MinY) const;

	// The street row's ridge, worked out from the 倒座房 slot before anything is spawned so the 大門 can be raised clear of it.
	mutable double StreetRowRidgeZ = 0.0;

	// Walks the slots and fills StreetRowRidgeZ in. Call before spawning any of them.
	void ResolveStreetRowRidge(const TArray<FHutongCompoundSlot>& Slots) const;

	UPROPERTY()
	TObjectPtr<UHutongCompoundToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

UCLASS()
class UHutongCompoundToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

namespace HutongCompound
{
	// The 廂房 the compound builds: the panel's preset plus whatever the court's walk asks of it.
	FHutongSiheyuanParams CourtWing(const FHutongSiheyuanParams& Base, EHutongCourtWalk Walk);

	// The hall row's 後檐, which is the plan's answer.
	FHutongSiheyuanParams CourtRow(const FHutongSiheyuanParams& Base, EHutongCompoundPlan Plan,
		EHutongBaySide Facing);
}
