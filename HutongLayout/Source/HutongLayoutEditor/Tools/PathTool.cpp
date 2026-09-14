#include "Tools/PathTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongPathToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongPathTool>(SceneState.ToolManager);
}

void UHutongPathTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongPathToolProperties>(this);

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Path"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongPathToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongPathTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);

	// Unlike the wall and the paifang the short axis is dragged rather than collapsed, and only held to a band.
	const FHutongPathParams& P = Settings ? Settings->Params : FHutongPathParams();
	const double Extras = 2.0 * P.GetKerbWidth();
	const double Lo = FMath::Max(P.WidthMin, 10.0) + Extras;
	const double Hi = FMath::Max(P.WidthMax, FMath::Max(P.WidthMin, 10.0) + 1.0) + Extras;

	// The anchor is one end of each extent, so the far end is the one that moves.
	auto Band = [&](double& OutLo, double& OutHi, double Reach)
	{
		const double Want = FMath::Clamp(FMath::Abs(Reach), Lo, Hi);
		if (Reach >= 0.0) { OutLo = 0.0;     OutHi = Want; }
		else              { OutLo = -Want;   OutHi = 0.0;  }
	};

	if (FMath::Abs(Local.X) >= FMath::Abs(Local.Y))
	{
		OutMinX = FMath::Min(0.0, Local.X);
		OutMaxX = FMath::Max(0.0, Local.X);
		Band(OutMinY, OutMaxY, Local.Y);
	}
	else
	{
		OutMinY = FMath::Min(0.0, Local.Y);
		OutMaxY = FMath::Max(0.0, Local.Y);
		Band(OutMinX, OutMaxX, Local.X);
	}
}

void UHutongPathTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	const FHutongPathParams P = Settings ? Settings->Params : FHutongPathParams();
	UHutongPathBuildingComponent::BuildPathMesh(
		P, FMath::Max(SizeX, SizeY), P.WidthFromFootprint(FMath::Min(SizeX, SizeY)),
		SizeY > SizeX, OutMesh, Level);
}

void UHutongPathTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongPathBuildingComponent* Building =
		NewObject<UHutongPathBuildingComponent>(Actor, TEXT("Path"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->Width = Building->Params.WidthFromFootprint(FMath::Min(SizeX, SizeY));
	Building->Length = FMath::Max(SizeX, SizeY);
	Building->bLengthAlongY = SizeY > SizeX;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongPathTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.Rise = FMath::Clamp(Settings->Params.Rise + 0.1 * DeltaCm, 0.0, 40.0);
}

double UHutongPathTool::GetPreviewHeight() const
{
	// A path has no height to preview; the corner posts would be noise.
	return 0.0;
}

FString UHutongPathTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongPathParams& P = Settings->Params;
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double Cross = FMath::Min(MaxX - MinX, MaxY - MinY);
	return FString::Printf(TEXT("Paved path (甬路) · width (寬) %.0f cm"), P.WidthFromFootprint(Cross));
}

TArray<FText> UHutongPathTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongPathTool", "HelpSpan",
		"Drag to set the run and the width. The short axis is held between the Min and Max Width below.");
	Lines.Insert(NSLOCTEXT("HutongPathTool", "HelpWhat",
		"A paved path (甬路) runs from the gate to the steps of the main hall (正房). A courtyard without one reads as open ground."), 1);
	return Lines;
}
