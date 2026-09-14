#include "Tools/HutongWallRun.h"
#include "Generation/HutongBuildingComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	constexpr double JoinTolerance = 3.0;
	constexpr double ThicknessTolerance = 0.5;

	double ThicknessOf(const UHutongWallBuildingComponent* Leg) { return Leg->GetBuiltThickness(); }
}

namespace HutongWallRun
{
	bool FRun::Contains(const UHutongWallBuildingComponent* Leg) const
	{
		for (const TWeakObjectPtr<UHutongWallBuildingComponent>& L : Legs)
		{
			if (L.Get() == Leg) return true;
		}
		return false;
	}

	void LegEnds(const UHutongWallBuildingComponent* Leg, FVector2D& OutStart, FVector2D& OutEnd)
	{
		const AActor* Owner = Leg ? Leg->GetOwner() : nullptr;
		if (!Owner) { OutStart = OutEnd = FVector2D::ZeroVector; return; }
		const FTransform Xf = Owner->GetActorTransform();
		const double T = ThicknessOf(Leg);
		const double L = Leg->Length;
		const FVector S = Leg->bLengthAlongY ? FVector(0.5 * T, 0.0, 0.0) : FVector(0.0, 0.5 * T, 0.0);
		const FVector E = Leg->bLengthAlongY ? FVector(0.5 * T, L, 0.0) : FVector(L, 0.5 * T, 0.0);
		const FVector WS = Xf.TransformPosition(S), WE = Xf.TransformPosition(E);
		OutStart = FVector2D(WS.X, WS.Y);
		OutEnd = FVector2D(WE.X, WE.Y);
	}

	HutongWallChain::FEndFace OuterFace(const UHutongWallBuildingComponent* Leg, bool bStart)
	{
		HutongWallChain::FEndFace Face;
		const AActor* Owner = Leg ? Leg->GetOwner() : nullptr;
		if (!Owner) return Face;
		// Corners from the origin anticlockwise: the run's start edge is 0-3 and its end edge 1-2
		// along X; along Y the start edge is 0-1 and the end edge 3-2.
		const FHutongFootprintSkew& K = Leg->FootprintSkew;
		const bool bY = Leg->bLengthAlongY;
		const int32 A = bStart ? 0 : (bY ? 3 : 1);
		const int32 B = bStart ? (bY ? 1 : 3) : 2;
		if (K.Get(A).IsNearlyZero() && K.Get(B).IsNearlyZero()) return Face;
		FVector2D Local[4];
		Leg->GetFootprintCorners(Local);
		const FTransform Xf = Owner->GetActorTransform();
		const FVector WA = Xf.TransformPosition(FVector(Local[A].X, Local[A].Y, 0.0));
		const FVector WB = Xf.TransformPosition(FVector(Local[B].X, Local[B].Y, 0.0));
		Face.bSet = true;
		Face.Point = FVector2D(WA.X, WA.Y);
		Face.Dir = FVector2D(WB.X - WA.X, WB.Y - WA.Y).GetSafeNormal();
		return Face;
	}

	bool Gather(UHutongWallBuildingComponent* Seed, const TArray<UHutongWallBuildingComponent*>& Candidates, FRun& Out)
	{
		Out = FRun();
		if (!Seed || !Seed->GetOwner()) return false;
		const double T = ThicknessOf(Seed);

		auto Joined = [&](UHutongWallBuildingComponent* From, bool bForward) -> UHutongWallBuildingComponent*
		{
			FVector2D FS, FE;
			LegEnds(From, FS, FE);
			const FVector2D& At = bForward ? FE : FS;
			for (UHutongWallBuildingComponent* C : Candidates)
			{
				if (!C || C == From || !C->GetOwner() || Out.Contains(C)) continue;
				if (FMath::Abs(ThicknessOf(C) - T) > ThicknessTolerance) continue;
				FVector2D CS, CE;
				LegEnds(C, CS, CE);
				if (FVector2D::Distance(bForward ? CS : CE, At) <= JoinTolerance) return C;
			}
			return nullptr;
		};

		TArray<UHutongWallBuildingComponent*> Legs = { Seed };
		Out.Legs.Add(Seed);
		for (UHutongWallBuildingComponent* Next = Joined(Seed, true); Next; Next = Joined(Next, true))
		{
			Legs.Add(Next);
			Out.Legs.Add(Next);
		}
		for (UHutongWallBuildingComponent* Prev = Joined(Seed, false); Prev; Prev = Joined(Prev, false))
		{
			Legs.Insert(Prev, 0);
			Out.Legs.Insert(Prev, 0);
		}

		Out.Thickness = T;
		Out.Z = Seed->GetOwner()->GetActorLocation().Z;
		FVector2D S, E;
		LegEnds(Legs[0], S, E);
		Out.Vertices.Add(S);
		for (UHutongWallBuildingComponent* Leg : Legs)
		{
			LegEnds(Leg, S, E);
			Out.Vertices.Add(E);
		}
		Out.StartFace = OuterFace(Legs[0], true);
		Out.EndFace = OuterFace(Legs.Last(), false);
		return true;
	}

	bool Rebuild(const FRun& Run, const TArray<FVector2D>& Vertices,
		const HutongWallChain::FEndFace& StartFace, const HutongWallChain::FEndFace& EndFace,
		TArray<HutongWallChain::FSegment>& OutSegments)
	{
		if (Vertices.Num() != Run.NumLegs() + 1) return false;
		// Every vertex is kept: a leg pulled short is a short leg, not a dropped one.
		return HutongWallChain::Build(Vertices, Run.Thickness, 0.5 * Run.Thickness, StartFace, EndFace, OutSegments, 0.0)
			&& OutSegments.Num() == Run.NumLegs();
	}

	void Apply(const FRun& Run, const TArray<HutongWallChain::FSegment>& Segments)
	{
		for (int32 i = 0; i < Run.NumLegs() && i < Segments.Num(); ++i)
		{
			UHutongWallBuildingComponent* Leg = Run.Legs[i].Get();
			AActor* Owner = Leg ? Leg->GetOwner() : nullptr;
			if (!Owner) continue;
			const HutongWallChain::FSegment& S = Segments[i];
			const FTransform Old = Owner->GetActorTransform();
			Owner->SetActorTransform(FTransform(FRotator(0.0, S.YawDeg, 0.0), FVector(S.Origin.X, S.Origin.Y, Run.Z), Old.GetScale3D()));
			Leg->SetRunAlongY(false);
			Leg->SetFootprintSize(FVector2D(FMath::Max(S.Length, 10.0), Run.Thickness));
			Leg->FootprintSkew = S.Skew;
			// The corner offsets are the joins; the square extension is the other way and they do not stack.
			Leg->StartExtend = 0.0;
			Leg->EndExtend = 0.0;
		}
	}
}
