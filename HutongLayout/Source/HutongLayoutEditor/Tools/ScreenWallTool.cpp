#include "Tools/ScreenWallTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongScreenWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongScreenWallTool>(SceneState.ToolManager);
}

void UHutongScreenWallTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongScreenWallToolProperties>(this);

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("ScreenWall"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongScreenWallToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongScreenWallTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	const double D = Settings ? Settings->Params.GetFootprintDepth() : 60.0;

	// The longer extent is the run; the other collapses. Same rule as the wall and the paifang.
	if (FMath::Abs(Local.X) >= FMath::Abs(Local.Y))
	{
		OutMinX = FMath::Min(0.0, Local.X);
		OutMaxX = FMath::Max(0.0, Local.X);
		OutMinY = -0.5 * D;
		OutMaxY = 0.5 * D;
	}
	else
	{
		OutMinY = FMath::Min(0.0, Local.Y);
		OutMaxY = FMath::Max(0.0, Local.Y);
		OutMinX = -0.5 * D;
		OutMaxX = 0.5 * D;
	}
}

void UHutongScreenWallTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	UHutongScreenWallBuildingComponent::BuildScreenWallMesh(
		Settings ? Settings->Params : FHutongScreenWallParams(),
		FMath::Max(SizeX, SizeY), SizeY > SizeX, OutMesh, Level);
}

void UHutongScreenWallTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongScreenWallBuildingComponent* Building =
		NewObject<UHutongScreenWallBuildingComponent>(Actor, TEXT("ScreenWall"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->Length = FMath::Max(SizeX, SizeY);
	Building->bLengthAlongY = SizeY > SizeX;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongScreenWallTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.Height = FMath::Clamp(Settings->Params.Height + DeltaCm, 60.0, 800.0);
}

double UHutongScreenWallTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

FString UHutongScreenWallTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	return FString::Printf(TEXT("Screen wall (影壁)%s"),
		Settings->Params.bHasPanel ? TEXT(" · screen panel (影壁心)") : TEXT(""));
}

TArray<FText> UHutongScreenWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongScreenWallTool", "HelpSpan",
		"Drag to set the screen's length. The depth is fixed by the moulded plinth (須彌座), so only the long axis follows the cursor.");
	Lines.Insert(NSLOCTEXT("HutongScreenWallTool", "HelpPlace",
		"A screen wall (影壁) stands facing a gate — a few paces inside a main gate (大門), or across the lane from one."), 1);
	return Lines;
}
