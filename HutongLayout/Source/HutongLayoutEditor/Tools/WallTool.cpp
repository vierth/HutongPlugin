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

// The name of the key that ignores snapping, from the base's file.
FText SnapKeyName();

UHutongLaneWallToolProperties::UHutongLaneWallToolProperties()
{
	Params.Role = EHutongWallRole::Perimeter;
	// A wall whose job is that the household is not seen carries neither; the generator refuses
	// them under this role anyway, so offering them switched on would be a checkbox that lies.
	Params.bHasWindows = false;
	Params.Doorway = EHutongWallDoorway::None;
}

UHutongCourtWallToolProperties::UHutongCourtWallToolProperties()
{
	Params.Role = EHutongWallRole::Courtyard;
	Params.bHasWindows = true;
	// No opening until one is asked for. A doorway is a hole through a wall and the mesh is its own
	// collision, so a run that comes out cut when nothing was ticked reads as the tool placing a
	// gate of its own — which is what it looked like. 隨牆門 is one entry down the dropdown.
	Params.Doorway = EHutongWallDoorway::None;
	Params.bHasGate = false;
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

	// One preset key for both walls, so a run saved under either is offered by both. The role is
	// the tool's, not the preset's: a 隔牆 preset loaded here comes back as this tool's kind of wall.
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

	// After RegisterSettings, which restores whatever the last session left on this set class.
	Settings->Params.Role = GetWallRole();
}

void UHutongWallTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongWallParams& P = Settings->Params;

	// The keys turn the role's derivation off.
	if (P.bDeriveFromRole)
	{
		P.Height = P.GetHeight();
		P.bDeriveFromRole = false;
	}
	// A wall with a gate in it cannot go below the height that gate's opening needs.
	P.Height = FMath::Clamp(P.Height + DeltaCm, FMath::Max(10.0, P.GetMinHeight()), 5000.0);
}

double UHutongWallTool::GetPreviewHeight() const
{
	// Body height only: that is what the keys move, and the cap sits on top of it.
	return Settings ? Settings->Params.GetHeight() : 0.0;
}

double UHutongWallTool::GetLaneFaceOffset() const
{
	return 0.5 * (Settings ? Settings->Params.GetThickness() : 30.0);
}

void UHutongWallTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// The segment being drawn, in its own frame: length along X, the thickness across it where
	// the drawn line puts it. The readout reads sizes off this; nothing else does.
	const double T = Settings ? Settings->Params.GetThickness() : 30.0;
	const double DrawnY = GetDrawnY();
	OutMinX = 0.0;
	OutMaxX = bIsDragging ? FVector::Dist2D(StartWorld, CurrentWorld) : 0.0;
	OutMinY = -DrawnY;
	OutMaxY = T - DrawnY;
}

double UHutongWallTool::GetDrawnY() const
{
	const double T = Settings ? Settings->Params.GetThickness() : 30.0;
	if (ChainPoints.Num() == 0) return 0.5 * T;
	// Snapping off — the key tapped — is free movement, the line centred, whatever the anchor
	// snapped to when it was placed.
	if (!SnappingActive()) return 0.5 * T;
	// The drawn line is the run's outer face when any vertex of it snapped to a face that runs
	// along the segment there — a run starting on a house's front wall continues that wall
	// flush, and so does one ending on it. The first such vertex decides for the whole run, the
	// anchor first, the cursor's end last.
	TArray<FVector> Points = ChainPoints;
	TArray<double> Yaw = ChainSnapYawDeg, Yaw2 = ChainSnapYaw2Deg;
	TArray<FVector2D> Inward = ChainSnapInward;
	if (bIsDragging)
	{
		Points.Add(CurrentWorld);
		Yaw.Add(CursorSnapYawDeg);
		Yaw2.Add(CursorSnapYaw2Deg);
		Inward.Add(CursorSnapInward);
	}
	const int32 Last = Points.Num() - 1;
	// The anchor alone is no segment yet, and asking which way it runs indexed before it.
	if (Last < 1) return 0.5 * T;
	for (int32 k = 0; k <= Last && k < Yaw.Num(); ++k)
	{
		if (Inward[k].IsNearlyZero()) continue;
		const FVector& A = Points[k < Last ? k : k - 1];
		const FVector& B = Points[k < Last ? k + 1 : k];
		const FVector2D D = FVector2D(B.X - A.X, B.Y - A.Y).GetSafeNormal();
		if (D.IsNearlyZero()) continue;
		double DrawnY = 0.5 * T;
		// The cursor's segment is still moving — at either of its ends, the anchor's included on
		// a run of one — so once it has decided it keeps deciding until the segment is well off
		// the face, or the run would jump sideways with every pixel.
		const bool bCursor = bIsDragging && k >= Last - 1;
		const double Tolerance = (bCursor && bCursorSideOn) ? HutongWallChain::AlongFaceDegAfter : HutongWallChain::AlongFaceDeg;
		const bool bAlong = HutongWallChain::SideAlongFace(D, Yaw[k], Yaw2[k], Inward[k], T, DrawnY, Tolerance,
			Settings->Params.GetAbuttingSetback());
		if (bCursor) bCursorSideOn = bAlong;
		if (bAlong) return DrawnY;
	}
	return 0.5 * T;
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

	auto FaceFrom = [](double YawDeg, const FVector2D& At)
	{
		HutongWallChain::FEndFace Face;
		if (YawDeg > -900.0)
		{
			Face.bSet = true;
			Face.Point = At;
			const double R = FMath::DegreesToRadians(YawDeg);
			Face.Dir = FVector2D(FMath::Cos(R), FMath::Sin(R));
		}
		return Face;
	};
	const HutongWallChain::FEndFace Start = FaceFrom(AnchorSnapYawDeg, Points[0]);
	const HutongWallChain::FEndFace End = bWithCursor
		? FaceFrom(CursorSnapYawDeg, Points.Last())
		: FaceFrom(ChainSnapYawDeg.Num() > 1 ? ChainSnapYawDeg.Last() : -1000.0, Points.Last());
	return HutongWallChain::Build(Points, Settings->Params.GetThickness(), GetDrawnY(), Start, End, OutSegments);
}

void UHutongWallTool::OnPlacementStarted(const FVector& HitWorld)
{
	bCursorSideOn = false;
	bAxisSnapOn = false;
	ChainPoints = { StartWorld };
	ChainSnapYawDeg = { AnchorSnapYawDeg };
	ChainSnapYaw2Deg = { AnchorSnapYaw2Deg };
	ChainSnapInward = { AnchorSnapInward };
	ChainGateFlags.Reset();
	ChainEndBuilding.Reset();
}

bool UHutongWallTool::OnRectCommitted(const FVector& HitWorld)
{
	// The base has set bRectCommitted for a two-click placement; this one takes as many clicks
	// as there are segments, so it is put back and every click lands here again until the run is
	// finished — which is a click on the end just placed: a segment of no length is the sign to stop.
	bRectCommitted = false;
	const FVector P = CurrentWorld;
	// Judged on the raw cursor as well as the snapped one: an end that abuts a neighbour has
	// the cursor pulled onto that neighbour's face, a few centimetres along it from the end,
	// and a click meant to close the run then missed the end it was over.
	if (IsOnRunEnd(HitWorld) || FVector::Dist2D(P, ChainPoints.Last()) <= FMath::Max(10.0, 8.0 * WorldPerPixelAt(P)))
	{
		// On the anchor alone there is nothing to build; the run waits for a segment.
		return ChainPoints.Num() >= 2;
	}
	ChainPoints.Add(P);
	ChainSnapYawDeg.Add(CursorSnapYawDeg);
	ChainSnapYaw2Deg.Add(CursorSnapYaw2Deg);
	ChainSnapInward.Add(CursorSnapInward);
	ChainGateFlags.Add(bOpeningKeyHeld);
	ChainEndBuilding = CursorSnapBuilding;
	// The next segment is drawn from here: the frame the base rotates and measures in moves with it.
	StartWorld = P;
	return false;
}

bool UHutongWallTool::IsOnRunEnd(const FVector& RawCursor) const
{
	if (ChainPoints.Num() < 2) return false;
	const FVector& End = ChainPoints.Last();
	// A few pixels, not the snap radius: within that a short return leg could not be started.
	return FVector::Dist2D(RawCursor, End) <= FMath::Max(10.0, 8.0 * WorldPerPixelAt(End));
}

void UHutongWallTool::OnPlacementHover(const FVector& HitWorld)
{
	// Over the run's own end the cursor sits on it, whatever neighbour would have taken it:
	// the closing click is the one gesture that must not be snapped away from.
	if (!bRotateModeActive && IsOnRunEnd(HitWorld))
	{
		// The base snapped first, to whatever neighbour was nearer; the end's own bearing and
		// marker replace that, so the preview's end cut is the one the placed run keeps.
		CurrentWorld = ChainPoints.Last();
		bSnapActive = true;
		SnapPoint = CurrentWorld;
		bStickyCursorValid = false;
		CursorSnapYawDeg = ChainSnapYawDeg.Last();
		CursorSnapYaw2Deg = ChainSnapYaw2Deg.Last();
		CursorSnapInward = ChainSnapInward.Last();
		CursorSnapBuilding = ChainEndBuilding;
		bAxisSnapOn = false;
		return;
	}
	// A segment goes where the cursor points, pulled onto the frame's axes when it is nearly on
	// one, and onto 15° steps under Shift. A point snap has already placed the end and is left.
	if (bRotateModeActive || bSnapActive) { bAxisSnapOn = false; return; }
	const double dx = CurrentWorld.X - StartWorld.X, dy = CurrentWorld.Y - StartWorld.Y;
	const double Len = FMath::Sqrt(dx * dx + dy * dy);
	if (Len < 1.0) return;
	const double Angle = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
	double Snapped = Angle;
	const bool bShift = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	if (bShift)
	{
		Snapped = PlacementYawDeg + FMath::RoundToDouble((Angle - PlacementYawDeg) / 15.0) * 15.0;
	}
	else
	{
		double Rel = Angle - PlacementYawDeg;
		Rel -= 90.0 * FMath::RoundToDouble(Rel / 90.0);
		// Once on an axis it stays until the pull is well off it, or the end hops on and off with every pixel.
		const double Tolerance = bAxisSnapOn ? 10.0 : 6.0;
		bAxisSnapOn = FMath::Abs(Rel) <= Tolerance;
		if (!bAxisSnapOn) return;
		Snapped = Angle - Rel;
	}
	const double R = FMath::DegreesToRadians(Snapped);
	CurrentWorld = StartWorld + FVector(FMath::Cos(R) * Len, FMath::Sin(R) * Len, 0.0);
}

void UHutongWallTool::CancelPlacement()
{
	// The chain goes before the base's cancel: that refreshes the readout, which reads the chain.
	ChainPoints.Reset();
	ChainSnapYawDeg.Reset();
	ChainSnapYaw2Deg.Reset();
	ChainSnapInward.Reset();
	ChainGateFlags.Reset();
	ChainEndBuilding.Reset();
	Super::CancelPlacement();
}

FText UHutongWallTool::GetStagePromptText() const
{
	if (!bIsDragging || bRotateModeActive) return Super::GetStagePromptText();
	return FText::Format(NSLOCTEXT("WallTool", "PromptSegment",
		"Click to end this segment and start the next, at any angle; click the last end again to finish the run. Hold G on the click for a gate on the segment it closes. Shift snaps the angle to 15°, - and = change height, Esc drops the run.{0}"),
		SnapKeyClause());
}

TArray<FText> UHutongWallTool::GetStageNames() const
{
	return { NSLOCTEXT("WallTool", "StageAnchor", "Anchor"), NSLOCTEXT("WallTool", "StageSegments", "Segments") };
}

void UHutongWallTool::SpawnFinalActor()
{
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
		// The joins and the flush ends are corner offsets on the footprint; the square miter
		// extension is the other way of filling a corner and the two are not stacked.
		Building->FootprintSkew = S.Skew;
		Building->StartExtend = 0.0;
		Building->EndExtend = 0.0;
		if (Appearance) Building->Palette = Appearance->Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		// Through the same seam a rebuild takes: the plan outline or the baked mesh, warped to the corners.
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
	ChainGateFlags.Reset();
	ChainEndBuilding.Reset();
}

FHutongWallParams UHutongWallTool::SegmentParams(int32 Index, int32 NumSegments, bool bCursorLeg) const
{
	FHutongWallParams P = Settings ? Settings->Params : FHutongWallParams();
	const bool bMarked = bCursorLeg ? bOpeningKeyHeld : (ChainGateFlags.IsValidIndex(Index) && ChainGateFlags[Index]);
	bool bAnyMarked = bOpeningKeyHeld && bIsDragging;
	for (int32 i = 0; i < NumSegments && i < ChainGateFlags.Num(); ++i) bAnyMarked = bAnyMarked || ChainGateFlags[i];

	bool bCarries = bMarked;
	if (!bAnyMarked && (P.bHasGate || P.Doorway != EHutongWallDoorway::None))
	{
		// Nothing asked for: the run's one opening goes on its longest leg.
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
	return P;
}

void UHutongWallTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	if (!Settings) return;
	FHutongWallParams& P = Settings->Params;

	// A fraction of the run rather than a distance, so the step feels the same on a short wall and a long one.
	const double Step = bFine ? 0.01 : (bCoarse ? 0.10 : 0.03);
	// Whichever opening the run has: the gate on a lane wall, the garden doorway on a court one.
	if (P.bHasGate)
	{
		P.GatePosition = FMath::Clamp(P.GatePosition + Delta * Step, 0.0, 1.0);
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
		Parts.Add(FString::Printf(TEXT("gate at %.0f%%"), 100.0 * P.GatePosition));
	}
	else if (P.Doorway != EHutongWallDoorway::None)
	{
		Parts.Add(FString::Printf(TEXT("doorway at %.0f%%"), 100.0 * P.DoorwayPosition));
	}

	// The lane this run is forming with whatever it is being drawn opposite — the same question the
	// width snap asks, and one a wall inside a compound is not answering.
	if (bIsDragging && WantsLaneWidthSnap())
	{
		const double Half = GetLaneFaceOffset();
		const FVector Mid = 0.5 * (StartWorld + CurrentWorld);
		// Twice the hutong pitch is as far as a lane could sensibly be; past that, whatever was found is a different street.
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
	// Its own line, without the base's R: a run's segments go where the cursor points, and the frame is not worth a key.
	return FText::Format(NSLOCTEXT("WallTool", "KeyHint", "{0} · G+click gate on segment · Shift 15° · - = height · {1} {2} · Esc cancel · Ctrl+Z undo"),
		What, SnapKeyName(),
		SnappingActive() ? NSLOCTEXT("WallTool", "KeyHintNoSnap", "no snap") : NSLOCTEXT("WallTool", "KeyHintSnap", "snap"));
}

TArray<FText> UHutongWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpRun",
		"A wall is drawn as a run: click to anchor it, click again to end each segment and start the next at whatever angle you like, and click the last end once more to finish; hold G on a click and the segment it closes gets a gate. Every segment is its own piece; where two meet they are cut on the bisector and meld, and an end that rests on a neighbour's face is cut flush against it. Esc drops the whole run."), 1);
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpOpening",
		"[ and ] slide the opening the run carries — the gate (牆垣門) if it has one, otherwise the garden doorway — along the wall. Shift for a nudge, Ctrl to jump; the orange bracket previews where it lands."), 1);
	return Lines;
}

TArray<FText> UHutongLaneWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpLane",
		"A boundary wall (院牆) onto the lane: tall, thick and blank, since its job is that the household is not seen. Decorative windows (什錦窗) and garden doorways are refused on this run; the way through it is the gate (牆垣式門), under its own hood. While dragging, the readout names the street this run is forming, and the anchor snaps onto a canonical lane width."), 1);
	return Lines;
}

TArray<FText> UHutongCourtWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongWallTool", "HelpCourt",
		"A dividing wall (隔牆) inside the compound: lower and thinner, so an inner gate (垂花門) standing in it rises clear. This is the run that carries openings — a plain doorway (隨牆門) or a shaped one, dressed as a 牆垣式垂花門 if you want it, and decorative windows (什錦窗). It forms no street, so nothing here is measured against a lane."), 1);
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

	// Every segment as it will be built, joins and flush ends included; the height posts on the one being drawn.
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

	// The opening brackets, on every leg that carries one — G held shows it on the leg being drawn.
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const HutongWallChain::FSegment& Seg = Segments[i];
		const bool bCursorLeg = (i == Segments.Num() - 1);
		double Along = 0.0, Width = 0.0, Head = 0.0;
		bool bIsGate = false;
		if (!GetPreviewOpening(SegmentParams(i, Segments.Num(), bCursorLeg), Seg.Length, Along, Width, Head, bIsGate)) continue;

		const double Yaw = FMath::DegreesToRadians(Seg.YawDeg);
		const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
		const FVector L(-D.Y, D.X, 0.0);
		const FVector O(Seg.Origin.X, Seg.Origin.Y, Z);
		auto At = [&](double A, double Cross, double Up) { return O + D * A + L * Cross + FVector(0.0, 0.0, Up); };

		// Orange against the footprint's yellow and the height preview's blue.
		const FLinearColor Gate(1.0f, 0.45f, 0.1f);
		const double HalfW = 0.5 * Width;
		const double Edges[2] = { Along - HalfW, Along + HalfW };
		for (double E : Edges)
		{
			// Up both faces of the wall, so the opening reads from either side.
			DrawPreviewLine(PDI, At(E, 0.0, 0.0), At(E, 0.0, Head), Gate, 5.0f);
			DrawPreviewLine(PDI, At(E, T, 0.0), At(E, T, Head), Gate, 5.0f);
			DrawPreviewLine(PDI, At(E, 0.0, Head), At(E, T, Head), Gate, 5.0f);
		}
		DrawPreviewLine(PDI, At(Edges[0], 0.0, Head), At(Edges[1], 0.0, Head), Gate, 5.0f);
		DrawPreviewLine(PDI, At(Edges[0], T, Head), At(Edges[1], T, Head), Gate, 5.0f);
	}
}

// Worked out exactly as BuildWall works it out, or the bracket promises an opening the run does
// not get: the generator clamps the centre inward by half the opening plus its margin and then
// refuses it outright when the masonry either side has gone, which key repeat reaches by landing
// the position on 0 or 1. The heads are the accessors', not the raw fields, so the clearance floor
// lifts the preview with the built one.
bool UHutongWallTool::GetPreviewOpening(const FHutongWallParams& P, double Run, double& OutCentreAlong, double& OutWidth,
	double& OutHead, bool& bOutIsGate) const
{
	if (Run < 1.0) return false;
	const double H = FMath::Max(P.GetHeight(), 30.0);
	const double T = FMath::Max(P.GetThickness(), 2.0);

	if (P.bHasGate)
	{
		const double FrameT = FMath::Clamp(P.GateFrameThickness, 2.0, T);
		const double W = FMath::Clamp(P.GateWidth, 40.0, FMath::Max(Run - 4.0 * FrameT, 40.0));
		const double Along = FMath::Clamp(P.GatePosition * Run, 0.5 * W + FrameT, Run - 0.5 * W - FrameT);
		// The gate the generator would refuse is not previewed.
		if (Along - 0.5 * W - FrameT <= 0.0 || Along + 0.5 * W + FrameT >= Run) return false;

		OutCentreAlong = Along;
		OutWidth = W;
		OutHead = FMath::Clamp(P.GetGateHeadHeight(), 80.0, H - 10.0);
		bOutIsGate = true;
		return true;
	}

	if (P.Doorway == EHutongWallDoorway::None || !P.HasDecorativeDoorway(Run)) return false;

	OutCentreAlong = P.GetDoorwayCentre(Run);
	OutWidth = P.GetDoorwayWidth();
	OutHead = FMath::Min(P.GetDoorwayHeight(), H);
	bOutIsGate = false;
	return true;
}
