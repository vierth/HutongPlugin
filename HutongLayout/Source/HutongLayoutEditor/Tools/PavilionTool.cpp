#include "Tools/PavilionTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongPavilionToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongPavilionTool>(SceneState.ToolManager);
}

void UHutongPavilionTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongPavilionToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Pavilion"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongPavilionToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongPavilionTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);

	// Held roughly square.
	const double SX = OutMaxX - OutMinX;
	const double SY = OutMaxY - OutMinY;
	if (SX < 1.0 || SY < 1.0) return;

	auto Hold = [](double& Lo, double& Hi, double Want)
	{
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};

	if (SX > SY * 1.34)      Hold(OutMinX, OutMaxX, SY * 1.34);
	else if (SY > SX * 1.34) Hold(OutMinY, OutMaxY, SX * 1.34);
}

void UHutongPavilionTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	UHutongPavilionBuildingComponent::BuildPavilionMesh(
		Settings ? Settings->Params : FHutongPavilionParams(), SizeX, SizeY, OutMesh,
		Level);
}

void UHutongPavilionTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongPavilionBuildingComponent* Building =
		NewObject<UHutongPavilionBuildingComponent>(Actor, TEXT("Pavilion"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->Width = SizeX;
	Building->Depth = SizeY;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongPavilionTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.EaveHeight = FMath::Clamp(Settings->Params.EaveHeight + DeltaCm, 120.0, 600.0);
}

double UHutongPavilionTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

FString UHutongPavilionTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongPavilionParams& P = Settings->Params;
	const int32 Open = FMath::Clamp(P.OpenSides, 0, 4);
	return FString::Printf(TEXT("Pavilion (亭) · pyramidal roof (攢尖) · %d open side%s"), Open, Open == 1 ? TEXT("") : TEXT("s"));
}

TArray<FText> UHutongPavilionTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongPavilionTool", "HelpDrag",
		"Click to anchor, move, click to place. There is no facing stage — a pavilion (亭) has no front — and the plan is held roughly square.");
	Lines.Insert(NSLOCTEXT("HutongPavilionTool", "HelpSides",
		"Open Sides below leaves that many sides without a bench, counted from the first, so the pavilion (亭) can be walked into."), 1);
	return Lines;
}
