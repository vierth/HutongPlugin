#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongFootprint.h"
#include "Generation/HutongDetail.h"
#include "Generation/WallGenerator.h"

namespace UE::Geometry { class FDynamicMesh3; }

// Lengths are whole 120 cm steps: whole 24 cm brick faces and 20 cm cap 壟, so courses and tile rows carry across a joint.
namespace HutongGen::WallPieces
{
	constexpr double StepCm = 120.0;
	constexpr double LongCm = 2.0 * StepCm;
	constexpr double ShortCm = StepCm;
	constexpr double FillerStepCm = 8.0;
	constexpr double CornerStepCm = 2.0;

	bool IsEnabled();

	enum class EKind : uint8 { Plain, Window, Gate, Doorway, Filler, Whole };

	struct FPiece
	{
		EKind Kind = EKind::Plain;
		double X0 = 0.0;
		double Length = 0.0;
		// A filler is built to a whole step and stretched this much to fit exactly.
		double ScaleX = 1.0;
		bool bRunsOnStart = false;
		bool bRunsOnEnd = false;
		FHutongFootprintSkew Skew;
	};

	// An opening that will not fit a piece, or Whole-mode corners, gives one Whole piece.
	TArray<FPiece> Plan(const FHutongWallParams& Params, double Length, double Thickness, double StartExtend,
		double EndExtend, const FHutongFootprintSkew& Skew);

	// Only what the piece carries, so like pieces of unlike walls agree.
	FHutongWallParams PieceParams(const FHutongWallParams& Params, const FPiece& Piece);

	// A filler is built back from its far end, so its courses match the module it meets.
	inline double OriginX(const FPiece& Piece)
	{
		return Piece.Kind == EKind::Filler ? Piece.X0 + Piece.Length * Piece.ScaleX : Piece.X0;
	}

	void BuildPiece(const FHutongWallParams& Params, const FPiece& Piece, double Thickness,
		UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Detail);

	int32 BuildPieceLODs(const FHutongWallParams& Params, const FPiece& Piece, double Thickness, EHutongDetail Detail,
		bool bLODChain, TArray<UE::Geometry::FDynamicMesh3>& OutLODs);

	FString PieceIdentity(const FHutongWallParams& Params, const FPiece& Piece, double Thickness);
}
