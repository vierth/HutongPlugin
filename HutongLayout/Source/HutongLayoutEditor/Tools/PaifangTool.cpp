#include "Tools/PaifangTool.h"
#include "Generation/PaifangGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongPaifangToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongPaifangTool>(SceneState.ToolManager);
}

void UHutongPaifangTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongPaifangToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Paifang"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongPaifangToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Panel order is registration order: presets after the params they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

double UHutongPaifangTool::GetFootprintDepth() const
{
	if (!Settings) return 200.0;
	const FHutongPaifangParams& P = Settings->Params;
	// The 夾杆石 spread either side of a column is the deepest thing on the ground.
	return FMath::Max(2.0 * (P.GetColumnRadius() + FMath::Max(P.PlinthSpread, 0.0)), 20.0);
}

void UHutongPaifangTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	const double dx = Local.X;
	const double dy = Local.Y;
	const double D = GetFootprintDepth();

	if (FMath::Abs(dx) >= FMath::Abs(dy))
	{
		OutMinX = FMath::Min(0.0, dx);
		OutMaxX = FMath::Max(0.0, dx);
		OutMinY = -0.5 * D;
		OutMaxY = 0.5 * D;
	}
	else
	{
		OutMinY = FMath::Min(0.0, dy);
		OutMaxY = FMath::Max(0.0, dy);
		OutMinX = -0.5 * D;
		OutMaxX = 0.5 * D;
	}
}

void UHutongPaifangTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	// Only two scalars arrive; compare them to recover the run direction.
	UHutongPaifangBuildingComponent::BuildPaifangMesh(
		Settings ? Settings->Params : FHutongPaifangParams(),
		FMath::Max(SizeX, SizeY),
		FMath::Min(SizeX, SizeY),
		SizeY > SizeX,
		OutMesh, Level);
}

void UHutongPaifangTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongPaifangBuildingComponent* Building =
		NewObject<UHutongPaifangBuildingComponent>(Actor, TEXT("Paifang"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->Length = FMath::Max(SizeX, SizeY);
	Building->Depth = FMath::Min(SizeX, SizeY);
	Building->bLengthAlongY = SizeY > SizeX;
	if (Appearance)
	{
		Building->Palette = Appearance->Palette;
	}

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline and lights attach to the actor's root, which needs the component live.
	Building->ApplyPlacementAttachments();
}

void UHutongPaifangTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.Height = FMath::Clamp(Settings->Params.Height + DeltaCm, 100.0, 5000.0);
}

double UHutongPaifangTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetHeight() : 0.0;
}

FString UHutongPaifangTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongPaifangParams& P = Settings->Params;
	return FString::Printf(TEXT("%s · %s"),
		P.GetBayCount() == 1 ? TEXT("One bay, two posts (一間二柱)") : TEXT("Three bays, four posts (三間四柱)"),
		P.bHasRoofs ? TEXT("Roofed memorial arch (牌樓)") : TEXT("Memorial arch (牌坊)"));
}

TArray<FText> UHutongPaifangTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongPaifangTool", "HelpSpan",
		"Drag across the street to set the span; the depth is fixed.");
	Lines.Insert(NSLOCTEXT("HutongPaifangTool", "HelpType",
		"Has Roofs: off for a stone arch (牌坊), on for a roofed arch (牌樓)."), 1);
	return Lines;
}
