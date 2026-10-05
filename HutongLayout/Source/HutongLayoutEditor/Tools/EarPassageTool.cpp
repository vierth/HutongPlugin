#include "Tools/EarPassageTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongEarPassageToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongEarPassageTool>(SceneState.ToolManager);
}

void UHutongEarPassageToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Same clamps as the house tool, on the inner room.
	Params.Room.ClampAfterEdit(PropertyChangedEvent.GetPropertyName());
}

void UHutongEarPassageTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (!Settings || !SnappingActive()) return;
	const FHutongEarPassageParams& P = Settings->Params;
	if (!P.Room.bSnapToSuggested) return;

	// Suggested frontage = room + passage strip (or the passage alone); depth = room's.
	const double Extra = P.HasRoom() ? P.GetStripWidth() : P.GetSuggestedWidth() - P.Room.SuggestedFrontage;
	auto SnapSide = [&](double& Lo, double& Hi)
	{
		const double Want = P.Room.SnapExtent(FMath::Abs(Hi - Lo), Extra);
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};
	SnapSide(OutMinX, OutMaxX);
	SnapSide(OutMinY, OutMaxY);
}

void UHutongEarPassageTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongEarPassageToolProperties>(this);

	// Registration order is panel order; presets before params, as on the house tool (the three kinds
	// of ear room are a preset pick).
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("EarPassage"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongEarPassageToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Presets);

	RegisterSettings(Settings);

	ApplyDefaultPreset(Presets, HutongPresets::DefaultEarRoomName());
}

FHutongEarPassageParams UHutongEarPassageTool::GetResolvedParams() const
{
	FHutongEarPassageParams P = Settings ? Settings->Params : FHutongEarPassageParams();
	P.BayCountOverride = BayCountOverride;
	if (bIsDragging)
	{
		double MinX, MinY, MaxX, MaxY;
		GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = FMath::Max(bAlongX ? MaxX - MinX : MaxY - MinY, 1.0);
		P.Depth = FMath::Max(bAlongX ? MaxY - MinY : MaxX - MinX, 1.0);
	}
	else if (P.GetSuggestedWidth() > 0.0)
	{
		P.Width = P.GetSuggestedWidth();
		P.Depth = FMath::Max(P.Room.GetSuggestedDepth(), 1.0);
	}
	P.Width = P.GetWidth();
	return P;
}

FString UHutongEarPassageTool::GetPlacementDetail() const
{
	const FHutongEarPassageParams P = GetResolvedParams();
	const FHutongSiheyuanParams R = P.RoomParams();
	const TCHAR* End = P.bPassageAtFarEnd ? TEXT("far") : TEXT("origin");
	switch (P.Passageway)
	{
	case EHutongEarPassage::None:
		return FString::Printf(TEXT("room %d bays @ %.0f cm%s"), R.GetBayCount(), R.Width / FMath::Max(R.GetBayCount(), 1),
			R.bHasFrontDoorCenter ? TEXT("") : TEXT(" · no front door"));
	case EHutongEarPassage::Whole:
		return FString::Printf(TEXT("one-bay passage (過道) %.0f cm clear · outer wall at the %s end"), P.GetClearWidth(), End);
	default:
		return FString::Printf(TEXT("room %d bays @ %.0f cm · passage (過道) %.0f cm clear at the %s end"),
			R.GetBayCount(), R.Width / FMath::Max(R.GetBayCount(), 1), P.GetClearWidth(), End);
	}
}

FText UHutongEarPassageTool::GetKeyHintText() const
{
	if (!Settings || !IsPlacingActive()) return Super::GetKeyHintText();
	return Settings->Params.HasPassage()
		? FText::Format(NSLOCTEXT("EarPassageTool", "KeyHint", "[ ] bays · Shift+[ ] passage end · {0}"), Super::GetKeyHintText())
		: FText::Format(NSLOCTEXT("EarPassageTool", "KeyHintRoom", "[ ] bays · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongEarPassageTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("EarPassageTool", "HelpFacade",
		"Move toward the facade side, then click to place. [ and ] set one or two bays; Shift+[ and ] swap the passage end."), 1);
	return Lines;
}

void UHutongEarPassageTool::AdjustBracketValue(int32 Delta, bool bShift, bool)
{
	if (!Settings) return;
	// Shift: the passage's end. Plain: the room's bays, stepping off what the drag derives.
	if (bShift)
	{
		if (Settings->Params.HasPassage()) Settings->Params.bPassageAtFarEnd = !Settings->Params.bPassageAtFarEnd;
		return;
	}
	const int32 Current = GetResolvedParams().RoomParams().GetBayCount();
	BayCountOverride = FMath::Clamp(Current + Delta, 1, FHutongEarPassageParams::MaxRoomBays);
}

void UHutongEarPassageTool::CancelPlacement()
{
	BayCountOverride = 0;
	Super::CancelPlacement();
}

void UHutongEarPassageTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongSiheyuanParams& R = Settings->Params.Room;
	if (R.bDeriveProportions && R.bDeriveEaveFromBays)
	{
		R.EaveHeight = GetResolvedParams().RoomParams().GetEaveHeight();
		R.bDeriveEaveFromBays = false;
	}
	R.EaveHeight = FMath::Clamp(R.EaveHeight + DeltaCm, FMath::Max(R.DoorTopHeight + 10.0, R.GetMinEaveHeight()), 5000.0);
}

double UHutongEarPassageTool::GetPreviewHeight() const
{
	return Settings ? GetResolvedParams().GetEaveHeight() : 0.0;
}

void UHutongEarPassageTool::OnPlacementStarted(const FVector& HitWorld)
{
	BayCountOverride = 0;
	BaySide = ComputeDefaultBaySide();
}

bool UHutongEarPassageTool::OnRectCommitted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	return false;
}

void UHutongEarPassageTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	BaySide = bRectCommitted ? ComputeClosestSide(HitWorld.X, HitWorld.Y) : ComputeDefaultBaySide();
}

TArray<FText> UHutongEarPassageTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("EarPassageTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongEarPassageTool::GetStagePromptText() const
{
	if (bIsDragging && bRectCommitted && !bRotateModeActive && Settings && !Settings->Params.HasPassage())
	{
		return NSLOCTEXT("EarPassageTool", "PromptFacadeRoom", "Move to pick the side that gets the facade (green ticks), then click to place.");
	}
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("EarPassageTool", "PromptBaySide",
			"Move to pick the facade and doorway side (green ticks), then click to place. [ ] bays, Shift+[ ] passage end.");
	}
	if (!bIsDragging && Presets && !Presets->Preset.IsEmpty() && GetPlanEditPromptText().IsEmpty())
	{
		return FText::Format(NSLOCTEXT("EarPassageTool", "PromptAnchorPreset",
			"Placing {0}: click the ground to anchor a corner of the footprint."), FText::FromString(Presets->Preset));
	}
	return Super::GetStagePromptText();
}

void UHutongEarPassageTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double SizeX = MaxX - MinX, SizeY = MaxY - MinY;
	if (SizeX < 1.0 || SizeY < 1.0) return;

	const FHutongEarPassageParams P = GetResolvedParams();

	const FLinearColor StripColor(0.60f, 0.85f, 0.45f, 1.0f);

	const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double DepthMin = Edge.bAlongX ? MinY : MinX;
	const double DepthMax = Edge.bAlongX ? MaxY : MaxX;

	// Point at T along the frontage, U across, drag frame.
	auto At = [&](double T, double U) { return Edge.bAlongX ? LocalRectToWorld(SpanMin + T, U) : LocalRectToWorld(U, SpanMin + T); };
	auto OnEdge = [&](double T) { return At(T, Edge.FixedCoord); };

	// Computed in build frame (facade on -Y), then turned by the same RotateVertex as the mesh, so strip
	// end and doorway land where built on +Y and -X, which reverse the frontage.
	auto Facade = [&](double X)
	{
		const FVector3d V = HutongGen::BaySide::RotateVertex(BaySide, FVector3d(X, 0.0, 0.0), SizeX, SizeY);
		return Edge.bAlongX ? V.X : V.Y;
	};

	// Room bay ticks along the facade edge, strip line included.
	FHutongPlanBays Bays = UHutongEarPassageBuildingComponent::PlanBaysInBuildFrame(P);
	HutongGen::PlanBays::OntoFacade(Bays, BaySide, SizeX, SizeY);
	DrawRectBaysAndFacing(PDI, BaySide, MinX, MinY, MaxX, MaxY, Bays.Boundaries, Bays.DoorBay);

	if (!P.HasPassage()) return;

	// Passage strip and clear way, dashed across the depth.
	const double ClearA = Facade(P.GetClearX0());
	const double ClearB = Facade(P.GetClearX0() + P.GetClearWidth());
	const double StripA = Facade(P.GetStripX0());
	const double StripB = Facade(P.GetStripX0() + P.GetStripWidth());
	for (const double T : { StripA, StripB, ClearA, ClearB })
	{
		DrawDashedPreviewLine(PDI, At(T, DepthMin), At(T, DepthMax), StripColor, 2.5f, 28.0);
	}
	// Closing-wall doorway on the facade edge, as the wall cuts it.
	if (P.HasClosingDoorway())
	{
		const FHutongWallParams C = P.ClosingWallParams();
		const double Centre = P.GetClearX0() + C.GetDoorwayCentre(C.Length);
		const double Half = 0.5 * C.GetDoorwayWidth();
		DrawPreviewLine(PDI, OnEdge(Facade(Centre - Half)), OnEdge(Facade(Centre + Half)), FLinearColor(1.0f, 0.45f, 0.1f), 7.0f);
	}
}

void UHutongEarPassageTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Level)
{
	FHutongEarPassageParams P = Settings ? Settings->Params : FHutongEarPassageParams();
	P.BayCountOverride = BayCountOverride;
	UHutongEarPassageBuildingComponent::BuildEarPassageMesh(P, BaySide, SizeX, SizeY, OutMesh, Level);
}

void UHutongEarPassageTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor || !Settings) return;
	UHutongEarPassageBuildingComponent* Building = NewObject<UHutongEarPassageBuildingComponent>(Actor, NAME_None, RF_Transactional);
	Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	Building->BayCountOverride = BayCountOverride;
	if (Appearance) Building->Palette = Appearance->Palette;
	StampDetail(Building);
	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	Building->ApplyPlacementAttachments();
}
