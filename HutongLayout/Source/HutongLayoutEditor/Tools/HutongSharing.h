#pragma once

#include "CoreMinimal.h"
#include "Tools/HutongAudit.h"

class UHutongBuildingComponent;

namespace HutongSharing
{
	struct FRule
	{
		FString Name;
		// Footprint sizes (and wall openings) land on multiples of this; zero leaves them as drawn.
		double SizeStep = 0.0;
		// Either neighbouring step, whichever the Qing figures favour (then the nearer), both sides sharing the move;
		// otherwise the nearest step, one side moving.
		bool bTowardFigures = false;
		double SquareUpTo = 0.0;
		double HeightStep = 0.0;
		// With bTowardFigures: how much further outside the figures a building may go so that a neighbour's change
		// cancels. Negative: it takes its own best size.
		double CancelSlack = -1.0;
	};

	TArray<FRule> Rules();
	// The entry of Rules() that Apply uses: each size changing about its centre, neighbours cancelling.
	inline constexpr int32 AppliedRule = 3;

	struct FChosen
	{
		FVector2D Size = FVector2D::ZeroVector;
		double Height = 0.0;
		FVector2D Shift = FVector2D::ZeroVector;
		int32 GroupSize = 0;
		double Stretch = 0.0;
		bool bHeldBack = false;
		bool bCollides = false;
		FString CollidesWith;
		double CollideDepth = 0.0;
		double DrawnDepth = 0.0;
	};

	struct FOutcome
	{
		FVector2D Size = FVector2D::ZeroVector;
		double Height = 0.0;
		int32 Bays = 0;
		bool bSquared = false;
		double EdgeMove = 0.0;
		FString Mesh;
		int32 Triangles = 0;
		int32 AuditOff = 0;
		FChosen Chosen;
		FString AuditRole;
		TArray<HutongAudit::FCheck> AuditChecks;
	};

	TMap<const UHutongBuildingComponent*, FChosen> ChooseWithNeighbours(const TArray<UHutongBuildingComponent*>& Buildings, const FRule& Rule, EHutongTracedOutline Outline);

	FOutcome Measure(const UHutongBuildingComponent* Building, const FRule& Rule, EHutongTracedOutline Outline, const FChosen* Chosen = nullptr);

	struct FRow
	{
		const UHutongBuildingComponent* Building = nullptr;
		TArray<FOutcome> Outcomes;
	};

	bool MeasureAll(const TArray<UHutongBuildingComponent*>& Buildings, EHutongTracedOutline Outline, bool bShowDialog,
		TArray<FRow>& OutRows);

	FString SummaryTable(const TArray<FRow>& Rows);

	struct FWallPieceCount
	{
		int32 Walls = 0;
		int32 Placed = 0;
		int32 Distinct = 0;
		int32 WholeWalls = 0;
		int64 TrianglesStored = 0;
		int64 TrianglesDrawn = 0;
	};
	FWallPieceCount CountWallPieces(const TArray<UHutongBuildingComponent*>& Buildings, const TArray<FRow>* Rows = nullptr);
	FString DescribeWallPieces(const FWallPieceCount& Count);

	struct FApplied
	{
		int32 Resized = 0;
		int32 Moved = 0;
		int32 HeldBack = 0;
		int32 Collides = 0;
	};
	FApplied Apply(const TArray<UHutongBuildingComponent*>& Buildings, EHutongTracedOutline Outline, bool bShowDialog);

	bool WriteCsv(const TArray<FRow>& Rows, const FString& Path);
	bool WriteChecksCsv(const TArray<FRow>& Rows, const FString& Path);
}
