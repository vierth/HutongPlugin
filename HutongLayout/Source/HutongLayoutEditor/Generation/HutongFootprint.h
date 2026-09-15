#pragma once

#include "CoreMinimal.h"
#include "HutongFootprint.generated.h"

// How the corner offsets reach the mesh.
// Ends: only a zone at each end of the run moves — the end is cut on the bias, or pushed across,
// and the body stays exactly as built, which is what a wall meeting an off-square neighbour
// wants, and a house whose one gable follows the street. The corner may be pulled along the run
// or across it; the drag lets it go one way at a time, the offset itself carries both.
// Whole: every vertex moves by the bilinear blend of the four offsets, in any direction — a true
// trapezoid plan, bays fanning with it.
UENUM(BlueprintType)
enum class EHutongSkewMode : uint8
{
	Ends UMETA(DisplayName="Ends only (端斜)"),
	Whole UMETA(DisplayName="Whole footprint (整體)"),
};

// A footprint that is not a rectangle. Every generator builds a rectangle at the actor's origin;
// this is how much each of its four corners is then moved, in the actor's local XY, so a house
// on a lot that meets the street off square can follow it. Zero on all four is the rectangle.
// Corners are numbered from the origin anticlockwise: (0,0), (W,0), (W,D), (0,D).
USTRUCT(BlueprintType)
struct FHutongFootprintSkew
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Mode (方式)", ToolTip="Which part of the footprint the corner offsets move."))
	EHutongSkewMode Mode = EHutongSkewMode::Ends;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Corner at Origin (原點角)", Units="cm", ToolTip="Offset of the corner at the actor's origin, in local X and Y, in cm."))
	FVector2D Corner00 = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Corner +X (X端角)", Units="cm", ToolTip="Offset of the corner at the far end of local X, in local X and Y, in cm."))
	FVector2D Corner10 = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Corner +X +Y (對角)", Units="cm", ToolTip="Offset of the corner opposite the origin, in local X and Y, in cm."))
	FVector2D Corner11 = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Corner +Y (Y端角)", Units="cm", ToolTip="Offset of the corner at the far end of local Y, in local X and Y, in cm."))
	FVector2D Corner01 = FVector2D::ZeroVector;

	FVector2D Get(int32 Corner) const
	{
		switch (Corner)
		{
		case 1:  return Corner10;
		case 2:  return Corner11;
		case 3:  return Corner01;
		default: return Corner00;
		}
	}

	void Set(int32 Corner, const FVector2D& Offset)
	{
		switch (Corner)
		{
		case 1:  Corner10 = Offset; break;
		case 2:  Corner11 = Offset; break;
		case 3:  Corner01 = Offset; break;
		default: Corner00 = Offset; break;
		}
	}

	bool IsZero() const
	{
		return Corner00.IsNearlyZero() && Corner10.IsNearlyZero()
			&& Corner11.IsNearlyZero() && Corner01.IsNearlyZero();
	}

	bool operator==(const FHutongFootprintSkew& Other) const
	{
		return Mode == Other.Mode && Corner00 == Other.Corner00 && Corner10 == Other.Corner10
			&& Corner11 == Other.Corner11 && Corner01 == Other.Corner01;
	}
	bool operator!=(const FHutongFootprintSkew& Other) const { return !(*this == Other); }
};

namespace HutongFootprint
{
	// The run is the long axis: the ends a bias cut belongs to are its ends.
	inline bool RunAlongX(const FVector2D& Size) { return Size.X >= Size.Y; }

	// The offset of a corner as the mode lets it stand: both modes now keep both components, and
	// the name stays as the one seam a future mode would narrow it at.
	inline FVector2D EffectiveOffset(const FVector2D& Size, const FHutongFootprintSkew& Skew, int32 Corner)
	{
		return Skew.Get(Corner);
	}

	// The four local corners, rectangle plus offsets, in the order the skew numbers them.
	inline void Corners(const FVector2D& Size, const FHutongFootprintSkew& Skew, FVector2D OutCorners[4])
	{
		OutCorners[0] = FVector2D(0.0, 0.0) + EffectiveOffset(Size, Skew, 0);
		OutCorners[1] = FVector2D(Size.X, 0.0) + EffectiveOffset(Size, Skew, 1);
		OutCorners[2] = FVector2D(Size.X, Size.Y) + EffectiveOffset(Size, Skew, 2);
		OutCorners[3] = FVector2D(0.0, Size.Y) + EffectiveOffset(Size, Skew, 3);
	}

	// Where a point of the rectangle lands on the quadrilateral under Whole: the bilinear blend of
	// the four corners weighted by where the point sits in the rectangle. The identity on a
	// rectangle, and defined outside it too, which is how eaves and platforms follow the wall.
	inline FVector2D MapWhole(const FVector2D& Size, const FVector2D C[4], double X, double Y)
	{
		const double U = X / FMath::Max(Size.X, 1.0e-6);
		const double V = Y / FMath::Max(Size.Y, 1.0e-6);
		return C[0] * ((1.0 - U) * (1.0 - V)) + C[1] * (U * (1.0 - V))
			 + C[2] * (U * V) + C[3] * ((1.0 - U) * V);
	}

	// Under Ends, the two corners at one end of the run: the one on the across == 0 side first,
	// then the across == T side. bStart is the origin end.
	inline void EndCorners(const FVector2D& Size, bool bStart, int32& OutA, int32& OutB)
	{
		const bool bX = RunAlongX(Size);
		OutA = bStart ? 0 : (bX ? 1 : 3);
		OutB = bStart ? (bX ? 3 : 1) : 2;
	}

	// Under Ends, the along-run offsets at one end of the run, in EndCorners order.
	inline void EndOffsets(const FVector2D& Size, const FHutongFootprintSkew& Skew, bool bStart, double& OutA, double& OutB)
	{
		int32 CornerA, CornerB;
		EndCorners(Size, bStart, CornerA, CornerB);
		const bool bX = RunAlongX(Size);
		OutA = bX ? Skew.Get(CornerA).X : Skew.Get(CornerA).Y;
		OutB = bX ? Skew.Get(CornerB).X : Skew.Get(CornerB).Y;
	}

	// The same across the run: positive is towards the across == T side.
	inline void EndAcrossOffsets(const FVector2D& Size, const FHutongFootprintSkew& Skew, bool bStart, double& OutA, double& OutB)
	{
		int32 CornerA, CornerB;
		EndCorners(Size, bStart, CornerA, CornerB);
		const bool bX = RunAlongX(Size);
		OutA = bX ? Skew.Get(CornerA).Y : Skew.Get(CornerA).X;
		OutB = bX ? Skew.Get(CornerB).Y : Skew.Get(CornerB).X;
	}

	// How far in from that end the body starts to move: the across size plus half again the
	// largest offset there, either way, never past the middle of the run. Nothing nearer the middle moves.
	inline double EndZone(const FVector2D& Size, const FHutongFootprintSkew& Skew, bool bStart)
	{
		const double L = RunAlongX(Size) ? Size.X : Size.Y;
		const double T = RunAlongX(Size) ? Size.Y : Size.X;
		double A, B, AA, AB;
		EndOffsets(Size, Skew, bStart, A, B);
		EndAcrossOffsets(Size, Skew, bStart, AA, AB);
		const double MaxAbs = FMath::Max(FMath::Max(FMath::Abs(A), FMath::Abs(B)), FMath::Max(FMath::Abs(AA), FMath::Abs(AB)));
		if (MaxAbs < 1.0e-6) return 0.0;
		return FMath::Clamp(T + 1.5 * MaxAbs, 20.0, 0.5 * L);
	}

	// Where a point of the rectangle lands, whichever the mode.
	inline FVector2D Map(const FVector2D& Size, const FHutongFootprintSkew& Skew, double X, double Y)
	{
		if (Skew.Mode == EHutongSkewMode::Whole)
		{
			FVector2D C[4];
			Corners(Size, Skew, C);
			return MapWhole(Size, C, X, Y);
		}
		const bool bX = RunAlongX(Size);
		const double L = bX ? Size.X : Size.Y;
		const double T = bX ? Size.Y : Size.X;
		const double R = bX ? X : Y;
		const double Across = bX ? Y : X;
		const double S = Across / FMath::Max(T, 1.0e-6);
		// A course standing proud of a long face (S outside 0..1: a 墀頭, a cap, the 下鹼) follows
		// the cut plane, but no steeper than 45° to the run past its corner: on the plane's own
		// continuation a course proud by c reaches c / tan(angle) past the corner, and a cap on a
		// 37 cm wall cut at 3° ran out two metres as a blade. A house's gable at a few degrees is
		// well under the limit and its 墀頭 stays on the plane.
		const double Sc = FMath::Clamp(S, 0.0, 1.0);
		auto Along = [&](double A, double B) { return FMath::Lerp(A, B, Sc) + (S - Sc) * FMath::Clamp(B - A, -T, T); };
		// Along the run and across it, each fading from the corner's full offset at the end to
		// nothing at the zone seam: the end face stays a straight line between its two corners,
		// and each long face runs straight from the seam to its moved corner.
		double DR = 0.0, DA = 0.0;
		const double ZStart = EndZone(Size, Skew, true);
		const double ZEnd = EndZone(Size, Skew, false);
		if (ZStart > 0.0 && R < ZStart)
		{
			double A, B, AA, AB;
			EndOffsets(Size, Skew, true, A, B);
			EndAcrossOffsets(Size, Skew, true, AA, AB);
			const double W = (ZStart - R) / ZStart;
			DR += W * Along(A, B);
			DA += W * Along(AA, AB);
		}
		if (ZEnd > 0.0 && R > L - ZEnd)
		{
			double A, B, AA, AB;
			EndOffsets(Size, Skew, false, A, B);
			EndAcrossOffsets(Size, Skew, false, AA, AB);
			const double W = (R - (L - ZEnd)) / ZEnd;
			DR += W * Along(A, B);
			DA += W * Along(AA, AB);
		}
		return bX ? FVector2D(X + DR, Y + DA) : FVector2D(X + DA, Y + DR);
	}

	inline double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

	// A quadrilateral the warp can be trusted on: strictly convex, wound the way the rectangle is,
	// no edge shorter than MinEdge. The warp's determinant is bilinear in the rectangle's
	// coordinates, so positive at the four corners is positive everywhere between them.
	inline bool IsQuadValid(const FVector2D C[4], double MinEdge = 20.0)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2D E0 = C[(i + 1) % 4] - C[i];
			const FVector2D E1 = C[(i + 2) % 4] - C[(i + 1) % 4];
			if (E0.Size() < MinEdge) return false;
			if (Cross(E0, E1) <= 0.0) return false;
		}
		return true;
	}

	// Inside a convex quadrilateral wound anticlockwise, edges included.
	inline bool PointInQuad(const FVector2D C[4], const FVector2D& P)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (Cross(C[(i + 1) % 4] - C[i], P - C[i]) < 0.0) return false;
		}
		return true;
	}

	inline double DistanceToSegment(const FVector2D& A, const FVector2D& B, const FVector2D& P)
	{
		const FVector2D AB = B - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > 0.0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		return FVector2D::Distance(P, A + AB * T);
	}

	// The nearest edge, for the band a press must clear to count as the inside.
	inline double DistanceToQuadEdge(const FVector2D C[4], const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0; i < 4; ++i)
		{
			Best = FMath::Min(Best, DistanceToSegment(C[i], C[(i + 1) % 4], P));
		}
		return Best;
	}

	// Whether the offsets, as the mode reads them, build: under Whole a convex quadrilateral; under
	// Ends no corner pulled back past the zone it moves in, so the end face never folds over the
	// body. A corner pushed across past the other corner's line fails the convexity check.
	inline bool IsSkewValid(const FVector2D& Size, const FHutongFootprintSkew& Skew, double MinEdge = 20.0)
	{
		FVector2D C[4];
		Corners(Size, Skew, C);
		if (!IsQuadValid(C, MinEdge)) return false;
		if (Skew.Mode == EHutongSkewMode::Ends)
		{
			for (const bool bStart : { true, false })
			{
				const double Z = EndZone(Size, Skew, bStart);
				if (Z <= 0.0) continue;
				double A, B;
				EndOffsets(Size, Skew, bStart, A, B);
				// Outward is positive at the far end and negative at the origin end.
				const double InA = bStart ? A : -A, InB = bStart ? B : -B;
				if (InA > Z - MinEdge || InB > Z - MinEdge) return false;
			}
		}
		return true;
	}

	inline double QuadArea(const FVector2D C[4])
	{
		double Twice = 0.0;
		for (int32 i = 0; i < 4; ++i) Twice += Cross(C[i], C[(i + 1) % 4]);
		return 0.5 * FMath::Abs(Twice);
	}

	// A footprint that is a run rather than a room: a wall, a corridor, a path. Its plan has a
	// length and a thickness, and moving a corner of it means nothing.
	inline bool IsLineLike(const FVector2D& Size)
	{
		const double Lo = FMath::Min(Size.X, Size.Y), Hi = FMath::Max(Size.X, Size.Y);
		return Lo < 0.35 * Hi && Lo < 150.0;
	}
}
