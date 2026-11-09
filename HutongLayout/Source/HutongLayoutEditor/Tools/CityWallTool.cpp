#include "Tools/CityWallTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "InteractiveToolManager.h"
#include "Engine/StaticMeshActor.h"
#include "SceneManagement.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UObjectIterator.h"

using UE::Geometry::FDynamicMesh3;

// Snap-bypass key name; defined in the base's file.
FText SnapKeyName();

UInteractiveTool* UHutongCityWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongCityWallTool>(SceneState.ToolManager);
}

UHutongPalaceWallToolProperties::UHutongPalaceWallToolProperties()
{
	Params.Rank = EHutongCityWallRank::Palace;
	Params.bBastions = false;
	Params.bLoopholes = false;
}

UInteractiveTool* UHutongPalaceWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongPalaceWallTool>(SceneState.ToolManager);
}

void UHutongCityWallTool::RegisterToolSettings()
{
	Settings = NewCitySettings();

	auto Force = [this]()
	{
		const TOptional<EHutongCityWallRank> Rank = ForcedRank();
		if (Rank && Settings->Params.Rank != *Rank) Settings->Params.ChooseRank(*Rank);
		LastRank = Settings->Params.Rank;
	};
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("CityWall"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongCityWallToolProperties, Params));
	Presets->OnPresetLoaded = [this, Force]() { Force(); NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Settings);
	RegisterSettings(Presets);
	Force();
}

TArray<FText> UHutongPalaceWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongCityWallTool", "HelpPalace",
		"The Forbidden City's wall (紫禁城城牆): red on the side facing the palace, grey brick outside."), 1);
	return Lines;
}

void UHutongCityWallTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	Super::OnPropertyModified(PropertySet, Property);
	if (PropertySet != Settings || Settings->Params.Rank == LastRank) return;
	Settings->Params.ChooseRank(ForcedRank().Get(Settings->Params.Rank));
	LastRank = Settings->Params.Rank;
	NotifyOfPropertyChangeByTool(Settings);
}

void UHutongCityWallTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongCityWallParams& P = Settings->Params;
	P.PinFromRank();
	P.Height = FMath::Clamp(P.GetHeight() + 10.0 * DeltaCm, 200.0, 3000.0);
}

double UHutongCityWallTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetHeight() : 0.0;
}

void UHutongCityWallTool::FlipFacing()
{
	if (Settings) Settings->bFlipOuterSide = !Settings->bFlipOuterSide;
}

EHutongBaySide UHutongCityWallTool::OuterSide() const
{
	// A leg builds along +X with its thickness on +Y.
	return Settings && Settings->bFlipOuterSide ? EHutongBaySide::PlusY : EHutongBaySide::MinusY;
}

bool UHutongCityWallTool::IsNear(const FVector& Cursor, const FVector& Point) const
{
	return FVector::Dist2D(Cursor, Point) <= FMath::Max(10.0, 8.0 * WorldPerPixelAt(Point));
}

bool UHutongCityWallTool::FindFreeEnd(const FVector& Near, FLegEnd& Out) const
{
	if (!Settings) return false;
	const UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	const double B = Settings->Params.GetBaseWidth();
	const double Radius = FMath::Max(0.4 * B, 14.0 * WorldPerPixelAt(Near));
	double Best = Radius;
	bool bFound = false;
	for (TObjectIterator<UHutongCityWallBuildingComponent> It; It; ++It)
	{
		UHutongCityWallBuildingComponent* Leg = *It;
		const AActor* Owner = Leg->GetOwner();
		if (!Owner || Leg->GetWorld() != World || Leg->bLocked || !Owner->GetComponents().Contains(Leg)) continue;
		if (FMath::Abs(Leg->Params.GetBaseWidth() - B) > 1.0) continue;
		const FTransform& Xf = Owner->GetActorTransform();
		const double Lb = Leg->Params.GetBaseWidth();
		for (const bool bStart : { true, false })
		{
			const FHutongFootprintSkew& S = Leg->FootprintSkew;
			if (!(bStart ? S.Get(0).IsNearlyZero() && S.Get(3).IsNearlyZero() : S.Get(1).IsNearlyZero() && S.Get(2).IsNearlyZero())) continue;
			const FVector P = Xf.TransformPosition(FVector(bStart ? 0.0 : Leg->Length, 0.5 * Lb, 0.0));
			const double D = FVector::Dist2D(P, Near);
			if (D > Best) continue;
			const FVector F = Xf.TransformPosition(FVector(bStart ? Leg->Length : 0.0, 0.5 * Lb, 0.0));
			Out = { Leg, bStart, FVector(P.X, P.Y, Near.Z), FVector2D(F.X, F.Y) };
			Best = D;
			bFound = true;
		}
	}
	TArray<UHutongBuildingComponent::FWallPort> Ports;
	for (TObjectIterator<UHutongBuildingComponent> It; It; ++It)
	{
		const UHutongBuildingComponent* Piece = *It;
		const AActor* Owner = Piece->GetOwner();
		if (!Owner || Piece->GetWorld() != World || !Owner->GetComponents().Contains(Piece)) continue;
		Ports.Reset();
		Piece->GetWallPorts(Ports);
		for (const UHutongBuildingComponent::FWallPort& Port : Ports)
		{
			if (FMath::Abs(Port.BaseWidth - B) > 1.0) continue;
			const double D = FVector::Dist2D(Port.Point, Near);
			if (D > Best) continue;
			FLegEnd End;
			End.Point = FVector(Port.Point.X, Port.Point.Y, Near.Z);
			End.bPort = true;
			End.Out = Port.Out;
			End.OuterCorner = FVector2D(Port.Outside);
			Out = End;
			Best = D;
			bFound = true;
		}
	}
	return bFound;
}

bool UHutongCityWallTool::IsSameEnd(const FLegEnd& A, const FLegEnd& B) const
{
	return A.bPort == B.bPort && FVector::Dist2D(A.Point, B.Point) < 1.0;
}

void UHutongCityWallTool::TakeOuterSideFromPort(const FLegEnd& Port, const FVector2D& Dir)
{
	if (!Settings || !Port.bPort || Dir.IsNearlyZero()) return;
	// A leg's local +Y is to the left of its run; the battlements go to the outside corner's side.
	const FVector2D Left(-Dir.Y, Dir.X);
	Settings->bFlipOuterSide = (Port.OuterCorner - FVector2D(Port.Point)).Dot(Left) > 0.0;
}

bool UHutongCityWallTool::BuildJoinedChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments,
	HutongWallChain::FSegment* OutStartLeg, HutongWallChain::FSegment* OutEndLeg) const
{
	OutSegments.Reset();
	if (!Settings || ChainPoints.Num() == 0 || bChainClosed) return false;
	TArray<FVector2D> Points;
	for (const FVector& P : ChainPoints) Points.Add(FVector2D(P.X, P.Y));
	if (bWithCursor && FVector::Dist2D(CurrentWorld, ChainPoints.Last()) >= 10.0) Points.Add(FVector2D(CurrentWorld.X, CurrentWorld.Y));
	const bool bStart = StartJoin.IsSet() && !StartJoin->bPort && StartJoin->Leg.IsValid();
	const bool bEnd = !bWithCursor && EndJoin.IsSet() && !EndJoin->bPort && EndJoin->Leg.IsValid();
	if (bStart) Points.Insert(StartJoin->Far, 0);
	if (bEnd) Points.Add(EndJoin->Far);
	// A run on a port leaves at any angle, its end cut flush on the port's face.
	auto PortFace = [](const TOptional<FLegEnd>& End)
	{
		HutongWallChain::FEndFace Face;
		if (!End || !End->bPort) return Face;
		Face.bSet = true;
		Face.Point = FVector2D(End->Point);
		Face.Dir = FVector2D(-End->Out.Y, End->Out.X);
		return Face;
	};
	TOptional<FLegEnd> CursorEnd;
	FLegEnd Free;
	if (bWithCursor && FindFreeEnd(CurrentWorld, Free) && Free.bPort && FVector::Dist2D(Free.Point, CurrentWorld) < 1.0) CursorEnd = Free;
	const double B = Settings->Params.GetBaseWidth();
	TArray<HutongWallChain::FSegment> All;
	if (!HutongWallChain::Build(Points, B, 0.5 * B, PortFace(StartJoin), PortFace(bWithCursor ? CursorEnd : EndJoin), All)
		|| All.Num() != Points.Num() - 1) return false;
	const int32 First = bStart ? 1 : 0, Last = All.Num() - (bEnd ? 1 : 0);
	if (Last <= First) return false;
	if (bStart && OutStartLeg) *OutStartLeg = All[0];
	if (bEnd && OutEndLeg) *OutEndLeg = All.Last();
	OutSegments.Append(All.GetData() + First, Last - First);
	return true;
}

void UHutongCityWallTool::MitreJoinedLeg(const FLegEnd& End, const HutongWallChain::FSegment& Rebuilt) const
{
	UHutongCityWallBuildingComponent* Leg = End.Leg.Get();
	if (!Leg) return;
	Leg->Modify();
	Leg->FootprintSkew = HutongWallChain::CarryJoinSkew(Leg->FootprintSkew, End.bAtStart, Rebuilt.Skew);
	Leg->Rebuild();
	Leg->ApplyPlacementAttachments();
}

void UHutongCityWallTool::OnPlacementStarted(const FVector& HitWorld)
{
	StartJoin.Reset();
	EndJoin.Reset();
	FLegEnd Free;
	if (FindFreeEnd(HitWorld, Free))
	{
		StartJoin = Free;
		StartWorld = Free.Point;
		CurrentWorld = Free.Point;
		TakeOuterSideFromPort(Free, Free.Out);
	}
	ChainPoints = { StartWorld };
	bChainClosed = false;
	ChainRamps.Reset();
	CursorRamp = 0;
}

void UHutongCityWallTool::ToggleOpeningMark()
{
	CursorRamp = (CursorRamp + 1) % 3;
}

void UHutongCityWallTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	if (!Settings) return;
	const double Step = bFine ? 0.01 : (bCoarse ? 0.10 : 0.03);
	Settings->Params.RampPosition = FMath::Clamp(Settings->Params.RampPosition + Delta * Step, 0.0, 1.0);
}

FHutongCityWallParams UHutongCityWallTool::SegmentParams(int32 Index, const TArray<HutongWallChain::FSegment>& Segments) const
{
	FHutongCityWallParams P = Settings ? Settings->Params : FHutongCityWallParams();
	const int32 Mark = ChainRamps.IsValidIndex(Index) ? ChainRamps[Index] : (bIsDragging ? CursorRamp : 0);
	const bool bAnyMarked = CursorRamp != 0 || ChainRamps.ContainsByPredicate([](int32 R) { return R != 0; });
	if (bAnyMarked)
	{
		P.bRamp = Mark != 0;
		P.bRampRisesTowardStart = Mark == 2;
		return P;
	}
	int32 Longest = 0;
	for (int32 i = 1; i < Segments.Num(); ++i)
	{
		if (Segments[i].Length > Segments[Longest].Length) Longest = i;
	}
	P.bRamp = P.bRamp && Index == Longest;
	return P;
}

bool UHutongCityWallTool::OnRectCommitted(const FVector& HitWorld)
{
	// Every click lands here: undo the base's commit until the run ends.
	bRectCommitted = false;
	// The click re-read the cursor: step it as the hover did.
	if (!IsNear(HitWorld, ChainPoints.Last()) && !(ChainPoints.Num() >= 3 && IsNear(HitWorld, ChainPoints[0]))) StepCursorBearing(0.0);
	FLegEnd Free;
	if (!bChainClosed && FindFreeEnd(HitWorld, Free) && !(StartJoin && IsSameEnd(*StartJoin, Free))
		&& FVector::Dist2D(Free.Point, ChainPoints.Last()) > 10.0)
	{
		// Landing on another leg's free end or a corner finishes the run there.
		if (Free.bPort) TakeOuterSideFromPort(Free, -Free.Out);
		ChainPoints.Add(Free.Point);
		ChainRamps.Add(CursorRamp);
		CursorRamp = 0;
		EndJoin = Free;
		return true;
	}
	const FVector P = CurrentWorld;
	if (ChainPoints.Num() >= 2 && (IsNear(HitWorld, ChainPoints.Last()) || IsNear(P, ChainPoints.Last()))) return true;
	if (IsNear(P, ChainPoints.Last())) return false;
	if (ChainPoints.Num() >= 3 && IsNear(HitWorld, ChainPoints[0]))
	{
		bChainClosed = true;
		ChainRamps.Add(CursorRamp);
		CursorRamp = 0;
		return true;
	}
	ChainPoints.Add(P);
	ChainRamps.Add(CursorRamp);
	CursorRamp = 0;
	StartWorld = P;
	return false;
}

void UHutongCityWallTool::OnPlacementHover(const FVector& HitWorld)
{
	if (ChainPoints.Num() >= 2 && IsNear(HitWorld, ChainPoints.Last())) { CurrentWorld = ChainPoints.Last(); return; }
	if (ChainPoints.Num() >= 3 && IsNear(HitWorld, ChainPoints[0])) { CurrentWorld = ChainPoints[0]; return; }
	FLegEnd Free;
	if (FindFreeEnd(HitWorld, Free) && !(StartJoin && IsSameEnd(*StartJoin, Free)))
	{
		CurrentWorld = Free.Point;
		return;
	}
	StepCursorBearing(0.0);
}

bool UHutongCityWallTool::CanFinishPlacement() const
{
	return ChainPoints.Num() >= 2;
}

void UHutongCityWallTool::CancelPlacement()
{
	StartJoin.Reset();
	EndJoin.Reset();
	ChainPoints.Reset();
	bChainClosed = false;
	ChainRamps.Reset();
	CursorRamp = 0;
	Super::CancelPlacement();
}

bool UHutongCityWallTool::BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const
{
	OutSegments.Reset();
	if (!Settings || ChainPoints.Num() == 0) return false;
	if (!bChainClosed) return BuildJoinedChain(bWithCursor, OutSegments, nullptr, nullptr);
	TArray<FVector2D> Points;
	for (const FVector& P : ChainPoints) Points.Add(FVector2D(P.X, P.Y));
	if (bWithCursor && FVector::Dist2D(CurrentWorld, ChainPoints.Last()) >= 10.0) Points.Add(FVector2D(CurrentWorld.X, CurrentWorld.Y));
	const double B = Settings->Params.GetBaseWidth();

	// Closed: pad with the neighbours across the closing vertex so every join is a mitre.
	const int32 N = Points.Num();
	TArray<FVector2D> Loop = { Points[N - 1] };
	Loop.Append(Points);
	Loop.Add(Points[0]);
	Loop.Add(Points[1]);
	TArray<HutongWallChain::FSegment> All;
	if (!HutongWallChain::Build(Loop, B, 0.5 * B, {}, {}, All) || All.Num() != N + 2) return false;
	OutSegments.Append(All.GetData() + 1, N);
	return true;
}

void UHutongCityWallTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const double B = Settings ? Settings->Params.GetBaseWidth() : HutongCanon::CityWall::BaseWidthCm;
	OutMinX = 0.0;
	OutMaxX = bIsDragging ? FVector::Dist2D(StartWorld, CurrentWorld) : 0.0;
	OutMinY = -0.5 * B;
	OutMaxY = 0.5 * B;
}

void UHutongCityWallTool::SpawnFinalActor()
{
	// Cleared: the cursor's leg is unbuilt, and its mark would suppress the committed legs' ramp.
	CursorRamp = 0;
	TArray<HutongWallChain::FSegment> Segments;
	HutongWallChain::FSegment StartLeg, EndLeg;
	const bool bBuilt = bChainClosed ? BuildChain(false, Segments) : BuildJoinedChain(false, Segments, &StartLeg, &EndLeg);
	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!bBuilt || !World || !Settings)
	{
		ChainPoints.Reset();
		bChainClosed = false;
		return;
	}

	FScopedSlowTask Task((float)Segments.Num(), NSLOCTEXT("CityWallTool", "PlacingRun", "Building the city wall…"));
	Task.MakeDialogDelayed(0.4f);

	const double Z = ChainPoints[0].Z;
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(NSLOCTEXT("CityWallTool", "PlaceRun", "Place Hutong City Wall"));
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const HutongWallChain::FSegment& S = Segments[i];
		Task.EnterProgressFrame(1.0f);
		const FTransform Xform(FRotator(0.0, S.YawDeg, 0.0), FVector(S.Origin.X, S.Origin.Y, Z));
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase());
		if (!Actor) continue;

		UHutongCityWallBuildingComponent* Building = NewObject<UHutongCityWallBuildingComponent>(Actor, NAME_None, RF_Transactional);
		Building->Params = SegmentParams(i, Segments);
		Building->Length = S.Length;
		Building->OuterSide = OuterSide();
		Building->FootprintSkew = S.Skew;
		if (Appearance) Building->Palette = Appearance->Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		Building->Rebuild();
		Building->ApplyPlacementAttachments();
	}
	if (!bChainClosed && StartJoin && !StartJoin->bPort && StartJoin->Leg.IsValid()) MitreJoinedLeg(*StartJoin, StartLeg);
	if (!bChainClosed && EndJoin && !EndJoin->bPort && EndJoin->Leg.IsValid())
	{
		// Built from the join toward the far end: read it from the far end.
		HutongWallChain::FSegment FromFar = EndLeg;
		FromFar.Skew = HutongWallChain::Reversed(EndLeg.Skew);
		MitreJoinedLeg(*EndJoin, FromFar);
	}
	ToolManager->EndUndoTransaction();

	StartJoin.Reset();
	EndJoin.Reset();
	ChainPoints.Reset();
	bChainClosed = false;
	ChainRamps.Reset();
	CursorRamp = 0;
}

FString UHutongCityWallTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongCityWallParams& P = Settings->Params;
	return FString::Printf(TEXT("City wall (城牆) · %.1f m high, %.1f m at the base, %.1f m on top · battlements on the orange side"),
		P.GetHeight() / 100.0, P.GetBaseWidth() / 100.0, P.GetTopWidth() / 100.0);
}

FText UHutongCityWallTool::GetStagePromptText() const
{
	if (!bIsDragging) return Super::GetStagePromptText();
	return FText::Format(NSLOCTEXT("CityWallTool", "PromptSegment",
		"Click to end this leg; click the last end again or press Enter to finish, or the first point to close the circuit. G puts a ramp on this leg (again to turn it round, again to remove it), [ and ] slide it, F flips the battlements, Shift snaps to 15°, Esc drops the run.{0}"),
		SnapKeyClause());
}

TArray<FText> UHutongCityWallTool::GetStageNames() const
{
	return { NSLOCTEXT("CityWallTool", "StageAnchor", "Anchor"), NSLOCTEXT("CityWallTool", "StageLegs", "Legs") };
}

FText UHutongCityWallTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("CityWallTool", "KeyHint", "G ramp on this leg / turn round / none · [ ] slide ramp · F battlements side · Enter finish · Shift 15° · - = height · {0} {1} · Esc cancel · Ctrl+Z undo"),
		SnapKeyName(),
		SnappingActive() ? NSLOCTEXT("CityWallTool", "KeyHintNoSnap", "no snap") : NSLOCTEXT("CityWallTool", "KeyHintSnap", "snap"));
}

TArray<FText> UHutongCityWallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongCityWallTool", "HelpRun",
		"Click to start the city wall (城牆) on its centre line, click to end each leg, click the last end again or press Enter to finish, or the first point to close the circuit.");
	Lines.Insert(NSLOCTEXT("HutongCityWallTool", "HelpRamp",
		"G puts a ramp (馬道) on the leg being drawn, rising toward where you are heading; press again to turn it round, again to remove it. The preview shows it in green, an arrow up its slope; [ and ] slide it along."), 1);
	Lines.Insert(NSLOCTEXT("HutongCityWallTool", "HelpSide",
		"The crenellated parapet (垛牆) faces out of the city; F flips which side of the run it stands on."), 1);
	return Lines;
}

void UHutongCityWallTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* EndPDI = RenderAPI ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	FLegEnd Free;
	if (EndPDI && FindFreeEnd(CurrentWorld, Free))
	{
		// A free end in reach: the run will carry the leg on from here.
		const double R = FMath::Max(0.25 * Settings->Params.GetBaseWidth(), 10.0 * WorldPerPixelAt(Free.Point));
		const FLinearColor JoinColor(0.2f, 1.0f, 0.4f, 1.0f);
		const FVector C = Free.Point;
		const FVector D[4] = { C + FVector(R, 0, 0), C + FVector(0, R, 0), C - FVector(R, 0, 0), C - FVector(0, R, 0) };
		for (int32 k = 0; k < 4; ++k) DrawPreviewLine(EndPDI, D[k], D[(k + 1) % 4], JoinColor, 5.0f);
	}
	if (!bIsDragging)
	{
		Super::Render(RenderAPI);
		return;
	}
	if (!RenderAPI || !Settings) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	TArray<HutongWallChain::FSegment> Segments;
	if (!BuildChain(true, Segments)) return;
	const double B = Settings->Params.GetBaseWidth();
	const double Z = ChainPoints[0].Z;
	const FLinearColor Color(1.0f, 0.9f, 0.15f, 1.0f);
	const FLinearColor Outer(1.0f, 0.45f, 0.1f, 1.0f);
	const FLinearColor HeightColor(0.45f, 0.8f, 1.0f, 1.0f);
	const FVector Up(0.0, 0.0, GetPreviewHeight());
	// Corners anticlockwise from the origin: 0→1 is the -Y face, 2→3 the +Y face.
	const int32 OuterEdge = OuterSide() == EHutongBaySide::MinusY ? 0 : 2;

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		FVector2D C[4];
		HutongWallChain::Corners(Segments[i], B, C);
		FVector W[4];
		for (int32 k = 0; k < 4; ++k) W[k] = FVector(C[k].X, C[k].Y, Z);
		for (int32 k = 0; k < 4; ++k)
		{
			const bool bOuter = k == OuterEdge;
			DrawPreviewLine(PDI, W[k], W[(k + 1) % 4], bOuter ? Outer : Color, bOuter ? 8.0f : 5.0f);
		}
		if (i == Segments.Num() - 1)
		{
			for (int32 k = 0; k < 4; ++k)
			{
				DrawDashedPreviewLine(PDI, W[k], W[k] + Up, HeightColor, 2.5f);
				DrawDashedPreviewLine(PDI, W[k] + Up, W[(k + 1) % 4] + Up, HeightColor, 3.0f);
			}
		}

		FVector2D RC[4], UpFrom, UpTo, Gap[2];
		const HutongWallChain::FSegment& Seg = Segments[i];
		if (UHutongCityWallBuildingComponent::GetRampOutline(SegmentParams(i, Segments), Seg.Length, OuterSide(), RC, UpFrom, UpTo, Gap))
		{
			const FLinearColor RampColor(0.3f, 1.0f, 0.4f, 1.0f);
			const double Yaw = FMath::DegreesToRadians(Seg.YawDeg);
			const FVector2D Dx(FMath::Cos(Yaw), FMath::Sin(Yaw)), Dy(-Dx.Y, Dx.X);
			auto ToWorld = [&](const FVector2D& L, double Zw)
			{
				const FVector2D P2 = Seg.Origin + Dx * L.X + Dy * L.Y;
				return FVector(P2.X, P2.Y, Zw);
			};
			for (int32 k = 0; k < 4; ++k) DrawPreviewLine(PDI, ToWorld(RC[k], Z), ToWorld(RC[(k + 1) % 4], Z), RampColor, 5.0f);
			const FVector A = ToWorld(UpFrom, Z), Bp = ToWorld(UpTo, Z);
			DrawPreviewLine(PDI, A, Bp, RampColor, 6.0f);
			const FVector Along = (Bp - A).GetSafeNormal(), Across(-Along.Y, Along.X, 0.0);
			const double Head = FMath::Min(250.0, 0.25 * FVector::Dist(A, Bp));
			DrawPreviewLine(PDI, Bp, Bp - Along * Head + Across * 0.6 * Head, RampColor, 6.0f);
			DrawPreviewLine(PDI, Bp, Bp - Along * Head - Across * 0.6 * Head, RampColor, 6.0f);
			DrawPreviewLine(PDI, ToWorld(Gap[0], Z + Up.Z), ToWorld(Gap[1], Z + Up.Z), RampColor, 8.0f);
		}
	}
}
