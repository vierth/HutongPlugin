#include "Tools/CorridorTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongCorridorToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongCorridorTool>(SceneState.ToolManager);
}

void UHutongCorridorTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongCorridorToolProperties>(this);

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Corridor"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongCorridorToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongCorridorTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);

	// Unlike the wall and the paifang the short axis is dragged.
	const FHutongCorridorParams& P = Settings ? Settings->Params : FHutongCorridorParams();
	const double Extras = P.GetCrossExtras();
	const double Lo = FMath::Max(P.WalkWidthMin, 10.0) + Extras;
	const double Hi = FMath::Max(P.WalkWidthMax, FMath::Max(P.WalkWidthMin, 10.0) + 1.0) + Extras;

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

void UHutongCorridorTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	// Both extents matter now: the long one is the run, the short one sets the walk width.
	FHutongCorridorParams P = Settings ? Settings->Params : FHutongCorridorParams();
	P.Width = P.WalkWidthFromFootprint(FMath::Min(SizeX, SizeY));

	UHutongCorridorBuildingComponent::BuildCorridorMesh(
		P, FMath::Max(SizeX, SizeY), SizeY > SizeX, bFlipOpenSide, OutMesh,
		Level);
}

void UHutongCorridorTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongCorridorBuildingComponent* Building =
		NewObject<UHutongCorridorBuildingComponent>(Actor, TEXT("Corridor"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->Params.Width = Building->Params.WalkWidthFromFootprint(FMath::Min(SizeX, SizeY));
	Building->Width = Building->Params.Width;
	Building->Length = FMath::Max(SizeX, SizeY);
	Building->bLengthAlongY = SizeY > SizeX;
	Building->bFlipOpenSide = bFlipOpenSide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongCorridorTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.EaveHeight = FMath::Clamp(Settings->Params.EaveHeight + DeltaCm, 120.0, 600.0);
}

double UHutongCorridorTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

FString UHutongCorridorTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongCorridorParams& P = Settings->Params;
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double Cross = FMath::Min(MaxX - MinX, MaxY - MinY);
	return FString::Printf(TEXT("%s · width (寬) %.0f cm · %s"),
		P.bClosedSide ? TEXT("Ring corridor (抄手遊廊)") : TEXT("Covered corridor (遊廊)"),
		P.WalkWidthFromFootprint(Cross),
		bFlipOpenSide ? TEXT("opens -Y") : TEXT("opens +Y"));
}

FText UHutongCorridorTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("CorridorTool", "KeyHint", "[ ] open side · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongCorridorTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongCorridorTool", "HelpSpan",
		"Drag to set both the run and the walk width. The short axis is held between the Min and Max Walk Width below — wide enough to pass along, narrow enough to still read as a walk.");
	Lines.Insert(NSLOCTEXT("HutongCorridorTool", "HelpFlip",
		"[ and ] flip which side the colonnade opens onto. Closed On One Side below puts a wall along the other."), 1);
	return Lines;
}

void UHutongCorridorTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	// Each key picks a side rather than both toggling.
	if (Delta == 0) return;
	bFlipOpenSide = (Delta > 0);
}
