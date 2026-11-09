#include "Tools/RectDragToolBase.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Application/SlateApplication.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "SceneView.h"
#include "ToolContextInterfaces.h"
#include "GameFramework/Actor.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "SceneManagement.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongGroupTransform.h"

#define LOCTEXT_NAMESPACE "HutongRectDragTool"

namespace
{
	const FLinearColor GroupBoxColor(0.35f, 0.75f, 1.0f);
	const FLinearColor GroupCornerColor(1.0f, 0.9f, 0.15f);
	const FLinearColor GroupEdgeColor(0.55f, 1.0f, 0.65f);
	const FLinearColor GroupRingColor(0.45f, 0.8f, 1.0f);
	const FLinearColor GroupActiveColor(1.0f, 0.45f, 0.1f);
	const FLinearColor GroupPivotColor(1.0f, 0.3f, 0.9f);
	// Handles on a box spanning a city stay this many pixels across.
	constexpr double GroupHandleMinPixels = 12.0;
	// A click takes a corner this many pixels off, at least.
	constexpr double PivotPickPixels = 30.0;
	constexpr double GroupMinFactor = 0.05;

	// Which side of the box a handle pulls: -1 min, +1 max, 0 neither (as the footprint handles).
	void GroupHandleSides(int32 Handle, int32& OutX, int32& OutY)
	{
		static const int32 X[8] = { -1, +1, +1, -1, 0, +1, 0, -1 };
		static const int32 Y[8] = { -1, -1, +1, +1, -1, 0, +1, 0 };
		const int32 H = FMath::Clamp(Handle, 0, 7);
		OutX = X[H];
		OutY = Y[H];
	}

}

void URectDragToolBase::DrawGroupCircle(FPrimitiveDrawInterface* PDI, const FVector& C, double R, const FLinearColor& Colour, float Thickness, double Turn)
{
	constexpr int32 Segments = 24;
	for (int32 i = 0; i < Segments; ++i)
	{
		const double A0 = Turn * i / Segments, A1 = Turn * (i + 1) / Segments;
		DrawPreviewLine(PDI, C + FVector(R * FMath::Cos(A0), R * FMath::Sin(A0), 0.0),
			C + FVector(R * FMath::Cos(A1), R * FMath::Sin(A1), 0.0), Colour, Thickness);
	}
}

FVector2D URectDragToolBase::FGroupBox::Corner(int32 Index) const
{
	switch (Index & 3)
	{
	case 0: return Min;
	case 1: return FVector2D(Max.X, Min.Y);
	case 2: return Max;
	default: return FVector2D(Min.X, Max.Y);
	}
}

FVector URectDragToolBase::FGroupBox::World(const FVector2D& Frame) const
{
	return HutongGroupTransform::FromFrame(Frame, YawDeg, Z);
}

TArray<UHutongBuildingComponent*> URectDragToolBase::GetGroupMembers() const
{
	TArray<UHutongBuildingComponent*> Members = HutongDetailOps::CollectSelected();
	Members.RemoveAll([](const UHutongBuildingComponent* B)
	{
		const FVector2D Size = B ? B->GetFootprintSize() : FVector2D::ZeroVector;
		return !B || !B->GetOwner() || Size.X <= 0.0 || Size.Y <= 0.0;
	});
	if (Members.Num() == 1 && Members[0]->bPlanOnly) Members.Reset();
	return Members;
}

bool URectDragToolBase::GetGroupBox(const TArray<UHutongBuildingComponent*>& Members, FGroupBox& Out)
{
	TArray<double> Yaws;
	for (const UHutongBuildingComponent* B : Members) Yaws.Add(B->GetOwner()->GetActorRotation().Yaw);
	Out.YawDeg = HutongGroupTransform::FrameYaw(Yaws);
	FBox2D Box(ForceInit);
	Out.Z = -UE_BIG_NUMBER;
	for (const UHutongBuildingComponent* B : Members)
	{
		const FTransform Xf = B->GetOwner()->GetActorTransform();
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		for (const FVector2D& Q : Quad) Box += HutongGroupTransform::ToFrame(Xf.TransformPosition(FVector(Q.X, Q.Y, 0.0)), Out.YawDeg);
		Out.Z = FMath::Max(Out.Z, Xf.GetLocation().Z);
	}
	if (!Box.bIsValid) return false;
	Out.Min = Box.Min;
	Out.Max = Box.Max;
	return true;
}

double URectDragToolBase::GroupHandleSize(const FGroupBox& Box) const
{
	const FVector Centre = Box.World(0.5 * (Box.Min + Box.Max));
	return FMath::Max(PlanHandleSize(Box.Max - Box.Min), GroupHandleMinPixels * WorldPerPixelAt(Centre));
}

FVector URectDragToolBase::GroupHandleWorld(const FGroupBox& Box, int32 Handle) const
{
	if (Handle == PlanHandleRing)
	{
		return Box.World(FVector2D(0.5 * (Box.Min.X + Box.Max.X), Box.Min.Y - 2.0 * GroupHandleSize(Box)));
	}
	if (Handle < 4) return Box.World(Box.Corner(Handle));
	const int32 Edge = Handle - 4;
	return Box.World(0.5 * (Box.Corner(Edge) + Box.Corner(Edge + 1)));
}

int32 URectDragToolBase::HitTestGroup(const FVector& Ground) const
{
	const TArray<UHutongBuildingComponent*> Members = GetGroupMembers();
	FGroupBox Box;
	if (Members.IsEmpty() || !GetGroupBox(Members, Box)) return INDEX_NONE;
	const double Pick = 0.75 * GroupHandleSize(Box);
	if (FVector::Dist2D(Ground, GroupHandleWorld(Box, PlanHandleRing)) <= Pick) return PlanHandleRing;
	for (int32 H = 0; H < 8; ++H)
	{
		if (FVector::Dist2D(Ground, GroupHandleWorld(Box, H)) <= Pick) return H;
	}
	for (const UHutongBuildingComponent* B : Members)
	{
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		const FVector Local = B->GetOwner()->GetActorTransform().InverseTransformPosition(Ground);
		if (HutongFootprint::PointInQuad(Quad, FVector2D(Local.X, Local.Y))) return PlanHandleInside;
	}
	return INDEX_NONE;
}

void URectDragToolBase::BeginGroupEdit(int32 Hit, const FVector& Ground)
{
	const TArray<UHutongBuildingComponent*> Members = bTurnMode ? GetTurnMembers() : GetGroupMembers();
	if (Members.IsEmpty() || !GetGroupBox(Members, GroupStartBox)) return;
	GroupStart.Reset();
	for (UHutongBuildingComponent* B : Members) CaptureLegState(B, GroupStart.AddDefaulted_GetRef());
	GroupPressGround = Ground;
	GroupGrabbed = INDEX_NONE;
	EditedPlan.Reset();
	EditHandle = Hit;
	bPlanDragMoved = false;

	TSet<const AActor*> Owners;
	for (const UHutongBuildingComponent* B : Members) Owners.Add(B->GetOwner());
	GroupSnapTargets = GetFootprints().FilterByPredicate([&](const HutongSnap::FFootprint& F) { return !Owners.Contains(F.Actor.Get()); });

	if (Hit == PlanHandleInside)
	{
		PlanEdit = EPlanEdit::GroupMove;
		for (int32 i = 0; i < Members.Num(); ++i)
		{
			FVector2D Quad[4];
			Members[i]->GetFootprintCorners(Quad);
			const FVector Local = GroupStart[i].Transform.InverseTransformPosition(Ground);
			if (HutongFootprint::PointInQuad(Quad, FVector2D(Local.X, Local.Y))) { GroupGrabbed = i; break; }
		}
	}
	else if (Hit == PlanHandleRing)
	{
		PlanEdit = EPlanEdit::GroupRotate;
		FVector Pinned;
		EditPivot = ResolvePivot(Pinned) ? Pinned : GroupStartBox.World(0.5 * (GroupStartBox.Min + GroupStartBox.Max));
		EditStartCursorAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Ground.Y - EditPivot.Y, Ground.X - EditPivot.X));
	}
	else
	{
		PlanEdit = EPlanEdit::GroupScale;
	}
}

void URectDragToolBase::UpdateGroupEdit(const FVector& Ground)
{
	switch (PlanEdit)
	{
	case EPlanEdit::GroupMove:
	{
		FVector Delta = Ground - GroupPressGround;
		Delta.Z = 0.0;
		if (SnappingActive() && GroupStart.IsValidIndex(GroupGrabbed))
		{
			const FVector Start = GroupStart[GroupGrabbed].Transform.GetLocation();
			const FVector Origin = Start + Delta;
			const HutongSnap::FResult R = HutongSnap::FindSnap(GroupSnapTargets, Origin, EffectiveSnapRadius(Origin));
			if (R.bSnapped) Delta = FVector(R.Point.X - Start.X, R.Point.Y - Start.Y, 0.0);
		}
		for (const FRunLegState& St : GroupStart)
		{
			AActor* Owner = St.Building.IsValid() ? St.Building->GetOwner() : nullptr;
			if (Owner) Owner->SetActorLocation(St.Transform.GetLocation() + Delta);
		}
		break;
	}
	case EPlanEdit::GroupRotate:
	{
		const double dx = Ground.X - EditPivot.X, dy = Ground.Y - EditPivot.Y;
		if (dx * dx + dy * dy < 1.0) return;
		double Delta = FMath::RadiansToDegrees(FMath::Atan2(dy, dx)) - EditStartCursorAngleDeg;
		if (IsSkewKeyDown())
		{
			Delta = FMath::RoundToDouble(Delta / 5.0) * 5.0;
		}
		else if (SnappingActive() && Snap->bAdoptAngle)
		{
			const TArray<double> Yaws = HutongSnap::GatherEdgeYaws(GroupSnapTargets, EditPivot, FMath::Max(Snap->Radius, 1.0) * 12.0);
			Delta = HutongSnap::SnapYaw(GroupStartBox.YawDeg + Delta, Yaws, FMath::Max(Snap->AngleToleranceDeg, 0.0)) - GroupStartBox.YawDeg;
		}
		for (const FRunLegState& St : GroupStart)
		{
			AActor* Owner = St.Building.IsValid() ? St.Building->GetOwner() : nullptr;
			if (Owner) Owner->SetActorTransform(HutongGroupTransform::RotateAbout(St.Transform, EditPivot, Delta));
		}
		break;
	}
	case EPlanEdit::GroupScale:
	{
		const FGroupBox& Box = GroupStartBox;
		int32 SideX, SideY;
		GroupHandleSides(EditHandle, SideX, SideY);
		const FVector2D Mid = 0.5 * (Box.Min + Box.Max);
		const FVector2D Anchor(SideX < 0 ? Box.Max.X : SideX > 0 ? Box.Min.X : Mid.X, SideY < 0 ? Box.Max.Y : SideY > 0 ? Box.Min.Y : Mid.Y);
		const FVector2D Handle(SideX < 0 ? Box.Min.X : SideX > 0 ? Box.Max.X : Mid.X, SideY < 0 ? Box.Min.Y : SideY > 0 ? Box.Max.Y : Mid.Y);
		const FVector2D Cursor = HutongGroupTransform::ToFrame(Ground, Box.YawDeg);
		const FVector2D Reach = Handle - Anchor;
		FVector2D Factors(1.0, 1.0);
		if (EditHandle < 4)
		{
			if (Reach.SizeSquared() < 1.0) return;
			const double S = FVector2D::DotProduct(Cursor - Anchor, Reach) / Reach.SizeSquared();
			Factors = FVector2D(S, S);
		}
		else
		{
			if (SideX != 0 && FMath::Abs(Reach.X) >= 1.0) Factors.X = (Cursor.X - Anchor.X) / Reach.X;
			if (SideY != 0 && FMath::Abs(Reach.Y) >= 1.0) Factors.Y = (Cursor.Y - Anchor.Y) / Reach.Y;
		}
		Factors = FVector2D(FMath::Max(Factors.X, GroupMinFactor), FMath::Max(Factors.Y, GroupMinFactor));

		for (const FRunLegState& St : GroupStart)
		{
			UHutongBuildingComponent* B = St.Building.Get();
			AActor* Owner = B ? B->GetOwner() : nullptr;
			if (!Owner || St.Size.X <= 0.0 || St.Size.Y <= 0.0) continue;
			const FVector2D Sides = B->bLocked ? FVector2D(1.0, 1.0)
				: HutongGroupTransform::SideFactors(St.Transform.Rotator().Yaw, Box.YawDeg, Factors, St.Size, IsLineLikePlan(St.Size));
			B->SetFootprintSize(St.Size * Sides);
			const FVector2D Accepted = B->GetFootprintSize();
			B->FootprintSkew = HutongGroupTransform::ScaledSkew(St.Skew, Accepted / St.Size);
			const FVector Centre = HutongGroupTransform::ScaledCentre(
				HutongGroupTransform::FootprintCentre(St.Transform, St.Size), Box.YawDeg, Anchor, Factors);
			FTransform Xf = St.Transform;
			Xf.SetLocation(HutongGroupTransform::OriginForCentre(St.Transform, Centre, Accepted));
			Owner->SetActorTransform(Xf);
			if (B->bPlanOnly) B->ApplyPlanOutline();
		}
		break;
	}
	default:
		break;
	}
}

void URectDragToolBase::CommitGroupEdit()
{
	const bool bScaled = PlanEdit == EPlanEdit::GroupScale;
	TArray<FRunLegState> Final;
	bool bChanged = false;
	int32 BuiltToRebuild = 0;
	TArray<bool> Reshaped;
	for (const FRunLegState& St : GroupStart)
	{
		FRunLegState& F = Final.AddDefaulted_GetRef();
		CaptureLegState(St.Building.Get(), F);
		const bool bReshaped = !F.Size.Equals(St.Size, 1.0e-6) || F.Skew != St.Skew;
		Reshaped.Add(bScaled && bReshaped);
		bChanged |= bReshaped || !F.Transform.Equals(St.Transform, 1.0e-6);
		if (Reshaped.Last() && St.Building.IsValid() && !St.Building->bPlanOnly) ++BuiltToRebuild;
	}
	if (!bChanged)
	{
		CancelGroupEdit();
		return;
	}

	// Rewound and reapplied inside one transaction: one Ctrl+Z puts the whole selection back.
	for (const FRunLegState& St : GroupStart) RestoreLegState(St);
	{
		const FScopedTransaction Transaction(bScaled ? LOCTEXT("GroupScale", "Scale Hutong Buildings")
			: PlanEdit == EPlanEdit::GroupRotate ? LOCTEXT("GroupRotate", "Rotate Hutong Buildings") : LOCTEXT("GroupMove", "Move Hutong Buildings"));
		FScopedSlowTask Task((float)BuiltToRebuild, LOCTEXT("GroupRebuild", "Rebuilding the scaled buildings..."));
		if (BuiltToRebuild > 0) Task.MakeDialog();
		for (int32 i = 0; i < Final.Num(); ++i)
		{
			const FRunLegState& F = Final[i];
			UHutongBuildingComponent* B = F.Building.Get();
			AActor* Owner = B ? B->GetOwner() : nullptr;
			if (!Owner) continue;
			Owner->Modify();
			B->Modify();
			if (UActorComponent* Outline = Owner->FindComponentByClass<UHutongPlanOutlineComponent>()) Outline->Modify();
			RestoreLegState(F);
			if (!Reshaped[i]) continue;
			if (!B->bPlanOnly) Task.EnterProgressFrame();
			B->Rebuild();
		}
	}
	PlanEdit = EPlanEdit::None;
	EditHandle = INDEX_NONE;
	GroupStart.Reset();
	GroupSnapTargets.Reset();
	GroupGrabbed = INDEX_NONE;
}

void URectDragToolBase::CancelGroupEdit()
{
	const bool bScaled = PlanEdit == EPlanEdit::GroupScale;
	for (const FRunLegState& St : GroupStart)
	{
		RestoreLegState(St);
		if (bScaled && St.Building.IsValid() && St.Building->bPlanOnly) St.Building->ApplyPlanOutline();
	}
	PlanEdit = EPlanEdit::None;
	EditHandle = INDEX_NONE;
	GroupStart.Reset();
	GroupSnapTargets.Reset();
	GroupGrabbed = INDEX_NONE;
}

void URectDragToolBase::DrawGroupHandles(FPrimitiveDrawInterface* PDI) const
{
	const TArray<UHutongBuildingComponent*> Members = GetGroupMembers();
	FGroupBox Box;
	if (!PDI || Members.IsEmpty() || !GetGroupBox(Members, Box)) return;
	const double S = GroupHandleSize(Box);
	const FVector Lift(0.0, 0.0, 3.0);
	const FVector AX = HutongGroupTransform::FromFrame(FVector2D(1.0, 0.0), Box.YawDeg, 0.0);
	const FVector AY = HutongGroupTransform::FromFrame(FVector2D(0.0, 1.0), Box.YawDeg, 0.0);

	auto ColorFor = [&](int32 H, const FLinearColor& Base)
	{
		const bool bActive = (PlanEdit == EPlanEdit::GroupScale && EditHandle == H) || (PlanEdit == EPlanEdit::GroupRotate && H == PlanHandleRing);
		return (bActive || HoverGroupHandle == H) ? GroupActiveColor : Base;
	};
	auto Thick = [&](int32 H) { return (HoverGroupHandle == H || IsGroupEdit()) ? 3.5f : 2.5f; };

	if (Members.Num() > 1)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			DrawDashedPreviewLine(PDI, Box.World(Box.Corner(i)) + Lift, Box.World(Box.Corner(i + 1)) + Lift, GroupBoxColor, 2.0f);
		}
	}
	for (int32 H = 0; H < 8; ++H)
	{
		const FVector P = GroupHandleWorld(Box, H) + Lift;
		const FLinearColor C = ColorFor(H, H < 4 ? GroupCornerColor : GroupEdgeColor);
		const float T = Thick(H);
		const double R = 0.5 * S;
		if (H < 4)
		{
			const FVector Q[4] = { P - AX * R - AY * R, P + AX * R - AY * R, P + AX * R + AY * R, P - AX * R + AY * R };
			for (int32 i = 0; i < 4; ++i) DrawPreviewLine(PDI, Q[i], Q[(i + 1) % 4], C, T);
		}
		else
		{
			const double D = 0.8 * R;
			const FVector Q[4] = { P - AX * D, P - AY * D, P + AX * D, P + AY * D };
			for (int32 i = 0; i < 4; ++i) DrawPreviewLine(PDI, Q[i], Q[(i + 1) % 4], C, T);
		}
	}
	const FVector Ring = GroupHandleWorld(Box, PlanHandleRing) + Lift;
	const FLinearColor RC = ColorFor(PlanHandleRing, GroupRingColor);
	const float RT = Thick(PlanHandleRing);
	DrawPreviewLine(PDI, GroupHandleWorld(Box, 4) + Lift, Ring, RC, 2.0f);
	DrawGroupCircle(PDI, Ring, 0.6 * S, RC, RT);
	DrawGroupCircle(PDI, Ring, 0.33 * S, RC, RT, 1.5 * PI);
}

bool URectDragToolBase::FindPivotCorner(const FVector& Ground, FPlanPivot& Out, FVector& OutWorld) const
{
	const TArray<UHutongBuildingComponent*> Members = GetTurnMembers();
	if (Members.IsEmpty() || GEditor == nullptr) return false;

	double Best = FMath::Max(EffectiveSnapRadius(Ground), PivotPickPixels * WorldPerPixelAt(Ground));
	bool bFound = false;
	for (UHutongBuildingComponent* B : Members)
	{
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		const FTransform Xf = B->GetOwner()->GetActorTransform();
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector W = Xf.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 0.0));
			const double D = FVector::Dist2D(Ground, W);
			if (D > Best) continue;
			Best = D;
			Out.Building = B;
			Out.Corner = i;
			OutWorld = W;
			bFound = true;
		}
	}
	FGroupBox Box;
	if (Members.Num() > 1 && GetGroupBox(Members, Box))
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector W = Box.World(Box.Corner(i));
			const double D = FVector::Dist2D(Ground, W);
			if (D >= Best) continue;
			Best = D;
			Out.Building.Reset();
			Out.Corner = i;
			OutWorld = W;
			bFound = true;
		}
	}
	if (!bFound) return false;
	Out.Selection.Reset();
	for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
	{
		if (AActor* Actor = Cast<AActor>(*It)) Out.Selection.Add(Actor);
	}
	return true;
}

bool URectDragToolBase::ResolvePivot(FVector& Out) const
{
	if (!Pivot.IsSet() || GEditor == nullptr) return false;
	const USelection* Selected = GEditor->GetSelectedActors();
	if (!Selected || Selected->Num() != Pivot->Selection.Num()) return false;
	for (const TWeakObjectPtr<AActor>& Actor : Pivot->Selection)
	{
		if (!Actor.IsValid() || !Actor->IsSelected()) return false;
	}
	if (Pivot->Building.IsValid())
	{
		const UHutongBuildingComponent* B = Pivot->Building.Get();
		if (!B->GetOwner()) return false;
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		const FVector2D& Q = Quad[Pivot->Corner & 3];
		Out = B->GetOwner()->GetActorTransform().TransformPosition(FVector(Q.X, Q.Y, 0.0));
		return true;
	}
	if (Pivot->Building.IsExplicitlyNull())
	{
		FGroupBox Box;
		const TArray<UHutongBuildingComponent*> Members = GetTurnMembers();
		if (Members.IsEmpty() || !GetGroupBox(Members, Box)) return false;
		Out = Box.World(Box.Corner(Pivot->Corner));
		return true;
	}
	return false;
}

TArray<UHutongBuildingComponent*> URectDragToolBase::GetTurnMembers() const
{
	TArray<UHutongBuildingComponent*> Members = GetGroupMembers();
	if (Members.IsEmpty())
	{
		if (UHutongBuildingComponent* Plan = GetSelectedPlanBuilding()) Members.Add(Plan);
	}
	return Members;
}

bool URectDragToolBase::ToggleTurnMode()
{
	if (bTurnMode)
	{
		EndTurnMode();
		return true;
	}
	if (IsPlacingActive() || IsEditingPlan() || GetTurnMembers().IsEmpty()) return false;
	bTurnMode = true;
	Pivot.Reset();
	return true;
}

void URectDragToolBase::EndTurnMode()
{
	if (PlanEdit == EPlanEdit::GroupRotate) CancelPlanEdit();
	bTurnMode = false;
	bHoverPivot = false;
	Pivot.Reset();
}

void URectDragToolBase::DrawPivot(FPrimitiveDrawInterface* PDI) const
{
	if (!PDI || !bTurnMode) return;
	const FVector Lift(0.0, 0.0, 4.0);
	auto Cross = [&](const FVector& P, double R, float Thickness)
	{
		DrawPreviewLine(PDI, P - FVector(1.6 * R, 0.0, 0.0), P + FVector(1.6 * R, 0.0, 0.0), GroupPivotColor, Thickness);
		DrawPreviewLine(PDI, P - FVector(0.0, 1.6 * R, 0.0), P + FVector(0.0, 1.6 * R, 0.0), GroupPivotColor, Thickness);
	};
	FVector Pinned;
	const bool bPinned = ResolvePivot(Pinned);
	if (!IsEditingPlan())
	{
		const TArray<UHutongBuildingComponent*> Members = GetTurnMembers();
		auto Offer = [&](const FVector& W)
		{
			if (bPinned && FVector::Dist2D(W, Pinned) < 1.0) return;
			DrawGroupCircle(PDI, W + Lift, 5.0 * WorldPerPixelAt(W), GroupPivotColor * FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), 1.5f);
		};
		for (const UHutongBuildingComponent* B : Members)
		{
			FVector2D Quad[4];
			B->GetFootprintCorners(Quad);
			const FTransform Xf = B->GetOwner()->GetActorTransform();
			for (const FVector2D& Q : Quad) Offer(Xf.TransformPosition(FVector(Q.X, Q.Y, 0.0)));
		}
		FGroupBox Box;
		if (Members.Num() > 1 && GetGroupBox(Members, Box))
		{
			for (int32 i = 0; i < 4; ++i) Offer(Box.World(Box.Corner(i)));
		}
		if (bHoverPivot) DrawGroupCircle(PDI, HoverPivotWorld + Lift, 10.0 * WorldPerPixelAt(HoverPivotWorld), GroupPivotColor, 2.5f);
	}
	if (bPinned)
	{
		const double R = 10.0 * WorldPerPixelAt(Pinned);
		DrawGroupCircle(PDI, Pinned + Lift, R, GroupPivotColor, 3.0f);
		Cross(Pinned + Lift, R, 2.0f);
		return;
	}
	const TArray<UHutongBuildingComponent*> Members = GetTurnMembers();
	FGroupBox Box;
	const FVector Centre = IsEditingPlan() ? EditPivot
		: (GetGroupBox(Members, Box) ? Box.World(0.5 * (Box.Min + Box.Max)) : FVector::ZeroVector);
	if (IsEditingPlan() || !Members.IsEmpty()) Cross(Centre + Lift, 8.0 * WorldPerPixelAt(Centre), 2.0f);
}

void URectDragToolBase::DrawTurnHint(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const
{
	if (!bTurnMode || !bTurnCursor || !Canvas || !RenderAPI) return;
	const FSceneView* View = RenderAPI->GetSceneView();
	const UFont* Font = UEngine::GetMediumFont();
	FVector2D Pixel;
	if (!View || !Font || !View->WorldToPixel(TurnCursorWorld, Pixel)) return;
	FVector Pinned;
	const bool bPinned = ResolvePivot(Pinned);
	FString Text;
	if (PlanEdit == EPlanEdit::GroupRotate) Text = TEXT("Release to set  -  Shift: 5 degree steps  -  Esc puts it back");
	else if (bHoverPivot && bPinned && FVector::Dist2D(HoverPivotWorld, Pinned) < 1.0) Text = TEXT("Click to turn about the centre instead");
	else if (bHoverPivot) Text = TEXT("Click to turn about this corner");
	else if (bPinned) Text = TEXT("Drag to turn about the marked corner  -  Esc or R to stop");
	else Text = TEXT("Click a corner to turn about it, or drag to turn about the centre  -  Esc or R to stop");

	// Engine bitmap font on a dark tile: a Slate font draws nothing in this pass.
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);
	const FVector2D At = Pixel / DPIScale + FVector2D(20.0, 20.0);
	const double Width = 1.15 * Font->GetStringSize(*Text);
	FCanvasTileItem Backing(At - FVector2D(6.0, 4.0), FVector2D(Width + 12.0, 22.0), FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	Backing.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Backing);
	FCanvasTextItem Item(At, FText::FromString(Text), Font, FLinearColor(1.0f, 0.6f, 0.95f));
	Canvas->DrawItem(Item);
}

#undef LOCTEXT_NAMESPACE
