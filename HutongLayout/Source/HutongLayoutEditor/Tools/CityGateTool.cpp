#include "Tools/CityGateTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongCityGateToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongCityGateTool>(SceneState.ToolManager);
}

void UHutongCityGateTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongCityGateToolProperties>(this);
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("CityGate"), Settings, GET_MEMBER_NAME_CHECKED(UHutongCityGateToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

EHutongBaySide UHutongCityGateTool::FacadeFromCursor() const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	if (FMath::Abs(Local.X) > FMath::Abs(Local.Y)) return Local.X > 0.0 ? EHutongBaySide::MinusX : EHutongBaySide::PlusX;
	return Local.Y < 0.0 ? EHutongBaySide::PlusY : EHutongBaySide::MinusY;
}

void UHutongCityGateTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// The click is the middle of the front edge; the platform reaches back toward the cursor.
	const FHutongCityGateParams P = Settings ? Settings->Params : FHutongCityGateParams();
	const double W = P.GetLength(), D = P.GetDepth();
	switch (FacadeFromCursor())
	{
	case EHutongBaySide::MinusX: OutMinX = 0.0; OutMaxX = D; OutMinY = -0.5 * W; OutMaxY = 0.5 * W; break;
	case EHutongBaySide::PlusX:  OutMinX = -D; OutMaxX = 0.0; OutMinY = -0.5 * W; OutMaxY = 0.5 * W; break;
	case EHutongBaySide::PlusY:  OutMinX = -0.5 * W; OutMaxX = 0.5 * W; OutMinY = -D; OutMaxY = 0.0; break;
	default:                     OutMinX = -0.5 * W; OutMaxX = 0.5 * W; OutMinY = 0.0; OutMaxY = D; break;
	}
}

void UHutongCityGateTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Level)
{
	const FHutongCityGateParams P = Settings ? Settings->Params : FHutongCityGateParams();
	UHutongCityGateBuildingComponent::BuildCityGateMesh(P, FacadeFromCursor(), OutMesh, Level);
}

void UHutongCityGateTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;
	UHutongCityGateBuildingComponent* Building = NewObject<UHutongCityGateBuildingComponent>(Actor, TEXT("CityGate"));
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

void UHutongCityGateTool::AdjustHeight(double DeltaCm)
{
	if (Settings) Settings->Params.TowerHeight = FMath::Clamp(Settings->Params.TowerHeight + 5.0 * DeltaCm, 150.0, 1500.0);
}

double UHutongCityGateTool::GetPreviewHeight() const
{
	if (!Settings) return 0.0;
	const FHutongCityGateParams& P = Settings->Params;
	const double Tower = P.Tower == EHutongCityGateTower::Chenglou ? 2200.0 : (P.Tower == EHutongCityGateTower::None ? 0.0 : P.TowerHeight);
	return P.GetWall().GetHeight() + Tower;
}

FString UHutongCityGateTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongCityGateParams& P = Settings->Params;
	return FString::Printf(TEXT("City gate (城門) · %.1f m along the wall · passage %.1f m wide, %.1f m high · front away from the cursor"),
		P.GetLength() / 100.0, P.GetPassageWidth() / 100.0, P.GetPassageHeight() / 100.0);
}

TArray<FText> UHutongCityGateTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongCityGateTool", "HelpPlace",
		"Click the middle of the platform's front, move toward the inside of the city, and click again.");
	Lines.Insert(NSLOCTEXT("HutongCityGateTool", "HelpWhat",
		"The arrow tower (箭樓) at the front of a barbican (甕城); city wall runs started or ended on its sides join them."), 1);
	return Lines;
}
