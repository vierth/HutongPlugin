#include "Tools/ArrowTowerTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongArrowTowerToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongArrowTowerTool>(SceneState.ToolManager);
}

void UHutongArrowTowerTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongArrowTowerToolProperties>(this);
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("ArrowTower"), Settings, GET_MEMBER_NAME_CHECKED(UHutongArrowTowerToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

EHutongBaySide UHutongArrowTowerTool::FacadeFromCursor() const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	if (FMath::Abs(Local.X) > FMath::Abs(Local.Y)) return Local.X > 0.0 ? EHutongBaySide::MinusX : EHutongBaySide::PlusX;
	return Local.Y < 0.0 ? EHutongBaySide::PlusY : EHutongBaySide::MinusY;
}

void UHutongArrowTowerTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// The click is the middle of the front edge; the platform reaches back toward the cursor.
	const FHutongArrowTowerParams P = Settings ? Settings->Params : FHutongArrowTowerParams();
	const double W = P.GetPlatformWidth(), D = P.GetPlatformDepth();
	switch (FacadeFromCursor())
	{
	case EHutongBaySide::MinusX: OutMinX = 0.0; OutMaxX = D; OutMinY = -0.5 * W; OutMaxY = 0.5 * W; break;
	case EHutongBaySide::PlusX:  OutMinX = -D; OutMaxX = 0.0; OutMinY = -0.5 * W; OutMaxY = 0.5 * W; break;
	case EHutongBaySide::PlusY:  OutMinX = -0.5 * W; OutMaxX = 0.5 * W; OutMinY = -D; OutMaxY = 0.0; break;
	default:                     OutMinX = -0.5 * W; OutMaxX = 0.5 * W; OutMinY = 0.0; OutMaxY = D; break;
	}
}

void UHutongArrowTowerTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Level)
{
	const FHutongArrowTowerParams P = Settings ? Settings->Params : FHutongArrowTowerParams();
	UHutongArrowTowerBuildingComponent::BuildArrowTowerMesh(P, FacadeFromCursor(), OutMesh, Level);
}

void UHutongArrowTowerTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;
	UHutongArrowTowerBuildingComponent* Building = NewObject<UHutongArrowTowerBuildingComponent>(Actor, TEXT("ArrowTower"));
	if (!Building) return;
	if (Settings) Building->Params = Settings->Params;
	Building->Facade = FacadeFromCursor();
	if (Appearance) Building->Palette = Appearance->Palette;
	StampDetail(Building);
	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: attachments need the component live.
	Building->ApplyPlacementAttachments();
}

void UHutongArrowTowerTool::AdjustHeight(double DeltaCm)
{
	if (Settings) Settings->Params.LowerHeight = FMath::Clamp(Settings->Params.LowerHeight + 5.0 * DeltaCm, 200.0, 3000.0);
}

double UHutongArrowTowerTool::GetPreviewHeight() const
{
	if (!Settings) return 0.0;
	const FHutongArrowTowerParams& P = Settings->Params;
	return P.GetPlatformHeight() + P.LowerHeight + P.UpperHeight;
}

FString UHutongArrowTowerTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongArrowTowerParams& P = Settings->Params;
	return FString::Printf(TEXT("Arrow tower (箭樓) · %.1f × %.1f m on a %.1f m platform · front away from the cursor"),
		P.TowerWidth / 100.0, P.TowerDepth / 100.0, P.GetPlatformHeight() / 100.0);
}

TArray<FText> UHutongArrowTowerTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongArrowTowerTool", "HelpPlace",
		"Click the middle of the platform's front, move toward the inside of the city, and click again.");
	Lines.Insert(NSLOCTEXT("HutongArrowTowerTool", "HelpWhat",
		"The arrow tower (箭樓) at the front of a barbican (甕城); city wall runs started or ended on its sides join them."), 1);
	return Lines;
}
