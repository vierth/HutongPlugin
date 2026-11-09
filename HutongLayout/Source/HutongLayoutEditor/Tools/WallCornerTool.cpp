#include "Tools/WallCornerTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongWallCornerToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongWallCornerTool>(SceneState.ToolManager);
}

void UHutongWallCornerTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongWallCornerToolProperties>(this);
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("WallCorner"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongWallCornerToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { LastRank = Settings->Params.Rank; NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Settings);
	RegisterSettings(Presets);
	LastRank = Settings->Params.Rank;
}

void UHutongWallCornerTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	Super::OnPropertyModified(PropertySet, Property);
	if (PropertySet != Settings || Settings->Params.Rank == LastRank) return;
	Settings->Params.ChooseRank(Settings->Params.Rank);
	LastRank = Settings->Params.Rank;
	NotifyOfPropertyChangeByTool(Settings);
}

double UHutongWallCornerTool::PlatformSize() const
{
	if (!Settings) return UHutongWallCornerBuildingComponent::SizeFor(FHutongCityWallParams(), EHutongWallCornerShape::Platform, 0.0);
	return UHutongWallCornerBuildingComponent::SizeFor(Settings->Params, Settings->Shape, Settings->PlatformSize);
}

void UHutongWallCornerTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// The square opens from the clicked corner toward the cursor: that way is inside the city.
	const double S = PlatformSize();
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	const double SX = Local.X < 0.0 ? -S : S;
	const double SY = Local.Y < 0.0 ? -S : S;
	OutMinX = FMath::Min(0.0, SX); OutMaxX = FMath::Max(0.0, SX);
	OutMinY = FMath::Min(0.0, SY); OutMaxY = FMath::Max(0.0, SY);
}

int32 UHutongWallCornerTool::OuterCornerIndex() const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	const bool bLeft = Local.X < 0.0, bDown = Local.Y < 0.0;
	// Footprint corners anticlockwise from its min corner; the click is the one opposite the cursor's quadrant.
	if (!bLeft && !bDown) return 0;
	if (bLeft && !bDown) return 1;
	if (bLeft && bDown) return 2;
	return 3;
}

void UHutongWallCornerTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Level)
{
	const FHutongCityWallParams P = Settings ? Settings->Params : FHutongCityWallParams();
	UHutongWallCornerBuildingComponent::BuildCornerMesh(P, Settings ? Settings->Shape : EHutongWallCornerShape::Platform,
		PlatformSize(), OuterCornerIndex(), OutMesh, Level, Settings ? Settings->Tower : EHutongCornerTower::None);
}

void UHutongWallCornerTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;
	UHutongWallCornerBuildingComponent* Building = NewObject<UHutongWallCornerBuildingComponent>(Actor, TEXT("WallCorner"));
	if (!Building) return;
	if (Settings)
	{
		Building->Params = Settings->Params;
		Building->Shape = Settings->Shape;
		Building->Tower = Settings->Tower;
		Building->PlatformSize = Settings->PlatformSize;
	}
	Building->OuterCorner = OuterCornerIndex();
	if (Appearance) Building->Palette = Appearance->Palette;
	StampDetail(Building);
	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: attachments need the component live.
	Building->ApplyPlacementAttachments();
}

void UHutongWallCornerTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongCityWallParams& P = Settings->Params;
	P.PinFromRank();
	P.Height = FMath::Clamp(P.GetHeight() + 10.0 * DeltaCm, 200.0, 3000.0);
}

double UHutongWallCornerTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetHeight() : 0.0;
}

FString UHutongWallCornerTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	return FString::Printf(TEXT("Wall corner (城角) · platform %.1f m square · wall %.1f m high, %.1f m at the base"),
		PlatformSize() / 100.0, Settings->Params.GetHeight() / 100.0, Settings->Params.GetBaseWidth() / 100.0);
}

TArray<FText> UHutongWallCornerTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongWallCornerTool", "HelpPlace",
		"Click the outside corner of the wall, move toward the inside of the city, and click again.");
	Lines.Insert(NSLOCTEXT("HutongWallCornerTool", "HelpWhat",
		"The corner platform (城角) under the corner tower; city and palace wall runs started or ended on it join its two faces."), 1);
	return Lines;
}
