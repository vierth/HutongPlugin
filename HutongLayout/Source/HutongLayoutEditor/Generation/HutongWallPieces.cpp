#include "Generation/HutongWallPieces.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongMeshTurn.h"
#include "Generation/HutongMeshUtils.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "HAL/IConsoleManager.h"

namespace
{
	using namespace HutongGen::WallPieces;

	double WholeSteps(double Cm)
	{
		return FMath::CeilToDouble(Cm / StepCm) * StepCm;
	}

	double WindowPieceCm(const FHutongWallParams& P)
	{
		return WholeSteps(FMath::Max(P.WindowSpacing, 80.0));
	}

	FVector2D SnapPieceCorner(const FVector2D& Corner)
	{
		return FVector2D(FMath::GridSnap(Corner.X, CornerStepCm), FMath::GridSnap(Corner.Y, CornerStepCm));
	}

	void PlanWallStretch(double X0, double X1, double Module, EKind Kind, TArray<FPiece>& Out)
	{
		// Filler first, modules back from the far end: the stretch's far end is exact.
		double Left = X1 - X0;
		TArray<FPiece> Modules;
		auto Take = [&](double Cm, EKind PieceKind)
		{
			while (Left >= Cm)
			{
				Left -= Cm;
				FPiece Piece;
				Piece.Kind = PieceKind;
				Piece.Length = Cm;
				Modules.Add(Piece);
			}
		};
		Take(Module, Kind);
		Take(ShortCm, EKind::Plain);
		double Cursor = X0 + Left;
		if (Left > 0.01)
		{
			FPiece Piece;
			Piece.Kind = EKind::Filler;
			Piece.X0 = X0;
			Piece.Length = FMath::Max(FMath::GridSnap(Left, FillerStepCm), FillerStepCm);
			Piece.ScaleX = Left / Piece.Length;
			Out.Add(Piece);
		}
		for (int32 i = Modules.Num() - 1; i >= 0; --i)
		{
			Modules[i].X0 = Cursor;
			Cursor += Modules[i].Length;
			Out.Add(Modules[i]);
		}
	}
}

namespace HutongGen::WallPieces
{
	static TAutoConsoleVariable<bool> CVarWallPieces(
		TEXT("hutong.WallPieces"), true,
		TEXT("Walls build from repeated pieces, instanced on each wall's actor, instead of one mesh each."));

	bool IsEnabled()
	{
		return CVarWallPieces.GetValueOnGameThread();
	}

	TArray<FPiece> Plan(const FHutongWallParams& Params, double Length, double Thickness, double StartExtend,
		double EndExtend, const FHutongFootprintSkew& Skew)
	{
		const double L = FMath::Max(Length, 1.0);
		const double RunX0 = -FMath::Clamp(StartExtend, 0.0, 3.0 * Thickness);
		const double RunX1 = L + FMath::Clamp(EndExtend, 0.0, 3.0 * Thickness);

		TArray<FPiece> Openings;
		bool bFits = Skew.IsZero() || Skew.Mode == EHutongSkewMode::Ends;
		const TArray<FHutongWallGateSpan> Gates = Params.GetGateSpans(L);
		for (const FHutongWallGateSpan& Gate : Gates)
		{
			const double From = FMath::Min(Gate.MasonryX0, Gate.PierX0);
			const double To = FMath::Max(Gate.MasonryX1, Gate.PierX1);
			FPiece Piece;
			Piece.Kind = EKind::Gate;
			Piece.Length = WholeSteps(To - From);
			Piece.X0 = 0.5 * (From + To - Piece.Length);
			Openings.Add(Piece);
		}
		if (Params.HasDecorativeDoorway(L))
		{
			FPiece Piece;
			Piece.Kind = EKind::Doorway;
			Piece.Length = WholeSteps(Params.GetDoorwayRunNeed());
			Piece.X0 = Params.GetDoorwayCentre(L) - 0.5 * Piece.Length;
			// BuildWall drops a doorway that would collide with a gate.
			const bool bClear = !Openings.ContainsByPredicate([&Piece](const FPiece& Gate)
				{ return Piece.X0 < Gate.X0 + Gate.Length && Gate.X0 < Piece.X0 + Piece.Length; });
			if (bClear) Openings.Add(Piece);
		}
		Openings.Sort([](const FPiece& A, const FPiece& B) { return A.X0 < B.X0; });

		TArray<FPiece> Pieces;
		const bool bWindows = Params.GetWindowCount(L) > 0;
		const double Module = bWindows ? WindowPieceCm(Params) : LongCm;
		const EKind Kind = bWindows ? EKind::Window : EKind::Plain;
		double Cursor = RunX0;
		for (const FPiece& Opening : Openings)
		{
			if (Opening.X0 < Cursor) bFits = false;
			PlanWallStretch(Cursor, Opening.X0, Module, Kind, Pieces);
			Pieces.Add(Opening);
			Cursor = Opening.X0 + Opening.Length;
		}
		if (Cursor > RunX1) bFits = false;
		if (!bFits)
		{
			FPiece Whole;
			Whole.Kind = EKind::Whole;
			Whole.Length = L;
			Whole.Skew = Skew;
			return { Whole };
		}
		PlanWallStretch(Cursor, RunX1, Module, Kind, Pieces);

		for (int32 i = 0; i < Pieces.Num(); ++i)
		{
			Pieces[i].bRunsOnStart = i > 0;
			Pieces[i].bRunsOnEnd = i < Pieces.Num() - 1;
		}
		if (Pieces.Num() > 0)
		{
			Pieces[0].Skew.Corner00 = SnapPieceCorner(Skew.Corner00);
			Pieces[0].Skew.Corner01 = SnapPieceCorner(Skew.Corner01);
			Pieces.Last().Skew.Corner10 = SnapPieceCorner(Skew.Corner10);
			Pieces.Last().Skew.Corner11 = SnapPieceCorner(Skew.Corner11);
		}
		return Pieces;
	}

	FHutongWallParams PieceParams(const FHutongWallParams& Params, const FPiece& Piece)
	{
		FHutongWallParams P = Params;
		if (Piece.Kind == EKind::Whole) return P;
		P.StartExtend = P.EndExtend = 0.0;
		P.MoreGatePositions.Reset();
		P.GatePosition = P.DoorwayPosition = 0.5;
		P.bHasGate = Piece.Kind == EKind::Gate;
		if (Piece.Kind != EKind::Doorway) P.Doorway = EHutongWallDoorway::None;
		P.bHasWindows = Piece.Kind == EKind::Window;
		// Rows counted back from where a filler meets its module.
		P.CapRowOrigin = Piece.Kind == EKind::Filler ? Piece.Length : 0.0;
		P.bCapRunsOnStart = Piece.bRunsOnStart;
		P.bCapRunsOnEnd = Piece.bRunsOnEnd;
		return P;
	}

	void BuildPiece(const FHutongWallParams& Params, const FPiece& Piece, double Thickness,
		UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Detail)
	{
		UHutongWallBuildingComponent::BuildWallMesh(PieceParams(Params, Piece), Piece.Length, Thickness, /*bAlongY*/ false, OutMesh, Detail);
		if (!Piece.Skew.IsZero())
		{
			FHutongFootprintSkew Skew = Piece.Skew;
			Skew.RunAxis = 0;
			HutongMeshUtils::WarpFootprint(OutMesh, Piece.Length, Thickness, Skew);
		}
		if (Piece.Kind == EKind::Filler)
		{
			HutongMeshUtils::TransformVerticesFrom(OutMesh, 0, FTransform(FVector(-Piece.Length, 0.0, 0.0)));
		}
	}

	int32 BuildPieceLODs(const FHutongWallParams& Params, const FPiece& Piece, double Thickness, EHutongDetail Detail,
		bool bLODChain, TArray<UE::Geometry::FDynamicMesh3>& OutLODs)
	{
		HutongGen::FMeshTurn Turn;
		return HutongGen::Detail::BuildPlacementLODs(false, Piece.Length, Thickness, Detail, bLODChain,
			[&Params, &Piece, Thickness](UE::Geometry::FDynamicMesh3& LOD, EHutongDetail Level)
			{
				BuildPiece(Params, Piece, Thickness, LOD, Level);
			}, OutLODs, &Turn);
	}

	FString PieceIdentity(const FHutongWallParams& Params, const FPiece& Piece, double Thickness)
	{
		const FHutongWallParams Built = PieceParams(Params, Piece);
		FString Text;
		FHutongWallParams::StaticStruct()->ExportText(Text, &Built, nullptr, nullptr, PPF_None, nullptr);
		return FString::Printf(TEXT("%s|%d|%.2f|%.2f|%.2f|%d%d|%s%s%s%s"), *Text, (int32)Piece.Kind, Piece.Length, Built.CapRowOrigin, Thickness,
			Piece.bRunsOnStart ? 1 : 0, Piece.bRunsOnEnd ? 1 : 0,
			*Piece.Skew.Corner00.ToString(), *Piece.Skew.Corner10.ToString(), *Piece.Skew.Corner11.ToString(), *Piece.Skew.Corner01.ToString());
	}
}
