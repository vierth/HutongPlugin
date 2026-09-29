#pragma once

#include "CoreMinimal.h"
#include "HutongFootprint.generated.h"

// How corner offsets move the mesh.
// Ends: only a zone at each end moves (cut on the bias or pushed across); the body stays as built.
// Whole: every vertex moves by a bilinear blend of the four offsets — a true trapezoid plan.
UENUM(BlueprintType)
enum class EHutongSkewMode : uint8
{
	Ends UMETA(DisplayName="Ends only (端斜)"),
	Whole UMETA(DisplayName="Whole footprint (整體)"),
};

// Off-square footprint: offsets of the built rectangle's four corners in actor-local XY; all zero = rectangle.
// Corners anticlockwise from the origin: (0,0), (W,0), (W,D), (0,D).
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

	// Ends: the end bay's reach from each end of the run (origin end, far end), filled from the
	// building's bays by UHutongBuildingComponent::GetFootprintSkew; zero = the generic zone. Derived,
	// never saved or compared.
	double EndBayReach[2] = { 0.0, 0.0 };

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
	// The run is the long axis; bias cuts apply at its ends.
	inline bool RunAlongX(const FVector2D& Size) { return Size.X >= Size.Y; }

	// Corner offset as the mode reads it. Both modes keep both components; the seam a future mode would narrow.
	inline FVector2D EffectiveOffset(const FVector2D& Size, const FHutongFootprintSkew& Skew, int32 Corner)
	{
		return Skew.Get(Corner);
	}

	// Local corners (rectangle + offsets), in skew order.
	inline void Corners(const FVector2D& Size, const FHutongFootprintSkew& Skew, FVector2D OutCorners[4])
	{
		OutCorners[0] = FVector2D(0.0, 0.0) + EffectiveOffset(Size, Skew, 0);
		OutCorners[1] = FVector2D(Size.X, 0.0) + EffectiveOffset(Size, Skew, 1);
		OutCorners[2] = FVector2D(Size.X, Size.Y) + EffectiveOffset(Size, Skew, 2);
		OutCorners[3] = FVector2D(0.0, Size.Y) + EffectiveOffset(Size, Skew, 3);
	}

	// Whole: bilinear map of a rectangle point onto the quad. Defined outside the rectangle too, so
	// eaves and platforms follow the wall.
	inline FVector2D MapWhole(const FVector2D& Size, const FVector2D C[4], double X, double Y)
	{
		const double U = X / FMath::Max(Size.X, 1.0e-6);
		const double V = Y / FMath::Max(Size.Y, 1.0e-6);
		return C[0] * ((1.0 - U) * (1.0 - V)) + C[1] * (U * (1.0 - V))
			 + C[2] * (U * V) + C[3] * ((1.0 - U) * V);
	}

	// Ends: the two corners at one end, across == 0 first, then across == T. bStart = origin end.
	inline void EndCorners(const FVector2D& Size, bool bStart, int32& OutA, int32& OutB)
	{
		const bool bX = RunAlongX(Size);
		OutA = bStart ? 0 : (bX ? 1 : 3);
		OutB = bStart ? (bX ? 3 : 1) : 2;
	}

	// Ends: along-run offsets at one end, in EndCorners order.
	inline void EndOffsets(const FVector2D& Size, const FHutongFootprintSkew& Skew, bool bStart, double& OutA, double& OutB)
	{
		int32 CornerA, CornerB;
		EndCorners(Size, bStart, CornerA, CornerB);
		const bool bX = RunAlongX(Size);
		OutA = bX ? Skew.Get(CornerA).X : Skew.Get(CornerA).Y;
		OutB = bX ? Skew.Get(CornerB).X : Skew.Get(CornerB).Y;
	}

	// Across-run offsets; positive toward across == T.
	inline void EndAcrossOffsets(const FVector2D& Size, const FHutongFootprintSkew& Skew, bool bStart, double& OutA, double& OutB)
	{
		int32 CornerA, CornerB;
		EndCorners(Size, bStart, CornerA, CornerB);
		const bool bX = RunAlongX(Size);
		OutA = bX ? Skew.Get(CornerA).Y : Skew.Get(CornerA).X;
		OutB = bX ? Skew.Get(CornerB).Y : Skew.Get(CornerB).X;
	}

	// End zone length: the end bay where the building has bays (a slid corner widens or narrows that
	// bay alone), else across size plus 1.5× the largest offset there; capped at mid-run.
	inline double EndZone(const FVector2D& Size, const FHutongFootprintSkew& Skew, bool bStart)
	{
		const double L = RunAlongX(Size) ? Size.X : Size.Y;
		const double T = RunAlongX(Size) ? Size.Y : Size.X;
		double A, B, AA, AB;
		EndOffsets(Size, Skew, bStart, A, B);
		EndAcrossOffsets(Size, Skew, bStart, AA, AB);
		const double MaxAbs = FMath::Max(FMath::Max(FMath::Abs(A), FMath::Abs(B)), FMath::Max(FMath::Abs(AA), FMath::Abs(AB)));
		if (MaxAbs < 1.0e-6) return 0.0;
		const double Bay = Skew.EndBayReach[bStart ? 0 : 1];
		return FMath::Clamp(Bay > 0.0 ? Bay : T + 1.5 * MaxAbs, 20.0, 0.5 * L);
	}

	// Map a rectangle point under either mode.
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
		// Courses proud of a long face (S outside 0..1: 墀頭, cap, 下鹼) follow the cut plane, capped at
		// 45°: uncapped, proud c reaches c/tan(angle) past the corner (a 3° cut ran a cap out 2 m).
		const double Sc = FMath::Clamp(S, 0.0, 1.0);
		auto Along = [&](double A, double B) { return FMath::Lerp(A, B, Sc) + (S - Sc) * FMath::Clamp(B - A, -T, T); };
		// Along and across offsets fade from full at the end to zero at the seam: end face and long
		// faces stay straight.
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

	// Strictly convex, rectangle winding, edges ≥ MinEdge. Determinant is bilinear, so positive at
	// the corners means positive throughout.
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

	// Inside a CCW convex quad, edges included.
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

	// Distance to nearest edge (press must clear this band to count as inside).
	inline double DistanceToQuadEdge(const FVector2D C[4], const FVector2D& P)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0; i < 4; ++i)
		{
			Best = FMath::Min(Best, DistanceToSegment(C[i], C[(i + 1) % 4], P));
		}
		return Best;
	}

	// Buildable: Whole needs a convex quad; Ends needs no corner pulled back past its zone (no fold),
	// and a corner pushed across past the other's line fails convexity.
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
				// Outward: positive at the far end, negative at the origin end.
				const double InA = bStart ? A : -A, InB = bStart ? B : -B;
				if (InA > Z - MinEdge || InB > Z - MinEdge) return false;
			}
		}
		return true;
	}

	// Whether a convex quad and an axis-aligned box share any area or edge (separating axes: the box's
	// two and the quad's four edge normals). A box across a footprint picks it without holding a corner.
	inline bool QuadOverlapsBox(const FVector2D C[4], const FVector2D& Lo, const FVector2D& Hi)
	{
		const FVector2D Box[4] = { Lo, FVector2D(Hi.X, Lo.Y), Hi, FVector2D(Lo.X, Hi.Y) };
		auto Separated = [&](const FVector2D& Axis)
		{
			double AMin = TNumericLimits<double>::Max(), AMax = -AMin, BMin = AMin, BMax = -AMin;
			for (int32 i = 0; i < 4; ++i)
			{
				const double A = FVector2D::DotProduct(C[i], Axis), B = FVector2D::DotProduct(Box[i], Axis);
				AMin = FMath::Min(AMin, A); AMax = FMath::Max(AMax, A);
				BMin = FMath::Min(BMin, B); BMax = FMath::Max(BMax, B);
			}
			return AMax < BMin || BMax < AMin;
		};
		if (Separated(FVector2D(1.0, 0.0)) || Separated(FVector2D(0.0, 1.0))) return false;
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2D E = C[(i + 1) % 4] - C[i];
			if (Separated(FVector2D(-E.Y, E.X))) return false;
		}
		return true;
	}

	inline double QuadArea(const FVector2D C[4])
	{
		double Twice = 0.0;
		for (int32 i = 0; i < 4; ++i) Twice += Cross(C[i], C[(i + 1) % 4]);
		return 0.5 * FMath::Abs(Twice);
	}

	// A run, not a room (wall, corridor, path): corner offsets mean nothing.
	inline bool IsLineLike(const FVector2D& Size)
	{
		const double Lo = FMath::Min(Size.X, Size.Y), Hi = FMath::Max(Size.X, Size.Y);
		return Lo < 0.35 * Hi && Lo < 150.0;
	}
}
