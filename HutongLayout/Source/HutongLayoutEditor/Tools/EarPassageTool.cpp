#include "Tools/EarPassageTool.h"
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
	// The same clamps the house tool applies, on the room inside.
	Params.Room.ClampAfterEdit(PropertyChangedEvent.GetPropertyName());
}

void UHutongEarPassageTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (!Settings || !SnappingActive()) return;
	const FHutongEarPassageParams& P = Settings->Params;
	if (!P.Room.bSnapToSuggested) return;

	// The suggested frontage is the room's plus the passage's strip; the depth the room's own.
	auto SnapSide = [&](double& Lo, double& Hi)
	{
		const double Want = P.Room.SnapExtent(FMath::Abs(Hi - Lo), P.GetStripWidth());
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};
	SnapSide(OutMinX, OutMaxX);
	SnapSide(OutMinY, OutMaxY);
}

void UHutongEarPassageTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongEarPassageToolProperties>(this);

	RegisterSettings(Settings);

	// Presets after the parameters they save, as on every tool but the house.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("EarPassage"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongEarPassageToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Presets);
}

FHutongEarPassageParams UHutongEarPassageTool::GetResolvedParams() const
{
	FHutongEarPassageParams P = Settings ? Settings->Params : FHutongEarPassageParams();
	if (bIsDragging)
	{
		double MinX, MinY, MaxX, MaxY;
		GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = FMath::Max(bAlongX ? MaxX - MinX : MaxY - MinY, 1.0);
		P.Depth = FMath::Max(bAlongX ? MaxY - MinY : MaxX - MinX, 1.0);
	}
	else if (P.Room.SuggestedFrontage > 0.0)
	{
		P.Width = P.Room.SuggestedFrontage + P.GetStripWidth();
		P.Depth = FMath::Max(P.Room.GetSuggestedDepth(), 1.0);
	}
	P.Width = P.GetWidth();
	return P;
}

FString UHutongEarPassageTool::GetPlacementDetail() const
{
	const FHutongEarPassageParams P = GetResolvedParams();
	const FHutongSiheyuanParams R = P.RoomParams();
	return FString::Printf(TEXT("room %d bays @ %.0f cm · passage (過道) %.0f cm clear at the %s end"),
		R.GetBayCount(), R.Width / FMath::Max(R.GetBayCount(), 1), P.GetClearWidth(),
		P.bPassageAtFarEnd ? TEXT("far") : TEXT("origin"));
}

FText UHutongEarPassageTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("EarPassageTool", "KeyHint", "[ ] passage end · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongEarPassageTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("EarPassageTool", "HelpFacade",
		"After the footprint, move to pick which side gets the room's facade and the passage's doorway, then click to place. [ and ] swap which end the passage is at."), 1);
	return Lines;
}

void UHutongEarPassageTool::AdjustBracketValue(int32 /*Delta*/, bool, bool)
{
	if (Settings) Settings->Params.bPassageAtFarEnd = !Settings->Params.bPassageAtFarEnd;
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
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("EarPassageTool", "PromptBaySide",
			"Move to pick the side that gets the facade and the passage's doorway (green ticks), [ and ] swap the passage's end, then click to place.");
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

	const FLinearColor TickColor(0.25f, 1.0f, 0.45f, 1.0f);
	const FLinearColor StripColor(0.60f, 0.85f, 0.45f, 1.0f);
	const double TickLen = FMath::Max(30.0, 0.04 * FMath::Max(SizeX, SizeY));
	const double TickInset = 0.4 * TickLen;
	const FRotator Rot(0.0, PlacementYawDeg, 0.0);

	const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double DepthMin = Edge.bAlongX ? MinY : MinX;
	const double DepthMax = Edge.bAlongX ? MaxY : MaxX;

	// A point at T along the frontage and U across it, in the drag's frame.
	auto At = [&](double T, double U) { return Edge.bAlongX ? LocalRectToWorld(SpanMin + T, U) : LocalRectToWorld(U, SpanMin + T); };
	auto OnEdge = [&](double T) { return At(T, Edge.FixedCoord); };
	auto DrawTick = [&](double T)
	{
		const FVector Base = OnEdge(T);
		const FVector OutDir = Rot.RotateVector(FVector(Edge.OutDir.X, Edge.OutDir.Y, 0.0));
		DrawPreviewLine(PDI, Base - OutDir * TickInset, Base + OutDir * TickLen, TickColor, 5.0f);
	};

	// Everything is worked out in the build frame, facade on -Y, and turned onto the facade
	// the way the built mesh is — the one RotateVertex, so the strip's end and the doorway land
	// where they are built on +Y and -X, which reverse the frontage.
	auto Facade = [&](double X)
	{
		const FVector3d V = HutongGen::BaySide::RotateVertex(BaySide, FVector3d(X, 0.0, 0.0), SizeX, SizeY);
		return Edge.bAlongX ? V.X : V.Y;
	};

	// The room's bay ticks along the facade edge, the strip's line among them.
	FHutongPlanBays Bays = UHutongEarPassageBuildingComponent::PlanBaysInBuildFrame(P);
	HutongGen::PlanBays::OntoFacade(Bays, BaySide, SizeX, SizeY);
	for (const double T : Bays.Boundaries) DrawTick(T);

	// The passage strip and the clear way dashed across the depth.
	const double ClearA = Facade(P.GetClearX0());
	const double ClearB = Facade(P.GetClearX0() + P.GetClearWidth());
	const double StripA = Facade(P.bPassageAtFarEnd ? P.GetWidth() - P.GetStripWidth() : 0.0);
	const double StripB = Facade(P.bPassageAtFarEnd ? P.GetWidth() : P.GetStripWidth());
	for (const double T : { StripA, StripB, ClearA, ClearB })
	{
		DrawDashedPreviewLine(PDI, At(T, DepthMin), At(T, DepthMax), StripColor, 2.5f, 28.0);
	}
	// The doorway through the closing wall, on the facade edge, as the wall will cut it.
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
	UHutongEarPassageBuildingComponent::BuildEarPassageMesh(
		Settings ? Settings->Params : FHutongEarPassageParams(), BaySide, SizeX, SizeY, OutMesh, Level);
}

void UHutongEarPassageTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor || !Settings) return;
	UHutongEarPassageBuildingComponent* Building = NewObject<UHutongEarPassageBuildingComponent>(Actor, NAME_None, RF_Transactional);
	Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;
	StampDetail(Building);
	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	Building->ApplyPlacementAttachments();
}
