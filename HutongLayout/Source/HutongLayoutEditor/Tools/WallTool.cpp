#include "Tools/WallTool.h"
#include "Tools/HutongSnap.h"
#include "Generation/HutongUrban.h"
#include "SceneManagement.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "InteractiveToolManager.h"
#include "Engine/StaticMeshActor.h"
#include "Generation/HutongActorSpawn.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"

using UE::Geometry::FDynamicMesh3;

// Snap-bypass key name; defined in the base's file.
FText SnapKeyName();

UHutongLaneWallToolProperties::UHutongLaneWallToolProperties()
{
	Params.Role = EHutongWallRole::Perimeter;
	Params.bHasWindows = false;
	Params.Doorway = EHutongWallDoorway::None;
}

UHutongCourtWallToolProperties::UHutongCourtWallToolProperties()
{
	Params.Role = EHutongWallRole::Courtyard;
	Params.bHasWindows = true;
	// A doorway cuts the mesh and its collision, so it would read as a placed gate.
	Params.Doorway = EHutongWallDoorway::None;
	Params.bHasGate = false;
}

UHutongImperialWallToolProperties::UHutongImperialWallToolProperties()
{
	Params.Role = EHutongWallRole::ImperialCity;
	Params.bHasWindows = false;
	Params.Doorway = EHutongWallDoorway::None;
	Params.bHasGate = false;
}

UInteractiveTool* UHutongImperialWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongImperialWallTool>(SceneState.ToolManager);
}

UInteractiveTool* UHutongLaneWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongLaneWallTool>(SceneState.ToolManager);
}

UInteractiveTool* UHutongCourtWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongCourtWallTool>(SceneState.ToolManager);
}

void UHutongWallTool::RegisterToolSettings()
{
	Settings = NewWallSettings();

	// Role belongs to the tool: a loaded 隔牆 preset becomes this tool's wall.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Wall"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongWallToolProperties, Params));
	Presets->OnPresetLoaded = [this]()
	{
		if (Settings) Settings->Params.Role = GetWallRole();
		NotifyOfPropertyChangeByTool(Settings);
	};
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);

	// After RegisterSettings, which restores last session's values for this class.
	Settings->Params.Role = GetWallRole();
}

void UHutongWallTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongWallParams& P = Settings->Params;

	if (P.bDeriveFromRole)
	{
		P.Height = P.GetHeight();
		P.bDeriveFromRole = false;
	}
	P.Height = FMath::Clamp(P.Height + DeltaCm, FMath::Max(10.0, P.GetMinHeight()), 5000.0);
}

double UHutongWallTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetHeight() : 0.0;
}

double UHutongWallTool::GetLaneFaceOffset() const
{
	return 0.5 * (Settings ? Settings->Params.GetThickness() : 30.0);
}

void UHutongWallTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const double T = Settings ? Settings->Params.GetThickness() : 30.0;
	const double DrawnY = GetDrawnY();
	OutMinX = 0.0;
	OutMaxX = bIsDragging ? FVector::Dist2D(StartWorld, CurrentWorld) : 0.0;
	OutMinY = -DrawnY;
	OutMaxY = T - DrawnY;
}

double UHutongWallTool::GetDrawnY() const
{
	double DrawnY;
	HutongWallChain::FEndFace Start, End;
	EndsOnNeighbours(bIsDragging, DrawnY, Start, End);
	return DrawnY;
}

void UHutongWallTool::EndsOnNeighbours(bool bWithCursor, double& OutDrawnY, HutongWallChain::FEndFace& OutStart,
	HutongWallChain::FEndFace& OutEnd) const
{
	const double T = Settings ? Settings->Params.GetThickness() : 30.0;
	OutDrawnY = 0.5 * T;
	OutStart = OutEnd = HutongWallChain::FEndFace();
	if (ChainPoints.Num() == 0) return;

	TArray<FVector2D> Points;
	TArray<double> Yaw = ChainSnapYawDeg, Yaw2 = ChainSnapYaw2Deg;
	TArray<FVector2D> Inward = ChainSnapInward;
	for (const FVector& P : ChainPoints) Points.Add(FVector2D(P.X, P.Y));
	// A cursor within 10 cm of the vertex just added adds no segment, matching Build.
	if (bWithCursor && FVector::Dist2D(CurrentWorld, ChainPoints.Last()) >= 10.0)
	{
		Points.Add(FVector2D(CurrentWorld.X, CurrentWorld.Y));
		Yaw.Add(CursorSnapYawDeg);
		Yaw2.Add(CursorSnapYaw2Deg);
		Inward.Add(CursorSnapInward);
	}
	const int32 Last = Points.Num() - 1;
	if (Last < 1 || Yaw.Num() <= Last) return;

	auto EdgeFace = [](double YawDeg, const FVector2D& At)
	{
		HutongWallChain::FEndFace Face;
		Face.bSet = true;
		Face.Point = At;
		const double R = FMath::DegreesToRadians(YawDeg);
		Face.Dir = FVector2D(FMath::Cos(R), FMath::Sin(R));
		return Face;
	};
	double EndY = OutDrawnY;
	const bool bStartCorner = Yaw[0] > -900.0 && Yaw2[0] > -900.0;
	const bool bEndCorner = Yaw[Last] > -900.0 && Yaw2[Last] > -900.0;
	if (bStartCorner) HutongWallChain::CornerEnd(Points[1] - Points[0], true, Yaw[0], Yaw2[0], Inward[0], Points[0], T, OutDrawnY, OutStart);
	else if (Yaw[0] > -900.0) OutStart = EdgeFace(Yaw[0], Points[0]);
	if (bEndCorner) HutongWallChain::CornerEnd(Points[Last] - Points[Last - 1], false, Yaw[Last], Yaw2[Last], Inward[Last], Points[Last], T, EndY, OutEnd);
	else if (Yaw[Last] > -900.0) OutEnd = EdgeFace(Yaw[Last], Points[Last]);
	if (!bStartCorner && bEndCorner) OutDrawnY = EndY;
}

double UHutongWallTool::CurrentSegmentYawDeg() const
{
	const double dx = CurrentWorld.X - StartWorld.X, dy = CurrentWorld.Y - StartWorld.Y;
	if (dx * dx + dy * dy < 1.0) return PlacementYawDeg;
	return FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
}

bool UHutongWallTool::BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const
{
	OutSegments.Reset();
	if (!Settings || ChainPoints.Num() == 0) return false;
	TArray<FVector2D> Points;
	for (const FVector& P : ChainPoints) Points.Add(FVector2D(P.X, P.Y));
	if (bWithCursor) Points.Add(FVector2D(CurrentWorld.X, CurrentWorld.Y));
	double DrawnY;
	HutongWallChain::FEndFace Start, End;
	EndsOnNeighbours(bWithCursor, DrawnY, Start, End);
	return HutongWallChain::Build(Points, Settings->Params.GetThickness(), DrawnY, Start, End, OutSegments);
}

void UHutongWallTool::OnPlacementStarted(const FVector& HitWorld)
{
	ChainPoints = { StartWorld };
	ChainSnapYawDeg = { AnchorSnapYawDeg };
	ChainSnapYaw2Deg = { AnchorSnapYaw2Deg };
	ChainSnapInward = { AnchorSnapInward };
	ChainGateCounts.Reset();
	ChainEndBuilding.Reset();
	CursorGateCount = 0;
	bOpeningMarked = false;
}

bool UHutongWallTool::OnRectCommitted(const FVector& HitWorld)
{
	// Multi-click: undo the base's bRectCommitted so every click lands here; a zero-length segment ends the run.
	bRectCommitted = false;
	if (!IsOnRunEnd(HitWorld)) StepCursorBearing(PlacementYawDeg);
	const FVector P = CurrentWorld;
	// Also test the raw cursor: an abutting end pulls the snapped cursor along the neighbour's face, missing the close click.
	if (IsOnRunEnd(HitWorld) || FVector::Dist2D(P, ChainPoints.Last()) <= FMath::Max(10.0, 8.0 * WorldPerPixelAt(P)))
	{
		return ChainPoints.Num() >= 2;
	}
	ChainPoints.Add(P);
	ChainSnapYawDeg.Add(CursorSnapYawDeg);
	ChainSnapYaw2Deg.Add(CursorSnapYaw2Deg);
	ChainSnapInward.Add(CursorSnapInward);
	ChainGateCounts.Add(CursorGateCount);
	CursorGateCount = 0;
	bOpeningMarked = false;
	ChainEndBuilding = CursorSnapBuilding;
	if (IsOnOutline(P, CursorSnapBuilding.Get())) return true;
	StartWorld = P;
	return false;
}

bool UHutongWallTool::IsOnRunEnd(const FVector& RawCursor) const
{
	if (ChainPoints.Num() < 2) return false;
	const FVector& End = ChainPoints.Last();
	// A few pixels, so a short return leg can still start.
	return FVector::Dist2D(RawCursor, End) <= FMath::Max(10.0, 8.0 * WorldPerPixelAt(End));
}

void UHutongWallTool::OnPlacementHover(const FVector& HitWorld)
{
	if (!bRotateModeActive && IsOnRunEnd(HitWorld))
	{
		// Replace the base's neighbour snap with the end's bearing and marker, so preview and placed end cut match.
		CurrentWorld = ChainPoints.Last();
		bSnapActive = true;
		SnapPoint = CurrentWorld;
		bStickyCursorValid = false;
		CursorSnapYawDeg = ChainSnapYawDeg.Last();
		CursorSnapYaw2Deg = ChainSnapYaw2Deg.Last();
		CursorSnapInward = ChainSnapInward.Last();
		CursorSnapBuilding = ChainEndBuilding;
		return;
	}
	StepCursorBearing(PlacementYawDeg);
}

bool UHutongWallTool::CanFinishPlacement() const
{
	return ChainPoints.Num() >= 2;
}

void UHutongWallTool::CancelPlacement()
{
	// Reset the chain before the base's cancel, which refreshes the readout from it.
	ChainPoints.Reset();
	ChainSnapYawDeg.Reset();
	ChainSnapYaw2Deg.Reset();
	ChainSnapInward.Reset();
	ChainGateCounts.Reset();
	ChainEndBuilding.Reset();
	CursorGateCount = 0;
	bOpeningMarked = false;
	Super::CancelPlacement();
}

FText UHutongWallTool::GetStagePromptText() const
{
	if (!bIsDragging || bRotateModeActive) return Super::GetStagePromptText();
	return FText::Format(NSLOCTEXT("WallTool", "PromptSegment",
		"Click to end this segment; click the last end again, a building's outline, or press Enter to finish. G adds a gate (again for more), Shift snaps to 15°, - and = change height, Esc drops the run.{0}"),
		SnapKeyClause());
}

TArray<FText> UHutongWallTool::GetStageNames() const
{
	return { NSLOCTEXT("WallTool", "StageAnchor", "Anchor"), NSLOCTEXT("WallTool", "StageSegments", "Segments") };
}

void UHutongWallTool::SpawnFinalActor()
{
	// Only committed legs are built; clearing the cursor leg's G marks keeps their gates.
	CursorGateCount = 0;
	bOpeningMarked = false;
	TArray<HutongWallChain::FSegment> Segments;
	const bool bBuilt = BuildChain(false, Segments);
	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!bBuilt || !World || !Settings)
	{
		ChainPoints.Reset(); ChainSnapYawDeg.Reset(); ChainEndBuilding.Reset();
		return;
	}

	FScopedSlowTask Task((float)Segments.Num(), NSLOCTEXT("WallTool", "PlacingRun", "Building the wall run…"));
	Task.MakeDialogDelayed(0.4f);

	const double Z = ChainPoints[0].Z;

	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(NSLOCTEXT("WallTool", "PlaceRun", "Place Hutong Wall Run"));
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const HutongWallChain::FSegment& S = Segments[i];
		Task.EnterProgressFrame(1.0f);
		const FTransform Xform(FRotator(0.0, S.YawDeg, 0.0), FVector(S.Origin.X, S.Origin.Y, Z));
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase());
		if (!Actor) continue;

		UHutongWallBuildingComponent* Building = NewObject<UHutongWallBuildingComponent>(Actor, NAME_None, RF_Transactional);
		Building->Params = SegmentParams(i, Segments.Num(), false);
		Building->Length = S.Length;
		Building->bLengthAlongY = false;
		Building->FootprintThickness = 0.0;
		Building->FootprintSkew = S.Skew;
		Building->StartExtend = 0.0;
		Building->EndExtend = 0.0;
		if (Appearance) Building->Palette = Appearance->Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		Building->Rebuild();
		Building->ApplyPlacementAttachments();

		if (i == 0) AdoptBaseCourseFrom(Actor, AnchorSnapBuilding.Get());
		if (i == Segments.Num() - 1) AdoptBaseCourseFrom(Actor, ChainEndBuilding.Get());
	}
	ToolManager->EndUndoTransaction();

	ChainPoints.Reset();
	ChainSnapYawDeg.Reset();
	ChainSnapYaw2Deg.Reset();
	ChainSnapInward.Reset();
	ChainGateCounts.Reset();
	ChainEndBuilding.Reset();
}

FHutongWallParams UHutongWallTool::SegmentParams(int32 Index, int32 NumSegments, bool bCursorLeg) const
{
	FHutongWallParams P = Settings ? Settings->Params : FHutongWallParams();
	const int32 Marks = bCursorLeg ? CursorGateCount : (ChainGateCounts.IsValidIndex(Index) ? ChainGateCounts[Index] : 0);
	const bool bMarked = Marks > 0;
	bool bAnyMarked = CursorGateCount > 0 && bIsDragging;
	for (int32 i = 0; i < NumSegments && i < ChainGateCounts.Num(); ++i) bAnyMarked = bAnyMarked || ChainGateCounts[i] > 0;

	bool bCarries = bMarked;
	if (!bAnyMarked && (P.bHasGate || P.Doorway != EHutongWallDoorway::None))
	{
		int32 Longest = 0;
		double Best = -1.0;
		for (int32 i = 0; i < NumSegments; ++i)
		{
			const FVector From = ChainPoints.IsValidIndex(i) ? ChainPoints[i] : CurrentWorld;
			const FVector To = ChainPoints.IsValidIndex(i + 1) ? ChainPoints[i + 1] : CurrentWorld;
			const double L = FVector::Dist2D(From, To);
			if (L > Best) { Best = L; Longest = i; }
		}
		bCarries = (Index == Longest);
	}
	if (!bCarries)
	{
		P.bHasGate = false;
		P.Doorway = EHutongWallDoorway::None;
	}
	else if (bMarked && !P.bHasGate && P.Doorway == EHutongWallDoorway::None)
	{
		P.bHasGate = true;
	}
	if (bCarries && P.bHasGate && Marks >= 2)
	{
		const double Offset = P.GatePosition - 0.5;
		P.MoreGatePositions.Reset();
		for (int32 i = 0; i < Marks; ++i)
		{
			const double At = FMath::Clamp((i + 1.0) / (Marks + 1.0) + Offset, 0.0, 1.0);
			if (i == 0) P.GatePosition = At;
			else P.MoreGatePositions.Add(At);
		}
	}
	return P;
}

void UHutongWallTool::ToggleOpeningMark()
{
	CursorGateCount = (CursorGateCount + 1) % (MaxGatesPerLeg + 1);
	bOpeningMarked = CursorGateCount > 0;
}

void UHutongWallTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	if (!Settings) return;
	FHutongWallParams& P = Settings->Params;

	// Fraction of the run, so the step feels the same on any length.
	const double Step = bFine ? 0.01 : (bCoarse ? 0.10 : 0.03);
	if (P.bHasGate)
	{
		P.GatePosition = FMath::Clamp(P.GatePosition + Delta * Step, 0.0, 1.0);
		for (double& Position : P.MoreGatePositions) Position = FMath::Clamp(Position + Delta * Step, 0.0, 1.0);
	}
	else if (P.Doorway != EHutongWallDoorway::None)
	{
		P.DoorwayPosition = FMath::Clamp(P.DoorwayPosition + Delta * Step, 0.0, 1.0);
	}
}

FString UHutongWallTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongWallParams& P = Settings->Params;

	TArray<FString> Parts;
	if (P.bHasGate)
	{
		Parts.Add(P.MoreGatePositions.IsEmpty() ? FString::Printf(TEXT("gate at %.0f%%"), 100.0 * P.GatePosition)
			: FString::Printf(TEXT("%d gates"), 1 + P.MoreGatePositions.Num()));
	}
	else if (P.Doorway != EHutongWallDoorway::None)
	{
		Parts.Add(FString::Printf(TEXT("doorway at %.0f%%"), 100.0 * P.DoorwayPosition));
	}
	if (bIsDragging && CursorGateCount > 0)
	{
		Parts.Add(FString::Printf(TEXT("%d gate%s on this segment"), CursorGateCount, CursorGateCount > 1 ? TEXT("s") : TEXT("")));
	}

	if (bIsDragging && WantsLaneWidthSnap())
	{
		const double Half = GetLaneFaceOffset();
		const FVector Mid = 0.5 * (StartWorld + CurrentWorld);
		// Beyond twice the hutong pitch it is a different street.
		const HutongSnap::FGap Gap = HutongSnap::FindParallelGap(
			GetFootprints(), Mid, CurrentSegmentYawDeg(), 2.0 * HutongGen::Urban::PitchCm);
		if (Gap.bFound)
		{
			Parts.Add(FString::Printf(TEXT("lane %s"),
				*HutongGen::Urban::DescribeWidth(FMath::Max(Gap.DistanceCm - Half, 0.0))));
		}
	}

	return FString::Join(Parts, TEXT(" · "));
}

FText UHutongWallTool::GetKeyHintText() const
{
	const FHutongWallParams* P = Settings ? &Settings->Params : nullptr;
	const bool bGate = P && P->bHasGate;
	const FText What = bGate
		? NSLOCTEXT("WallTool", "KeyHintGate", "[ ] gate position")
		: NSLOCTEXT("WallTool", "KeyHintDoorway", "[ ] doorway position");
	return FText::Format(NSLOCTEXT("WallTool", "KeyHint", "{0} · G gates on this segment · Enter finish · Shift 15° · - = height · {1} {2} · Esc cancel · Ctrl+Z undo"),
		What, SnapKeyName(),
		SnappingActive() ? NSLOCTEXT("WallTool", "KeyHintNoSnap", "no snap") : NSLOCTEXT("WallTool", "KeyHintSnap", "snap"));
}

TArray<FText> UHutongWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongWallTool", "HelpRun",
		"Click to start the wall, click to end each segment, click the last end again, a building's outline, or press Enter to finish. G puts a gate on the current segment; press again for another, up to four, then none.");
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpOpening",
		"[ and ] slide the gates or doorway along the wall (Shift nudges, Ctrl jumps)."), 1);
	return Lines;
}

TArray<FText> UHutongLaneWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpLane",
		"A boundary wall (院牆) onto the lane: tall and blank. Its only opening is a gate (牆垣式門)."), 1);
	return Lines;
}

TArray<FText> UHutongCourtWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpCourt",
		"A dividing wall (隔牆) inside the compound: lower, and can take doorways and decorative windows (什錦窗)."), 1);
	return Lines;
}

TArray<FText> UHutongImperialWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpImperial",
		"The imperial city wall (皇城牆): red plaster, battered, under a yellow glazed tile cap."), 1);
	return Lines;
}

void UHutongWallTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (!bIsDragging)
	{
		Super::Render(RenderAPI);
		return;
	}
	if (RenderAPI == nullptr || Settings == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	TArray<HutongWallChain::FSegment> Segments;
	if (!BuildChain(true, Segments)) return;
	const double T = Settings->Params.GetThickness();
	const double Z = ChainPoints[0].Z;
	const FLinearColor Color(1.0f, 0.9f, 0.15f, 1.0f);
	const FLinearColor HeightColor(0.45f, 0.8f, 1.0f, 1.0f);
	const double PreviewHeight = GetPreviewHeight();

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		FVector2D C[4];
		HutongWallChain::Corners(Segments[i], T, C);
		FVector W[4];
		for (int32 k = 0; k < 4; ++k) W[k] = FVector(C[k].X, C[k].Y, Z);
		for (int32 k = 0; k < 4; ++k) DrawPreviewLine(PDI, W[k], W[(k + 1) % 4], Color, 5.0f);
		if (i == Segments.Num() - 1 && PreviewHeight > 0.0)
		{
			const FVector Up(0.0, 0.0, PreviewHeight);
			for (int32 k = 0; k < 4; ++k)
			{
				DrawDashedPreviewLine(PDI, W[k], W[k] + Up, HeightColor, 2.5f);
				DrawDashedPreviewLine(PDI, W[k] + Up, W[(k + 1) % 4] + Up, HeightColor, 3.0f);
			}
		}
	}

	if (bSnapActive)
	{
		const FLinearColor SnapColor(0.25f, 1.0f, 0.45f, 1.0f);
		const double Arm = 26.0;
		DrawPreviewLine(PDI, SnapPoint - FVector(Arm, 0, 0), SnapPoint + FVector(Arm, 0, 0), SnapColor, 4.0f);
		DrawPreviewLine(PDI, SnapPoint - FVector(0, Arm, 0), SnapPoint + FVector(0, Arm, 0), SnapColor, 4.0f);
		DrawPreviewLine(PDI, SnapPoint, SnapPoint + FVector(0, 0, 1.6 * Arm), SnapColor, 3.0f);
	}

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const HutongWallChain::FSegment& Seg = Segments[i];
		const bool bCursorLeg = (i == Segments.Num() - 1);
		const TArray<FVector> Openings = GetPreviewOpenings(SegmentParams(i, Segments.Num(), bCursorLeg), Seg.Length);
		if (Openings.IsEmpty()) continue;

		const double Yaw = FMath::DegreesToRadians(Seg.YawDeg);
		const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
		const FVector L(-D.Y, D.X, 0.0);
		const FVector O(Seg.Origin.X, Seg.Origin.Y, Z);
		auto At = [&](double A, double Cross, double Up) { return O + D * A + L * Cross + FVector(0.0, 0.0, Up); };

		const FLinearColor Gate(1.0f, 0.45f, 0.1f);
		for (const FVector& Opening : Openings)
		{
			const double Along = Opening.X, Head = Opening.Z;
			const double HalfW = 0.5 * Opening.Y;
			const double Edges[2] = { Along - HalfW, Along + HalfW };
			for (double E : Edges)
			{
				DrawPreviewLine(PDI, At(E, 0.0, 0.0), At(E, 0.0, Head), Gate, 5.0f);
				DrawPreviewLine(PDI, At(E, T, 0.0), At(E, T, Head), Gate, 5.0f);
				DrawPreviewLine(PDI, At(E, 0.0, Head), At(E, T, Head), Gate, 5.0f);
			}
			DrawPreviewLine(PDI, At(Edges[0], 0.0, Head), At(Edges[1], 0.0, Head), Gate, 5.0f);
			DrawPreviewLine(PDI, At(Edges[0], T, Head), At(Edges[1], T, Head), Gate, 5.0f);
		}
	}
}

// Mirrors BuildWall's clamp (centre inward by half the opening plus margin), so the bracket matches the built opening.
TArray<FVector> UHutongWallTool::GetPreviewOpenings(const FHutongWallParams& P, double Run) const
{
	TArray<FVector> Out;
	if (Run < 1.0) return Out;
	const double H = FMath::Max(P.GetHeight(), 30.0);

	if (P.bHasGate)
	{
		const double Head = FMath::Clamp(P.GetGateHeadHeight(), 80.0, H - 10.0);
		for (const FHutongWallGateSpan& G : P.GetGateSpans(Run)) Out.Emplace(G.Centre, G.ClearX1 - G.ClearX0, Head);
		return Out;
	}
	if (P.Doorway == EHutongWallDoorway::None || !P.HasDecorativeDoorway(Run)) return Out;
	Out.Emplace(P.GetDoorwayCentre(Run), P.GetDoorwayWidth(), FMath::Min(P.GetDoorwayHeight(), H));
	return Out;
}
